#include "settings.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <locale>
#include <sys/stat.h>

namespace npcai {
namespace {
float IniFloat(const std::string& s,size_t* n){std::istringstream in(s);in.imbue(std::locale::classic());float v;in>>std::noskipws>>v;if(!in || !in.eof())throw std::runtime_error("float");*n=s.size();return v;}
std::string Trim(const std::string& text){auto a=text.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};return text.substr(a,text.find_last_not_of(" \t\r\n")-a+1);}
bool Exists(const std::string& path){struct stat st{};return !path.empty() && stat(path.c_str(),&st)==0;}
bool ProviderKey(const std::string& key){return key=="gemini_enabled" || key=="gemini_model" || key=="gemini_api_key" || key=="gemini_key_storage" || key=="gemini_key_action" || key=="gemini_timeout_ms" || key=="gemini_max_retries" || key=="npc_ai_transport";}
std::string Join(const std::string& base){if(base.empty())return {};auto end=base.find_last_not_of('/');if(end==std::string::npos)return {};return base.substr(0,end+1)+"/npcai.ini";}
}
std::string SelectedTransport(const Settings& s){return s.legacyHttp?"legacy_loopback":s.geminiEnabled?"android_https_gemini":"native_mock";}
Settings LoadSettings(const std::string& path) {
    Settings s; ConfigurationReport report;report.path=path;report.found=Exists(path);
    bool surrenderExplicit=false,followExplicit=false;std::map<std::string,size_t> seen;std::map<std::string,std::string> voiceValues;std::set<std::string> providerInvalid;
    auto issue=[&](const std::string& text){report.valid=false;if(report.errors.size()<32)report.errors.push_back(text);};
    auto warning=[&](const std::string& text){if(report.warnings.size()<32)report.warnings.push_back(text);};
    std::ifstream f(path,std::ios::binary);
    if(!f){if(report.found){issue("INI inacessível; verifique permissão de leitura");s.geminiEnabled=true;s.providerConfigError="INI inacessível";}s.configuration=std::move(report);return s;}
    auto apply=[&](const std::string& key,const std::string& v)->bool {
        if(key=="elevenlabs_api_key"){
            auto value=v;if(value.size()>=2 && value.front()=='"' && value.back()=='"')value=value.substr(1,value.size()-2);
            if(value.size()>512 || value.find_first_of(" \t\r\n")!=std::string::npos)throw std::runtime_error("credential");
            s.elevenKey=value;
        }else if(key=="elevenlabs_model" || key=="elevenlabs_key_storage" || key=="elevenlabs_key_action" || key=="elevenlabs_voice_assignment"){
            if(key=="elevenlabs_model"){if(v!="eleven_v3" && v!="eleven_flash_v2_5")throw std::runtime_error("eleven_model");s.elevenModel=v;}
            else if(key=="elevenlabs_key_storage"){if(v!="ini" && v!="keystore")throw std::runtime_error("eleven_storage");s.elevenStorage=v;}
            else if(key=="elevenlabs_key_action"){if(v!="none" && v!="replace" && v!="remove")throw std::runtime_error("eleven_action");s.elevenKeyAction=v;}
            else {if(v!="character_id")throw std::runtime_error("eleven_assignment");s.elevenAssignment=v;}
        }else if(key=="elevenlabs_voice_ids" || key=="elevenlabs_model_voices" || key=="elevenlabs_character_voices"){
            if(v.size()>4096 || v.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-,=:")!=std::string::npos)throw std::runtime_error("voice_ids");
            if(key=="elevenlabs_voice_ids")s.elevenVoices=v;else if(key=="elevenlabs_model_voices")s.elevenModelVoices=v;else s.elevenCharacterVoices=v;
        }else if(key=="elevenlabs_voice_variation" || key=="elevenlabs_cache_enabled" || key=="elevenlabs_cache_persistent" || key=="npc_emotions_enabled" || key=="npc_relationship_recovery" || key=="npc_profanity_repetition_control" || key=="npc_personality_persistent"){
            if(v!="0" && v!="1")throw std::runtime_error("boolean");
            bool b=v=="1";
            if(key=="elevenlabs_voice_variation")s.elevenVariation=b;else if(key=="elevenlabs_cache_enabled")s.elevenCache=b;else if(key=="elevenlabs_cache_persistent")s.elevenCachePersistent=b;else if(key=="npc_emotions_enabled")s.emotionsEnabled=b;else if(key=="npc_relationship_recovery")s.relationshipRecovery=b;else if(key=="npc_profanity_repetition_control")s.profanityRepetition=b;else s.personalityPersistent=b;
        }else if(key=="elevenlabs_target_chars" || key=="elevenlabs_max_chars" || key=="elevenlabs_cache_max_mb" || key=="elevenlabs_timeout_ms" || key=="npc_profanity_level"){
            size_t n=0;int i=std::stoi(v,&n);int low=1,high=200;if(key=="elevenlabs_timeout_ms"){low=1000;high=60000;}else if(key=="npc_profanity_level"){low=0;high=3;}else if(key=="elevenlabs_cache_max_mb"){high=500;}
            if(n!=v.size() || i<low || i>high)throw std::runtime_error("range");
            if(key=="elevenlabs_target_chars")s.elevenTarget=i;else if(key=="elevenlabs_max_chars")s.elevenMax=i;else if(key=="elevenlabs_cache_max_mb")s.elevenCacheMb=i;else if(key=="npc_profanity_level")s.profanityLevel=i;else s.elevenTimeout=i;
        }else if(key=="elevenlabs_credit_per_char" || key=="elevenlabs_credit_limit_day" || key=="elevenlabs_credit_limit_hour" || key=="npc_emotion_intensity" || key=="npc_aggression_multiplier"){
            size_t n=0;float f=IniFloat(v,&n);float high=key=="npc_emotion_intensity"?1:key=="npc_aggression_multiplier"?2:key=="elevenlabs_credit_per_char"?10:1000000;
            if(n!=v.size() || !std::isfinite(f) || f<0 || f>high)throw std::runtime_error("range");
            if(key=="elevenlabs_credit_per_char")s.elevenRate=f;else if(key=="elevenlabs_credit_limit_day")s.elevenDay=f;else if(key=="elevenlabs_credit_limit_hour")s.elevenHour=f;else if(key=="npc_emotion_intensity")s.emotionIntensity=f;else s.aggressionMultiplier=f;
        }else if(key=="npc_tts_class_voices" || key=="npc_tts_fallback_default" || key=="npc_tts_models_resident" || (key.rfind("npc_tts_voice_",0)==0 && key!="npc_tts_voice_variation")){
            warning("Opção Piper descontinuada: "+key+"; use npc_tts_voice e npc_tts_voice_variation");
        }else if(key=="npc_tts_voice"){
            if(v!="pm_alex" && v!="pm_santa" && v!="pf_dora")throw std::runtime_error("tts_speaker");
            s.ttsVoice=v;
        }else if(key=="npc_tts_min_available_mb"){
            size_t n=0;int i=std::stoi(v,&n);if(n!=v.size() || i<0 || i>4096)throw std::runtime_error("tts_memory");s.ttsMinMemoryMb=i;
        }else if(key=="npc_tts_idle_unload_seconds"){
            size_t n=0;int i=std::stoi(v,&n);if(n!=v.size() || i<0 || i>3600)throw std::runtime_error("tts_idle");s.ttsIdleSeconds=i;
        }else if(key=="npc_tts_enabled" || key=="npc_tts_sync_animation" || key=="npc_tts_stop_on_distance" || key=="npc_tts_cache_enabled" || key=="npc_tts_spatial_audio" || key=="npc_tts_voice_variation"){
            if(v!="0" && v!="1")throw std::runtime_error("tts_boolean");
            bool b=v=="1";
            if(key=="npc_tts_enabled")s.ttsEnabled=b;else if(key=="npc_tts_sync_animation")s.ttsSync=b;else if(key=="npc_tts_stop_on_distance")s.ttsStopDistance=b;else if(key=="npc_tts_cache_enabled")s.ttsCache=b;else if(key=="npc_tts_spatial_audio")s.ttsSpatial=b;else s.ttsVariation=b;
        }else if(key=="npc_tts_volume" || key=="npc_tts_speed" || key=="npc_tts_max_distance"){
            size_t n=0;float f=IniFloat(v,&n);float low=key=="npc_tts_volume"?0:key=="npc_tts_speed"?.6f:3;float high=key=="npc_tts_volume"?1:key=="npc_tts_speed"?1.5f:40;
            if(n!=v.size() || !std::isfinite(f) || f<low || f>high)throw std::runtime_error("tts_range");
            if(key=="npc_tts_volume")s.ttsVolume=f;else if(key=="npc_tts_speed")s.ttsSpeed=f;else s.ttsMaxDistance=f;
        }else if(key=="npc_tts_timeout_ms"){
            size_t n=0;int i=std::stoi(v,&n);if(n!=v.size() || i<1000 || i>60000)throw std::runtime_error("tts_timeout");s.ttsTimeoutMs=i;
        }else if(key=="npc_tts_threads"){
            size_t n=0;int i=std::stoi(v,&n);if(n!=v.size() || i<1 || i>4)throw std::runtime_error("tts_threads");s.ttsThreads=i;
        }else if(key=="npc_tts_engine" || key=="npc_tts_language" || key=="npc_tts_model"){
            if(key=="npc_tts_engine"){if(v=="piper")warning("Motor Piper migrado para Kokoro; instale o novo pacote FP32");else if(v!="kokoro" && v!="elevenlabs")throw std::runtime_error("tts_engine");if(v=="elevenlabs" || v=="kokoro")s.ttsEngine=v;}
            else if(key=="npc_tts_language"){if(v!="pt-BR")throw std::runtime_error("tts_language");s.ttsLanguage=v;}
            else {if(v.empty() || v.size()>80 || v.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)throw std::runtime_error("tts_model");if(v.rfind("pt_BR-",0)==0){s.ttsModel="kokoro-pt-br";warning("Modelo Piper migrado para kokoro-pt-br; os arquivos ONNX antigos não são compatíveis");}else s.ttsModel=v;}
        }else if(key=="gemini_api_key"){
            auto credential=v;
            if(credential.size()>=2 && (credential.front()=='"' || credential.front()=='\'') && credential.back()==credential.front())credential=credential.substr(1,credential.size()-2);
            if(credential.size()>256 || std::any_of(credential.begin(),credential.end(),[](unsigned char c){return !((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.');}))throw std::runtime_error("Credencial Gemini inválida; conteúdo omitido");
            s.geminiApiKey=std::move(credential);
        } else if(key=="gemini_model" || key=="npc_memory_profile"){
            size_t limit=key=="gemini_model"?100:48;
            if(v.empty() || v.size()>limit || std::any_of(v.begin(),v.end(),[](unsigned char c){return !std::isalnum(c) && c!='_' && c!='-' && c!='.';}))throw std::runtime_error("Modelo/perfil inválido");
            if(key=="npc_memory_profile" && (v=="." || v==".."))throw std::runtime_error("Perfil inválido");
            if(key=="gemini_model")s.geminiModel=v;else s.memoryProfile=v;
        } else if(key=="gemini_key_storage" || key=="gemini_key_action" || key=="npc_reaction_mode" || key=="npc_ai_transport"){
            if(key=="gemini_key_storage"){if(v!="ini" && v!="keystore")throw std::runtime_error("Armazenamento de credencial inválido");s.credentialStorage=v;}
            else if(key=="gemini_key_action"){if(v!="none" && v!="replace" && v!="remove")throw std::runtime_error("Operação de credencial inválida");s.credentialAction=v;}
            else if(key=="npc_reaction_mode"){if(v!="economy" && v!="balanced" && v!="immersive")throw std::runtime_error("Modo de reação inválido");s.reactionMode=v;}
            else {if(v!="native" && v!="legacy_http")throw std::runtime_error("Transporte inválido");s.legacyHttp=v=="legacy_http";}
        } else if(key=="gemini_enabled" || key=="npc_memory_enabled" || key=="npc_memory_persistent" || key=="npc_memory_global" || key=="npc_memory_individual" || key=="npc_memory_send_context" || key=="npc_memory_clear" || key=="npc_perplexity_enabled" || key=="npc_perplexity_decay" || key=="world_perception" || key=="world_event_memory" || key=="world_expectations" || key=="npc_spontaneous_ai"){
            if(v!="0" && v!="1")throw std::runtime_error("Booleano de inteligência inválido");
            bool b=v=="1";
            if(key=="gemini_enabled")s.geminiEnabled=b;else if(key=="npc_memory_enabled")s.memoryEnabled=b;else if(key=="npc_memory_persistent")s.memoryPersistent=b;
            else if(key=="npc_memory_global")s.memoryGlobal=b;else if(key=="npc_memory_individual")s.memoryIndividual=b;else if(key=="npc_memory_send_context")s.memorySend=b;else if(key=="npc_memory_clear")s.memoryClear=b;
            else if(key=="npc_perplexity_enabled")s.perplexityEnabled=b;else if(key=="npc_perplexity_decay")s.perplexityDecay=b;else if(key=="world_perception")s.worldPerception=b;
            else if(key=="world_event_memory")s.worldEventMemory=b;else if(key=="world_expectations")s.worldExpectations=b;else s.spontaneousAI=b;
        } else if(key=="gemini_timeout_ms" || key=="gemini_max_retries" || key=="npc_memory_autosave_seconds" || key=="npc_memory_max_mb" || key=="world_perception_interval_ms" || key=="world_max_active_expectations" || key=="npc_context_tokens" || key=="npc_spontaneous_cooldown_s" || key=="npc_spontaneous_budget_hour"){
            size_t n=0;int value=std::stoi(v,&n);if(n!=v.size())throw std::runtime_error("Limite de inteligência inválido");
            int low=1,high=60;
            if(key=="gemini_timeout_ms"){low=1000;high=60000;}else if(key=="gemini_max_retries"){low=0;high=3;}
            else if(key=="world_perception_interval_ms"){low=250;high=5000;}else if(key=="world_max_active_expectations"){high=3;}
            else if(key=="npc_context_tokens"){low=256;high=4096;}else if(key=="npc_spontaneous_cooldown_s"){low=30;high=3600;}else if(key=="npc_spontaneous_budget_hour"){low=0;high=30;}
            else if(key=="npc_memory_max_mb"){high=32;}else if(key=="npc_memory_autosave_seconds"){low=2;high=300;}
            if(value<low || value>high)throw std::runtime_error("Limite de inteligência fora do intervalo");
            if(key=="gemini_timeout_ms")s.geminiTimeoutMs=value;else if(key=="gemini_max_retries")s.geminiRetries=value;else if(key=="npc_memory_autosave_seconds")s.memoryAutosaveSeconds=value;
            else if(key=="npc_memory_max_mb")s.memoryMaxMb=value;else if(key=="world_perception_interval_ms")s.worldIntervalMs=value;else if(key=="world_max_active_expectations")s.maxExpectations=value;
            else if(key=="npc_context_tokens")s.contextTokens=value;else if(key=="npc_spontaneous_cooldown_s")s.spontaneousCooldownS=value;else s.spontaneousBudgetHour=value;
        } else if(key=="npc_perplexity_gain"){
            size_t n=0;s.perplexityGain=IniFloat(v,&n);if(n!=v.size() || !std::isfinite(s.perplexityGain) || s.perplexityGain<.1f || s.perplexityGain>3.f)throw std::runtime_error("Ganho emocional inválido");
        } else if(key=="token") {
            if(v.size()>256 || std::any_of(v.begin(),v.end(),[](unsigned char c){return c<33 || c>126;}))
                throw std::runtime_error("token inválido no npcai.ini");
            s.http.token=v;
        } else if(key=="npc_hold_surrender" || key=="npc_personality_variation" || key=="npc_animation_debug" || key=="npc_bubble_dynamic_duration"){
            if(v!="0" && v!="1")throw std::runtime_error("Booleano de animação inválido");
            const bool b=v=="1";
            if(key=="npc_hold_surrender")s.holdSurrender=b;else if(key=="npc_personality_variation")s.personalityVariation=b;
            else if(key=="npc_animation_debug")s.animationDebug=b;else s.dynamicBubble=b;
        } else if(key=="npc_surrender_release_distance" || key=="npc_follow_release_distance"){
            size_t n=0;float f=IniFloat(v,&n);if(n!=v.size() || !std::isfinite(f) || f<4 || f>100)throw std::runtime_error("Distância persistente inválida");
            if(key=="npc_surrender_release_distance"){s.surrenderReleaseDistance=f;surrenderExplicit=true;}else{s.followReleaseDistance=f;followExplicit=true;}
        } else if(key=="npc_surrender_release_delay_ms" || key=="npc_dialogue_reading_wpm" || key=="npc_dialogue_min_ms" || key=="npc_dialogue_max_ms"){
            size_t n=0;int value=std::stoi(v,&n);if(n!=v.size())throw std::runtime_error("Tempo de animação inválido");
            int low=500,high=30000;
            if(key=="npc_dialogue_reading_wpm"){low=60;high=600;}
            if(key=="npc_surrender_release_delay_ms"){low=500;high=10000;}
            if(value<low || value>high)throw std::runtime_error("Tempo de animação fora do intervalo");
            if(key=="npc_dialogue_reading_wpm")s.readingWpm=value;else if(key=="npc_surrender_release_delay_ms")s.surrenderReleaseDelayMs=value;
            else if(key=="npc_dialogue_min_ms")s.dialogueMinMs=value;else s.dialogueMaxMs=value;
        } else if(key=="npc_face_player" || key=="npc_stop_on_talk" || key=="npc_ai_actions" || key=="npc_money_drop" || key=="npc_money_repeat") {
            if(v!="0" && v!="1")throw std::runtime_error("Booleano de ações inválido");
            bool b=v=="1";
            if(key=="npc_face_player")s.facePlayer=b;else if(key=="npc_stop_on_talk")s.stopOnTalk=b;
            else if(key=="npc_ai_actions")s.aiActions=b;else if(key=="npc_money_drop")s.moneyDrop=b;else s.moneyRepeat=b;
        } else if(key=="npc_money_max" || key=="npc_money_pickups_max" || key=="npc_action_timeout_ms" || key=="npc_money_session_max") {
            size_t n=0;int value=std::stoi(v,&n);if(n!=v.size())throw std::runtime_error("Limite de ação inválido");
            int low=0,high=10000;
            if(key=="npc_money_pickups_max"){low=1;high=5;}
            if(key=="npc_action_timeout_ms"){low=500;high=10000;}
            if(key=="npc_money_session_max")high=100000;
            if(value<low || value>high)throw std::runtime_error("Limite de ação fora do intervalo");
            if(key=="npc_money_max")s.moneyMax=value;else if(key=="npc_money_pickups_max")s.moneyPickupsMax=value;
            else if(key=="npc_action_timeout_ms")s.actionTimeoutMs=value;else s.moneySessionMax=value;
        } else if(key=="npc_stand_still" || key=="npc_dialogue_animations") {
            if(v!="0" && v!="1") throw std::runtime_error("Opção de NPC deve ser 0 ou 1");
            if(key=="npc_stand_still") s.npcStandStill=v=="1";else s.npcDialogueAnimations=v=="1";
        } else if(key=="bubble_height" || key=="bubble_screen_offset_y") {
            size_t n=0;float value=IniFloat(v,&n);
            if(n!=v.size() || !std::isfinite(value) || (key=="bubble_height"?(value<0.5f || value>3.5f):(value<0 || value>200)))
                throw std::runtime_error("Configuração de balão inválida");
            if(key=="bubble_height") s.bubbleHeight=value;else s.bubbleScreenOffsetY=value;
        } else if(key=="radius") {
            size_t n=0;float r=IniFloat(v,&n);
            if(n!=v.size() || !std::isfinite(r) || r<1 || r>10) throw std::runtime_error("radius deve ser 1..10");
            s.radius=r;
        } else if(key=="connect_timeout_ms" || key=="request_timeout_ms" || key=="history_limit") {
            size_t n=0;int i=std::stoi(v,&n);if(n!=v.size()) throw std::runtime_error("Número inválido no npcai.ini");
            if(key=="connect_timeout_ms") s.http.connectTimeoutMs=std::clamp(i,50,10000);
            else if(key=="request_timeout_ms") s.http.requestTimeoutMs=std::clamp(i,100,60000);
            else s.historyLimit=static_cast<size_t>(std::clamp(i,2,100));
        } else return false;
        return true;
    };
    size_t total=0,lineNumber=0;bool stop=false;
    while(!stop && f.peek()!=EOF){
        std::string line;bool longLine=false;char c=0;
        while(f.get(c)){
            if(++total>65536){issue("INI excede 64 KiB; restante ignorado");s.providerConfigError="INI excede limite de tamanho";s.geminiEnabled=true;stop=true;break;}
            if(c=='\n')break;
            if(line.size()<4096)line.push_back(c);else longLine=true;
        }
        ++lineNumber;if(stop)break;
        if(lineNumber==1 && line.compare(0,3,"\xef\xbb\xbf")==0)line.erase(0,3);
        if((lineNumber==1 && (line.compare(0,2,"\xff\xfe")==0 || line.compare(0,2,"\xfe\xff")==0)) || line.find('\0')!=std::string::npos){
            issue("Codificação do INI não suportada; salve como UTF-8");s.providerConfigError="INI deve usar codificação UTF-8";s.geminiEnabled=true;break;
        }
        line=Trim(line);if(line.empty() || line[0]=='#' || line[0]==';')continue;
        if(line[0]=='[' && line.back()==']')continue; // conventional section headers, flat legacy keys
        auto eq=line.find('=');
        if(eq==std::string::npos){issue("Linha "+std::to_string(lineNumber)+": esperado parâmetro=valor");s.providerConfigError="INI contém linha sem parâmetro=valor";s.geminiEnabled=true;continue;}
        auto key=Trim(line.substr(0,eq)),v=Trim(line.substr(eq+1));
        for(char& letter:key)if(letter>='A' && letter<='Z')letter=static_cast<char>(letter-'A'+'a');
        // Inline comments require whitespace. '#'/' ;' cannot be part of a Gemini key.
        for(size_t i=0;i<v.size();++i)if((v[i]=='#' || v[i]==';') && (i==0 || v[i-1]==' ' || v[i-1]=='\t')){v=Trim(v.substr(0,i));break;}
        const auto before=s;const bool oldSurrender=surrenderExplicit,oldFollow=followExplicit;
        bool known=false,conflictingVoice=false;
        try{
            if(longLine)throw std::runtime_error("long_line");
            if((key.rfind("npc_tts_",0)==0 || key.rfind("elevenlabs_",0)==0) && voiceValues.count(key) && voiceValues[key]!=v){conflictingVoice=true;throw std::runtime_error("tts_duplicate");}
            known=apply(key,v);
            if(!known){warning("Linha "+std::to_string(lineNumber)+": parâmetro desconhecido ignorado");continue;}
            ++report.accepted;if(key.rfind("npc_tts_",0)==0 || key.rfind("elevenlabs_",0)==0)voiceValues[key]=v;
            if(seen.count(key))warning("Linha "+std::to_string(lineNumber)+": "+key+" duplicado; última ocorrência válida utilizada");
            seen[key]=lineNumber;providerInvalid.erase(key);
        }catch(...){
            s=before;surrenderExplicit=oldSurrender;followExplicit=oldFollow;
            // Do not echo values or exception text. Only recognized parameter names are safe to report.
            static const std::set<std::string> safeKeys={"gemini_enabled","gemini_model","gemini_api_key","gemini_key_storage","gemini_key_action","gemini_timeout_ms","gemini_max_retries","npc_ai_transport"};
            // Existing validators only throw for recognized keys; overlong arbitrary lines are unnamed.
            const std::string label=longLine?(safeKeys.count(key)?key:"linha"):(key.size()<=64 && key.find_first_not_of("abcdefghijklmnopqrstuvwxyz_0123456789")==std::string::npos?key:"parâmetro");
            issue("Linha "+std::to_string(lineNumber)+": "+label+(conflictingVoice?" duplicado com valores conflitantes; remova o bloco antigo":longLine?" excede 4096 bytes":" inválido; valor omitido e padrão/valor anterior preservado"));
            if(key.rfind("npc_tts_",0)==0 || key.rfind("elevenlabs_",0)==0){s.ttsConfigError="Configuração de voz inválida: "+key+"; texto preservado";}
            if(ProviderKey(key)){providerInvalid.insert(key);if(key=="gemini_enabled" || key=="npc_ai_transport")s.geminiEnabled=true;}
            if(longLine && !ProviderKey(key)){s.providerConfigError="INI contém linha excessiva; revise a configuração";s.geminiEnabled=true;}
        }
    }
    if(f.bad()){issue("Falha ao ler INI");s.providerConfigError="Falha ao ler INI";s.geminiEnabled=true;}
    if(!surrenderExplicit)s.surrenderReleaseDistance=std::max(s.surrenderReleaseDistance,s.radius+3.f);
    if(!followExplicit)s.followReleaseDistance=std::max(s.followReleaseDistance,s.radius+3.f);
    if(s.dialogueMinMs>s.dialogueMaxMs){issue("npc_dialogue_min_ms excede npc_dialogue_max_ms; restaurados padrões seguros");s.dialogueMinMs=2500;s.dialogueMaxMs=12000;}
    if(s.surrenderReleaseDistance<=s.radius){issue("npc_surrender_release_distance deve superar radius; aplicado radius+3");s.surrenderReleaseDistance=s.radius+3.f;}
    if(s.followReleaseDistance<=s.radius){issue("npc_follow_release_distance deve superar radius; aplicado radius+3");s.followReleaseDistance=s.radius+3.f;}
    if(!providerInvalid.empty())s.providerConfigError="Configuração de IA inválida: "+*providerInvalid.begin()+"; revise o INI";
    if(s.elevenTarget>s.elevenMax){issue("elevenlabs_target_chars excede elevenlabs_max_chars");s.elevenTarget=s.elevenMax;}
    s.configuration=std::move(report);return s;
}
Settings LoadSettingsFromLoader(const std::string& dataPath,const std::string& configPath){
    std::vector<std::string> paths;for(const auto& dir:{dataPath,configPath}){auto path=Join(dir);if(!path.empty() && std::find(paths.begin(),paths.end(),path)==paths.end())paths.push_back(path);}
    std::string selected=paths.empty()?std::string{}:paths.front();for(const auto& path:paths)if(Exists(path)){selected=path;break;}
    auto s=LoadSettings(selected);s.configuration.candidates=std::move(paths);return s;
}
}
