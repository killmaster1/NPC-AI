#include "ui.h"
#include "game_bridge.h"
#include "../native/backend_control.h"
#include <mod/thirdparty/imgui/imgui.h>
#include <mod/thirdparty/imgui/imgui_internal.h>
#include <plugin-sdk/renderware/imgui/imgui_shared.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <optional>

namespace { bool opened=true,capture=false; std::optional<std::string> pendingEdit; bool imeActive=false,allowNative=false; }
namespace npcai::NativeKeyboard {
bool Show(const std::string&,const std::string&){imeActive=allowNative;return imeActive;}
bool Take(std::string& out){if(!pendingEdit) return false;out=std::move(*pendingEdit);pendingEdit.reset();imeActive=false;return true;}
void Invalidate(){imeActive=false;pendingEdit.reset();}
bool Active(){return imeActive || pendingEdit.has_value();} void Heartbeat(){}
}
namespace npcai::GameBridge {
const char* Status(){return "NPC disponível";}
bool Bubble(float& x,float& y,float,float){x=300;y=150;return true;}
void Open(bool b){opened=b;} void Sent(){opened=false;} bool IsOpen(){return opened;} void Switch(){}
}
void NpcImGuiCaptureAll(bool b){capture=b;}
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);}while(0)
static void Layout(float width,float height) {
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={width,height};io.DeltaTime=1.f/60;
    unsigned char* pixels;int fw,fh;io.Fonts->GetTexDataAsRGBA32(&pixels,&fw,&fh);
    npcai::Chat chat([](const npcai::Request&,const std::atomic_bool&){return npcai::Result{true,"Olá, ação!"};});
    chat.Select(npcai::Target{"s1.p1","Pedestre · modelo 105",257,2,{}});NpcUi::Init(chat);opened=true;
    for(int i=0;i<4;++i) {
        ImGui::NewFrame();NpcUi::Draw();
        auto* baked=io.Fonts->Fonts[0]->GetFontBaked(13.f);
        CHECK(baked && baked->FindGlyphNoFallback(0xE3)); // ã
        CHECK(baked->FindGlyphNoFallback(0xE7)); // ç
        ImGui::Render();
    }
    CHECK(capture);
    bool found=false;
    for(const auto* window:ImGui::GetCurrentContext()->Windows) {
        if(window->Active && !(window->Flags&ImGuiWindowFlags_ChildWindow)) {
            found=true;
            CHECK(window->Pos.x>=0 && window->Pos.y>=0);
            CHECK(window->Pos.x+window->Size.x<=width+1);
            CHECK(window->Pos.y+window->Size.y<=height+1);
            if(window->ScrollMax.y>1) {
                std::cerr<<"overflow "<<width<<"x"<<height<<": "<<window->ScrollMax.y<<" px\n";
                throw std::runtime_error("Teclado excedeu janela");
            }
        }
    }
    CHECK(found);
    opened=false;ImGui::NewFrame();NpcUi::Draw();ImGui::Render();CHECK(!capture);
    auto* bubble=ImGui::FindWindowByName("##NPCBubble");
    CHECK(bubble && bubble->Active);
    auto center=ImVec2(bubble->Pos.x+bubble->Size.x*.5f,bubble->Pos.y+bubble->Size.y*.5f);
    io.AddMousePosEvent(center.x,center.y);io.AddMouseButtonEvent(0,true);
    for(int n=0;n<2;++n){ImGui::NewFrame();NpcUi::Draw();ImGui::Render();}
    io.AddMouseButtonEvent(0,false);
    for(int n=0;n<2;++n){ImGui::NewFrame();NpcUi::Draw();ImGui::Render();}
    CHECK(opened);
    pendingEdit="Olá pelo teclado Android!";
    ImGui::NewFrame();NpcUi::Draw();ImGui::Render();
    CHECK(!opened);
    CHECK(!chat.Get().messages.empty());
    CHECK(chat.Get().messages.front().text=="Olá pelo teclado Android!");
    NpcUi::Shutdown();chat.Shutdown();ImGui::DestroyContext();
    std::cout<<"PASS balão clicável/envio direto "<<width<<"x"<<height<<"\n";
    std::cout<<"PASS layout "<<width<<"x"<<height<<"\n";
}
static void TouchRouting() {
    using namespace ImGuiShared;
    Coordinator c;int events=0;Client client{&events,[](void* p,const Touch*,uint32_t n,int32_t,int32_t,float){*static_cast<int*>(p)+=n;},[](void*){}};
    auto id=c.Add(client);Rect button{100,100,200,150};c.SetRects(id,&button,1);
    CHECK(!c.ProcessTouch(2,0,10,10,0,0));CHECK(!c.ProcessTouch(1,0,10,10,0,0));
    CHECK(c.ProcessTouch(2,0,110,110,0,0));CHECK(c.ProcessTouch(3,0,300,300,0,0));
    CHECK(c.ProcessTouch(1,0,300,300,0,0));c.Frame(800,600,.016f);CHECK(events==3);
    Rect modal{0,0,800,600};c.SetRects(id,&modal,1);
    CHECK(c.ProcessTouch(2,1,790,590,0,0));c.Remove(id);
    CHECK(c.ProcessTouch(1,1,790,590,0,0)); // fechamento não entrega release sem press ao jogo.
    CHECK(!c.ProcessTouch(2,1,790,590,0,0));
    std::cout<<"PASS captura/restauração de gestos touch\n";
}
static void NativeOverlayState(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1280,720};io.DeltaTime=1.f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    npcai::Chat chat([](const npcai::Request&,const std::atomic_bool&){return npcai::Result{true,"Olá"};});
    chat.Select(npcai::Target{"A","Pedestre A",257,2,{}});NpcUi::Init(chat);
    auto frame=[](){ImGui::NewFrame();NpcUi::Draw();ImGui::Render();};
    opened=true;allowNative=false;frame();allowNative=true;
    for(int i=0;i<5;++i){
        opened=true;npcai::NativeKeyboard::Show("","");frame();CHECK(capture && opened && imeActive);
        auto* history=ImGui::FindWindowByName("NPC AI · Conversa");CHECK(!history || !history->Active);
        pendingEdit=std::string{};frame();CHECK(!opened && !capture && !imeActive);CHECK(chat.Get().messages.empty());
    }
    opened=true;npcai::NativeKeyboard::Show("","");frame();pendingEdit="Texto Android enviado";frame();
    CHECK(!opened && !capture);CHECK(chat.Get().messages.front().text=="Texto Android enviado");
    opened=true;npcai::NativeKeyboard::Show("","");frame();
    chat.Select(npcai::Target{"B","Pedestre B",513,2,{}});pendingEdit="Resposta antiga";frame();
    CHECK(!imeActive && chat.Get().messages.empty());
    opened=true;npcai::NativeKeyboard::Show("","");frame();chat.Select({});opened=false;frame();
    CHECK(!imeActive && !opened && !capture);
    chat.Select(npcai::Target{"C","Pedestre C",769,2,{}});opened=true;frame();
    npcai::NativeKeyboard::Show("","");io.DisplaySize={200,100};frame();CHECK(capture);
    pendingEdit=std::string{};frame();CHECK(!capture && !opened && !imeActive);
    io.DisplaySize={1280,720};frame();
    NpcUi::Shutdown();chat.Shutdown();ImGui::DestroyContext();allowNative=false;
    std::cout<<"PASS editor ativo/cancelar/reabrir/trocar/remover NPC/envio\n";
}
struct UnavailableTts:npcai::VoiceBackend{bool Load(std::string& error)override{error="Modelo ausente";return false;}bool Generate(const std::string&,float,const std::atomic_bool&,const std::function<bool(const float*,size_t,int)>&)override{return false;}void Cancel()override{}uint64_t ModelBytes()const override{return 0;}};
struct UnavailableAudio:npcai::VoiceAudio{bool Open(int,float)override{return false;}int Write(const int16_t*,size_t)override{return -1;}int64_t Head()override{return -1;}void Close()override{}};
static void ProviderFailureKeepsUi(bool eleven=false){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1280,720};io.DeltaTime=1.f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    npcai::Settings settings;settings.ttsEngine=eleven?"elevenlabs":"kokoro";settings.geminiEnabled=true;settings.providerConfigError="Configuração de IA inválida: gemini_model";
    auto client=std::make_shared<npcai::GeminiClient>(settings,npcai::GeminiClient::HttpsTransport{});
    npcai::Chat chat([client](const npcai::Request& r,const std::atomic_bool& cancel){return client->Chat(r,{},npcai::JsonValue::array(),cancel);});
    auto diag=std::make_shared<npcai::Diagnostics>(settings,npcai::GeminiClient::HttpsTransport{},"");diag->Bridge(true);diag->Services(false,1,false,"jni_unavailable");
    auto voice=std::make_shared<npcai::VoiceManager>(settings,std::make_unique<UnavailableTts>(),std::make_unique<UnavailableAudio>());
    CHECK(voice->Test());for(int i=0;i<200 && voice->Status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));CHECK(!voice->Status().loaded && !voice->Status().error.empty());
    chat.Select(npcai::Target{"ui-error","Pedestre",257,2,{}});NpcUi::Init(chat,6,diag,voice);opened=false;
    auto frame=[](){ImGui::NewFrame();NpcUi::Draw();ImGui::Render();};frame();
    CHECK(ImGui::FindWindowByName("##NPCBubble")->Active);CHECK(ImGui::FindWindowByName("##NPCAILauncher")->Active);
    CHECK(ImGui::FindWindowByName("##NPCAIDiagnosticLauncher")->Active);CHECK(diag->Get().bubbles=="Balão projetado na tela");
    opened=true;allowNative=true;npcai::NativeKeyboard::Show("","");frame();CHECK(imeActive && capture);
    pendingEdit="Oi";frame();
    for(int n=0;n<100 && chat.Get().waiting;++n)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    CHECK(!chat.Get().waiting && !chat.Get().error.empty());CHECK(chat.Get().messages.size()==1 && chat.Get().messages[0].fromPlayer);
    frame();CHECK(ImGui::FindWindowByName("##NPCBubble")->Active);
    chat.Select({});frame();CHECK(ImGui::FindWindowByName("##NPCAIDiagnosticLauncher")->Active);
    auto* launcher=ImGui::FindWindowByName("##NPCAIDiagnosticLauncher");auto center=ImVec2(launcher->Pos.x+launcher->Size.x*.5f,launcher->Pos.y+launcher->Size.y*.5f);
    io.AddMousePosEvent(center.x,center.y);io.AddMouseButtonEvent(0,true);frame();frame();io.AddMouseButtonEvent(0,false);frame();frame();
    CHECK(ImGui::FindWindowByName("NPC AI · Diagnóstico") && ImGui::FindWindowByName("NPC AI · Diagnóstico")->Active && capture);
    NpcUi::Shutdown();chat.Shutdown();diag->Shutdown();ImGui::DestroyContext();allowNative=false;
    std::cout<<"PASS balão/IME/diagnóstico preservados com Gemini e Kokoro indisponíveis\n";
    std::cout<<"PASS diagnóstico acessível sem NPC selecionado\n";
}
int main(){try {for(auto size:{ImVec2{640,360},ImVec2{1280,720},ImVec2{1920,1080},ImVec2{720,1280}})Layout(size.x,size.y);TouchRouting();NativeOverlayState();ProviderFailureKeepsUi();ProviderFailureKeepsUi(true);}
catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
