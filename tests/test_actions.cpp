#include "npc_action_controller.h"
#include <iostream>
#include <stdexcept>
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include <fstream>
#include <cstdio>
using namespace npcai;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #x);}while(0)
static Target T(std::string id="npc-live-17"){return {id,"Civil",257,1,{}};}
static ActionRequest R(Action action=Action::SurrenderMoney,std::string nonce="r1",int amount=100){Decision d;d.action=action;d.requestId=nonce;d.amount=amount;d.dialogue="Tá bom!";d.durationMs=8000;return {T(),"c1",d};}
struct Fake:ActionEngine {
    ActionContext context{true,true,false,true,true};
    std::vector<Action> starts;std::map<int,int> money;int stops=0,created=0,removed=0,failAt=-1;bool unavailable=false,loaded=true,automaticFace=true;Effect effect=Effect::Applied;
    ActionContext Inspect(const Target&)override{return context;}
    bool Start(const Target&,Action a,int)override{starts.push_back(a);return !unavailable;}
    Effect Poll(const Target&,Action a)override{if(a==Action::FacePlayer)return context.facing||automaticFace?Effect::Applied:Effect::Pending;return effect;}
    void Stop(const Target&)override{++stops;}
    bool PrepareMoney(const Target&)override{return loaded;}
    int SpawnMoney(const Target&,int amount,int)override{if(created==failAt)return -1;int h=++created;money[h]=amount;return h;}
    bool PickupAlive(int h)override{return money.count(h);}
    void RemovePickup(int h)override{removed+=money.erase(h);}
};
struct Fixture {
    Fake engine;Settings settings;std::vector<bool> outcomes;std::vector<int> amounts;
    NpcActionController controller;
    explicit Fixture(Settings s={}):settings(s),controller(engine,s,[this](const ActionRequest&,bool ok,const std::string&,int a){outcomes.push_back(ok);amounts.push_back(a);}){}
    void Drop(){CHECK(controller.Submit(R(),0));controller.Tick(0);CHECK(outcomes.empty());controller.Tick(899);CHECK(engine.created==0);controller.Tick(900);}
};
static void Parser(){
    const std::string wire=R"({"dialogue":"Tá bom, ação!","action":"surrender_money","emotion":"afraid","parameters":{"amount":100,"duration_ms":8000},"request_id":"r1"})";
    auto d=ParseDecision(wire,"r1");CHECK(d.action==Action::SurrenderMoney && d.amount==100 && d.dialogue=="Tá bom, ação!");
    CHECK(ParseDecision(wire,"other").action==Action::None);
    for(const auto& replace:std::vector<std::pair<std::string,std::string>>{{"surrender_money","execute_native"},{"100,","-1,"},{"100,","10001,"},{"100,","true,"},{"8000","10001"},{"afraid","arbitrary"},{"\"amount\":100","\"amount\":100,\"amount\":1"}}){auto bad=wire;bad.replace(bad.find(replace.first),replace.first.size(),replace.second);CHECK(ParseDecision(bad,"r1").action==Action::None);}
    CHECK(ParseDecision("Olá, CJ!", "r1").dialogue=="Olá, CJ!");
    CHECK(ParseDecision("{broken","r1").action==Action::None);
    CHECK(ParseDecision(std::string(30,'[')+std::string(30,']'),"r1").action==Action::None);
    for(auto a:{Action::Idle,Action::FacePlayer,Action::TalkGesture,Action::HandsUp,Action::Flee,Action::Angry,Action::Refuse,Action::FollowPlayer,Action::StopFollowing,Action::Cower}){
        std::string w="{\"dialogue\":\"OK\",\"action\":\""+std::string(ActionName(a))+"\",\"emotion\":\"neutral\",\"parameters\":{\"amount\":0,\"duration_ms\":1000},\"request_id\":\"r1\"}";CHECK(ParseDecision(w,"r1").action==a);
    }
}
static void StopFaceRelease(){
    Fixture f;f.engine.context.facing=false;f.engine.automaticFace=false;
    CHECK(f.controller.Begin(T(),0));f.controller.Tick(0);CHECK(f.engine.starts.front()==Action::FacePlayer);CHECK(f.controller.State(T().id)==InteractionState::FACE_PLAYER);
    f.engine.context.facing=true;f.controller.Tick(100);CHECK(f.engine.starts.back()==Action::Idle);CHECK(f.controller.State(T().id)==InteractionState::CONVERSING);
    int before=f.engine.stops;f.controller.Cancel(T().id);CHECK(f.engine.stops>before);CHECK(f.controller.State(T().id)==InteractionState::IDLE);
}
static void Pause(){Fixture f;f.engine.context.paused=true;CHECK(f.controller.Begin(T(),0));f.controller.Tick(0);CHECK(f.engine.starts.empty());f.engine.context.paused=false;f.controller.Tick(100);CHECK(!f.engine.starts.empty());}
static void Money(){Fixture f;f.Drop();CHECK(f.outcomes.size()==1 && f.outcomes[0] && f.amounts[0]==100);CHECK(f.engine.money.size()==5);int sum=0;for(const auto& p:f.engine.money)sum+=p.second;CHECK(sum==100);f.controller.Tick(1000);CHECK(f.engine.created==5);f.controller.Tick(3000);CHECK(!f.controller.Submit(R(Action::SurrenderMoney,"r2"),3000));CHECK(f.engine.created==5);CHECK(!f.outcomes.back());}
static void Duplicate(){Fixture f;f.Drop();CHECK(!f.controller.Submit(R(),901));CHECK(f.engine.created==5);f.controller.Reset();CHECK(f.engine.money.empty());CHECK(!f.controller.Submit(R(),1000));CHECK(!f.controller.Submit(R(Action::SurrenderMoney,"r3"),1000));}
static void Limits(){for(int amount:{-1,0,201,10001}){Fixture f;CHECK(!f.controller.Submit(R(Action::SurrenderMoney,"r1",amount),0));CHECK(f.engine.created==0 && !f.outcomes.back());}Settings s;s.moneySessionMax=99;Fixture f(s);CHECK(!f.controller.Submit(R(),0));}
static void Partial(){Fixture f;f.engine.failAt=2;f.Drop();CHECK(f.engine.created==2 && f.engine.removed==2 && f.engine.money.empty());CHECK(f.outcomes.size()==1 && !f.outcomes[0]);CHECK(!f.controller.Submit(R(Action::SurrenderMoney,"r2"),1000));}
static void Disappear(){Fixture f;CHECK(f.controller.Submit(R(),0));f.controller.Tick(0);f.engine.context.valid=false;f.controller.Tick(900);CHECK(f.engine.created==0 && !f.outcomes.back());CHECK(f.controller.State(T().id)==InteractionState::IDLE);}
static void CombatMission(){Fixture f;f.Drop();f.engine.context.eligible=false;f.controller.Tick(901);CHECK(f.engine.money.empty());CHECK(f.controller.State(T().id)==InteractionState::IDLE);CHECK(!f.controller.Begin(T(),1000));}
static void AnimUnavailable(){Fixture f;f.engine.unavailable=true;CHECK(f.controller.Submit(R(),0));f.controller.Tick(0);CHECK(!f.outcomes.back() && f.engine.created==0);}
static void Timeout(){Fixture f;f.engine.effect=Effect::Pending;CHECK(f.controller.Submit(R(),0));f.controller.Tick(0);f.controller.Tick(10000);CHECK(!f.outcomes.back() && f.engine.created==0);}
static void FollowStop(){Fixture f;CHECK(f.controller.Submit(R(Action::FollowPlayer,"r1",0),0));f.controller.Tick(0);CHECK(f.outcomes.back());CHECK(f.controller.Executing(T().id));CHECK(f.controller.Submit(R(Action::StopFollowing,"r2",0),500));f.controller.Tick(500);CHECK(f.outcomes.back() && !f.controller.Executing(T().id));}
static void Expiry(){Fixture f;f.Drop();f.controller.Tick(60900);CHECK(f.engine.money.empty() && f.controller.PickupCount()==0);}
static void Configuration(){
    const std::string path="build/action-settings-test.ini";
    {std::ofstream f(path);f<<"npc_face_player=0\nnpc_stop_on_talk=0\nnpc_ai_actions=0\nnpc_money_drop=0\nnpc_money_max=150\nnpc_money_pickups_max=3\nnpc_action_timeout_ms=3000\nnpc_money_session_max=400\nnpc_money_repeat=1\n";}
    auto s=LoadSettings(path);CHECK(!s.facePlayer && !s.stopOnTalk && !s.aiActions && !s.moneyDrop && s.moneyRepeat);
    CHECK(s.moneyMax==150 && s.moneyPickupsMax==3 && s.actionTimeoutMs==3000 && s.moneySessionMax==400);
    for(const auto& text:{"npc_money_max=-1","npc_money_pickups_max=6","npc_action_timeout_ms=10001","npc_money_session_max=100001","npc_ai_actions=yes"}){
        {std::ofstream f(path);f<<text;}auto recovered=LoadSettings(path);CHECK(!recovered.configuration.valid && !recovered.configuration.errors.empty());
    }
    std::remove(path.c_str());
}
static void Feedback(){
    std::atomic_bool received{false};std::atomic_bool feedback{false};
    Chat c([&](const Request& r,const std::atomic_bool&){feedback=r.feedback && r.feedback->status=="executed";auto d=R(Action::HandsUp,r.requestId,0).decision;received=true;return Result{true,d.dialogue,d};},10);
    c.Select(T());CHECK(c.Send("Mãos ao alto!"));
    std::optional<ActionRequest> action;
    for(int n=0;n<1000 && !action;++n){action=c.TakeAction();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    CHECK(received && action && c.Get().waiting && c.Get().messages.size()==1);CHECK(!c.Send("aguarde"));
    c.ReportAction(*action,true,"Mãos levantadas.");CHECK(!c.Get().waiting && c.Get().messages.size()==2);received=false;CHECK(c.Send("Obrigado"));
    for(int n=0;n<1000 && !received;++n)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(feedback);
    c.Clear();CHECK(!c.Get().waiting);c.Shutdown();
    Request req{T().id,"c1","Olá",{}};req.requestId="r1";req.feedback=ActionFeedback{"r0","hands_up","executed",0};auto json=Json(req);CHECK(json.find("\"protocol\":2")!=std::string::npos && json.find("action_result")!=std::string::npos);
}
static ActionRequest AwaitAction(Chat& chat){
    for(int n=0;n<1000;++n){if(auto action=chat.TakeAction())return *action;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    throw std::runtime_error("action timeout");
}
static void SpeechSurvivesAnimationFailure(){
    Chat chat([](const Request& r,const std::atomic_bool&){auto d=R(Action::TalkGesture,r.requestId,0).decision;d.dialogue="E aí, CJ? Como tá o bairro?";return Result{true,d.dialogue,d};});
    chat.Select(T());CHECK(chat.Send("Oi"));auto proposal=AwaitAction(chat);
    CHECK(proposal.dialoguePublished && !chat.Get().waiting && chat.Get().messages.size()==2);
    CHECK(chat.Get().messages.back().text=="E aí, CJ? Como tá o bairro?");
    Fake engine;engine.context.eligible=false;engine.context.reason="foreign_temporary_event";
    NpcActionController controller(engine,Settings{},[&](const ActionRequest& r,bool ok,const std::string& text,int amount){CHECK(!ok && text.find("foreign_temporary_event")!=std::string::npos);chat.ReportAction(r,ok,text,amount);});
    CHECK(!controller.Submit(proposal,0));CHECK(chat.Get().messages.size()==2 && !chat.Get().waiting);
    CHECK(chat.Get().messages.back().text.find("entrega")==std::string::npos);chat.Shutdown();
}
static void CleanPostureReceipt(){
    for(bool success:{false,true}){
        Chat chat([](const Request& r,const std::atomic_bool&){auto d=R(Action::FacePlayer,r.requestId,0).decision;d.dialogue="Pode falar, CJ!";return Result{true,d.dialogue,d};});
        chat.Select(T());CHECK(chat.Send("Oi"));auto proposal=AwaitAction(chat);CHECK(chat.Get().waiting && !proposal.dialoguePublished);
        chat.ReportAction(proposal,success,success?"[Ação confirmada: face_player.]":"Não posso executar esse pedido nesta situação. Nenhuma entrega foi feita.");
        CHECK(!chat.Get().waiting && chat.Get().messages.size()==2);CHECK(chat.Get().messages.back().text=="Pode falar, CJ!");chat.Shutdown();
    }
}
static void NoFalsePhysicalClaim(){
    for(auto action:{Action::SurrenderMoney,Action::FollowPlayer,Action::HandsUp,Action::Flee,Action::Cower}){
        Chat chat([=](const Request& r,const std::atomic_bool&){auto d=R(action,r.requestId,action==Action::SurrenderMoney?100:0).decision;d.dialogue="Calma, mano. Vou tentar.";return Result{true,d.dialogue,d};});
        chat.Select(T());CHECK(chat.Send("Faça isso"));auto proposal=AwaitAction(chat);CHECK(!proposal.dialoguePublished && chat.Get().messages.size()==1);
        chat.ReportAction(proposal,false,"engine technical detail");auto text=chat.Get().messages.back().text;
        CHECK(text=="Calma, mano. Vou tentar." && text.find("technical")==std::string::npos);CHECK(!chat.Get().waiting);chat.Shutdown();
    }
}
int main(){int count=0;for(auto test:{Parser,StopFaceRelease,Pause,Money,Duplicate,Limits,Partial,Disappear,CombatMission,AnimUnavailable,Timeout,FollowStop,Expiry,Configuration,Feedback,SpeechSurvivesAnimationFailure,CleanPostureReceipt,NoFalsePhysicalClaim}){test();++count;}std::cout<<"Action controller: "<<count<<" scenarios passed (fake engine; device validation pending)\n";}
