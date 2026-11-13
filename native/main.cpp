#include <mod/amlmod.h>
#include <mod/logger.h>
#include <plugin-sdk/game_sa/Events.h>
#include <plugin-sdk/renderware/imgui/imgui_impl_renderware.h>
#include "src/npcai/chat.h"
#include "src/npcai/game_bridge.h"
#include "src/npcai/http_client.h"
#include "src/npcai/settings.h"
#include "src/npcai/ui.h"
#include "src/npcai/native_keyboard.h"
#include "src/npcai/android_services.h"
#include "src/npcai/intelligence_runtime.h"
#include "src/npcai/diagnostics.h"
#include "src/npcai/provider_startup.h"
#include <memory>
#include <algorithm>
#include <dlfcn.h>
#include "src/npcai/android_audio.h"
#include "src/npcai/android_eleven.h"
#include "src/npcai/eleven_backend.h"
#include "src/npcai/tts.h"

MYMOD(br.com.npcai.android, NPC_AI, 2.4.0, NPC_AI_Contributors)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4)
END_DEPLIST()

namespace {
std::unique_ptr<npcai::Chat> chat;
std::shared_ptr<npcai::IntelligenceRuntime> intelligence;
std::shared_ptr<npcai::Diagnostics> diagnostics;
std::shared_ptr<npcai::VoiceManager> voice;
struct AndroidVoiceAudio:npcai::VoiceAudio {bool Open(int rate,float volume)override{return npcai::AndroidAudio::Open(rate,volume);}int Write(const int16_t* data,size_t n)override{return npcai::AndroidAudio::Write(data,n);}void Interrupt()override{npcai::AndroidAudio::Close();}void Gains(float l,float r)override{npcai::AndroidAudio::Gains(l,r);}
 int64_t Head()override{return npcai::AndroidAudio::Head();}void Close()override{npcai::AndroidAudio::Close();}};
void Stop(){if(voice)voice->Shutdown();npcai::GameBridge::Stop();NpcUi::Shutdown();if(diagnostics)diagnostics->Shutdown();if(chat)chat->Shutdown();if(intelligence)intelligence->Shutdown();}
}
ON_MOD_LOAD() {
    logger->SetTag("NPC_AI");
    if(!aml)return;
    if(!aml->HasModOfVersion("net.rusjj.aml","1.4")){logger->Error("NPC AI 2.4.0: AML >=1.4 necessário");return;}
    logger->Info("NPC AI 2.4.0: etapa 1, ABI do AML aceita");
    std::string version;bool keyboardReady=false;
    try{keyboardReady=npcai::NativeKeyboard::Init(version);}catch(...){logger->Error("Editor JNI indisponível; conteúdo da exceção omitido");}
    if(version!="2.10"){logger->Error("Versão APK não confirmada como 2.10; integração da engine não instalada");return;}
    if(!keyboardReady)logger->Error("Editor Android indisponível; teclado touch permanece disponível");
    if(!GetMainLibrary() || aml->GetLibLength("libGTASA.so")<=0x852000){logger->Error("libGTASA.so ausente ou tamanho incompatível");return;}
    try {
        const char* data=aml->GetAndroidDataPath();const char* config=aml->GetConfigPath();
        auto settings=npcai::LoadSettingsFromLoader(data?data:"",config?config:"");
        logger->Info("Config path=%s found=%d valid=%d accepted=%zu",settings.configuration.path.c_str(),settings.configuration.found,settings.configuration.valid,settings.configuration.accepted);
        for(const auto& error:settings.configuration.errors)logger->Error("Config: %s",error.c_str());
        for(const auto& warning:settings.configuration.warnings)logger->Info("Config: %s",warning.c_str());
        // Provider initialization returns diagnostics; it is never a gate for the UI.
        const auto native=npcai::PrepareNativeProvider(settings,npcai::AndroidServices::Configure,npcai::AndroidServices::Status);
        const auto& privateDirectory=native.privateDirectory;const auto& serviceError=native.error;
        const bool serviceReady=native.available,credential=native.credential;const int internet=native.internet;
        if(!serviceError.empty())logger->Info("Native services state=%s",serviceError.c_str());
        std::string elevenDirectory;
        try{auto voiceSettings=settings;if(!voiceSettings.ttsConfigError.empty()){std::fill(voiceSettings.elevenKey.begin(),voiceSettings.elevenKey.end(),'\0');voiceSettings.elevenKey.clear();voiceSettings.elevenKeyAction="none";}auto voiceState=npcai::JsonValue::parse(npcai::AndroidEleven::Configure(voiceSettings));elevenDirectory=voiceState.value("private_directory",std::string{});std::fill(voiceSettings.elevenKey.begin(),voiceSettings.elevenKey.end(),'\0');}catch(...){logger->Error("ElevenLabs config indisponível; UI preservada");}
        std::fill(settings.elevenKey.begin(),settings.elevenKey.end(),'\0');settings.elevenKey.clear();
        std::fill(settings.geminiApiKey.begin(),settings.geminiApiKey.end(),'\0');settings.geminiApiKey.clear();
        if(privateDirectory.empty()){settings.memoryPersistent=false;logger->Error("Private storage unavailable; persistence disabled; interface preserved");}
        const auto slash=settings.configuration.path.find_last_of('/');
        const std::string exportDirectory=slash==std::string::npos?std::string{}:settings.configuration.path.substr(0,slash);
        diagnostics=std::make_shared<npcai::Diagnostics>(settings,npcai::AndroidServices::Https,exportDirectory);
        diagnostics->Services(serviceReady,internet,credential,serviceError);
        auto transport=[weak=std::weak_ptr<npcai::Diagnostics>(diagnostics)](const std::string& model,const std::string& body,int timeout,const std::atomic_bool& cancel){
            auto answer=npcai::AndroidServices::Https(model,body,timeout,cancel);if(auto diag=weak.lock())diag->ObserveHttps(answer);return answer;
        };
        try{
            intelligence=std::make_shared<npcai::IntelligenceRuntime>(settings,privateDirectory,transport,[](const std::string& metric){logger->Info("%s",metric.c_str());});
            npcai::AndroidServices::SetPauseObserver([weak=std::weak_ptr<npcai::IntelligenceRuntime>(intelligence)]{if(auto engine=weak.lock())engine->Flush();});
        }catch(...){logger->Error("Intelligence initialization failed; GameBridge/ImGui will still initialize");}
        chat=std::make_unique<npcai::Chat>([config=settings,engine=intelligence](const npcai::Request& r,const std::atomic_bool& c)->npcai::Result{
            if(!config.providerConfigError.empty() && (config.geminiEnabled || config.legacyHttp))return {false,config.providerConfigError};
            if(config.legacyHttp)return npcai::HttpChat(r,config.http,c);
            if(engine)return engine->Chat(r,c);
            return {false,"Sistema de IA indisponível; interface e controles locais continuam ativos."};
        },settings.historyLimit);
        if(intelligence)chat->SetDecisionObserver([engine=intelligence](const npcai::Request& r,const npcai::Decision& d){engine->Accepted(r,d);});
        try{
            std::string dataDirectory=data?data:exportDirectory;
            while(dataDirectory.size()>1 && dataDirectory.back()=='/')dataDirectory.pop_back();
            auto split=dataDirectory.find_last_of('/');std::string libraries=split==std::string::npos?std::string{}:dataDirectory.substr(0,split)+"/mods/npcai-tts/lib";
            const char* internal=aml->GetInternalModsPath();const char* privatePath=aml->GetDataPath();
            std::string alternate=internal?std::string(internal)+"/npcai-tts/lib":std::string{};
            std::string privateLibraries=privatePath?std::string(privatePath)+"/npcai-tts-lib":std::string{};
            std::string voicePrivate=!elevenDirectory.empty()?elevenDirectory+"/speech":privatePath && *privatePath?std::string(privatePath)+"/npcai-elevenlabs":!privateDirectory.empty()?privateDirectory+"/elevenlabs":"";
            auto factory=[settings,voicePrivate,dataDirectory,libraries,privateLibraries,alternate](const std::string& engine)->std::unique_ptr<npcai::VoiceBackend>{
                if(engine=="elevenlabs")return npcai::MakeElevenBackend(settings,voicePrivate,npcai::AndroidEleven::Https,npcai::AndroidEleven::Control);
                return npcai::MakeKokoroBackend(settings,dataDirectory+"/npcai-tts",libraries,npcai::AndroidAudio::Hash,privateLibraries,alternate);
            };
            voice=std::make_shared<npcai::VoiceManager>(settings,factory(settings.ttsEngine),std::make_unique<AndroidVoiceAudio>());voice->SetFactory(factory);voice->Control("status");
            npcai::GameBridge::AttachVoice(voice);
        }catch(...){logger->Error("Kokoro worker unavailable; chat textual preservado");}
        npcai::AndroidServices::SetPauseObserver([weak=std::weak_ptr<npcai::IntelligenceRuntime>(intelligence),tts=std::weak_ptr<npcai::VoiceManager>(voice)]{if(auto engine=weak.lock())engine->Flush();if(auto v=tts.lock())v->Cancel();});
        npcai::GameBridge::AttachRuntime(intelligence);
        logger->Info("NPC AI 2.4.0: etapa 2, iniciando GameBridge independentemente do provedor");
        const bool bridgeReady=npcai::GameBridge::Init(*chat,settings);diagnostics->Bridge(bridgeReady);
        logger->Info("NPC AI 2.4.0: etapa 3, iniciando ImGui/RW");
        NpcUi::Init(*chat,settings.bubbleScreenOffsetY,diagnostics,voice);
        if(!ImGui_ImplRenderWare_Init(NpcUi::Draw)){logger->Error("RenderWare/ImGui indisponível");Stop();return;}
        Events::shutdownRwEvent.before += Stop;
        logger->Info("NPC AI loaded transport=%s gemini_enabled=%d https=%d credential=%d internet=%d memory_private=%d bridge=%d",npcai::SelectedTransport(settings).c_str(),settings.geminiEnabled,serviceReady,credential,internet,settings.memoryPersistent,bridgeReady);
    }catch(...){logger->Error("Falha de inicialização local; conteúdo da exceção omitido");Stop();}
}
ON_MOD_UNLOAD() {Stop();}
