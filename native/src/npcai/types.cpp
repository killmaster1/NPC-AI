#include "types.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <unistd.h>

namespace npcai {
bool ValidUtf8(const std::string& s) {
    size_t i=0;
    while(i<s.size()) {
        auto c=static_cast<unsigned char>(s[i++]);
        if(c<0x80) { if(c==0) return false; continue; }
        unsigned value=0, count=0, minimum=0;
        if(c>=0xC2 && c<=0xDF) { value=c&31; count=1; minimum=0x80; }
        else if(c>=0xE0 && c<=0xEF) { value=c&15; count=2; minimum=0x800; }
        else if(c>=0xF0 && c<=0xF4) { value=c&7; count=3; minimum=0x10000; }
        else return false;
        if(i+count>s.size()) return false;
        while(count--) {
            auto b=static_cast<unsigned char>(s[i++]);
            if((b&0xC0)!=0x80) return false;
            value=(value<<6)|(b&63);
        }
        if(value<minimum || value>0x10FFFF || (value>=0xD800 && value<=0xDFFF)) return false;
    }
    return true;
}
static std::string Escape(const std::string& s) {
    std::string out="\"";
    for(unsigned char c:s) {
        if(c=='"' || c=='\\') { out+='\\'; out+=static_cast<char>(c); }
        else if(c<32) { char b[7]; std::snprintf(b,sizeof b,"\\u%04x",c); out+=b; }
        else out+=static_cast<char>(c);
    }
    return out+'"';
}
std::string Json(const Request& r) {
    const auto& c=r.context;
    std::string s="{\"npc_id\":"+Escape(r.npcId)+",\"conversation_id\":"+Escape(r.conversationId)+
        ",\"message\":"+Escape(r.message)+",\"context\":{\"model\":"+std::to_string(c.model)+
        ",\"ped_type\":"+std::to_string(c.pedType)+",\"ped_state\":"+std::to_string(c.pedState)+
        ",\"area\":"+std::to_string(c.area)+",\"in_vehicle\":"+(c.inVehicle?"true":"false");
    s+=",\"observed_taxi_driver\":"+std::string(c.observedTaxiDriver?"true":"false");
    if(c.hour>=0) s+=",\"hour\":"+std::to_string(c.hour);
    if(c.minute>=0) s+=",\"minute\":"+std::to_string(c.minute);
    if(c.regionLevel>=0) s+=",\"region_level\":"+std::to_string(c.regionLevel);
    s+=",\"personality_variation\":"+std::string(c.personalityVariation?"true":"false");
    if(c.hasPersonality && c.personalityVariation){const auto& p=c.personality;
        s+=",\"personality\":{\"courage\":"+std::to_string(p.courage)+",\"aggression\":"+std::to_string(p.aggression)+",\"kindness\":"+std::to_string(p.kindness)+",\"patience\":"+std::to_string(p.patience)+",\"fear\":"+std::to_string(p.fear)+",\"trust\":"+std::to_string(p.trust)+"}";}
    s+=",\"interaction_state\":"+Escape(c.interactionState);
    s+="}";
    s+=",\"client_timeout_ms\":"+std::to_string(r.timeoutMs);
    if(!r.requestId.empty()) s+=",\"protocol\":2,\"request_id\":"+Escape(r.requestId);
    if(r.feedback){const auto& f=*r.feedback;s+=",\"action_result\":{\"request_id\":"+Escape(f.requestId)+",\"action\":"+Escape(f.action)+",\"status\":"+Escape(f.status)+",\"amount\":"+std::to_string(f.amount)+"}";}
    return s+"}";
}
std::string SessionId() {
    unsigned char bytes[16]{};
    std::ifstream random("/dev/urandom",std::ios::binary);
    if(!random.read(reinterpret_cast<char*>(bytes),sizeof bytes)) {
        // A falha de entropia desativa a ponte; nunca reutilizar uma identidade previsível.
        return {};
    }
    char hex[33];
    for(size_t i=0;i<16;++i) std::snprintf(hex+2*i,3,"%02x",bytes[i]);
    return hex;
}
}
