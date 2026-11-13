#include "actions.h"
#include "types.h"
#include "expressive.h"
#include "../../../third_party/json/json.hpp"
#include <map>
#include <cmath>
#include <set>
#include <stdexcept>
namespace npcai {
std::string NaturalDialogue(std::string text){
    const std::string marker="[Ação confirmada:";size_t pos=0;
    while((pos=text.find(marker))!=std::string::npos){auto end=text.find(']',pos);if(end==std::string::npos){text.erase(pos);break;}text.erase(pos,end-pos+1);}
    while(!text.empty() && (text.back()==' ' || text.back()=='\n' || text.back()=='\r'))text.pop_back();
    if(text.rfind("Não posso executar esse pedido nesta situação.",0)==0 || text.rfind("A tarefa foi interrompida ou não produziu efeito.",0)==0)return "Calma aí. Vamos conversar de boa.";
    return text.empty()?"Pode falar, parceiro.":text;
}
bool IsSpeechAction(Action a) {
    return a==Action::TalkGesture || a==Action::Angry || a==Action::Refuse;
}
const char* ActionName(Action a) {
    switch(a){case Action::ReleaseSurrender:return "release_surrender";case Action::None:return "none";case Action::Idle:return "idle";case Action::FacePlayer:return "face_player";
    case Action::TalkGesture:return "talk_gesture";case Action::HandsUp:return "hands_up";case Action::SurrenderMoney:return "surrender_money";
    case Action::Flee:return "flee";case Action::Angry:return "angry";case Action::Refuse:return "refuse";
    case Action::FollowPlayer:return "follow_player";case Action::StopFollowing:return "stop_following";case Action::Cower:return "cower";}return "none";
}
std::optional<Action> ActionFromName(const std::string& name){
    for(auto a:{Action::None,Action::Idle,Action::FacePlayer,Action::TalkGesture,Action::HandsUp,Action::SurrenderMoney,
                Action::Flee,Action::Angry,Action::Refuse,Action::FollowPlayer,Action::StopFollowing,Action::Cower,Action::ReleaseSurrender})
        if(name==ActionName(a))return a;
    return {};
}
Decision ParseDecision(const std::string& body,const std::string& expected) {
    Decision fallback;fallback.dialogue="Peraí, não entendi. Fala de novo?";
    const auto start=body.find_first_not_of(" \t\r\n");
    if(start==std::string::npos)return fallback;
    if(body[start]!='{' && body[start]!='['){fallback.dialogue=NaturalDialogue(body);return fallback;}
    try {
        using J=nlohmann::json;
        std::map<int,std::set<std::string>> keys;
        auto j=J::parse(body,[&](int depth,J::parse_event_t event,J& value){
            if(depth>8)throw std::runtime_error("depth");
            if(event==J::parse_event_t::object_start)keys[depth+1].clear();
            if(event==J::parse_event_t::key && !keys[depth].insert(value.get<std::string>()).second)throw std::runtime_error("duplicate");
            return true;
        });
        if(j.is_object() && j.contains("dialogue") && j["dialogue"].is_string()){
            auto text=j["dialogue"].get<std::string>();
            if(!text.empty() && text.size()<=4000 && ValidUtf8(text))fallback.dialogue=NaturalDialogue(text);
        }
        if(!j.is_object() || j.size()>8 || !j.contains("dialogue") || !j["dialogue"].is_string() ||
           !j.contains("action") || !j["action"].is_string() || !j.contains("emotion") || !j["emotion"].is_string() ||
           !j.contains("parameters") || !j["parameters"].is_object())return fallback;
        for(const auto& item:j.items())if(item.key()!="dialogue" && item.key()!="action" && item.key()!="emotion" && item.key()!="parameters" && item.key()!="request_id" && item.key()!="expectations" && item.key()!="emotion_intensity" && item.key()!="delivery")return fallback;
        auto a=ActionFromName(j["action"].get<std::string>());if(!a)return fallback;
        Decision d;d.action=*a;d.dialogue=NaturalDialogue(j["dialogue"].get<std::string>());d.emotion=j["emotion"].get<std::string>();
        if(d.dialogue.empty() || d.dialogue.size()>4000 || !ValidUtf8(d.dialogue) ||
           std::find(Emotions().begin(),Emotions().end(),d.emotion)==Emotions().end())return fallback;
        if(j.contains("delivery") && j["delivery"].is_string()){auto v=j["delivery"].get<std::string>();if(std::set<std::string>{"normal","whisper","giggle","sarcastic","tense"}.count(v))d.delivery=v;}
        if(j.contains("emotion_intensity") && j["emotion_intensity"].is_number()){auto f=j["emotion_intensity"].get<float>();if(std::isfinite(f))d.emotionIntensity=std::clamp(f,0.f,1.f);}
        if(j.contains("request_id")){if(!j["request_id"].is_string())return fallback;d.requestId=j["request_id"].get<std::string>();}
        if(d.action!=Action::None && (expected.empty() || d.requestId!=expected || d.requestId.size()>160))return fallback;
        const auto& p=j["parameters"];if(p.size()>2)return fallback;
        for(const auto& item:p.items()){
            if(!item.value().is_number_integer() || item.value().is_boolean())return fallback;
            auto n=item.value().get<int64_t>();
            if(item.key()=="amount"){
                if((d.action==Action::SurrenderMoney && (n<1 || n>10000)) || (d.action!=Action::SurrenderMoney && n!=0))return fallback;
                d.amount=static_cast<int>(n);
            }else if(item.key()=="duration_ms"){
                if(n<100 || n>10000)return fallback;
                d.durationMs=static_cast<int>(n);
            }else return fallback;
        }
        if(d.action==Action::SurrenderMoney && d.amount==0)return fallback;
        if(j.contains("expectations") && j["expectations"].is_array() && d.requestId==expected && !expected.empty()){
            for(const auto& e:j["expectations"]){
                if(d.expectations.size()>=3)break;
                if(!e.is_object() || e.size()!=6 || !e.contains("event") || !e["event"].is_string() || !e.contains("count") || !e["count"].is_number_integer() || !e.contains("timeout_s") || !e["timeout_s"].is_number_integer() || !e.contains("radius") || !e["radius"].is_number() || !e.contains("min_confidence") || !e["min_confidence"].is_number() || !e.contains("require_visible") || !e["require_visible"].is_boolean())continue;
                ExpectationSpec spec;
                if(!ParseWorldKind(e["event"].get<std::string>(),spec.event) || !Monitorable(spec.event))continue;
                spec.count=e["count"].get<int>();spec.timeoutSeconds=e["timeout_s"].get<int>();spec.radius=e["radius"].get<float>();spec.minConfidence=e["min_confidence"].get<float>();spec.requireVisible=e["require_visible"].get<bool>();
                if(spec.count<1 || spec.count>10 || spec.timeoutSeconds<15 || spec.timeoutSeconds>180 || !std::isfinite(spec.radius) || spec.radius<1 || spec.radius>40 || !std::isfinite(spec.minConfidence) || spec.minConfidence<.35f || spec.minConfidence>1)continue;
                if(spec.count>1 && spec.event!=WorldKind::PedAppeared && spec.event!=WorldKind::VehicleAppeared)continue;
                d.expectations.push_back(spec);
            }
        }
        return d;
    }catch(...){return fallback;}
}
}
