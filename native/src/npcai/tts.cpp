#include "tts.h"
#include "memory_store.h"
#include "kokoro_prosody.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <map>
#include <set>
namespace npcai {
namespace {uint64_t Ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}}
VoiceManager::VoiceManager(Settings s,std::unique_ptr<VoiceBackend> b,std::unique_ptr<VoiceAudio> a):settings_(std::move(s)),backend_(std::move(b)),audio_(std::move(a)){
 settings_.geminiApiKey.clear();settings_.http.token.clear();settings_.elevenKey.clear();state_.engine=settings_.ttsEngine;state_.enabled=settings_.ttsEnabled && settings_.ttsConfigError.empty();state_.error=settings_.ttsConfigError;state_.model=settings_.ttsModel;state_.language=settings_.ttsLanguage;
 if(!state_.enabled)state_.state="TTS desativado; texto preservado";
 try{worker_=std::thread(&VoiceManager::Run,this);watcher_=std::thread(&VoiceManager::Watch,this);}catch(...){ {std::lock_guard<std::mutex> lock(mutex_);stop_=true;}cv_.notify_all();if(worker_.joinable())worker_.join();if(watcher_.joinable())watcher_.join();throw;}
}
VoiceManager::~VoiceManager(){Shutdown();}
void VoiceManager::Bind(const std::string& npc,const std::string& conversation,float distance,bool valid,bool paused,const Context& context){bool cancel=false;{
 std::lock_guard<std::mutex> lock(mutex_);const bool unavailable=!valid || (settings_.ttsStopDistance && distance>settings_.ttsMaxDistance);
 if(npc_!=npc){state_.voiceId=profiles_.count(npc)?profiles_[npc].voice:"";state_.voiceClass=state_.voiceId;state_.voiceName.clear();state_.emotion=context.emotion;state_.trust=context.relationship;state_.relationship=context.relationshipStage;}
 cancel=npc_!=npc || conversation_!=conversation || (unavailable && !testActive_ && !(job_ && job_->test)) || paused; npc_=npc;conversation_=conversation;paused_=paused;boundContext_=context;distance_=distance;
 if(cancel && (activeCancel_ || job_)){++epoch_;job_.reset();if(activeCancel_)*activeCancel_=true;}
 }
}
bool VoiceManager::Speak(const std::string& npc,const std::string& conversation,const std::string& text,bool test,const Context& context,uint64_t serial){Settings config=Options();if(serial){std::lock_guard<std::mutex> lock(mutex_);auto key=npc+"/"+conversation+"/"+std::to_string(serial);if(std::find(seenSpeech_.begin(),seenSpeech_.end(),key)!=seenSpeech_.end()){++state_.avoided;return false;}seenSpeech_.push_back(key);if(seenSpeech_.size()>256)seenSpeech_.pop_front();}return Queue(npc,conversation,text,test,SelectVoiceProfile(config,npc),context);}
bool VoiceManager::Queue(const std::string& npc,const std::string& conversation,const std::string& text,bool test,const VoiceProfile& profile,const Context& context){
 if(text.empty() || text.size()>1200 || !ValidUtf8(text) || text.find('\0')!=std::string::npos || text.find_first_not_of(" \t\r\n")==std::string::npos){std::lock_guard<std::mutex> lock(mutex_);state_.error="Fala fora do limite de 1200 bytes ou UTF-8 inválido; texto preservado";return false;}
 {std::lock_guard<std::mutex> lock(mutex_);if(stop_ || !state_.enabled || paused_ || (!test && (npc!=npc_ || conversation!=conversation_)))return false;
 VoiceProfile selected=profile;
 if(!test){auto old=profiles_.find(npc);if(old!=profiles_.end())selected=old->second;else{if(profiles_.size()>=256){state_.error="Limite de perfis de voz vivos atingido; texto preservado";return false;}profiles_.emplace(npc,selected);}}
 ++epoch_;if(activeCancel_)*activeCancel_=true;job_=Job{npc,conversation,text,epoch_,test,selected,context};state_.busy=true;state_.error.clear();}
 cv_.notify_all();return true;
}
void VoiceManager::Retain(const std::vector<std::string>& ids){const std::set<std::string> live(ids.begin(),ids.end());std::lock_guard<std::mutex> lock(mutex_);liveIds_=ids;liveDirty_=true;for(auto it=profiles_.begin();it!=profiles_.end();)if(!live.count(it->first))it=profiles_.erase(it);else ++it;}
Settings VoiceManager::Options()const{std::lock_guard<std::mutex> lock(mutex_);return settings_;}
void VoiceManager::SetVariation(bool enabled){std::lock_guard<std::mutex> lock(mutex_);if(settings_.ttsVariation!=enabled)profiles_.clear();settings_.ttsVariation=enabled;}
void VoiceManager::Configure(bool enabled,const std::string& voice,float speed,float volume,bool spatial){
 if((Options().ttsEngine=="kokoro" && KokoroSpeaker(voice)<0) || !std::isfinite(speed) || speed<.6f || speed>1.5f || !std::isfinite(volume) || volume<0 || volume>1)return;
 {std::lock_guard<std::mutex> lock(mutex_);if(!settings_.ttsVariation && settings_.ttsVoice!=voice)profiles_.clear();settings_.ttsEnabled=enabled;settings_.ttsVoice=voice;settings_.ttsSpeed=speed;settings_.ttsVolume=volume;settings_.ttsSpatial=spatial;state_.enabled=enabled && settings_.ttsConfigError.empty();}
 if(!enabled)Cancel();
}
void VoiceManager::Spatial(float pan,float distance){std::lock_guard<std::mutex> lock(mutex_);pan_=std::isfinite(pan)?std::clamp(pan,-1.f,1.f):0;distance_=std::isfinite(distance)?std::max(0.f,distance):settings_.ttsMaxDistance;}
bool VoiceManager::Test(){std::string npc,conversation;Settings config;Context context;{std::lock_guard<std::mutex> lock(mutex_);npc=npc_;conversation=conversation_;config=settings_;context=boundContext_;}config.ttsVariation=false;return Queue(npc,conversation,"Qual foi, parceiro! Agora eu consigo falar de verdade. Que loucura é essa, mano?",true,SelectVoiceProfile(config,""),context);}
void VoiceManager::SetFactory(std::function<std::unique_ptr<VoiceBackend>(const std::string&)> f){std::lock_guard<std::mutex> lock(mutex_);factory_=std::move(f);}
void VoiceManager::Control(const std::string& op,std::string value){if(value.size()>512)return;if(op=="remove_key")Cancel();{std::lock_guard<std::mutex> lock(mutex_);if(stop_ || command_)return;command_=std::make_pair(op,std::move(value));}cv_.notify_all();}
void VoiceManager::UpdateDetails(){try{auto j=JsonValue::parse(backend_->Details());std::lock_guard<std::mutex> lock(mutex_);if(!j.contains("npc_id") || j.value("npc_id","")==npc_){state_.voiceId=j.value("voice_id",state_.voiceClass);state_.voiceName=j.value("voice_name",state_.voiceId);}state_.sentCharacters=j.value("characters",uint64_t(0));state_.requests=j.value("requests",uint64_t(0));static thread_local uint64_t previousAvoided=0;auto currentAvoided=j.value("avoided",uint64_t(0));state_.avoided+=currentAvoided>=previousAvoided?currentAvoided-previousAvoided:currentAvoided;previousAvoided=currentAvoided;state_.cacheHits=j.value("cache_hits",state_.cacheHits);state_.hourCredits=j.value("hour",0.);state_.dayCredits=j.value("day",0.);state_.lastHttp=j.value("http",0);state_.creditStatus=j.value("budget","");state_.keyStatus=j.value("credential","");if(!j.value("error",std::string{}).empty())state_.error=j.value("error","");}catch(...){} }
void VoiceManager::Cancel(){{std::lock_guard<std::mutex> lock(mutex_);++epoch_;job_.reset();if(activeCancel_)*activeCancel_=true;state_.busy=activeCancel_!=nullptr;}cv_.notify_all();}
void VoiceManager::Shutdown(){std::lock_guard<std::mutex> once(shutdownMutex_);{std::lock_guard<std::mutex> lock(mutex_);stop_=true;++epoch_;job_.reset();if(activeCancel_)*activeCancel_=true;}cv_.notify_all();if(watcher_.joinable())watcher_.join();if(worker_.joinable())worker_.join();}
VoiceStatus VoiceManager::Status()const{std::lock_guard<std::mutex> lock(mutex_);return state_;}
bool VoiceManager::BusyFor(const std::string& npc)const{std::lock_guard<std::mutex> lock(mutex_);return state_.busy && (activeNpc_==npc || (job_ && job_->npc==npc));}
std::optional<VoiceEvent> VoiceManager::TakeEvent(){std::lock_guard<std::mutex> lock(mutex_);if(events_.empty())return {};auto e=events_.front();events_.pop_front();return e;}
bool VoiceManager::Current(const Job& job)const{std::lock_guard<std::mutex> lock(mutex_);return !stop_ && epoch_==job.epoch && (job.test || (npc_==job.npc && conversation_==job.conversation));}
void VoiceManager::Event(const Job& job,bool start){std::lock_guard<std::mutex> lock(mutex_);if(start && (epoch_!=job.epoch || job.test))return;if(job.test)return;if(events_.size()>=8)events_.pop_front();events_.push_back({job.npc,job.conversation,start,false,{}});}
std::vector<std::string> VoiceManager::Segments(const std::string& text){return KokoroSegments(text);}
float VoiceManager::ProfileSpeed(const std::string&,float speed){return std::clamp(speed,.6f,1.5f);}
void VoiceManager::Watch(){std::unique_lock<std::mutex> lock(mutex_);std::shared_ptr<std::atomic_bool> interrupted;
 while(!stop_){cv_.wait_for(lock,std::chrono::milliseconds(40));
  if(activeCancel_ && deadline_ && Ms()>deadline_){*activeCancel_=true;state_.error="Prazo de síntese/reprodução excedido";}
  // Keep job handover locked while interrupting this track, so a late stop cannot close the next NPC's audio.
  if(activeCancel_ && *activeCancel_ && interrupted!=activeCancel_){interrupted=activeCancel_;audio_->Interrupt();}
 }
}
void VoiceManager::Run(){
 struct Cached{std::vector<int16_t> pcm;int rate=0;};std::map<std::string,Cached> cache;size_t cacheBytes=0;bool loaded=false;std::string loadedModel;
 for(;;){Job job;std::shared_ptr<std::atomic_bool> cancel;{
  std::unique_lock<std::mutex> lock(mutex_);if(settings_.ttsIdleSeconds>0 && !cv_.wait_for(lock,std::chrono::seconds(settings_.ttsIdleSeconds),[&]{return stop_ || job_.has_value() || command_.has_value() || liveDirty_;})){
   lock.unlock();backend_->Unload();loaded=false;loadedModel.clear();lock.lock();state_.loaded=false;state_.residentModels=0;state_.approxBytes=cacheBytes;state_.state="Modelo descarregado por inatividade";continue;
  }else if(settings_.ttsIdleSeconds==0)cv_.wait(lock,[&]{return stop_ || job_.has_value() || command_.has_value() || liveDirty_;});if(stop_)break;
  if(command_){auto command=std::move(*command_);command_.reset();state_.error.clear();lock.unlock();try{if(command.first=="engine"){std::function<std::unique_ptr<VoiceBackend>(const std::string&)> factory;{std::lock_guard<std::mutex> guard(mutex_);factory=factory_;}if(factory && (command.second=="kokoro" || command.second=="elevenlabs")){auto replacement=factory(command.second);backend_=std::move(replacement);loaded=false;loadedModel.clear();cache.clear();cacheBytes=0;std::lock_guard<std::mutex> guard(mutex_);settings_.ttsEngine=command.second;state_.engine=command.second;state_.loaded=false;state_.error.clear();profiles_.clear();}}
   else if(command.first=="model" && (command.second=="eleven_v3" || command.second=="eleven_flash_v2_5")){backend_->Control(command.first,command.second);std::lock_guard<std::mutex> guard(mutex_);settings_.elevenModel=command.second;loaded=false;profiles_.clear();}else backend_->Control(command.first,command.second);}catch(...){std::lock_guard<std::mutex> guard(mutex_);state_.error="Não foi possível mudar o motor de voz";}std::fill(command.second.begin(),command.second.end(),'\0');UpdateDetails();continue;}
  if(liveDirty_){auto ids=liveIds_;liveDirty_=false;lock.unlock();backend_->Retain(ids);continue;}
  if(!job_)continue;
  job=std::move(*job_);job_.reset();cancel=std::make_shared<std::atomic_bool>(false);activeCancel_=cancel;activeNpc_=job.npc;testActive_=job.test;deadline_=Ms()+settings_.ttsTimeoutMs;state_.state="Preparando voz";
 }
 std::string error;bool ok=true;uint64_t synthMs=0,samples=0;int rate=0;bool opened=false,announced=false;std::vector<int16_t> complete;
 try{
 if(!loaded || loadedModel!=job.profile.model){loaded=backend_->SelectModel(job.profile.model,error);if(!loaded)ok=false;else loadedModel=job.profile.model;}
 if(loaded && !backend_->Prepare(job.npc,job.context,job.profile.voice,error))ok=false;
 backend_->Speaker(KokoroSpeaker(job.profile.voice));
 {std::lock_guard<std::mutex> lock(mutex_);state_.loaded=loaded;state_.requestedModel=job.profile.model;state_.model=job.profile.model;if(job.test || (npc_==job.npc && conversation_==job.conversation)){state_.voiceClass=job.profile.voice;if(!job.test && profiles_.count(job.npc))profiles_[job.npc].voice=job.profile.voice;state_.emotion=job.context.emotion;state_.intensity=job.context.emotionIntensity;state_.trust=job.context.relationship;state_.relationship=job.context.relationshipStage;}state_.profanity=settings_.profanityLevel;state_.aggression=settings_.aggressionMultiplier;state_.residentModels=loaded?backend_->ResidentModels():0;
  if(loaded){state_.phonemizer=backend_->Online()?"Síntese HTTPS ElevenLabs":"eSpeak NG pronto (pt)";state_.onnx=backend_->Online()?"Não utilizado pelo ElevenLabs":"Sherpa-ONNX CPU; sessão única";state_.approxBytes=backend_->ModelBytes();}}

 auto valid=[&]{return !*cancel && Current(job);};
 auto pcm=[&](const int16_t* data,size_t count,int hz)->bool{
  if(!valid() || hz<8000 || hz>48000 || (rate && rate!=hz) || samples+count>static_cast<uint64_t>(hz)*40)return false;
  rate=hz;if(!opened){opened=audio_->Open(rate,1.f);if(!opened){error="AudioTrack indisponível";return false;}}
  for(size_t n=0;n<count;){if(!valid())return false;auto len=std::min<size_t>(512,count-n);
   float gain,pan;{std::lock_guard<std::mutex> lock(mutex_);gain=settings_.ttsVolume;pan=0;if(settings_.ttsSpatial && !job.test){float d=distance_/settings_.ttsMaxDistance;gain*=std::max(0.f,1.f-d*d);pan=pan_;}}
   audio_->Gains(gain*std::sqrt(1.f-std::max(0.f,pan)),gain*std::sqrt(1.f+std::min(0.f,pan)));
   int written=audio_->Write(data+n,len);if(written<=0 || written>static_cast<int>(len)){error="Falha na reprodução PCM";return false;}n+=written;samples+=written;if(!announced && audio_->Head()>0){announced=true;Event(job,true);}
   {std::lock_guard<std::mutex> lock(mutex_);state_.playing=announced;state_.state=announced?"Reproduzindo":"Aguardando início AudioTrack";}
  }
  return true;
 };
 const auto options=Options();const float speed=backend_->Online()?options.ttsSpeed:KokoroProsodySpeed(options.ttsSpeed,job.context,options.ttsKokoroProsody);const auto cacheKey=loadedModel+"@"+job.profile.voice+"@"+job.text+"@"+std::to_string(speed);
 auto found=cache.find(cacheKey);
 if(ok && valid() && !backend_->Online() && settings_.ttsCache && found!=cache.end()){ok=pcm(found->second.pcm.data(),found->second.pcm.size(),found->second.rate);std::lock_guard<std::mutex> lock(mutex_);++state_.cacheHits;}
 else if(ok && valid()){
  for(const auto& segment:backend_->Online()?std::vector<std::string>{job.text}:Segments(job.text)){
   auto start=Ms();uint64_t playbackMs=0;
   ok=backend_->Generate(segment,speed,*cancel,[&](const float* data,size_t count,int hz){
    if(!valid() || !data || hz<8000 || hz>48000 || count>static_cast<size_t>(hz)*40)return false;
    std::vector<int16_t> values(count);double square=0;for(size_t i=0;i<count;++i){float f=std::isfinite(data[i])?std::clamp(data[i],-1.f,1.f):0;values[i]=static_cast<int16_t>(f*32767);square+=f*f;}
    {std::lock_guard<std::mutex> lock(mutex_);state_.rms=count?std::sqrt(square/count):0;}
    if(!backend_->Online() && settings_.ttsCache && complete.size()+count<=1024*1024)complete.insert(complete.end(),values.begin(),values.end());else complete.clear();
    auto began=Ms();bool success=pcm(values.data(),values.size(),hz);playbackMs+=Ms()-began;return success;
   });synthMs+=Ms()-start-playbackMs;if(!ok || !valid())break;
  }
  if(ok && valid() && !backend_->Online() && settings_.ttsCache && !complete.empty() && complete.size()==samples){while(!cache.empty() && (cache.size()>=4 || cacheBytes+complete.size()*2>4*1024*1024)){cacheBytes-=cache.begin()->second.pcm.size()*2;cache.erase(cache.begin());}cacheBytes+=complete.size()*2;cache.emplace(cacheKey,Cached{std::move(complete),rate});}
 }
 while(ok && opened && valid()){auto head=audio_->Head();if(head<0){error="Leitura do progresso AudioTrack falhou";ok=false;break;}if(head>0 && !announced){announced=true;Event(job,true);std::lock_guard<std::mutex> lock(mutex_);state_.playing=true;}if(static_cast<uint64_t>(head)>=samples)break;std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 if(!valid())ok=false;
 if(ok && samples==0){ok=false;error="Kokoro não produziu amostras de áudio";}
 }catch(...){ok=false;error="Falha de síntese local; detalhes privados omitidos";}
 if(!ok && error.empty())error=backend_->Error();
 audio_->Close();if(announced)Event(job,false);
 if(!ok && !announced && !*cancel && !job.test && Current(job)){std::lock_guard<std::mutex> lock(mutex_);if(events_.size()>=8)events_.pop_front();events_.push_back({job.npc,job.conversation,false,true,job.text});}
 {std::lock_guard<std::mutex> lock(mutex_);activeCancel_.reset();activeNpc_.clear();testActive_=false;deadline_=0;state_.playing=false;state_.busy=job_.has_value();state_.synthMs=synthMs;state_.audioMs=rate?samples*1000/rate:0;state_.rms=0;state_.approxBytes=backend_->ModelBytes()+cacheBytes;state_.state=ok?"Fala concluída":*cancel?"Fala cancelada":"TTS indisponível; texto preservado";if(!error.empty())state_.error=error;else if(!ok && !*cancel)state_.error="Kokoro não produziu áudio válido";}
 UpdateDetails();
 }
 audio_->Close();
}
}
