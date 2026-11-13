#include "tts.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <chrono>
#include <cmath>
using namespace npcai;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
static void Wait(std::function<bool()> p){for(int i=0;i<500;++i){if(p())return;std::this_thread::sleep_for(std::chrono::milliseconds(4));}throw std::runtime_error("timeout");}
struct Backend:VoiceBackend{int loads=0,runs=0,speaker=-1,unloads=0;bool Load(std::string&)override{++loads;return true;}uint64_t ModelBytes()const override{return 350000000;}void Cancel()override{}void Speaker(int id)override{speaker=id;}void Unload()override{++unloads;}bool Generate(const std::string&,float,const std::atomic_bool& cancel,const std::function<bool(const float*,size_t,int)>& sink)override{++runs;float pcm[240]={.5};return !cancel && sink(pcm,240,24000);}};
struct Audio:VoiceAudio{int samples=0;float left=1,right=1;bool Open(int,float)override{samples=0;return true;}int Write(const int16_t*,size_t n)override{samples+=n;return n;}int64_t Head()override{return samples;}void Close()override{}void Gains(float l,float r)override{left=l;right=r;}};
struct F{Backend* b;Audio* a;std::unique_ptr<VoiceManager> v;F(Settings s={}){auto backend=std::make_unique<Backend>();b=backend.get();auto audio=std::make_unique<Audio>();a=audio.get();v=std::make_unique<VoiceManager>(s,std::move(backend),std::move(audio));v->Bind("A","c",2,true,false);}void Done(){Wait([&]{return !v->Status().busy;});}void Test(const std::string& name){auto s=v->Options();v->Configure(true,name,1,.85,true);CHECK(v->Test());Done();}};
static Settings Ini(const std::string& text){auto p=std::filesystem::temp_directory_path()/("kokoro-"+SessionId()+".ini");{std::ofstream out(p);out<<text;}auto s=LoadSettings(p);std::filesystem::remove(p);return s;}
static void Defaults(){Settings s;CHECK(s.ttsModel=="kokoro-pt-br" && s.ttsVoice=="pm_alex" && s.ttsThreads==2);CHECK(KokoroSpeaker("pm_alex")==43 && KokoroSpeaker("pm_santa")==44 && KokoroSpeaker("pf_dora")==42 && KokoroSpeaker("other")==-1);}
static void Distribution(){Settings s;std::set<std::string> voices;for(int i=0;i<100;++i){auto id="session/ped/"+std::to_string(i);auto v=SelectVoiceProfile(s,id);voices.insert(v.voice);CHECK(v.voice==SelectVoiceProfile(s,id).voice && v.model==s.ttsModel);}CHECK(voices.size()==3);}
static void NoVariation(){Settings s;s.ttsVariation=false;s.ttsVoice="pf_dora";for(auto id:{"A","B","C"})CHECK(SelectVoiceProfile(s,id).voice=="pf_dora");}
static void ThreeSpeakersOneSession(){F f;for(auto name:KokoroVoices){f.Test(name);CHECK(f.b->speaker==KokoroSpeaker(name));}CHECK(f.b->loads==1 && f.b->runs>0);}
static void CacheIsolation(){F f;f.Test("pm_alex");f.Test("pm_santa");const int n=f.b->runs;f.Test("pm_alex");CHECK(f.b->runs==n && f.v->Status().cacheHits==1);}
static void StableLiveIdentity(){F f;CHECK(f.v->Speak("A","c","Mano!"));f.Done();auto voice=f.v->Status().voiceClass;f.v->Configure(true,"pf_dora",1,.5,true);Context c;c.model=105;c.pedType=6;CHECK(f.v->Speak("A","c","Outra fala.",false,c));f.Done();CHECK(f.v->Status().voiceClass==voice);}
static void Spatial(){F f;f.v->Spatial(1,5);CHECK(f.v->Speak("A","c","Direita!"));f.Done();CHECK(f.a->left==0 && f.a->right>0 && f.a->right<.85);f.v->Spatial(-1,5);CHECK(f.v->Speak("A","c","Esquerda!"));f.Done();CHECK(f.a->right==0 && f.a->left>0);}
static void Disable(){F f;f.v->Configure(false,"pm_alex",1,.8,true);CHECK(!f.v->Test());f.v->Configure(true,"pm_alex",1,.8,true);CHECK(f.v->Test());f.Done();}
static void Config(){auto s=Ini("npc_tts_engine=kokoro\nnpc_tts_voice=pf_dora\nnpc_tts_voice_variation=0\nnpc_tts_spatial_audio=0\nnpc_tts_idle_unload_seconds=60\n");CHECK(s.configuration.valid && s.ttsVoice=="pf_dora" && !s.ttsVariation && !s.ttsSpatial && s.ttsIdleSeconds==60);}
static void Migration(){auto s=Ini("npc_tts_engine=piper\nnpc_tts_model=pt_BR-faber-medium\nnpc_tts_voice_ballas=pt_BR-jeff-medium\n");CHECK(s.configuration.valid && !s.configuration.warnings.empty() && s.ttsModel=="kokoro-pt-br" && s.ttsConfigError.empty());}
static void Duplicate(){auto s=Ini("gemini_enabled=1\nnpc_tts_voice=pm_alex\nnpc_tts_voice=pm_santa\n");CHECK(!s.configuration.valid && !s.ttsConfigError.empty() && s.providerConfigError.empty() && s.geminiEnabled);}
static void Invalid(){for(auto line:{"npc_tts_voice=42","npc_tts_speed=nan","npc_tts_volume=-1","npc_tts_spatial_audio=yes","npc_tts_threads=99","npc_tts_idle_unload_seconds=-1"}){auto s=Ini(line);CHECK(!s.configuration.valid && !s.ttsConfigError.empty() && s.providerConfigError.empty());}}
static void IdleUnload(){Settings s;s.ttsIdleSeconds=1;F f(s);f.Test("pm_alex");Wait([&]{return !f.v->Status().loaded;});CHECK(f.b->unloads==1);f.Test("pm_santa");CHECK(f.b->loads==2);}
static void ManualVoiceChange(){Settings s;s.ttsVariation=false;F f(s);CHECK(f.v->Speak("A","c","Primeira fala."));f.Done();CHECK(f.b->speaker==43);f.v->Configure(true,"pf_dora",1,.8,true);CHECK(f.v->Speak("A","c","Outra fala."));f.Done();CHECK(f.b->speaker==42 && f.b->loads==1);}
static void VariationSwitch(){F f;CHECK(f.v->Speak("A","c","Primeira fala."));f.Done();f.v->SetVariation(false);f.v->Configure(true,"pm_santa",1,.8,true);CHECK(f.v->Speak("A","c","Outra fala."));f.Done();CHECK(f.b->speaker==44 && !f.v->Options().ttsVariation);}
int main(){int count=0;try{for(auto t:{Defaults,Distribution,NoVariation,ThreeSpeakersOneSession,CacheIsolation,StableLiveIdentity,Spatial,Disable,Config,Migration,Duplicate,Invalid,IdleUnload,ManualVoiceChange,VariationSwitch}){t();++count;}}catch(const std::exception& e){std::cerr<<"Kokoro scenario "<<count+1<<": "<<e.what()<<'\n';return 1;}std::cout<<"Kokoro profiles/spatial/config: "<<count<<" host scenarios passed\n";}
