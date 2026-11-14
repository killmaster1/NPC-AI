#include "tts.h"
#include "tts_staging.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <atomic>
#include <stdexcept>
using namespace npcai;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
static void Wait(std::function<bool()> check){for(int n=0;n<500;++n){if(check())return;std::this_thread::sleep_for(std::chrono::milliseconds(4));}throw std::runtime_error("deadline");}
struct Synth:VoiceBackend{
 std::atomic<int> loads{0},runs{0},cancels{0};std::atomic_bool block{false};bool missing=false;
 bool Load(std::string& error)override{++loads;if(missing){error="Modelo ausente";return false;}return true;}
 bool Generate(const std::string&,float,const std::atomic_bool& cancel,const std::function<bool(const float*,size_t,int)>& sink)override{++runs;while(block && !cancel)std::this_thread::sleep_for(std::chrono::milliseconds(2));if(cancel)return false;std::vector<float> samples(2205,.25f);return sink(samples.data(),samples.size(),22050);}
 void Cancel()override{++cancels;}uint64_t ModelBytes()const override{return 63000000;}
};
struct Audio:VoiceAudio {std::atomic<int> opens{0},closes{0},written{0},interrupted{0};bool fail=false;bool Open(int r,float v)override{CHECK(r==22050 && v>=0 && v<=1);++opens;written=0;return !fail;}int Write(const int16_t* p,size_t n)override{CHECK(n<=512 && p[0]>0);written+=n;return n;}int64_t Head()override{return written;}void Close()override{++closes;}void Interrupt()override{++interrupted;}};
struct F{Synth* synth;Audio* audio;std::unique_ptr<VoiceManager> voice;F(Settings s={} ){auto b=std::make_unique<Synth>();synth=b.get();auto a=std::make_unique<Audio>();audio=a.get();voice=std::make_unique<VoiceManager>(s,std::move(b),std::move(a));voice->Bind("A","c1",2,true,false);}void Say(const std::string& text="Qual foi, mano, tá ligado!"){CHECK(voice->Speak("A","c1",text));}void Done(){Wait([&]{return !voice->Status().busy;});}};
static void Offline(){F f;f.Say();f.Done();CHECK(f.audio->written==2205 && f.voice->Status().audioMs==100 && f.voice->Status().loaded);auto e=f.voice->TakeEvent();CHECK(e && e->started && e->npc=="A");e=f.voice->TakeEvent();CHECK(e && !e->started);}
static void Reuse(){Settings s;s.ttsCache=false;F f(s);f.Say();f.Done();f.Say("Outro papo.");f.Done();CHECK(f.synth->loads==1 && f.synth->runs==2);}
static void Cache(){F f;f.Say();f.Done();f.Say();f.Done();CHECK(f.synth->runs==1 && f.voice->Status().cacheHits==1);}
static void Missing(){F f;f.synth->missing=true;f.Say();f.Done();CHECK(f.voice->Status().error=="Modelo ausente" && f.audio->opens==0 && f.voice->Status().enabled);auto event=f.voice->TakeEvent();CHECK(event && event->failed && event->text=="Qual foi, mano, tá ligado!");}
static void Disabled(){Settings s;s.ttsEnabled=false;F f(s);CHECK(!f.voice->Speak("A","c1","Oi") && f.synth->loads==0);}
static void Switch(){F f;f.synth->block=true;f.Say();Wait([&]{return f.synth->runs==1;});f.voice->Bind("B","c2",2,true,false);f.Done();CHECK(f.audio->written==0 && !f.voice->TakeEvent());CHECK(!f.voice->Speak("A","c1","Antiga"));}
static void Conversation(){F f;f.synth->block=true;f.Say();Wait([&]{return f.synth->runs==1;});f.voice->Bind("A","c2",2,true,false);f.Done();CHECK(f.audio->written==0);}
static void Pause(){F f;f.synth->block=true;f.Say();Wait([&]{return f.synth->runs==1;});f.voice->Bind("A","c1",2,true,true);f.Done();CHECK(f.audio->written==0 && !f.voice->Test());}
static void Distance(){F f;f.synth->block=true;f.Say();Wait([&]{return f.synth->runs==1;});f.voice->Bind("A","c1",16,true,false);f.Done();CHECK(f.audio->written==0);}
static void Invalid(){F f;f.synth->block=true;f.Say();Wait([&]{return f.synth->runs==1;});f.voice->Bind("A","c1",2,false,false);f.Done();CHECK(f.audio->written==0);}
static void Shutdown(){F f;f.synth->block=true;f.Say();Wait([&]{return f.synth->runs==1;});auto start=std::chrono::steady_clock::now();f.voice->Shutdown();CHECK(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(300));}
static void Queue(){F f;f.synth->block=true;f.Say();Wait([&]{return f.synth->runs==1;});for(int n=0;n<30;++n)f.Say("Mensagem "+std::to_string(n));f.synth->block=false;f.Done();CHECK(f.synth->loads==1 && f.synth->runs<32 && f.voice->Status().approxBytes<70000000);}
static void AudioFailure(){F f;f.audio->fail=true;f.Say();f.Done();CHECK(!f.voice->Status().error.empty() && !f.voice->Status().playing);}
static void TextBound(){F f;CHECK(!f.voice->Speak("A","c1",std::string(1201,'x')));CHECK(!f.voice->Speak("A","c1",std::string("x\0y",3)));CHECK(!f.voice->Speak("A","c1",std::string(1,static_cast<char>(0xff))));}
static void Segments(){std::string text;for(int n=0;n<50;++n)text+="Mano, tá ligado? Ação, coração! ";auto parts=VoiceManager::Segments(text);std::string joined;for(auto& part:parts){CHECK(ValidUtf8(part) && part.size()<=320);joined+=part;}CHECK(joined==text);}
static void Profile(){CHECK(VoiceManager::ProfileSpeed("A",1)==VoiceManager::ProfileSpeed("A",1));CHECK(VoiceManager::ProfileSpeed("A",1.5f)<=1.5f);}
static void TestWithoutNpc(){F f;f.voice->Bind("","",0,false,false);CHECK(f.voice->Test());f.voice->Bind("","",0,false,false);f.Done();CHECK(f.audio->written>0 && !f.voice->TakeEvent());}
static void Timeout(){Settings s;s.ttsTimeoutMs=50;F f(s);f.synth->block=true;f.Say();f.Done();CHECK(f.audio->written==0 && f.voice->Status().error.find("Prazo")!=std::string::npos);}
static void Configuration(){auto path=std::filesystem::temp_directory_path()/("npcai-tts-"+SessionId()+".ini");{std::ofstream o(path);o<<"gemini_enabled=1\nnpc_tts_volume=nan\nnpc_tts_threads=99\nnpc_tts_model=../escape\n";}auto s=LoadSettings(path);CHECK(s.geminiEnabled && s.providerConfigError.empty() && !s.ttsConfigError.empty());F f(s);CHECK(!f.voice->Test());std::filesystem::remove(path);}
static void LibraryStaging(){
 auto base=std::filesystem::temp_directory_path()/("npcai-stage-"+SessionId());auto source=base/"source",dest=base/"private",bad=base/"bad";std::filesystem::create_directories(source);std::filesystem::create_directories(bad);
 {std::ofstream f(source/"libnpcai_kokoro_ort.so");f<<"verified-ort";}{std::ofstream f(source/"libnpcai_kokoro.so");f<<"verified-kokoro";}
 auto hash=[](const std::string& path){std::ifstream f(path);if(!f)return std::string{};return std::string((std::istreambuf_iterator<char>(f)),{});};std::string error;
 CHECK(StageVoiceLibraries({bad.string(),source.string()},dest.string(),"verified-ort","verified-kokoro",hash,error));
 CHECK(hash((dest/"libnpcai_kokoro_ort.so").string())=="verified-ort");auto time=std::filesystem::last_write_time(dest/"libnpcai_kokoro_ort.so");
 CHECK(StageVoiceLibraries({source.string()},dest.string(),"verified-ort","verified-kokoro",hash,error));CHECK(std::filesystem::last_write_time(dest/"libnpcai_kokoro_ort.so")==time);
 CHECK(!StageVoiceLibraries({bad.string()},dest.string(),"verified-ort","verified-kokoro",hash,error));std::filesystem::remove_all(base);
}
static void InterruptBeforeInferenceReturns(){
 struct Uninterruptible:VoiceBackend{std::atomic_bool entered{false};bool Load(std::string&)override{return true;}uint64_t ModelBytes()const override{return 0;}void Cancel()override{}bool Generate(const std::string&,float,const std::atomic_bool&,const std::function<bool(const float*,size_t,int)>&)override{entered=true;std::this_thread::sleep_for(std::chrono::milliseconds(500));return false;}};
 auto b=std::make_unique<Uninterruptible>();auto* backend=b.get();auto a=std::make_unique<Audio>();auto* audio=a.get();VoiceManager voice(Settings{},std::move(b),std::move(a));voice.Bind("A","c",2,true,false);CHECK(voice.Speak("A","c","Fala antiga."));Wait([&]{return backend->entered.load();});voice.Cancel();Wait([&]{return audio->interrupted>0;});CHECK(voice.Status().busy);voice.Shutdown();CHECK(audio->written==0);
}
static void DuplicateSpeechSerial(){F f;CHECK(f.voice->Speak("A","c1","Uma fala.",false,{},777));f.Done();auto runs=f.synth->runs.load();CHECK(!f.voice->Speak("A","c1","Uma fala.",false,{},777));CHECK(f.synth->runs==runs);CHECK(f.voice->Speak("A","c1","Outra fala.",false,{},778));f.Done();}
int main(){int count=0;try{for(auto t:{Offline,Reuse,Cache,Missing,Disabled,Switch,Conversation,Pause,Distance,Invalid,Shutdown,Queue,AudioFailure,TextBound,Segments,Profile,TestWithoutNpc,Timeout,Configuration,LibraryStaging,InterruptBeforeInferenceReturns,DuplicateSpeechSerial}){t();++count;}}catch(const std::exception& e){std::cerr<<"Voice scenario "<<count+1<<": "<<e.what()<<"\n";return 1;}std::cout<<"Voice controller: "<<count<<" deterministic host scenarios passed; AudioTrack/GTA pending\n";}
