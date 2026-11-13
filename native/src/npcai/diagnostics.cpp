#include "diagnostics.h"
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <set>
#include <algorithm>
namespace npcai {
std::string SafeServiceError(const std::string& code){
    if(code.empty() || code=="credential_stored" || code=="plaintext_credential_in_use")return {};
    static const std::set<std::string> known={"jni","jni_unavailable","jni_configuration_failed","jni_configuration","permission","permission_or_busy","credential_missing","credential_removed","credential_format","credential_storage","private_storage","network","timeout","tls","cancelled","busy","invalid_configuration"};
    return known.count(code)?code:"service_failure";
}
Diagnostics::Diagnostics(const Settings& s,GeminiClient::HttpsTransport https,std::string dir):settings_(s),https_(std::move(https)),exportDirectory_(std::move(dir)){
    std::fill(settings_.geminiApiKey.begin(),settings_.geminiApiKey.end(),'\0');settings_.geminiApiKey.clear();settings_.http.token.clear();
    state_.iniPath=s.configuration.path;state_.iniFound=s.configuration.found;state_.configValid=s.configuration.valid;state_.errors=s.configuration.errors;state_.warnings=s.configuration.warnings;
    state_.transport=SelectedTransport(s);state_.geminiEnabled=s.geminiEnabled;state_.model=s.geminiModel;
    state_.lastError=s.providerConfigError;worker_=std::thread(&Diagnostics::Run,this);
}
Diagnostics::~Diagnostics(){Shutdown();}
DiagnosticSnapshot Diagnostics::Get() const{std::lock_guard<std::mutex> lock(mutex_);return state_;}
void Diagnostics::Services(bool ready,int internet,bool credential,const std::string& code){std::lock_guard<std::mutex> lock(mutex_);state_.httpsAvailable=ready;state_.internet=internet;state_.credentialPresent=credential;auto error=SafeServiceError(code);if(!error.empty())state_.lastError=error;}
void Diagnostics::Bridge(bool ready){std::lock_guard<std::mutex> lock(mutex_);state_.gameBridgeReady=ready;if(!ready)state_.selection="GameBridge indisponível; veja diagnóstico da engine";}
void Diagnostics::Live(const std::string& selection,bool target,bool projected){std::lock_guard<std::mutex> lock(mutex_);state_.selection=selection;state_.bubbles=projected?"Balão projetado na tela":target?"NPC selecionado; projeção fora da tela/indisponível":"Sem NPC elegível próximo";}
void Diagnostics::ObserveHttps(const std::string& wire){
    int code=0;std::string error;
    try{auto j=JsonValue::parse(wire);code=j.value("status",0);if(code<0 || code>599)code=0;error=SafeServiceError(j.value("error",std::string{}));}catch(...){error="Resposta JNI inválida";}
    std::lock_guard<std::mutex> lock(mutex_);state_.lastHttp=code;
    if(code>=200 && code<300 && error.empty())state_.lastError.clear();else if(!error.empty())state_.lastError=error;else state_.lastError="Gemini HTTP "+std::to_string(code)+"; verifique modelo, chave e cota";
}
bool Diagnostics::TestGemini(){std::lock_guard<std::mutex> lock(mutex_);if(stop_ || state_.testing)return false;state_.testing=true;state_.testStatus="Testando Gemini...";test_=true;cv_.notify_one();return true;}
bool Diagnostics::Export(){std::lock_guard<std::mutex> lock(mutex_);if(stop_)return false;export_=true;state_.exportStatus="Exportação solicitada...";cv_.notify_one();return true;}
std::string Diagnostics::Report() const{
    const auto s=Get();std::ostringstream out;
    out<<"NPC AI "<<s.version<<" — diagnóstico sem conteúdo privado\nINI: "<<s.iniPath<<"\nINI encontrado: "<<s.iniFound<<"\nConfiguração válida: "<<s.configValid<<"\nTransporte selecionado: "<<s.transport<<"\nGemini ativado: "<<s.geminiEnabled<<"\nModelo: "<<s.model<<"\nCredencial presente: "<<s.credentialPresent<<"\nHTTPS JNI disponível: "<<s.httpsAvailable<<"\nINTERNET (-1 desconhecido): "<<s.internet<<"\nÚltimo HTTP: "<<s.lastHttp<<"\nÚltimo erro: "<<s.lastError<<"\nGameBridge: "<<s.gameBridgeReady<<"\nSeleção: "<<s.selection<<"\nBalões: "<<s.bubbles<<"\nTeste Gemini: "<<s.testStatus<<"\n";
    for(const auto& e:s.errors)out<<"Configuração: "<<e<<"\n";
    for(const auto& w:s.warnings)out<<"Aviso: "<<w<<"\n";
    return out.str();
}
void Diagnostics::Run(){
    for(;;){bool test=false,exportReport=false;{
        std::unique_lock<std::mutex> lock(mutex_);cv_.wait(lock,[&]{return stop_ || test_ || export_;});if(stop_)return;test=test_;exportReport=export_;test_=false;export_=false;
    }
    if(test){
        Result result;try{
            if(!settings_.geminiEnabled)result={false,"Gemini desativado no INI; o teste não usa Mock."};
            else if(!settings_.providerConfigError.empty())result={false,settings_.providerConfigError};
            else if(!Get().httpsAvailable)result={false,"Serviço HTTPS/JNI indisponível; interface permanece ativa."};
            else {
                auto config=settings_;config.legacyHttp=false;GeminiClient client(config,[this](const std::string& m,const std::string& b,int timeout,const std::atomic_bool& c){auto response=https_?https_(m,b,timeout,c):"{\"status\":0,\"error\":\"jni_unavailable\"}";ObserveHttps(response);return response;});
                result=client.Probe(cancel_);
            }
        }catch(...){result={false,"Falha interna do teste HTTPS; conteúdo omitido."};}
        std::lock_guard<std::mutex> lock(mutex_);state_.testing=false;state_.testStatus=result.ok?"Sucesso: Gemini respondeu via HTTPS.":result.text;if(!result.ok)state_.lastError=result.text;
    }
    if(exportReport){const auto report=Report();const auto path=exportDirectory_.empty()?std::string{}:exportDirectory_+"/npcai-diagnostic.txt";bool ok=false;
        if(!path.empty()){int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);if(fd>=0){size_t done=0;while(done<report.size()){auto n=write(fd,report.data()+done,report.size()-done);if(n<=0)break;done+=static_cast<size_t>(n);}ok=done==report.size() && fsync(fd)==0;close(fd);}}
        std::lock_guard<std::mutex> lock(mutex_);state_.exportStatus=ok?"Relatório exportado: "+path:"Não foi possível exportar; verifique a permissão de escrita no diretório AML.";
    }
    }
}
void Diagnostics::Shutdown(){std::lock_guard<std::mutex> once(shutdownMutex_);{std::lock_guard<std::mutex> lock(mutex_);stop_=true;cancel_=true;}cv_.notify_all();if(worker_.joinable())worker_.join();}
}
