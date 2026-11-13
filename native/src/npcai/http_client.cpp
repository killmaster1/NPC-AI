#include "http_client.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace npcai {
namespace {
using Clock=std::chrono::steady_clock;
struct Socket { int fd=-1; ~Socket(){if(fd>=0) close(fd);} };
bool Wait(int fd, short events, Clock::time_point end, const std::atomic_bool& cancel) {
    while(!cancel.load()) {
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(end-Clock::now()).count();
        if(ms<=0) return false;
        pollfd p{fd,events,0};
        int result=poll(&p,1,static_cast<int>(std::min<int64_t>(ms,50)));
        if(result>0) return (p.revents&(events|POLLHUP|POLLERR))!=0;
        if(result<0 && errno!=EINTR) return false;
    }
    return false;
}
bool Size(const std::string& s, size_t& n) {
    if(s.empty() || s.size()>8) return false;
    n=0;
    for(unsigned char c:s) { if(c<'0'||c>'9') return false; n=n*10+c-'0'; }
    return true;
}
bool Headers(const std::string& h, int& status, size_t& length) {
    auto e=h.find("\r\n");
    std::string line=h.substr(0,e);
    if(line.size()<12 || (line.compare(0,9,"HTTP/1.0 ") && line.compare(0,9,"HTTP/1.1 ")) ||
       !std::isdigit(static_cast<unsigned char>(line[9])) || !std::isdigit(static_cast<unsigned char>(line[10])) ||
       !std::isdigit(static_cast<unsigned char>(line[11])) || (line.size()>12 && line[12]!=' ')) return false;
    status=(line[9]-'0')*100+(line[10]-'0')*10+line[11]-'0';
    bool found=false;
    while(e!=std::string::npos && e+2<h.size()) {
        size_t start=e+2; e=h.find("\r\n",start);
        line=h.substr(start,e==std::string::npos?e:e-start);
        auto colon=line.find(':'); if(colon==std::string::npos) return false;
        std::string name=line.substr(0,colon), value=line.substr(colon+1);
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
        while(!value.empty() && value.front()==' ') value.erase(0,1);
        while(!value.empty() && value.back()==' ') value.pop_back();
        if(name=="transfer-encoding") return false; // Nosso servidor exige Content-Length.
        if(name=="content-length") { if(found || !Size(value,length) || length>6000) return false; found=true; }
    }
    return found;
}
}
Result HttpChat(const Request& req, const HttpConfig& cfg, const std::atomic_bool& cancel) {
    if(cancel.load()) return {false,"Cancelado"};
    if(req.message.size()>1200 || !ValidUtf8(req.message) || cfg.token.size()>256 ||
       cfg.token.find_first_of("\r\n")!=std::string::npos) return {false,"Entrada HTTP inválida"};
    Socket sock{socket(AF_INET,SOCK_STREAM,0)};
    if(sock.fd<0) return {false,"Falha ao criar socket"};
    if(fcntl(sock.fd,F_SETFL,O_NONBLOCK)<0) return {false,"Falha no socket não bloqueante"};
    const auto end=Clock::now()+std::chrono::milliseconds(std::clamp(cfg.requestTimeoutMs,50,60000));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(cfg.port);
    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    int rc=connect(sock.fd,reinterpret_cast<sockaddr*>(&addr),sizeof addr);
    if(rc<0 && errno!=EINPROGRESS) return {false,"Servidor offline. Inicie o servidor no Termux."};
    if(rc<0) {
        auto connectionEnd=std::min(end,Clock::now()+std::chrono::milliseconds(std::clamp(cfg.connectTimeoutMs,50,10000)));
        if(!Wait(sock.fd,POLLOUT,connectionEnd,cancel)) return {false,"Timeout de conexão ou cancelamento"};
        int err=0; socklen_t len=sizeof err;
        if(getsockopt(sock.fd,SOL_SOCKET,SO_ERROR,&err,&len)<0 || err) return {false,"Servidor offline ou conexão recusada"};
    }
    auto wireRequest=req;wireRequest.timeoutMs=std::clamp(cfg.requestTimeoutMs,50,60000);std::string body=Json(wireRequest);
    if(body.size()>8192) return {false,"Requisição excedeu o limite"};
    std::string wire="POST /chat HTTP/1.0\r\nHost: 127.0.0.1\r\nContent-Type: application/json; charset=utf-8\r\nConnection: close\r\n";
    if(!cfg.token.empty()) wire+="Authorization: Bearer "+cfg.token+"\r\n";
    wire+="Content-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
    size_t sent=0;
    while(sent<wire.size()) {
        if(!Wait(sock.fd,POLLOUT,end,cancel)) return {false,"Timeout ao enviar ou cancelamento"};
        ssize_t n=send(sock.fd,wire.data()+sent,wire.size()-sent,MSG_NOSIGNAL);
        if(n>0) sent+=static_cast<size_t>(n);
        else if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) continue;
        else return {false,"Erro ao enviar ao Termux"};
    }
    std::string response; size_t split=std::string::npos, length=0; int status=0;
    char buffer[2048];
    for(;;) {
        if(cancel.load()) return {false,"Cancelado"};
        if(split!=std::string::npos && response.size()>=split+4+length) break;
        if(!Wait(sock.fd,POLLIN,end,cancel)) return {false,"Timeout de resposta ou cancelamento"};
        ssize_t n=recv(sock.fd,buffer,sizeof buffer,0);
        if(n==0) return {false,"HTTP incompleto do Termux"};
        if(n<0) {
            if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) continue;
            return {false,"Erro ao ler resposta"};
        }
        response.append(buffer,static_cast<size_t>(n));
        if(response.size()>10240) return {false,"Resposta HTTP grande demais"};
        if(split==std::string::npos) {
            split=response.find("\r\n\r\n");
            if(split==std::string::npos) { if(response.size()>4096) return {false,"Cabeçalho HTTP grande demais"}; }
            else if(split>4096 || !Headers(response.substr(0,split),status,length)) return {false,"Cabeçalho HTTP inválido"};
        }
    }
    if(cancel.load()) return {false,"Cancelado"};
    body=response.substr(split+4,length);
    if(!ValidUtf8(body)) return {false,"Resposta não é UTF-8 válida"};
    if(status!=200) return {false,"HTTP "+std::to_string(status)+": "+body};
    if(body.empty()) return {false,"Resposta vazia"};
    auto d=ParseDecision(body,req.requestId);
    return {true,d.dialogue,d};
}
}
