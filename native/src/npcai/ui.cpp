#include "ui.h"
#include "game_bridge.h"
#include "native_keyboard.h"
#ifdef __ANDROID__
#include "android_eleven.h"
#endif
#include "bubble_layout.h"
#include "../../backend_control.h"
#include <mod/thirdparty/imgui/imgui.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace {
npcai::Chat* chat=nullptr;
char input[1201]{};
bool shift=false;
int page=0;
std::string lastId;
size_t lastMessages=0;
float bubbleGap=6;
std::shared_ptr<npcai::Diagnostics> diagnostics;
bool diagnosticOpen=false;
std::shared_ptr<npcai::VoiceManager> voice;
void DiagnosticPanel(float scale,ImVec2 screen){
    if(!diagnosticOpen || !diagnostics)return;
    ImGui::SetNextWindowPos({screen.x*.03f,screen.y*.03f},ImGuiCond_Appearing);
    ImGui::SetNextWindowSize({screen.x*.94f,screen.y*.94f},ImGuiCond_Appearing);
    if(ImGui::Begin("NPC AI · Diagnóstico",nullptr,ImGuiWindowFlags_NoSavedSettings)){
        if(ImGui::Button("Voltar",{100*scale,36*scale}))diagnosticOpen=false;
        ImGui::SameLine();const auto d=diagnostics->Get();
        ImGui::BeginDisabled(d.testing);if(ImGui::Button("Testar Gemini",{170*scale,36*scale}))diagnostics->TestGemini();ImGui::EndDisabled();
        if(ImGui::Button("Exportar relatório"))diagnostics->Export();
        ImGui::TextWrapped("%s",d.testStatus.c_str());
        if(!d.exportStatus.empty())ImGui::TextWrapped("%s",d.exportStatus.c_str());
        if(voice){
            ImGui::Separator();ImGui::TextUnformatted("Voz e personalidade");
            auto config=voice->Options();auto v=voice->Status();bool enabled=config.ttsEnabled,spatial=config.ttsSpatial;float speed=config.ttsSpeed,volume=config.ttsVolume;
            int engine=config.ttsEngine=="elevenlabs"?1:0;const char* engines[]={"Kokoro offline","ElevenLabs HTTPS"};
            if(ImGui::Combo("Motor",&engine,engines,2)){voice->Cancel();voice->Control("engine",engine?"elevenlabs":"kokoro");}
            bool changed=ImGui::Checkbox("Ativar voz",&enabled);int selected=config.ttsVoice=="pm_santa"?1:config.ttsVoice=="pf_dora"?2:0;
            if(engine==0){bool variation=config.ttsVariation;if(ImGui::Checkbox("Variar a voz por NPC",&variation))voice->SetVariation(variation);const char* names[]={"pm_alex (43)","pm_santa (44)","pf_dora (42)"};changed=ImGui::Combo("Voz padrão / teste",&selected,names,3)||changed;}
            else{
                int model=config.elevenModel=="eleven_flash_v2_5"?1:0;const char* models[]={"Eleven v3 (tags controladas)","Flash v2.5 (sem tags)"};
                if(ImGui::Combo("Modelo ElevenLabs",&model,models,2)){voice->Cancel();voice->Control("model",model?"eleven_flash_v2_5":"eleven_v3");}
                ImGui::TextWrapped("Voice ID: %s | Nome: %s",v.voiceId.empty()?"aguardando primeira fala":v.voiceId.c_str(),v.voiceName.c_str());
                ImGui::TextWrapped("Credencial própria: %s | HTTP: %d",v.keyStatus.empty()?"consulte status":v.keyStatus.c_str(),v.lastHttp);
#ifdef __ANDROID__
                if(ImGui::Button("Importar chave (Keystore)"))npcai::AndroidEleven::ImportEditor();
#endif
                if(ImGui::Button("Remover chave ElevenLabs"))voice->Control("remove_key");ImGui::SameLine();if(ImGui::Button("Atualizar status"))voice->Control("status");
                if(ImGui::Button("Consultar vozes da conta"))voice->Control("refresh_voices");
                ImGui::TextWrapped("Vozes locais/mapeamentos são configurados no INI. Consulta não sintetiza áudio. Não há fallback automático.");
                ImGui::TextWrapped("Caracteres enviados: %llu | Requisições: %llu | Cache hits: %llu | Evitadas: %llu",(unsigned long long)v.sentCharacters,(unsigned long long)v.requests,(unsigned long long)v.cacheHits,(unsigned long long)v.avoided);
                ImGui::TextWrapped("Estimativa hora: %.1f / %.1f | dia UTC: %.1f / %.1f",v.hourCredits,config.elevenHour,v.dayCredits,config.elevenDay);
                ImGui::TextWrapped("%s",v.creditStatus.c_str());if(ImGui::Button("Limpar cache de áudio"))voice->Control("clear_cache");
            }
            const bool canSpeed=engine==0 || config.elevenModel=="eleven_flash_v2_5";ImGui::BeginDisabled(!canSpeed);changed=ImGui::SliderFloat("Velocidade",&speed,.7f,1.2f)||changed;ImGui::EndDisabled();
            if(!canSpeed)ImGui::TextWrapped("v3: entonação pelo texto/tags; speed não é enviado.");
            changed=ImGui::SliderFloat("Volume",&volume,0.f,1.f)||changed;changed=ImGui::Checkbox("Áudio espacial estéreo",&spatial)||changed;
            if(changed)voice->Configure(enabled,npcai::KokoroVoices[selected],speed,volume,spatial);
            ImGui::TextWrapped("Emoção da última fala: %s | intensidade: %.2f | relação: %s | confiança em CJ: %.0f/100",v.emotion.c_str(),v.intensity,v.relationship.c_str(),v.trust);
            ImGui::TextWrapped("Multiplicador agressividade: %.2f | limite de palavrões: %d (INI)",config.aggressionMultiplier,config.profanityLevel);
            ImGui::TextWrapped("Ajustes do menu valem nesta sessão; use o INI para persistir.");
            if(ImGui::Button(engine?"Testar voz (cache ou créditos ElevenLabs)":"Testar voz"))voice->Test();ImGui::SameLine();if(ImGui::Button("Interromper voz"))voice->Cancel();
            ImGui::TextWrapped("%s: %s | Modelo: %s | Idioma: %s",v.engine.c_str(),v.state.c_str(),v.model.c_str(),v.language.c_str());
            ImGui::TextWrapped("Fonemização: %s | ONNX: %s",v.phonemizer.c_str(),v.onnx.c_str());
            ImGui::TextWrapped("Síntese: %llu ms | Áudio: %llu ms | memória mínima: %.1f MiB",(unsigned long long)v.synthMs,(unsigned long long)v.audioMs,v.approxBytes/1048576.);
            ImGui::TextWrapped("Erro TTS: %s",v.error.empty()?"nenhum":v.error.c_str());
        }
        ImGui::Separator();
        ImGui::TextWrapped("Versão: %s | INI: %s",d.version.c_str(),d.iniPath.c_str());
        ImGui::TextWrapped("INI %s | Configuração %s",d.iniFound?"encontrado":"ausente (padrões)",d.configValid?"válida":"com erros recuperados");
        ImGui::TextWrapped("Transporte: %s | Gemini: %s",d.transport.c_str(),d.geminiEnabled?"ativado":"desativado");
        ImGui::TextWrapped("Modelo: %s | Credencial: %s",d.model.c_str(),d.credentialPresent?"presente":"ausente");
        ImGui::TextWrapped("HTTPS: %s | INTERNET: %s",d.httpsAvailable?"disponível":"indisponível",d.internet<0?"desconhecida":d.internet?"permitida":"negada");
        ImGui::TextWrapped("Último HTTP: %d | Erro: %s",d.lastHttp,d.lastError.empty()?"nenhum":d.lastError.c_str());
        ImGui::TextWrapped("GameBridge: %s | Seleção: %s",d.gameBridgeReady?"pronto":"indisponível",d.selection.c_str());
        ImGui::TextWrapped("Balões: %s",d.bubbles.c_str());
        for(const auto& error:d.errors)ImGui::TextWrapped("Config: %s",error.c_str());
        for(const auto& warning:d.warnings)ImGui::TextWrapped("Aviso: %s",warning.c_str());
    }ImGui::End();
}


void Append(const char* s) {
    size_t n=std::strlen(input), m=std::strlen(s);
    if(n+m<sizeof input) std::memcpy(input+n,s,m+1);
}
void Backspace() {
    size_t n=std::strlen(input);if(!n) return;
    do{--n;}while(n && (static_cast<unsigned char>(input[n])&0xC0)==0x80);
    input[n]=0;
}
void Row(const std::array<const char*,10>& keys,float height) {
    float width=(ImGui::GetContentRegionAvail().x-9*ImGui::GetStyle().ItemSpacing.x)/10;
    for(int i=0;i<10;++i){
        if(i) ImGui::SameLine();
        ImGui::PushID(i);
        if(ImGui::Button(keys[i],{width,height})) Append(keys[i]);
        ImGui::PopID();
    }
}
void Keyboard(float height) {
    const std::array<const char*,10> low1={"q","w","e","r","t","y","u","i","o","p"};
    const std::array<const char*,10> low2={"a","s","d","f","g","h","j","k","l","ç"};
    const std::array<const char*,10> low3={"z","x","c","v","b","n","m",",",".","?"};
    const std::array<const char*,10> up1={"Q","W","E","R","T","Y","U","I","O","P"};
    const std::array<const char*,10> up2={"A","S","D","F","G","H","J","K","L","Ç"};
    const std::array<const char*,10> up3={"Z","X","C","V","B","N","M","!",".","?"};
    const std::array<const char*,10> num={"1","2","3","4","5","6","7","8","9","0"};
    const std::array<const char*,10> signs1={"!","?",",",".",":",";","'","\"","(",")"};
    const std::array<const char*,10> signs2={"+","-","*","/","=","@","#","$","%","&"};
    const std::array<const char*,10> accents1={"á","à","â","ã","é","ê","í","ó","ô","õ"};
    const std::array<const char*,10> accents2={"Á","À","Â","Ã","É","Ê","Í","Ó","Ô","Õ"};
    const std::array<const char*,10> accents3={"ú","ü","ç","Ú","Ü","Ç","[","]","_","\\"};
    ImGui::PushID("keyboard");
    ImGui::PushID(1);Row(page==1?num:page==2?(shift?accents2:accents1):(shift?up1:low1),height);ImGui::PopID();
    ImGui::PushID(2);Row(page==1?signs1:page==2?accents3:(shift?up2:low2),height);ImGui::PopID();
    ImGui::PushID(3);Row(page==1?signs2:(shift?up3:low3),height);ImGui::PopID();
    float width=(ImGui::GetContentRegionAvail().x-4*ImGui::GetStyle().ItemSpacing.x)/8;
    if(ImGui::Button(shift?"abc":"ABC",{width,height})) shift=!shift;
    ImGui::SameLine();if(ImGui::Button(page==1?"Letras":"123",{width,height})) page=page==1?0:1;
    ImGui::SameLine();if(ImGui::Button(page==2?"Letras":"áéí",{width,height})) page=page==2?0:2;
    ImGui::SameLine();if(ImGui::Button("Espaço",{width*3,height})) Append(" ");
    ImGui::SameLine();if(ImGui::Button("Apagar",{width*2,height})) Backspace();
    ImGui::PopID();
}
}
namespace NpcUi {
void Init(npcai::Chat& instance,float gap,std::shared_ptr<npcai::Diagnostics> diag,std::shared_ptr<npcai::VoiceManager> v){voice=std::move(v);chat=&instance;bubbleGap=gap;diagnostics=std::move(diag);diagnosticOpen=false;}
void Draw() {
    if(!chat) return;
    auto s=chat->Get();
    static std::string editorTarget;
    const std::string current=s.target?s.target->id:"";
    if(current!=editorTarget || !npcai::GameBridge::IsOpen()) {npcai::NativeKeyboard::Invalidate();editorTarget=current;}
    npcai::NativeKeyboard::Heartbeat();
    std::string edited;
    if(npcai::NativeKeyboard::Take(edited)) {
        if(edited.empty()) npcai::GameBridge::Open(false);
        else if(chat->Send(edited)) {input[0]=0;npcai::GameBridge::Sent();}
        else {std::memcpy(input,edited.data(),edited.size());input[edited.size()]=0;}
    }
    std::string transcript;
    for(const auto& msg:s.messages) transcript+=(msg.fromPlayer?"CJ: ":"NPC: ")+msg.text+"\n\n";
    if(s.waiting) transcript+="NPC está pensando...\n";
    if(!s.error.empty()) transcript+="Erro: "+s.error+"\n";
    auto& io=ImGui::GetIO();
    const auto screen=io.DisplaySize;
    if(screen.x<240 || screen.y<200) {NpcImGuiCaptureAll(npcai::GameBridge::IsOpen());return;}
    float scale=std::clamp(std::min(screen.y/600.f,screen.x/800.f),0.5f,2.5f);
    auto& style=ImGui::GetStyle();
    style.FontSizeBase=21*scale;style.FramePadding={8*scale,5*scale};style.ItemSpacing={4*scale,4*scale};
    style.ScrollbarSize=24*scale;style.WindowRounding=10*scale;
    bool open=npcai::GameBridge::IsOpen();
    float diagnosticX=0,diagnosticY=0;const bool projected=s.target && npcai::GameBridge::Bubble(diagnosticX,diagnosticY,screen.x,screen.y);
    if(diagnostics)diagnostics->Live(npcai::GameBridge::Status(),s.target.has_value(),projected && diagnosticX>=0 && diagnosticX<screen.x && diagnosticY>=0 && diagnosticY<screen.y);
    if(diagnosticOpen){npcai::NativeKeyboard::Invalidate();DiagnosticPanel(scale,screen);NpcImGuiCaptureAll(diagnosticOpen || open);return;}

    if(diagnostics){
        ImGui::SetNextWindowPos({screen.x*.02f,screen.y-64*scale});
        ImGui::SetNextWindowSize({std::min(225*scale,screen.x*.38f),56*scale});
        if(ImGui::Begin("##NPCAIDiagnosticLauncher",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings)){
            if(ImGui::Button("NPC AI · Diagnóstico",{-1,-1})){diagnosticOpen=true;npcai::NativeKeyboard::Invalidate();}
        }ImGui::End();
    }
    NpcImGuiCaptureAll(open || diagnosticOpen);
    if(open && npcai::NativeKeyboard::Active()) return; // Native view owns input; keep GTA visible.
    if(!open) {
        float bx=0,by=0;
        if(s.target && npcai::GameBridge::Bubble(bx,by,screen.x,screen.y) && bx>=0 && bx<screen.x && by>=0 && by<screen.y) {
            std::string caption="Conversar...";
            for(auto it=s.messages.rbegin();it!=s.messages.rend();++it) if(!it->fromPlayer) {caption=it->text;break;}
            static std::string spokenBy,spokenText;
            static double spokenUntil=0;static uint64_t spokenSerial=0;
            if(current!=spokenBy || caption!=spokenText || spokenSerial!=s.replySerial) {
                spokenBy=current;spokenText=caption;spokenSerial=s.replySerial;spokenUntil=ImGui::GetTime()+s.bubbleDurationMs/1000.0;
            }
            if(ImGui::GetTime()>spokenUntil) caption="Conversar...";
            if(s.waiting) caption="NPC está pensando...";
            if(caption.size()>280) {
                size_t n=280;
                while(n && (static_cast<unsigned char>(caption[n])&0xC0)==0x80) --n;
                caption=caption.substr(0,n)+"...";
            }
            const float bw=std::min(350*scale,screen.x*.76f);
            const float bh=std::min(185*scale,std::max(55*scale,
                ImGui::CalcTextSize(caption.c_str(),nullptr,false,bw-24*scale).y+30*scale));
            auto origin=npcai::BubbleOrigin({bx,by},bw,bh,screen.x,screen.y,bubbleGap,7*scale);
            if(origin) {
            ImGui::SetNextWindowPos({origin->x,origin->y});
            ImGui::SetNextWindowSize({bw,bh});
            if(ImGui::Begin("##NPCBubble",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::TextWrapped("%s",caption.c_str());
                const auto pos=ImGui::GetWindowPos();
                ImGui::SetCursorScreenPos(pos);
                bool clicked=ImGui::InvisibleButton("##bubbleTouch",{bw,bh});
                static std::string pressedTarget;
                if(ImGui::IsItemActivated()) pressedTarget=current;
                if(clicked && pressedTarget==current) {
                    npcai::GameBridge::Open(true);
                    npcai::NativeKeyboard::Show(input,transcript);
                }
                ImGui::GetWindowDrawList()->AddTriangleFilled({pos.x+bw*.5f-7*scale,pos.y+bh-2*scale},
                    {pos.x+bw*.5f+7*scale,pos.y+bh-2*scale},{pos.x+bw*.5f,pos.y+bh+7*scale},IM_COL32(55,60,70,255));
            }
            ImGui::End();
            }
        }
        ImGui::SetNextWindowPos({screen.x*0.68f,screen.y-64*scale});
        ImGui::SetNextWindowSize({std::min(235*scale,screen.x*0.31f),56*scale});
        if(ImGui::Begin("##NPCAILauncher",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings)) {
            if(ImGui::Button(s.target?"Conversar · NPC AI":"NPC AI · buscar",{-1,-1})) npcai::GameBridge::Open(true);
        }
        ImGui::End();NpcImGuiCaptureAll(npcai::GameBridge::IsOpen());return;
    }
    // Fullscreen capture no backend protege também as margens da janela modal.
    ImGui::SetNextWindowPos({screen.x*.02f,screen.y*.02f});
    ImGui::SetNextWindowSize({screen.x*.96f,screen.y*.96f});
    if(ImGui::Begin("NPC AI · Conversa",nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|
                    ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings)) {
        if(s.target) {
            const auto marker=s.target->id.substr(s.target->id.find_last_of('.')+1);
            ImGui::TextWrapped("%s · %s · %.1f m",s.target->name.c_str(),marker.c_str(),s.target->distance);
        } else ImGui::TextUnformatted("NPC indisponível");
        if(diagnostics){const auto d=diagnostics->Get();if(!d.lastError.empty())ImGui::TextWrapped("Config/rede: %s",d.lastError.c_str());}
        float buttonH=36*scale;
        if(ImGui::Button("Fechar",{110*scale,buttonH})) npcai::GameBridge::Open(false);
        ImGui::SameLine();if(ImGui::Button("Trocar NPC",{145*scale,buttonH})) {npcai::GameBridge::Switch();input[0]=0;}
        ImGui::SameLine();if(ImGui::Button("Limpar conversa",{185*scale,buttonH})) {npcai::NativeKeyboard::Invalidate();chat->Clear();input[0]=0;}
        float keyH=std::max(24.f,37*scale);
        float historyH=std::max(40.f,ImGui::GetContentRegionAvail().y-(4*(keyH+style.ItemSpacing.y)+105*scale));
        const std::string id=s.target?s.target->id:"";
        bool changed=lastId!=id || lastMessages!=s.messages.size();
        if(ImGui::BeginChild("##history",{0,historyH},ImGuiChildFlags_Borders)) {
            bool atBottom=ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-24*scale;
            for(const auto& msg:s.messages) {
                ImGui::PushStyleColor(ImGuiCol_Text,msg.fromPlayer?ImVec4(.45f,.9f,.55f,1):ImVec4(.85f,.85f,1,1));
                ImGui::TextWrapped("%s: %s",msg.fromPlayer?"CJ":"NPC",msg.text.c_str());ImGui::PopStyleColor();
                ImGui::Spacing();
            }
            if(s.waiting) ImGui::TextUnformatted("NPC está pensando...");
            if(!s.error.empty()) ImGui::TextWrapped("Erro: %s",s.error.c_str());
            if(ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) ImGui::SetScrollY(ImGui::GetScrollY()-io.MouseDelta.y);
            if(changed && (atBottom || lastId!=id)) ImGui::SetScrollHereY(1);
        }
        ImGui::EndChild();lastId=id;lastMessages=s.messages.size();
        ImGui::SetNextItemWidth(-125*scale);
        if(ImGui::Button("Teclado Android",{220*scale,32*scale})) npcai::NativeKeyboard::Show(input,transcript);
        ImGui::InputText("##message",input,sizeof input,ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::BeginDisabled(s.waiting || !s.target);
        if(ImGui::Button("Enviar",{110*scale,0}) && chat->Send(input)) input[0]=0;
        ImGui::EndDisabled();
        ImGui::TextDisabled("%zu/1200 bytes · toque nas teclas · arraste o histórico",std::strlen(input));
        Keyboard(keyH);
    }
    ImGui::End();
    NpcImGuiCaptureAll(npcai::GameBridge::IsOpen());
}
void Shutdown(){npcai::NativeKeyboard::Invalidate();if(chat) npcai::GameBridge::Open(false);chat=nullptr;diagnostics.reset();voice.reset();diagnosticOpen=false;input[0]=0;NpcImGuiCaptureAll(false);}
}
