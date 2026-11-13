#include "world_core.h"
#include <algorithm>
#include <cmath>
namespace npcai {
float WorldDistance(WorldPoint a,WorldPoint b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
std::vector<WorldEvent> WorldEventDetector::Update(const WorldFrame& f){
    std::vector<WorldEvent> out;
    if(!previous_){previous_=f;return out;}
    const auto& p=*previous_;
    auto emit=[&](WorldKind kind,WorldPoint pos,float confidence,float intensity,const std::string& subject="",const std::string& source="snapshot_change",bool audible=false){
        WorldEvent e;e.id="event-"+std::to_string(++serial_);e.kind=kind;e.position=pos;e.confidence=confidence;e.intensity=intensity;e.at=f.at;e.subject=subject;e.source=source;e.audible=audible;out.push_back(std::move(e));
    };
    int delta=f.minuteOfDay-p.minuteOfDay;
    if(delta<-720 && p.minuteOfDay>=1430 && f.minuteOfDay<=10)delta+=1440;
    if(std::abs(delta)>=30 && f.at>=p.at && f.at-p.at<=5000){emit(WorldKind::TimeJump,f.player,.95f,.95f);if(delta<0 && delta>=-720)emit(WorldKind::TimeReversed,f.player,.95f,.8f);}
    auto night=[](int minute){return minute>=1200 || minute<360;};
    if(night(f.minuteOfDay) && !night(p.minuteOfDay))emit(WorldKind::Night,f.player,.9f,std::abs(delta)>=30?.8f:.15f);
    if(f.weatherAvailable && p.weatherAvailable){if(f.rain>.15f && p.rain<=.15f)emit(WorldKind::RainStarted,f.player,.95f,.4f);if(f.rain<=.05f && p.rain>.05f)emit(WorldKind::RainStopped,f.player,.9f,.15f);}
    if(p.weapon>0 && f.weapon==0)emit(WorldKind::WeaponHolstered,f.player,.95f,.1f,"player");
    if(!f.inVehicle && !p.inVehicle && f.area==p.area && f.at-p.at<=3000 && WorldDistance(f.player,p.player)>50)emit(WorldKind::PlayerDiscontinuity,f.player,.7f,.8f,"player","position_discontinuity_not_proof_of_teleport");
    std::map<std::string,WorldEntity> old,current;
    for(const auto& e:p.entities)old[e.id]=e;
    for(const auto& e:f.entities){
        current[e.id]=e;
        auto found=old.find(e.id);
        if(found==old.end())emit(e.vehicle?WorldKind::VehicleAppeared:WorldKind::PedAppeared,e.position,e.ctorSeen?.85f:.55f,.35f,e.id,e.ctorSeen?"native_ctor_then_observed_not_proof_of_cause":"entered_sample_or_streamed");
        else{
            if(found->second.alive && !e.alive)emit(WorldKind::Death,e.position,.9f,.65f,e.id);
            if(!found->second.combat && e.combat)emit(WorldKind::Combat,e.position,.8f,.5f,e.id,"ped_combat_state");
        }
    }
    for(const auto& pair:old)if(!current.count(pair.first))emit(pair.second.vehicle?WorldKind::VehicleGone:WorldKind::PedGone,pair.second.position,.5f,.1f,pair.first,"left_sample_or_streamed_no_death_inference");
    for(const auto& observer:f.observers){
        auto prior=std::find_if(p.observers.begin(),p.observers.end(),[&](const auto& o){return o.id==observer.id;});
        if(prior!=p.observers.end() && prior->playerVisible && observer.losAvailable && !observer.playerVisible && WorldDistance(f.player,observer.position)>6){
            emit(WorldKind::PlayerLeft,observer.position,.8f,.2f,"player","lost_visual_contact_not_location_knowledge");out.back().observer=observer.id;out.back().magnitude=WorldDistance(f.player,observer.position);
        }
    }
    for(auto e:f.nativeEvents){e.id="event-"+std::to_string(++serial_);if(!e.at)e.at=f.at;out.push_back(std::move(e));}
    if(out.size()>64)out.resize(64);
    previous_=f;return out;
}
std::optional<PerceivedEvent> NpcPerception::Assess(const WorldEvent& e,const WorldObserverSample& o){
    if(o.at>e.at && o.at-e.at>5000)return {};
    if(!e.observer.empty() && e.observer!=o.id)return {};
    PerceivedEvent p;p.event=e;
    const bool ambient=e.kind==WorldKind::TimeJump || e.kind==WorldKind::TimeReversed || e.kind==WorldKind::Night || e.kind==WorldKind::RainStarted || e.kind==WorldKind::RainStopped;
    if(ambient){if(o.area!=0)return {};p.confidence=e.confidence*.85f;p.channel="ambient_inference_no_direct_visual_claim";return p;}
    float distance=WorldDistance(o.position,e.position);p.distance=distance;if(distance>60)return {};
    if(e.kind==WorldKind::PlayerLeft){p.channel="lost_visual_contact";p.confidence=e.confidence;p.distance=e.magnitude;return p;}
    const bool visible=std::find(o.visible.begin(),o.visible.end(),e.subject)!=o.visible.end() || (e.subject=="player" && o.playerVisible);
    if(visible){p.channel="line_of_sight_and_fov";p.confidence=e.confidence;p.event.visual=true;}
    else if(e.audible && distance<=45){p.channel="auditory_proximity_inference";p.confidence=e.confidence*.8f;}
    else if(distance<=15){p.channel="nearby_detector_visibility_unverified";p.confidence=std::min(.35f,e.confidence*.5f);}
    else return {};
    return p;
}
bool ExpectationManager::Register(const std::string& nonce,const std::vector<ExpectationSpec>& specs,uint64_t now,int maxActive,const WorldFrame& frame){
    if(nonce.empty() || registered_.count(nonce) || registered_.size()>=512)return false;
    registered_.insert(nonce);bool accepted=false;
    for(size_t i=0;i<specs.size() && active_.size()<static_cast<size_t>(std::clamp(maxActive,1,3));++i){
        const auto& s=specs[i];if(!Monitorable(s.event) || s.count<1 || s.count>10 || s.timeoutSeconds<15 || s.timeoutSeconds>180 || !std::isfinite(s.radius) || s.radius<1 || s.radius>40 || !std::isfinite(s.minConfidence) || s.minConfidence<.35f || s.minConfidence>1)continue;
        if((s.event==WorldKind::Night && (frame.minuteOfDay>=1200 || frame.minuteOfDay<360)) || (s.event==WorldKind::RainStarted && (!frame.weatherAvailable || frame.rain>.15f)) || (s.event==WorldKind::VehicleAppeared && !frame.vehiclesAvailable) || (s.event==WorldKind::WeaponHolstered && frame.weapon<=0))continue;
        if(s.count>1 && s.event!=WorldKind::PedAppeared && s.event!=WorldKind::VehicleAppeared)continue;
        ActiveExpectation x;x.id=nonce+".x"+std::to_string(i);x.spec=s;x.created=now;x.deadline=now+static_cast<uint64_t>(s.timeoutSeconds)*1000;active_.push_back(std::move(x));accepted=true;
    }return accepted;
}
std::vector<ExpectationMatch> ExpectationManager::Observe(const PerceivedEvent& p){
    std::vector<ExpectationMatch> matches;
    for(auto it=active_.begin();it!=active_.end();){
        auto& x=*it;
        bool same=x.spec.event==p.event.kind && p.event.at>=x.created && p.event.at<=x.deadline;
        if(p.event.kind==WorldKind::PlayerLeft)same=same && p.distance>=x.spec.radius;else same=same && p.distance<=x.spec.radius;
        if(same && p.confidence>=x.spec.minConfidence && (!x.spec.requireVisible || p.event.visual)){
            const bool entity=p.event.kind==WorldKind::PedAppeared || p.event.kind==WorldKind::VehicleAppeared;
            x.evidence.insert(entity?p.event.subject:p.event.id);
            if(x.evidence.size()>=static_cast<size_t>(x.spec.count)){matches.push_back({x,p});it=active_.erase(it);continue;}
        }++it;
    }return matches;
}
size_t ExpectationManager::Expire(uint64_t now){const auto before=active_.size();active_.erase(std::remove_if(active_.begin(),active_.end(),[&](const auto& x){return now>x.deadline;}),active_.end());return before-active_.size();}
NpcPerplexitySystem::NpcPerplexitySystem(const Personality& p){state.fear=p.fear;state.trust=p.trust;state.aggression=p.aggression;state.courage=p.courage;state.curiosity=std::clamp(70.f-p.patience*.3f,15.f,85.f);state.skepticism=std::clamp(45.f+p.courage*.3f,25.f,85.f);}
bool NpcPerplexitySystem::Observe(const PerceivedEvent& p,bool fulfilled,float gain){
    if(seen_.count(p.event.id))return false;
    if(seen_.size()>=256)seen_.erase(seen_.begin());
    seen_.insert(p.event.id);
    auto& times=habituation_[p.event.kind];float habituation=1.f/(1.f+.5f*std::min(times,20));++times;
    float impact=std::clamp(p.event.intensity,0.f,1.f)*std::clamp(p.confidence,0.f,1.f)*habituation*std::clamp(gain,.1f,3.f);
    float shock=(fulfilled?16.f:7.f)*impact;
    state.perplexity=std::clamp(state.perplexity+shock*(.8f+state.skepticism/200.f),0.f,100.f);
    state.curiosity=std::clamp(state.curiosity+impact*(fulfilled?6.f:2.f)*(1-state.fear/140.f),0.f,100.f);
    state.fear=std::clamp(state.fear+impact*(9-state.courage*.08f),0.f,100.f);
    state.trust=std::clamp(state.trust+impact*(state.aggression>60?-4.f:state.fear>50?-2.f:fulfilled && p.event.causeConfirmed?1.f:0.f),0.f,100.f);
    state.skepticism=std::clamp(state.skepticism-impact*(fulfilled?2.f:.3f),0.f,100.f);return impact>.005f;
}
bool NpcPerplexitySystem::Decay(uint64_t now){if(!lastDecay_){lastDecay_=now;return false;}if(now-lastDecay_<60000)return false;float minutes=std::min<float>(60,(now-lastDecay_)/60000.f);lastDecay_=now;auto old=state;state.perplexity=std::max(0.f,state.perplexity-.5f*minutes);state.fear=std::max(0.f,state.fear-.7f*minutes);if(!lastHabituation_)lastHabituation_=now;else if(now-lastHabituation_>=600000){for(auto& h:habituation_)h.second=std::max(0,h.second-1);lastHabituation_=now;}return old.perplexity!=state.perplexity || old.fear!=state.fear;}
std::string EventSummary(const PerceivedEvent& p,bool challenge){
    std::string s=challenge?"Mudança compatível temporalmente com um desafio; autoria não confirmada. ":"";
    switch(p.event.kind){case WorldKind::TimeJump:s+="O relógio do jogo mudou abruptamente; percepção ambiental inferida.";break;case WorldKind::TimeReversed:s+="O relógio sofreu salto regressivo.";break;case WorldKind::Night:s+="O horário passou para noite; causa não confirmada.";break;case WorldKind::RainStarted:s+="A chuva começou na região observada.";break;case WorldKind::RainStopped:s+="A chuva diminuiu.";break;case WorldKind::PedAppeared:s+="Um pedestre entrou no conjunto observado; criação, streaming ou entrada no raio não diferenciados.";break;case WorldKind::VehicleAppeared:s+="Um veículo entrou no conjunto observado; causa não confirmada.";break;case WorldKind::WeaponHolstered:s+="A arma equipada passou para desarmado.";break;case WorldKind::Gunshot:s+="A rotina de disparo foi acionada; som ou projétil não confirmados.";break;case WorldKind::PlayerLeft:s+="O NPC perdeu contato visual com o jogador.";break;default:s+="Ocorrência próxima: ";s+=WorldName(p.event.kind);s+="; causa não confirmada.";break;}
    if(!p.event.visual && p.channel!="ambient_inference_no_direct_visual_claim")s+=" Sem confirmação visual direta.";
    return s;
}
std::string LocalReaction(const EmotionalState& s,uint64_t variety){if(s.fear>65)return variety%2?"O que foi isso? Melhor eu ficar atento.":"Calma aí, mano... Isso tá me deixando nervoso.";if(s.aggression>65)return variety%2?"Espera... Que coisa estranha foi essa?":"Papo reto, tem alguma coisa estranha aqui.";return variety%2?"Mano do céu... Ainda tô tentando entender isso.":"Rapaz, que coisa esquisita. Como é que explica isso?";}
}
