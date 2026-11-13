#include "npc_action_controller.h"
#include "dialogue_timing.h"
#include "registry.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <atomic>
#include <thread>
#include <chrono>
using namespace npcai;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #x);}while(0)
static Target T(){return {"session-person-17","Civil",257,1,{}};}
struct Engine:ActionEngine {
    ActionContext c{true,true,false,true,true};std::map<Action,int> active,starts;std::set<Action> ended;int pickups=0;std::map<int,int> money;bool animationAvailable=true;
    ActionContext Inspect(const Target&)override{return c;}
    bool Start(const Target&,Action a,int duration)override{
        if(a!=Action::Idle && a!=Action::FacePlayer && !animationAvailable)return false;
        ended.erase(a);if(!active.count(a)){active[a]=duration;++starts[a];}return true;
    }
    Effect Poll(const Target&,Action a)override{
        if(!c.valid || !c.eligible)return Effect::Failed;
        if(a==Action::FacePlayer)return c.facing?Effect::Applied:Effect::Pending;
        if(ended.count(a))return Effect::Finished;
        return active.count(a)?Effect::Applied:Effect::Failed;
    }
    void Stop(const Target&)override{active.clear();}
    void StopAction(const Target&,Action a)override{active.erase(a);ended.erase(a);}
    bool PrepareMoney(const Target&)override{return true;}
    int SpawnMoney(const Target&,int amount,int)override{int handle=++pickups;money[handle]=amount;return handle;}
    bool PickupAlive(int h)override{return money.count(h);}
    void RemovePickup(int h)override{money.erase(h);}
};
struct Fixture {
    Engine engine;Settings settings;int confirmed=0,failed=0;NpcActionController controller;
    Fixture(Settings s={}):settings(s),controller(engine,s,[&](const ActionRequest&,bool ok,const std::string&,int){if(ok)++confirmed;else ++failed;}){}
    void Action(Action a,std::string nonce="r1",int amount=0){Decision d;d.action=a;d.requestId=nonce;d.dialogue="Calma, parceiro.";d.amount=amount;d.durationMs=1000;CHECK(controller.Submit({T(),"c1",d},0));controller.Tick(0);}
};
static void PersistentHands(){Fixture f;f.Action(Action::HandsUp);CHECK(f.confirmed==1 && f.controller.Persistent(T().id));for(uint64_t t=500;t<=30000;t+=500)f.controller.Tick(t);CHECK(f.engine.active.count(Action::HandsUp));CHECK(f.engine.starts[Action::HandsUp]==1);CHECK(f.controller.State(T().id)==InteractionState::SURRENDERING);}
static void GestureCannotCancelSurrender(){Fixture f;f.Action(Action::HandsUp);Decision d;d.action=Action::TalkGesture;d.requestId="r2";d.durationMs=500;d.dialogue="Qual foi?";CHECK(!f.controller.Submit({T(),"c1",d},2000));f.controller.Speak(T(),"Uma fala nova.",2000);f.controller.Tick(15000);CHECK(f.controller.Persistent(T().id) && f.engine.starts[Action::TalkGesture]==0 && f.engine.active.count(Action::HandsUp));}
static void DistanceHysteresis(){Fixture f;f.Action(Action::HandsUp);f.engine.c.distance=7;f.controller.Tick(1000);f.controller.Tick(2999);CHECK(f.controller.Persistent(T().id));f.engine.c.distance=5;f.controller.Tick(3000);f.engine.c.distance=7;f.controller.Tick(4000);f.controller.Tick(6000);CHECK(!f.controller.Persistent(T().id) && f.engine.active.empty());}
static void ChatClose(){Fixture f;f.Action(Action::HandsUp);f.controller.Detach(T().id);f.controller.Tick(15000);CHECK(f.controller.Persistent(T().id));}
static void InvalidAndCritical(){for(bool invalid:{false,true}){Fixture f;f.Action(Action::HandsUp);if(invalid)f.engine.c.valid=false;else f.engine.c.eligible=false;f.controller.Tick(1);CHECK(f.engine.active.empty() && !f.controller.Persistent(T().id));}}
static void Reading(){Settings s;CHECK(DialogueDuration("Qual foi, mano?",s)==2500);CHECK(DialogueDuration(std::string(1200,'x'),s)==2500);std::string longText;for(int i=0;i<100;++i)longText+="palavra ";CHECK(DialogueDuration(longText,s)==12000);s.readingWpm=90;CHECK(DialogueDuration("um dois três quatro cinco seis sete oito nove dez",s)>6000);}
static void LongSpeech(){Fixture f;CHECK(f.controller.Begin(T(),0));f.controller.Tick(0);std::string speech;for(int i=0;i<70;++i)speech+="palavra ";f.controller.Speak(T(),speech,0);for(uint64_t t=1;t<13000;t+=100)f.controller.Tick(t);CHECK(f.engine.starts[Action::TalkGesture]>=3 && f.engine.starts[Action::TalkGesture]<=5);CHECK(f.engine.starts[Action::Idle]==1);CHECK(!f.engine.active.count(Action::TalkGesture));}
static void ShortSpeech(){Fixture f;f.controller.Begin(T(),0);f.controller.Tick(0);f.controller.Speak(T(),"Qual foi?",0);for(uint64_t t=1;t<3000;t+=20)f.controller.Tick(t);CHECK(f.engine.starts[Action::TalkGesture]==1 && !f.engine.active.count(Action::TalkGesture));}
static void FaceAndGestures(){Fixture f;f.Action(Action::HandsUp);f.engine.c.needsTurn=true;f.engine.c.facing=false;f.controller.Tick(1000);CHECK(f.engine.active.count(Action::FacePlayer) && f.engine.active.count(Action::HandsUp));f.engine.c.needsTurn=false;f.engine.c.facing=true;f.controller.Tick(1500);CHECK(f.engine.active.count(Action::HandsUp) && f.engine.active.count(Action::Idle));}
static void PersistentFollow(){Fixture f;f.Action(Action::FollowPlayer);f.controller.Detach(T().id);f.engine.c.distance=8;f.controller.Tick(20000);CHECK(f.engine.active.count(Action::FollowPlayer) && f.engine.starts[Action::Idle]==0);f.controller.Speak(T(),"uma fala longa de conversa",20000);f.controller.Tick(22000);CHECK(f.engine.active.count(Action::FollowPlayer) && f.engine.starts[Action::TalkGesture]==0);}
static void MoneyKeepsPose(){Fixture f;f.Action(Action::SurrenderMoney,"r1",103);f.controller.Tick(900);CHECK(f.engine.pickups==5);int sum=0;for(auto& m:f.engine.money)sum+=m.second;CHECK(sum==103);f.controller.Tick(15000);CHECK(f.engine.active.count(Action::HandsUp) && f.engine.starts[Action::HandsUp]==1 && f.engine.pickups==5);Decision d;d.action=Action::SurrenderMoney;d.amount=100;d.requestId="r2";CHECK(!f.controller.Submit({T(),"c1",d},16000));CHECK(f.engine.pickups==5);}
static void MoneyRequiresCurrentPose(){Fixture f;f.Action(Action::SurrenderMoney,"r1",100);f.controller.Tick(500);f.engine.active.erase(Action::HandsUp);f.controller.Tick(900);CHECK(f.engine.pickups==0);f.controller.Tick(1000);CHECK(f.engine.active.count(Action::HandsUp) && f.engine.pickups==5);}
static void ReleaseCommand(){Fixture f;f.Action(Action::HandsUp);Decision d;d.action=Action::ReleaseSurrender;d.requestId="r2";CHECK(f.controller.Submit({T(),"c1",d},20000));CHECK(!f.controller.Persistent(T().id) && !f.engine.active.count(Action::HandsUp));}
static void PoseRepairBounded(){Fixture f;f.Action(Action::HandsUp);for(uint64_t t=1000;t<6000;t+=1000){f.engine.active.erase(Action::HandsUp);f.controller.Tick(t);}CHECK(!f.controller.Persistent(T().id));CHECK(f.engine.starts[Action::HandsUp]<=4);}
static void Traits(){auto a=MakePersonality("idA",22,4),b=MakePersonality("idB",22,4),again=MakePersonality("idA",22,4);CHECK(a.courage==again.courage && a.kindness==again.kindness);CHECK(a.courage!=b.courage || a.aggression!=b.aggression);Registry registry("session");Observation o;o.slot=1;o.ref=257;o.alive=true;o.position={1,0,0};o.context.model=22;registry.Observe(o);auto first=registry.Nearest({},0,3);CHECK(first && first->context.hasPersonality);registry.ActionConfirmed(first->id,Action::HandsUp);registry.Observe(o);auto second=registry.Nearest({},0,3);CHECK(second->context.personality.courage==first->context.personality.courage && second->context.personality.fear>first->context.personality.fear);registry.Destroy(1);registry.Observe(o);CHECK(registry.Nearest({},0,3)->id!=first->id);registry.Destroy(1);o.context.personalityVariation=false;registry.Observe(o);CHECK(registry.Nearest({},0,3)->context.personality.courage==50);}
static void UnknownSpeech(){auto d=ParseDecision(R"({"dialogue":"Tá louco, mano?","action":"dog_bark","emotion":"angry","parameters":{},"request_id":"r1"})","r1");CHECK(d.action==Action::None && d.dialogue=="Tá louco, mano?");}
static void NewSettings(){const std::string path="build/animation-test.ini";{std::ofstream f(path);f<<"npc_hold_surrender=1\nnpc_surrender_release_distance=7\nnpc_surrender_release_delay_ms=3000\nnpc_dialogue_reading_wpm=200\nnpc_dialogue_min_ms=3000\nnpc_dialogue_max_ms=15000\nnpc_personality_variation=0\nnpc_animation_debug=1\n";}auto s=LoadSettings(path);CHECK(s.holdSurrender && s.surrenderReleaseDistance==7 && s.surrenderReleaseDelayMs==3000 && s.readingWpm==200 && !s.personalityVariation && s.animationDebug);for(auto bad:{"npc_surrender_release_distance=3","npc_dialogue_reading_wpm=0","npc_dialogue_min_ms=15000\nnpc_dialogue_max_ms=2500"}){{std::ofstream f(path);f<<bad;}auto recovered=LoadSettings(path);CHECK(!recovered.configuration.valid);}std::remove(path.c_str());}
static void PublicationScheduling(){
    std::atomic_bool entered{false},release{false};Chat chat([&](const Request&,const std::atomic_bool& cancel){entered=true;while(!release && !cancel)std::this_thread::sleep_for(std::chrono::milliseconds(1));return Result{true,"Qual foi, parceiro?"};});
    Settings options;options.dynamicBubble=true;chat.ConfigureDialogue(options);chat.Select(T());CHECK(chat.Send("Oi"));
    for(int n=0;n<1000 && !entered;++n)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(entered && !chat.TakeSpeech());release=true;
    for(int n=0;n<1000 && chat.Get().waiting;++n)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto speech=chat.TakeSpeech();CHECK(speech && speech->text=="Qual foi, parceiro?" && !chat.TakeSpeech());CHECK(chat.Get().bubbleDurationMs==2500);
    Fixture f;f.controller.Begin(T(),0);f.controller.Tick(0);CHECK(f.engine.starts[Action::TalkGesture]==0);f.controller.Speak(speech->target,speech->text,1000);f.controller.Tick(1000);CHECK(f.engine.starts[Action::TalkGesture]==1);chat.Shutdown();
}
static void NaturallyEndedClips(){
    Fixture f;f.controller.Begin(T(),0);f.controller.Tick(0);std::string speech;for(int i=0;i<70;++i)speech+="palavra ";f.controller.Speak(T(),speech,0);
    for(uint64_t time=100;time<13000;time+=100){
        if(time%800==0 && f.engine.active.count(Action::TalkGesture)){f.engine.active.erase(Action::TalkGesture);f.engine.ended.insert(Action::TalkGesture);}
        f.controller.Tick(time);
    }
    CHECK(f.engine.starts[Action::TalkGesture]>3 && f.engine.starts[Action::TalkGesture]<14);CHECK(f.engine.starts[Action::Idle]==1);
}
static void LegacyRadius(){const std::string path="build/legacy-radius.ini";{std::ofstream f(path);f<<"radius=10\n";}auto settings=LoadSettings(path);CHECK(settings.surrenderReleaseDistance>settings.radius && settings.followReleaseDistance>settings.radius);std::remove(path.c_str());}
static void AudioDrivenGestures(){Fixture f;f.controller.Begin(T(),0);for(uint64_t time=0;time<16000;time+=100){f.controller.VoicePlayback(T(),true,time);f.controller.Tick(time);}CHECK(f.engine.starts[Action::TalkGesture]>2);f.controller.VoicePlayback(T(),false,16000);f.controller.Tick(16000);CHECK(!f.engine.active.count(Action::TalkGesture));}
static void AudioRespectsSurrender(){Fixture f;f.Action(Action::HandsUp);for(uint64_t time=0;time<15000;time+=100){f.controller.VoicePlayback(T(),true,time);f.controller.Tick(time);}CHECK(f.controller.State(T().id)==InteractionState::SURRENDERING && f.engine.starts[Action::TalkGesture]==0);}
int main(){int n=0;for(auto test:{PersistentHands,GestureCannotCancelSurrender,DistanceHysteresis,ChatClose,InvalidAndCritical,Reading,LongSpeech,ShortSpeech,FaceAndGestures,PersistentFollow,MoneyKeepsPose,MoneyRequiresCurrentPose,ReleaseCommand,PoseRepairBounded,Traits,UnknownSpeech,NewSettings,PublicationScheduling,LegacyRadius,NaturallyEndedClips,AudioDrivenGestures,AudioRespectsSurrender}){test();++n;}std::cout<<"Persistent animations: "<<n<<" host scenarios passed (no GTA device)\n";}
