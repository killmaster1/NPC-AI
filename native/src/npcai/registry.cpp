#include "registry.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace npcai {
std::string Identify(const Context& c) {
    std::string name="Pedestre";
    if(c.pedType==6) name="Policial";
    else if(c.model>=105 && c.model<=107) name="Families (modelo original)";
    else if(c.model>=102 && c.model<=104) name="Ballas (modelo original)";
    else if(c.observedTaxiDriver) name="Motorista de táxi observado";
    return name+" · modelo "+std::to_string(c.model);
}
void Registry::Begin(){for(auto& p:entries_) p.second.seen=false;}
void Registry::Observe(const Observation& o) {
    if(o.slot<0 || o.ref<0 || o.player || !o.alive || !std::isfinite(o.position.x) ||
       !std::isfinite(o.position.y) || !std::isfinite(o.position.z)) {Destroy(o.slot);return;}
    auto it=entries_.find(o.slot);
    if(it==entries_.end() || it->second.value.ref!=o.ref || it->second.value.context.model!=o.context.model) {
        Entry e{o,"s"+session_+".p"+std::to_string(++serial_),true};
        e.value.context.personality=o.context.personalityVariation?MakePersonality(e.id,o.context.model,o.context.pedType):Personality{};e.value.context.hasPersonality=true;
        entries_[o.slot]=std::move(e);
    } else {auto personality=it->second.value.context.personality;const bool taxi=it->second.value.context.observedTaxiDriver;auto state=it->second.value.context.interactionState;
        it->second.value=o;it->second.value.context.observedTaxiDriver=taxi || o.context.observedTaxiDriver;it->second.value.context.personality=personality;it->second.value.context.hasPersonality=true;it->second.value.context.interactionState=state;it->second.seen=true;}
}
void Registry::End(){for(auto it=entries_.begin();it!=entries_.end();) {
    if(!it->second.seen) it=entries_.erase(it); else ++it;
}}
void Registry::Destroy(int slot){entries_.erase(slot);}
bool Registry::Contains(const std::string& id,int ref) const{for(const auto& e:entries_)if(e.second.id==id && e.second.value.ref==ref)return true;return false;}
std::string Registry::IdAt(int slot) const{auto it=entries_.find(slot);return it==entries_.end()?"":it->second.id;}
void Registry::Behaviors(const std::map<std::string,std::string>& states){for(auto& p:entries_){auto it=states.find(p.second.id);p.second.value.context.interactionState=it==states.end()?"normal":it->second;}}
void Registry::ActionConfirmed(const std::string& id,Action action){
    for(auto& p:entries_)if(p.second.id==id){auto& traits=p.second.value.context.personality;
        if(action==Action::HandsUp || action==Action::SurrenderMoney){traits.fear=std::min(100,traits.fear+25);traits.trust=std::max(0,traits.trust-15);}
        else if(action==Action::FollowPlayer)traits.trust=std::min(100,traits.trust+5);
        else if(action==Action::ReleaseSurrender)traits.fear=std::max(0,traits.fear-10);
    }
}
void Registry::Reset(){entries_.clear();} // serial nunca volta a zero, inclusive após load game.
std::optional<Target> Registry::Resolve(const std::string& id,Vec3 p,int area,float radius) const {
    if(!std::isfinite(radius) || radius<0 || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return {};
    for(const auto& pair:entries_) {
        const auto& e=pair.second;
        if(e.id!=id || e.value.context.area!=area) continue;
        const auto& v=e.value.position;
        float d=std::sqrt((v.x-p.x)*(v.x-p.x)+(v.y-p.y)*(v.y-p.y)+(v.z-p.z)*(v.z-p.z));
        if(!std::isfinite(d) || d>radius) return {};
        return Target{e.id,Identify(e.value.context),e.value.ref,d,e.value.context};
    }
    return {};
}
std::optional<Target> Registry::Nearest(Vec3 p,int area,float radius,const std::string& exclude) const {
    std::optional<Target> best;
    if(!std::isfinite(radius) || radius<0 || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return {};
    for(const auto& pair:entries_) {
        const auto& e=pair.second;
        if(e.id==exclude || e.value.context.area!=area) continue;
        const auto& v=e.value.position;
        float d=std::sqrt((v.x-p.x)*(v.x-p.x)+(v.y-p.y)*(v.y-p.y)+(v.z-p.z)*(v.z-p.z));
        if(std::isfinite(d) && d<=radius && (!best || d<best->distance))
            best=Target{e.id,Identify(e.value.context),e.value.ref,d,e.value.context};
    }
    return best;
}
std::optional<Vec3> Registry::Position(const std::string& id) const {
    for(const auto& pair:entries_) if(pair.second.id==id) return pair.second.value.position;
    return {};
}
std::vector<Target> Registry::Nearby(Vec3 p,int area,float radius,size_t limit)const{
    std::vector<Target> out;out.reserve(std::min(limit,entries_.size()));
    for(const auto& pair:entries_){const auto& e=pair.second;if(e.value.context.area!=area)continue;const auto& v=e.value.position;float d=std::sqrt((v.x-p.x)*(v.x-p.x)+(v.y-p.y)*(v.y-p.y)+(v.z-p.z)*(v.z-p.z));if(std::isfinite(d) && d<=radius)out.push_back({e.id,Identify(e.value.context),e.value.ref,d,e.value.context});if(out.size()>=limit)break;}return out;
}
std::vector<std::string> Registry::Ids() const {
    std::vector<std::string> result;for(const auto& p:entries_) result.push_back(p.second.id);return result;
}
}
