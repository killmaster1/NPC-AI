#pragma once
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
namespace npcai {
inline bool StageVoiceLibraries(const std::vector<std::string>& candidates,const std::string& destination,
 const std::string& ortHash,const std::string& kokoroHash,const std::function<std::string(const std::string&)>& hash,std::string& error){
 namespace fs=std::filesystem;
   std::string source;
   for(const auto& candidate:candidates)if(!candidate.empty() && hash((fs::path(candidate)/"libnpcai_kokoro_ort.so").string())==ortHash && hash((fs::path(candidate)/"libnpcai_kokoro.so").string())==kokoroHash){source=candidate;break;}
   if(source.empty()){error="Bibliotecas TTS ausentes ou SHA-256 incompatível; reinstale as duas bibliotecas desta versão";return false;}
   fs::create_directories(destination);
   for(const auto& item:std::vector<std::pair<std::string,std::string>>{{"libnpcai_kokoro_ort.so",ortHash},{"libnpcai_kokoro.so",kokoroHash}}){
    auto target=fs::path(destination)/item.first;
    if(hash(target.string())==item.second)continue;
    auto temporary=target.string()+".tmp";int fd=open(temporary.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0){error="Falha ao preparar biblioteca TTS privada";return false;}
    std::ifstream input(fs::path(source)/item.first,std::ios::binary);char buffer[32768];bool success=static_cast<bool>(input);
    while(input && success){input.read(buffer,sizeof buffer);size_t size=static_cast<size_t>(input.gcount()),done=0;while(done<size){auto n=write(fd,buffer+done,size-done);if(n<=0){success=false;break;}done+=static_cast<size_t>(n);}}
    success=success && !input.bad();if(fsync(fd)!=0)success=false;close(fd);
    if(!success || hash(temporary)!=item.second || chmod(temporary.c_str(),0500)!=0){unlink(temporary.c_str());error="Cópia/integridade da biblioteca TTS privada falhou";return false;}
    if(rename(temporary.c_str(),target.c_str())!=0){unlink(temporary.c_str());error="Não foi possível instalar biblioteca TTS privada";return false;}
   }
 return true;
}
}
