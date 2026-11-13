#include "chat.h"
#include "registry.h"
#include "settings.h"
#include "editor_state.h"
#include "bubble_layout.h"
#include "focus_lease.h"
#include <fstream>
#include <cstdio>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace npcai;
using namespace std::chrono_literals;
#define CHECK(x) do {if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #x);}while(0)
static void Until(const std::function<bool()>& f) {
    auto end=std::chrono::steady_clock::now()+2s;
    while(!f()) {if(std::chrono::steady_clock::now()>end) throw std::runtime_error("timeout no teste");std::this_thread::sleep_for(1ms);}
}
static Target T(std::string id){return {id,"NPC",257,1,{}};}
static Observation O(int slot,int gen,float x) {Observation o;o.slot=slot;o.ref=(slot<<8)|gen;o.alive=true;o.position={x,0,0};o.context.model=105;return o;}
static void Utf8(){
    CHECK(ValidUtf8("Olá, CJ! Ação, ç, Ã, ê 😀"));
    CHECK(!ValidUtf8(std::string("\xc0\xaf",2)));
    CHECK(!ValidUtf8(std::string("\xed\xa0\x80",3)));
    CHECK(!ValidUtf8(std::string("\xf4\x90\x80\x80",4)));
    CHECK(!ValidUtf8(std::string("\xc3",1)));
    CHECK(!ValidUtf8(std::string("a\0b",3)));
    auto j=Json({"npc-19","c2","Olá \"CJ\"\n\\",{}});
    CHECK(j.find("Olá \\\"CJ\\\"\\u000a\\\\")!=std::string::npos);
    CHECK(SessionId().size()==32);CHECK(SessionId()!=SessionId());
}
static void Selection(){
    Registry r("session");r.Begin();
    auto player=O(0,1,0);player.player=true;r.Observe(player);
    r.Observe(O(1,1,2));r.Observe(O(2,2,1));r.Observe(O(3,1,4));
    auto bad=O(4,1,.1f);bad.alive=false;r.Observe(bad);
    bad=O(5,1,.1f);bad.position.x=std::numeric_limits<float>::quiet_NaN();r.Observe(bad);
    bad=O(6,1,.1f);bad.context.area=1;r.Observe(bad);r.End();
    auto t=r.Nearest({},0,3);CHECK(t && t->ref==514);
    auto other=r.Nearest({},0,3,t->id);CHECK(other && other->ref==257);
    CHECK(!r.Resolve(t->id,{},1,3));
    CHECK(!r.Nearest({},0,-1));CHECK(!r.Nearest({NAN,0,0},0,3));
    CHECK(r.Position(t->id) && r.Position(t->id)->x==1);
    r.Destroy(2);CHECK(!r.Position(t->id));CHECK(!r.Resolve(t->id,{},0,3));
    r.Observe(O(2,2,1));auto reborn=r.Nearest({},0,3);CHECK(reborn->id!=t->id);
    auto old=reborn->id;r.Observe(O(2,3,1));CHECK(r.Nearest({},0,3)->id!=old);
    old=r.Nearest({},0,3)->id;
    auto model=O(2,3,1);model.context.model=106;r.Observe(model);CHECK(r.Nearest({},0,3)->id!=old);
    r.Begin();r.End();CHECK(r.Ids().empty());
}
static void PoolWrap(){
    Registry r("session");r.Observe(O(1,1,1));auto first=r.Nearest({},0,3)->id;
    for(int n=0;n<256;++n) {r.Destroy(1);r.Observe(O(1,n%128,1));CHECK(r.Nearest({},0,3)->id!=first);}
    r.Reset();r.Observe(O(1,1,1));CHECK(r.Nearest({},0,3)->id!=first);
}
static void IsolationAndLimits(){
    Chat chat([](const Request& r,const std::atomic_bool&){return Result{true,"Olá "+r.npcId};},4);
    chat.Select(T("real-123"));CHECK(chat.Send("Oi, você é real?"));
    Until([&]{return !chat.Get().waiting;});CHECK(chat.Get().messages.size()==2);
    chat.Select(T("real-456"));CHECK(chat.Get().messages.empty());CHECK(chat.Send("ação"));
    Until([&]{return !chat.Get().waiting;});CHECK(chat.Get().messages.back().text=="Olá real-456");
    chat.Select(T("real-123"));CHECK(chat.Get().messages.back().text=="Olá real-123");
    CHECK(!chat.Send("   "));CHECK(!chat.Send(std::string(1201,'a')));
    for(int i=0;i<6;++i) {CHECK(chat.Send("oi"));Until([&]{return !chat.Get().waiting;});}
    CHECK(chat.Get().messages.size()==4);
    chat.Clear();CHECK(chat.Get().messages.empty());
    chat.Retain({"real-456"});CHECK(!chat.Get().target);CHECK(!chat.Send("oi"));
}
static void LateReply(){
    std::atomic_bool entered{false},release{false};
    Chat chat([&](const Request&,const std::atomic_bool&){entered=true;Until([&]{return release.load();});return Result{true,"ATRASADA"};});
    chat.Select(T("A"));CHECK(chat.Send("oi"));Until([&]{return entered.load();});CHECK(!chat.Send("outro"));
    chat.Select(T("B"));CHECK(!chat.Get().waiting);release=true;
    Until([&]{return chat.Send("nova");});Until([&]{return !chat.Get().waiting;});
    CHECK(chat.Get().messages.size()==2);
    chat.Select(T("A"));CHECK(chat.Get().messages.size()==1); // nenhuma resposta atrasada de A.
}
static void ClearWhileThinking(){
    std::atomic_bool entered{false},release{false};std::string first,second;std::atomic_int calls{0};
    Chat chat([&](const Request& r,const std::atomic_bool&){
        if(++calls==1) {first=r.conversationId;entered=true;Until([&]{return release.load();});}
        else second=r.conversationId;
        return Result{true,"resposta"};
    });
    chat.Select(T("A"));CHECK(chat.Send("antes"));Until([&]{return entered.load();});
    chat.Clear();release=true;Until([&]{return chat.Send("depois");});Until([&]{return !chat.Get().waiting;});
    CHECK(first!=second);CHECK(chat.Get().messages.size()==2);CHECK(chat.Get().messages[0].text=="depois");
}
static void Shutdown(){
    std::atomic_bool entered{false};
    Chat chat([&](const Request&,const std::atomic_bool& cancel){entered=true;Until([&]{return cancel.load();});return Result{false,"cancelado"};});
    chat.Select(T("A"));CHECK(chat.Send("oi"));Until([&]{return entered.load();});
    auto start=std::chrono::steady_clock::now();chat.Shutdown();CHECK(std::chrono::steady_clock::now()-start<500ms);
    chat.Shutdown();CHECK(!chat.Send("depois"));
}
static void BadReply(){
    Chat chat([](const Request&,const std::atomic_bool&){return Result{true,std::string(6001,'a')};});
    chat.Select(T("A"));CHECK(chat.Send("oi"));Until([&]{return !chat.Get().waiting;});
    CHECK(!chat.Get().error.empty());CHECK(chat.Get().messages.size()==1);
}
static void Projection(){
    auto p=ProjectToUi(640,360,10,1280,720,1920,1080);
    CHECK(p && p->x==960 && p->y==540);
    p=ProjectToUi(300,200,1,600,400,720,1280);CHECK(p && p->x==360 && p->y==640);
    CHECK(!ProjectToUi(10,10,-1,1280,720,1280,720));
    CHECK(!ProjectToUi(NAN,10,1,1280,720,1280,720));
    CHECK(!ProjectToUi(1280,10,1,1280,720,1280,720));
    CHECK(!ProjectToUi(1,1,1,0,720,1280,720));
    auto origin=BubbleOrigin({640,400},300,100,1280,720,6,7);
    CHECK(origin && origin->y+100+7==394);
    CHECK(!BubbleOrigin({10,30},300,100,1280,720,6,7));
    CHECK(BubbleOrigin({10,300},300,100,1280,720,6,7)->x==0);
}
static void EditorCancelAndRepeat(){
    EditorState e;std::string text;
    for(int i=0;i<100;++i){auto id=e.Begin();CHECK(id && e.Active());CHECK(!e.Begin());
        CHECK(e.Finish(*id,std::nullopt));CHECK(e.Take(text));CHECK(text.empty());CHECK(!e.Active());CHECK(!e.Take(text));}
    auto id=e.Begin();CHECK(e.Finish(*id,std::string("Olá ç 😀")));CHECK(e.Take(text));CHECK(text=="Olá ç 😀");
    id=e.Begin();CHECK(e.Finish(*id,std::string(1201,'a')));CHECK(e.Take(text) && text.empty());
}
static void EditorStaleAndThreads(){
    EditorState e;auto old=e.Begin();CHECK(e.Invalidate()==old);auto next=e.Begin();CHECK(next && next!=old);
    CHECK(!e.Finish(*old,std::string("stale")));CHECK(!e.Begin());
    std::thread callback([&]{CHECK(e.Finish(*next,std::string("resposta")));});callback.join();
    std::string text;CHECK(e.Take(text) && text=="resposta");
    CHECK(!e.Finish(*next,std::nullopt));
    old=e.Begin();CHECK(e.Finish(*old,std::string("late after destruction")));e.Invalidate();CHECK(!e.Take(text));
}
static void FocusLifetime(){
    FocusLease f;f.Begin("A",0);f.Tick(20000,true,false);CHECK(f.Id()=="A");
    f.Sent(20000);f.Tick(21000,false,true);CHECK(f.Id()=="A");
    f.Tick(22000,false,false);f.Tick(36999,false,false);CHECK(f.Id()=="A");
    f.Tick(37000,false,false);CHECK(f.Id().empty());
    f.Begin("B",40000);f.Release();CHECK(f.Id().empty());
    f.Begin("C",50000);CHECK(f.Id()=="C");f.Release();CHECK(f.Id().empty());
}
static void BubbleSettings(){
    const auto file=std::string("/tmp/npcai-settings-")+SessionId()+".ini";
    {std::ofstream f(file);f<<"bubble_height=1.6\nbubble_screen_offset_y=10\nnpc_stand_still=0\nnpc_dialogue_animations=1\n";}
    auto s=LoadSettings(file);CHECK(std::abs(s.bubbleHeight-1.6f)<.001f && s.bubbleScreenOffsetY==10 && !s.npcStandStill && s.npcDialogueAnimations);
    {std::ofstream f(file);f<<"bubble_height=nan\n";}
    auto recovered=LoadSettings(file);CHECK(!recovered.configuration.valid && recovered.bubbleHeight==1.75f);
    {std::ofstream f(file);f<<"bubble_screen_offset_y=-100\n";}
    recovered=LoadSettings(file);CHECK(!recovered.configuration.valid && recovered.bubbleScreenOffsetY==6);
    std::remove(file.c_str());
}
int main(){
    int count=0;
    for(const auto& t:std::vector<std::pair<const char*,std::function<void()>>>{{"UTF-8/JSON",Utf8},{"seleção/invalidação",Selection},
        {"wrap/reload",PoolWrap},{"memória/limites",IsolationAndLimits},{"resposta atrasada",LateReply},
        {"limpeza concorrente",ClearWhileThinking},{"shutdown cancelável",Shutdown},{"limite resposta",BadReply},{"projeção e cauda do balão",Projection},{"editor cancelar/reabrir",EditorCancelAndRepeat},{"editor retorno tardio/thread",EditorStaleAndThreads},{"foco do NPC e liberação",FocusLifetime},{"configuração dos balões",BubbleSettings}}) {
        try {t.second();std::cout<<"PASS "<<t.first<<"\n";++count;}
        catch(const std::exception& e){std::cerr<<"FAIL "<<t.first<<": "<<e.what()<<"\n";return 1;}
    }
    std::cout<<count<<" testes nativos aprovados\n";
}
