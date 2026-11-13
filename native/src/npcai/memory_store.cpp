#include "memory_store.h"
#include "types.h"
#include <fstream>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <cmath>
namespace npcai {
namespace fs=std::filesystem;
static uint64_t Wall(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());}
std::string IntegrityHash(const std::string& s){uint64_t h=1469598103934665603ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return std::to_string(h);}
static JsonValue Fresh(const std::string& profile){return JsonValue{{"profile",profile},{"global",JsonValue::array()},{"npcs",JsonValue::object()}};}
NpcMemoryManager::NpcMemoryManager(std::string dir,size_t max):directory_(std::move(dir)),maxBytes_(std::clamp<size_t>(max,16384,32*1024*1024)),data_(Fresh("unassigned")){}
fs::path NpcMemoryManager::File()const{return fs::path(directory_)/("memory-"+IntegrityHash(profile_)+".json");}
bool NpcMemoryManager::Read(const fs::path& path){
    try{
        if(!fs::is_regular_file(path) || fs::is_symlink(path) || fs::file_size(path)>maxBytes_)return false;
        std::ifstream f(path,std::ios::binary);std::string wire((std::istreambuf_iterator<char>(f)),{});
        auto root=JsonValue::parse(wire);if(!root.is_object() || !root.contains("schema") || !root.contains("data") || !root.contains("integrity") || root["integrity"]!=IntegrityHash(root["data"].dump()))return false;
        int schema=root["schema"].get<int>();if(schema>2){readOnly_=true;return false;}if(schema<1)return false;
        auto d=root["data"];if(!d.is_object() || d.value("profile","")!=profile_ || !d.contains("global") || !d["global"].is_array() || !d.contains("npcs") || !d["npcs"].is_object())return false;
        for(const auto& n:d["global"])if(!n.is_string() || n.get<std::string>().size()>1000 || !ValidUtf8(n.get<std::string>()))return false;
        for(auto it=d["npcs"].begin();it!=d["npcs"].end();++it){
            if(it.key().size()>160 || !it.value().is_object() || !it.value().contains("notes") || !it.value()["notes"].is_array())return false;
            for(const auto& n:it.value()["notes"])if(!n.is_string() || n.get<std::string>().size()>1000 || !ValidUtf8(n.get<std::string>()))return false;
        }
        data_=std::move(d);bytes_=wire.size();dirty_=schema!=2;Prune();return true;
    }catch(...){return false;}
}
void NpcMemoryManager::Initialize(const std::string& profile){profile_=profile;data_=Fresh(profile);dirty_=false;bytes_=0;readOnly_=false;}
bool NpcMemoryManager::Load(const std::string& profile){
    profile_=profile;data_=Fresh(profile);dirty_=false;bytes_=0;readOnly_=false;
    try{fs::create_directories(directory_);chmod(directory_.c_str(),0700);
        auto file=File();if(fs::exists(file.string()+".clear")){dirty_=true;return true;}
        if(!fs::exists(file) && !fs::exists(file.string()+".bak"))return true;
        if(Read(file))return true;
        ++corruptions_;data_=Fresh(profile_);dirty_=false;
        if(Read(file.string()+".bak")){dirty_=true;return true;}
        data_=Fresh(profile_);dirty_=true;return false;
    }catch(...){++corruptions_;return false;}
}
void NpcMemoryManager::Prune(){
    auto& global=data_["global"];while(global.size()>96){global.erase(global.begin());dirty_=true;}
    auto& npcs=data_["npcs"];
    while(npcs.size()>128){auto oldest=npcs.begin();for(auto it=npcs.begin();it!=npcs.end();++it)if(it.value().value("at",uint64_t(0))<oldest.value().value("at",uint64_t(0)))oldest=it;npcs.erase(oldest);dirty_=true;}
    for(auto it=npcs.begin();it!=npcs.end();){
        const auto at=it.value().value("at",Wall());if(Wall()>at && Wall()-at>30*86400){it=npcs.erase(it);dirty_=true;continue;}
        auto& notes=it.value()["notes"];while(notes.size()>24){notes.erase(notes.begin());dirty_=true;}++it;
    }
}
bool NpcMemoryManager::Note(const std::string& npc,const std::string& text,bool global){
    if(text.empty() || text.size()>1000 || !ValidUtf8(text) || npc.size()>160)return false;
    JsonValue* notes=nullptr;
    if(global)notes=&data_["global"];
    else{auto& entry=data_["npcs"][npc];if(entry.is_null())entry={{"notes",JsonValue::array()},{"binding","session"},{"at",Wall()}};notes=&entry["notes"];entry["at"]=Wall();}
    if(!notes->empty() && notes->back()==text)return false;
    notes->push_back(text);dirty_=true;Prune();return true;
}
void NpcMemoryManager::Emotion(const MemoryIdentity& id,const EmotionalState& s){
    auto& e=data_["npcs"][id.key];if(e.is_null())e={{"notes",JsonValue::array()},{"at",Wall()}};
    auto body=JsonValue{{"perplexity",s.perplexity},{"fear",s.fear},{"curiosity",s.curiosity},{"trust",s.trust},{"aggression",s.aggression},{"courage",s.courage},{"skepticism",s.skepticism}};
    if(e.contains("emotion") && e["emotion"]==body)return;
    e["emotion"]=body;e["binding"]=id.stableVerified?"verified_stable":"session";e["at"]=Wall();dirty_=true;Prune();
}
std::optional<EmotionalState> NpcMemoryManager::Restore(const MemoryIdentity& id)const{
    if(!id.stableVerified || !data_["npcs"].contains(id.key))return {};
    const auto& e=data_["npcs"][id.key];if(e.value("binding","")!="verified_stable" || !e.contains("emotion"))return {};
    try{const auto& j=e["emotion"];EmotionalState s;s.perplexity=j.at("perplexity");s.fear=j.at("fear");s.curiosity=j.at("curiosity");s.trust=j.at("trust");s.aggression=j.at("aggression");s.courage=j.at("courage");s.skepticism=j.at("skepticism");for(float v:{s.perplexity,s.fear,s.curiosity,s.trust,s.aggression,s.courage,s.skepticism})if(!std::isfinite(v) || v<0 || v>100)return {};return s;}catch(...){return {};}
}
std::vector<std::string> NpcMemoryManager::Notes(const MemoryIdentity& id,bool global)const{
    std::vector<std::string> out;
    if(global){const auto& a=data_["global"];size_t start=a.size()>3?a.size()-3:0;for(size_t i=start;i<a.size();++i)out.push_back("Registro narrativo global; não é lembrança pessoal deste NPC: "+a[i].get<std::string>());}
    if(data_["npcs"].contains(id.key)){
        const auto& e=data_["npcs"][id.key];if((id.stableVerified && e.value("binding","")=="verified_stable") || (!id.stableVerified && e.value("binding","")=="session")){
            const auto& a=e["notes"];size_t start=a.size()>6?a.size()-6:0;for(size_t i=start;i<a.size();++i)out.push_back(a[i].get<std::string>());
        }
    }return out;
}
void NpcMemoryManager::ForkFrom(const JsonValue& previous){auto next=previous;next["profile"]=profile_;if(next==data_)return;data_=std::move(next);dirty_=true;Prune();}
static bool WriteFile(const fs::path& path,const std::string& bytes){
    int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);if(fd<0)return false;
    size_t done=0;while(done<bytes.size()){ssize_t n=write(fd,bytes.data()+done,bytes.size()-done);if(n<=0){close(fd);return false;}done+=static_cast<size_t>(n);}
    bool ok=fsync(fd)==0;close(fd);return ok;
}
bool NpcMemoryManager::Save(){
    if(readOnly_)return false;
    if(!dirty_)return true;
    try{
        Prune();auto payload=data_.dump();
        while(payload.size()>maxBytes_/3 && !data_["npcs"].empty()){data_["npcs"].erase(data_["npcs"].begin());payload=data_.dump();}
        JsonValue root={{"schema",2},{"data",data_},{"integrity",IntegrityHash(payload)}};auto wire=root.dump();if(wire.size()>maxBytes_)return false;
        fs::create_directories(directory_);auto file=File(),tmp=fs::path(file.string()+".tmp");
        if(!WriteFile(tmp,wire))return false;
        std::error_code ec;if(fs::exists(file)){fs::remove(file.string()+".bak",ec);fs::rename(file,file.string()+".bak",ec);if(ec)return false;}
        fs::rename(tmp,file,ec);if(ec)return false;
        int dir=open(directory_.c_str(),O_RDONLY|O_DIRECTORY);if(dir>=0){fsync(dir);close(dir);}
        dirty_=false;++saves_;bytes_=wire.size();
        std::vector<fs::directory_entry> old;uintmax_t total=0;
        for(const auto& entry:fs::directory_iterator(directory_))if(entry.is_regular_file() && entry.path().filename().string().rfind("memory-",0)==0){total+=entry.file_size();if(entry.path()!=file)old.push_back(entry);}
        std::sort(old.begin(),old.end(),[](const auto& a,const auto& b){return a.last_write_time()<b.last_write_time();});
        for(const auto& entry:old){if(total<=maxBytes_)break;auto n=entry.file_size();fs::remove(entry.path(),ec);if(!ec)total-=n;}
        return true;
    }catch(...){return false;}
}
bool NpcMemoryManager::Clear(){try{auto flag=File().string()+".clear";if(!WriteFile(flag,"cleared"))return false;data_=Fresh(profile_);dirty_=true;readOnly_=false;bool ok=Save();if(ok){std::error_code ec;fs::remove(File().string()+".bak",ec);fs::remove(flag,ec);}return ok;}catch(...){return false;}}
}
