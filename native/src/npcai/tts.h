#pragma once
#include "settings.h"
#include "voice_profiles.h"
#include <condition_variable>
#include <deque>
#include <thread>
#include <mutex>
#include <memory>
#include <functional>
#include <map>
namespace npcai {
struct VoiceStatus {bool enabled=false,loaded=false,busy=false,playing=false;std::string state="Aguardando fala",model,requestedModel,voiceClass="Pedestre comum",warning,language,error,phonemizer="Não inicializado",onnx="Não inicializado";uint64_t synthMs=0,audioMs=0,approxBytes=0,cacheHits=0;float rms=0;int residentModels=0;std::string engine="kokoro",emotion="neutral",voiceId,voiceName,creditStatus,relationship="neutral",keyStatus;float intensity=0,aggression=.65f,trust=40;uint64_t sentCharacters=0,requests=0,avoided=0;double hourCredits=0,dayCredits=0;int lastHttp=0,profanity=1;};
struct VoiceEvent {std::string npc,conversation;bool started=false,failed=false;std::string text;};
class VoiceBackend {public:virtual ~VoiceBackend()=default;virtual bool Load(std::string&)=0;virtual bool Generate(const std::string&,float,const std::atomic_bool&,const std::function<bool(const float*,size_t,int)>&)=0;virtual void Cancel()=0;virtual uint64_t ModelBytes()const=0;virtual bool SelectModel(const std::string&,std::string& error){return Load(error);}virtual std::string ActiveModel()const{return {};}virtual int ResidentModels()const{return 1;}virtual void Speaker(int){}virtual void Unload(){}virtual bool Online()const{return false;}virtual bool Prepare(const std::string&,const Context&,std::string&,std::string&){return true;}virtual std::string Error()const{return {};}virtual std::string Details()const{return "{}";}virtual void Retain(const std::vector<std::string>&){}virtual void Control(const std::string&,const std::string&){}};
class VoiceAudio {public:virtual ~VoiceAudio()=default;virtual bool Open(int,float)=0;virtual int Write(const int16_t*,size_t)=0;virtual int64_t Head()=0;virtual void Close()=0;virtual void Gains(float,float){}virtual void Interrupt(){}};
class VoiceManager {
public:
 VoiceManager(Settings,std::unique_ptr<VoiceBackend>,std::unique_ptr<VoiceAudio>);~VoiceManager();
 void Bind(const std::string&,const std::string&,float,bool,bool,const Context& context={});
 bool Speak(const std::string&,const std::string&,const std::string&,bool test=false,const Context& context={},uint64_t serial=0);
 void Retain(const std::vector<std::string>& liveIds);
 bool Test();void Control(const std::string&,std::string value={});void SetFactory(std::function<std::unique_ptr<VoiceBackend>(const std::string&)>);void Configure(bool,const std::string&,float,float,bool);void Spatial(float pan,float distance);Settings Options()const;void SetVariation(bool);void Cancel();void Shutdown();VoiceStatus Status()const;std::optional<VoiceEvent> TakeEvent();
 bool BusyFor(const std::string&)const;
 static std::vector<std::string> Segments(const std::string&);static float ProfileSpeed(const std::string&,float);
private:
 struct Job {std::string npc,conversation,text;uint64_t epoch;bool test;VoiceProfile profile;Context context;};
 bool Queue(const std::string&,const std::string&,const std::string&,bool,const VoiceProfile&,const Context& context={});
 void UpdateDetails();void Run();void Watch();bool Current(const Job&)const;void Event(const Job&,bool);
 std::function<std::unique_ptr<VoiceBackend>(const std::string&)> factory_;Settings settings_;std::unique_ptr<VoiceBackend> backend_;std::unique_ptr<VoiceAudio> audio_;
 mutable std::mutex mutex_;std::mutex shutdownMutex_;std::condition_variable cv_;std::optional<Job> job_;std::deque<VoiceEvent> events_;
 std::map<std::string,VoiceProfile> profiles_;
 float pan_=0,distance_=0;std::optional<std::pair<std::string,std::string>> command_;std::vector<std::string> liveIds_;bool liveDirty_=false;std::deque<std::string> seenSpeech_;Context boundContext_;std::string npc_,conversation_,activeNpc_;uint64_t epoch_=0,deadline_=0;bool stop_=false,paused_=false,testActive_=false;std::shared_ptr<std::atomic_bool> activeCancel_;VoiceStatus state_;
 std::thread worker_,watcher_;
};
std::unique_ptr<VoiceBackend> MakeKokoroBackend(const Settings&,const std::string& root,const std::string& libraries,std::function<std::string(const std::string&)> hash,std::string privateLibraries={},std::string alternateLibraries={});
}
