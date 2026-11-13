#pragma once
#include "expressive.h"
#include "memory_store.h"
#include "../../../third_party/picosha2/picosha2.h"
#include <fstream>
#include <filesystem>
#include <set>
#include <functional>
#include <fcntl.h>
#include <unistd.h>
namespace npcai {
inline std::string Digest(const std::string& s){return picosha2::hash256_hex_string(s);}
inline bool AtomicPrivate(const std::filesystem::path& path,const std::string& bytes){
 std::filesystem::create_directories(path.parent_path());auto temp=path.string()+".tmp";int f=open(temp.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);if(f<0)return false;
 size_t n=0;while(n<bytes.size()){auto w=write(f,bytes.data()+n,bytes.size()-n);if(w<=0){close(f);unlink(temp.c_str());return false;}n+=w;}bool ok=fsync(f)==0;close(f);if(!ok || rename(temp.c_str(),path.c_str())!=0){unlink(temp.c_str());return false;}return true;
}
struct ElevenVoice {std::string id,name;};
inline bool ValidVoice(const std::string& s){return !s.empty() && s.size()<=128 && s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos;}
class NpcVoiceManager {
 Settings s_;std::vector<ElevenVoice> voices_;std::map<std::string,std::string> live_,models_,named_;
 static std::map<std::string,std::string> Map(const std::string& input){std::map<std::string,std::string> out;std::istringstream in(input);std::string piece;while(std::getline(in,piece,',')){auto p=piece.find(':');if(p==std::string::npos || p==0 || !ValidVoice(piece.substr(p+1)))throw std::runtime_error("Mapeamento Voice ID inválido");out[piece.substr(0,p)]=piece.substr(p+1);}return out;}
public:
 explicit NpcVoiceManager(const Settings& s):s_(s),models_(Map(s.elevenModelVoices)),named_(Map(s.elevenCharacterVoices)){std::istringstream in(s.elevenVoices);std::string id;while(std::getline(in,id,',')){if(!ValidVoice(id))throw std::runtime_error("Voice ID inválido");voices_.push_back({id,id});}if(voices_.size()>128)throw std::runtime_error("Máximo 128 vozes");}
 void Catalog(std::vector<ElevenVoice> v){std::set<std::string> seen;std::vector<ElevenVoice> clean;for(auto& x:v)if(ValidVoice(x.id) && seen.insert(x.id).second && clean.size()<128){x.name=x.name.substr(0,120);clean.push_back(x);}voices_=std::move(clean);}
 void Retain(const std::vector<std::string>& ids){for(auto it=live_.begin();it!=live_.end();)if(std::find(ids.begin(),ids.end(),it->first)==ids.end())it=live_.erase(it);else ++it;}
 ElevenVoice Assign(const std::string& id,const Context& c){
  if(live_.count(id))return Find(live_[id]);
  if(live_.size()>=256)throw std::runtime_error("Limite de identidades de voz");
  std::string chosen;if(!c.stableCharacterId.empty() && named_.count(c.stableCharacterId))chosen=named_[c.stableCharacterId];else if(models_.count(std::to_string(c.model)))chosen=models_[std::to_string(c.model)];
  if(chosen.empty()){if(voices_.empty())throw std::runtime_error("Configure Voice IDs da sua conta ou use Atualizar vozes");size_t start=std::stoull(Digest(id).substr(0,12),nullptr,16)%voices_.size();chosen=voices_[s_.elevenVariation?start:0].id;
   if(s_.elevenVariation){size_t best=static_cast<size_t>(-1);for(size_t i=0;i<voices_.size();++i){const auto& v=voices_[(start+i)%voices_.size()].id;size_t count=0;for(const auto& old:live_)if(old.second==v)++count;if(count<best){best=count;chosen=v;}if(best==0)break;}}
  }
  if(!ValidVoice(chosen))throw std::runtime_error("Voice ID não configurado");
  live_[id]=chosen;return Find(chosen);
 }
 ElevenVoice Find(const std::string& id)const{for(const auto& v:voices_)if(v.id==id)return v;return {id,id};}
 size_t Count()const{return voices_.size();}
};
class CreditBudget {
 std::filesystem::path path_;double hour_=0,day_=0;int64_t h_=-1,d_=-1;bool valid_=true;
public:
 uint64_t characters=0,requests=0,avoided=0;bool warning=false;
 explicit CreditBudget(const std::string& root):path_(root.empty()?std::filesystem::path{}:std::filesystem::path(root)/"budget.json"){
  if(path_.empty() || !std::filesystem::exists(path_))return;
  try{if(std::filesystem::file_size(path_)>4096)throw std::runtime_error("size");std::ifstream in(path_);auto j=JsonValue::parse(in);if(j.value("schema",0)!=1 || j.value("hash","")!=Digest(j.at("data").dump()))throw std::runtime_error("integrity");auto a=j.at("data");hour_=a.at("hour").get<double>();day_=a.at("day").get<double>();h_=a.at("h").get<int64_t>();d_=a.at("d").get<int64_t>();characters=a.value("characters",uint64_t(0));requests=a.value("requests",uint64_t(0));if(!std::isfinite(hour_) || !std::isfinite(day_) || hour_<0 || day_<0)throw std::runtime_error("range");}catch(...){valid_=false;}}
 bool Reserve(size_t chars,double rate,double hourLimit,double dayLimit,int64_t now,std::string& error){
  if(!valid_){error="Contador local corrompido; restaure o backup ou remova budget.json conscientemente";++avoided;return false;}
  if(rate<0 || !std::isfinite(rate)){error="Tarifa inválida";return false;}
  if(now/3600>h_){h_=now/3600;hour_=0;}if(now/86400>d_){d_=now/86400;day_=0;}
  double cost=chars*rate;if((hourLimit>0 && hour_+cost>hourLimit) || (dayLimit>0 && day_+cost>dayLimit)){error="Orçamento local de voz atingido; nova chamada bloqueada";++avoided;return false;}
  warning=(hourLimit>0 && hour_+cost>=hourLimit*.8) || (dayLimit>0 && day_+cost>=dayLimit*.8);
  hour_+=cost;day_+=cost;characters+=chars;++requests;
  if(!path_.empty()){JsonValue data={{"hour",hour_},{"day",day_},{"h",h_},{"d",d_},{"characters",characters},{"requests",requests}};if(!AtomicPrivate(path_,JsonValue{{"schema",1},{"data",data},{"hash",Digest(data.dump())}}.dump())){valid_=false;error="Falha ao persistir reserva de créditos; chamada bloqueada";return false;}}
  return true; // No refund after ambiguous/network errors. No automatic retries.
 }
 double Hour()const{return hour_;}double Day()const{return day_;}
};
class PcmCache {
 std::filesystem::path root_;size_t limit_;bool persistent_;std::map<std::string,std::string> ram_;std::map<std::string,uint64_t> touched_;uint64_t clock_=0;
 void Prune(){namespace fs=std::filesystem;std::vector<fs::directory_entry> files;size_t bytes=0;for(const auto& f:fs::directory_iterator(root_))if(f.is_regular_file() && f.path().extension()==".tmp"){auto name=f.path().filename().string();if(name.size()==72 && name.substr(64)==".pcm.tmp")fs::remove(f.path());}else if(f.is_regular_file() && f.path().extension()==".pcm"){files.push_back(f);bytes+=f.file_size();}std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.last_write_time()<b.last_write_time();});size_t count=files.size();for(const auto& f:files){if(bytes<=limit_ && count<=512)break;bytes-=f.file_size();--count;fs::remove(f.path());}}
public:
 uint64_t hits=0;
 PcmCache(std::string root,size_t limit,bool persist):root_(root.empty()?std::filesystem::path{}:std::filesystem::path(root)/"audio-cache"),limit_(limit),persistent_(persist && !root.empty()){if(persistent_){std::filesystem::create_directories(root_);Prune();}}
 bool Get(const std::string& key,std::vector<int16_t>& pcm){try{std::string wire;if(persistent_){auto p=root_/(key+".pcm");if(!std::filesystem::is_regular_file(p))return false;if(std::filesystem::file_size(p)>1920100){std::filesystem::remove(p);return false;}std::ifstream in(p,std::ios::binary);wire.assign(std::istreambuf_iterator<char>(in),{});}else{auto it=ram_.find(key);if(it==ram_.end())return false;wire=it->second;touched_[key]=++clock_;}
  if(wire.size()<70 || wire.substr(0,4)!="NPC1" || Digest(wire.substr(68))!=wire.substr(4,64) || (wire.size()-68)%2){Erase(key);return false;}auto data=wire.substr(68);pcm.resize(data.size()/2);for(size_t i=0;i<pcm.size();++i)pcm[i]=static_cast<int16_t>(static_cast<uint16_t>(static_cast<unsigned char>(data[2*i])) | (static_cast<uint16_t>(static_cast<unsigned char>(data[2*i+1]))<<8));if(persistent_)std::filesystem::last_write_time(root_/(key+".pcm"),std::filesystem::file_time_type::clock::now());++hits;return true;
 }catch(...){return false;}}
 void Erase(const std::string& key){if(persistent_)std::filesystem::remove(root_/(key+".pcm"));else{ram_.erase(key);touched_.erase(key);}}
 void Put(const std::string& key,const std::vector<int16_t>& pcm){if(pcm.empty() || pcm.size()>960000)return;std::string data;data.reserve(pcm.size()*2);for(int16_t x:pcm){data.push_back(static_cast<char>(x&255));data.push_back(static_cast<char>((static_cast<uint16_t>(x)>>8)&255));}auto wire="NPC1"+Digest(data)+data;if(wire.size()>limit_)return;if(persistent_){if(AtomicPrivate(root_/(key+".pcm"),wire))Prune();}else{ram_[key]=wire;touched_[key]=++clock_;size_t bytes=0;for(const auto& p:ram_)bytes+=p.second.size();while(bytes>std::min<size_t>(limit_,4*1024*1024) && !ram_.empty()){auto old=std::min_element(touched_.begin(),touched_.end(),[](const auto& a,const auto& b){return a.second<b.second;});bytes-=ram_[old->first].size();ram_.erase(old->first);touched_.erase(old);}}}
 void Clear(){if(persistent_)for(const auto& f:std::filesystem::directory_iterator(root_))if(f.is_regular_file())std::filesystem::remove(f.path());ram_.clear();touched_.clear();}
};
struct ElevenReply {int status=0;std::string error;std::vector<int16_t> pcm;std::vector<ElevenVoice> voices;};
using ElevenTransport=std::function<ElevenReply(const std::string&,const std::string&,int,const std::atomic_bool&)>;
}
