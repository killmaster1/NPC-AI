#include "intelligence_runtime.h"
#include "autosave_gate.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <cmath>
namespace npcai {
uint64_t RealMilliseconds(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
static std::string Bounded(std::string s,size_t n){if(s.size()>n){s.resize(n);while(!s.empty() && !ValidUtf8(s))s.pop_back();}return s;}
IntelligenceRuntime::IntelligenceRuntime(Settings s,std::string dir,GeminiClient::HttpsTransport transport,Log log):settings_(s),client_(s,std::move(transport)),memory_(std::move(dir),static_cast<size_t>(s.memoryMaxMb)*1024*1024),log_(std::move(log)),worker_(&IntelligenceRuntime::Run,this){std::unique_lock<std::mutex> lock(stateMutex_);readyCv_.wait(lock,[&]{return ready_;});}
IntelligenceRuntime::~IntelligenceRuntime(){Shutdown();}
void IntelligenceRuntime::Interest(const std::string& id){std::lock_guard<std::mutex> lock(queueMutex_);interests_.erase(std::remove(interests_.begin(),interests_.end(),id),interests_.end());interests_.push_front(id);while(interests_.size()>4)interests_.pop_back();}
std::vector<std::string> IntelligenceRuntime::Interests(){std::lock_guard<std::mutex> lock(queueMutex_);return {interests_.begin(),interests_.end()};}
IntelligenceRuntime::Mind& IntelligenceRuntime::MindFor(const std::string& id,const Personality& traits){auto it=minds_.find(id);if(it!=minds_.end())return it->second;if(minds_.size()>=128)minds_.erase(minds_.begin());auto result=minds_.emplace(id,Mind(traits));return result.first->second;}
JsonValue IntelligenceRuntime::Context(const Request& r){
    auto& mind=MindFor(r.npcId,r.context.personality);const auto& e=mind.emotion.state;const auto& p=r.context.personality;
    JsonValue out={{"npc",{{"personality_source",r.context.personalityVariation?"generated_once_per_session_identity":"default_without_individual_variation"},{"personality",{{"courage",p.courage},{"aggression",p.aggression},{"kindness",p.kindness},{"patience",p.patience}}},{"emotional_state_local",{{"perplexity",std::round(e.perplexity)},{"fear",std::round(e.fear)},{"curiosity",std::round(e.curiosity)},{"trust",std::round(e.trust)},{"skepticism",std::round(e.skepticism)}}}}},
        {"world",{{"area",r.context.area},{"region_level",r.context.regionLevel},{"available",lastFrame_.at!=0},{"period",!lastFrame_.at?"unknown":lastFrame_.minuteOfDay>=1200 || lastFrame_.minuteOfDay<360?"night":"day"},{"age_ms",lastFrame_.at?RealMilliseconds()-lastFrame_.at:0},{"source","periodic_engine_snapshot_inference"}}},{"monitorable_expectations",JsonValue::array()},{"recent_events",JsonValue::array()},{"active_expectations",JsonValue::array()},{"memory",JsonValue::array()}};
    out["npc"]["social"]={{"stage",mind.relation.Stage()},{"trust",std::round(mind.relation.trust)},{"irritation",std::round(mind.relation.irritation)},{"emotion",mind.relation.emotion},{"intensity",mind.relation.intensity}};
    out["npc"]["personal_style"]={{"sociability",p.sociability},{"humor",p.humor},{"empathy",p.empathy},{"slang",p.slang},{"profanity_tendency",p.profanity}};
    if(r.context.area==0 && lastFrame_.weatherAvailable)out["world"]["rain_intensity"]=lastFrame_.rain;
    if(settings_.worldExpectations && settings_.worldPerception && lastFrame_.at){if(lastFrame_.minuteOfDay>=360 && lastFrame_.minuteOfDay<1200)out["monitorable_expectations"].push_back("night");out["monitorable_expectations"].push_back("time_reversed");out["monitorable_expectations"].push_back("ped_appeared");if(lastFrame_.weatherAvailable && lastFrame_.rain<=.15f)out["monitorable_expectations"].push_back("rain_started");if(lastFrame_.vehiclesAvailable)out["monitorable_expectations"].push_back("vehicle_appeared");if(lastFrame_.weapon>0)out["monitorable_expectations"].push_back("weapon_holstered");out["monitorable_expectations"].push_back("player_left");}
    for(const auto& observer:lastFrame_.observers)if(observer.id==r.npcId){out["world"]["player_visibility_checked"]=observer.losAvailable;out["world"]["player_visible_verified"]=observer.playerVisible;out["world"]["player_nearby_detector"]=WorldDistance(lastFrame_.player,observer.position)<=15;if(observer.playerVisible){if(lastFrame_.weapon>=0)out["world"]["equipped_weapon_type"]=lastFrame_.weapon;out["world"]["player_distance_m_approx"]=std::round(WorldDistance(lastFrame_.player,observer.position));out["world"]["moving_observed"]=lastFrame_.speed>.01f;out["world"]["aiming_flag"]=lastFrame_.aiming;out["world"]["player_in_vehicle"]=lastFrame_.inVehicle;}}
    size_t start=mind.recent.size()>3?mind.recent.size()-3:0;for(size_t i=start;i<mind.recent.size();++i)out["recent_events"].push_back(mind.recent[i]);
    for(const auto& x:mind.expectations.Active())out["active_expectations"].push_back({{"event",WorldName(x.spec.event)},{"remaining_ms",x.deadline>RealMilliseconds()?x.deadline-RealMilliseconds():0},{"count",x.spec.count}});
    if(settings_.memoryEnabled && settings_.memorySend){for(auto note:memory_.Notes({r.npcId,false},settings_.memoryGlobal))out["memory"].push_back(Bounded(note,500));}
    out["request_kind"]=r.spontaneous?"spontaneous_comment_no_player_utterance":"player_conversation";
    if(r.feedback)out["action_result"]={{"action",r.feedback->action},{"status",r.feedback->status},{"amount",r.feedback->amount}};
    const size_t budget=static_cast<size_t>(settings_.contextTokens)*3;
    for(const auto* key:{"memory","recent_events","active_expectations"})while(out.dump().size()>budget && !out[key].empty())out[key].erase(out[key].begin());
    if(out.dump().size()>budget)out["npc"].erase("personality_source");
    if(out.dump().size()>budget)out["npc"].erase("personality");
    if(out.dump().size()>budget){auto period=out["world"]["period"];auto available=out["world"]["available"];out["world"]={{"period",period},{"available",available}};out.erase("active_expectations");}
    return out;
}
Result IntelligenceRuntime::Chat(const Request& r,const std::atomic_bool& cancel){
    Interest(r.npcId);JsonValue context,history;
    {std::unique_lock<std::mutex> lock(stateMutex_);auto until=RealMilliseconds()+5000;
        while(appliedEpoch_!=CurrentEpoch() && !cancel && RealMilliseconds()<until)readyCv_.wait_for(lock,std::chrono::milliseconds(25));
        if(cancel || appliedEpoch_!=CurrentEpoch())return {false,"Contexto de save em transição; tente novamente."};
        if(pendingEpochs_.size()>=32)pendingEpochs_.erase(pendingEpochs_.begin());
        pendingEpochs_[r.requestId]=appliedEpoch_;
        auto& social=MindFor(r.npcId,r.context.personality).relation;bool nearbyAim=false;
        for(const auto& observer:lastFrame_.observers)if(observer.id==r.npcId && observer.playerVisible)nearbyAim=lastFrame_.aiming;
        social.Observe(r.spontaneous?"":r.message,r.context.personality,nearbyAim,RealMilliseconds(),settings_);
        context=Context(r);auto it=histories_.find(r.npcId+"/"+r.conversationId);history=it==histories_.end()?JsonValue::array():it->second;}
    while(history.size()>6)history.erase(history.begin());
    const size_t budget=static_cast<size_t>(settings_.contextTokens)*3;
    while(!history.empty() && history.dump().size()+context.dump().size()>budget)history.erase(history.begin());
    auto response=client_.Chat(r,context,history,cancel);
    if(response.ok){auto d=response.decision.value_or(ParseDecision(response.text,r.requestId));std::vector<std::string> recent;
        for(const auto& item:history)if(!item.value("player",true))recent.push_back(item.value("text",""));
        d.dialogue=CleanLanguage(PlainSpeech(d.dialogue),settings_.profanityLevel,settings_.profanityRepetition,recent);
        auto shortText=ShortSpeech(d.dialogue,settings_.elevenMax);if(!shortText.empty())d.dialogue=shortText;
        if(settings_.emotionsEnabled){d.emotion=EmotionName(d.emotion);d.emotionIntensity*=settings_.emotionIntensity;}else{d.emotion="neutral";d.emotionIntensity=0;}
        if(context["npc"]["social"].value("irritation",0.f)<35 && d.action==Action::Angry){d.action=Action::None;if(d.emotion=="angry" || d.emotion=="furious")d.emotion="neutral";}
        d.relationship=context["npc"]["social"].value("trust",40.f);d.relationshipStage=context["npc"]["social"].value("stage","neutral");
        response.text=d.dialogue;response.decision=d;
    }
    {std::lock_guard<std::mutex> lock(stateMutex_);auto pending=pendingEpochs_.find(r.requestId);
        if(response.ok && pending!=pendingEpochs_.end() && (pending->second!=appliedEpoch_ || appliedEpoch_!=CurrentEpoch()))response={false,"Save/contexto mudou; resposta anterior descartada."};
        if(!response.ok)pendingEpochs_.erase(r.requestId);
    }return response;
}
void IntelligenceRuntime::Accepted(const Request& r,const Decision& d){
    Interest(r.npcId);std::lock_guard<std::mutex> lock(stateMutex_);
    auto epoch=pendingEpochs_.find(r.requestId);if(epoch!=pendingEpochs_.end()){const bool stale=epoch->second!=appliedEpoch_ || appliedEpoch_!=CurrentEpoch();pendingEpochs_.erase(epoch);if(stale)return;}
    auto& mind=MindFor(r.npcId,r.context.personality);mind.relation.emotion=d.emotion;mind.relation.intensity=d.emotionIntensity;mind.interestUntil=RealMilliseconds()+180000;
    if(histories_.size()>=64 && !histories_.count(r.npcId+"/"+r.conversationId))histories_.erase(histories_.begin());
    auto& history=histories_[r.npcId+"/"+r.conversationId];if(history.is_null())history=JsonValue::array();history.push_back({{"player",true},{"text",Bounded(r.message,1200)}});history.push_back({{"player",false},{"text",Bounded(d.dialogue,1200)}});while(history.size()>std::min<size_t>(24,settings_.historyLimit))history.erase(history.begin());
    if(settings_.worldExpectations && settings_.worldPerception && lastFrame_.at)mind.expectations.Register(r.requestId,d.expectations,RealMilliseconds(),settings_.maxExpectations,lastFrame_);
    if(settings_.memoryEnabled && settings_.memoryIndividual){if(settings_.personalityPersistent)memory_.Note(r.npcId,"Perfil da identidade de sessão: sociabilidade "+std::to_string(r.context.personality.sociability)+", paciência "+std::to_string(r.context.personality.patience)+"; relação "+mind.relation.Stage()+". Não restaurar como outro pedestre.");memory_.Note(r.npcId,(r.spontaneous?"Gatilho de comentário, não fala do jogador: ":"O jogador disse (alegação, não fato): ")+Bounded(r.message,260));memory_.Note(r.npcId,"O NPC respondeu/propos: "+Bounded(d.dialogue,260));memory_.Emotion({r.npcId,false},mind.emotion.state);}
}
uint64_t IntelligenceRuntime::CurrentEpoch(){std::lock_guard<std::mutex> q(queueMutex_);return queuedEpoch_;}
void IntelligenceRuntime::Confirmed(const std::string& npc,Action action,bool ok,int amount){std::lock_guard<std::mutex> q(queueMutex_);if(receipts_.size()<64)receipts_.push_back({npc,action,ok,amount,queuedEpoch_});cv_.notify_one();}
void IntelligenceRuntime::ApplyReceipt(const Receipt& receipt){
    const auto& npc=receipt.npc;const auto action=receipt.action;const bool ok=receipt.ok;const int amount=receipt.amount;
    auto found=minds_.find(npc);
    if(ok && found!=minds_.end()){
        auto& e=found->second.emotion.state;
        if(action==Action::HandsUp || action==Action::SurrenderMoney){e.fear=std::min(100.f,e.fear+15);e.trust=std::max(0.f,e.trust-10);}
        else if(action==Action::FollowPlayer)e.trust=std::min(100.f,e.trust+3);
        else if(action==Action::ReleaseSurrender)e.fear=std::max(0.f,e.fear-8);
        if(settings_.memoryEnabled && settings_.memoryIndividual)memory_.Emotion({npc,false},e);
    }
    if(!settings_.memoryEnabled || !settings_.memoryIndividual)return;
    std::string text=ok?"Efeito confirmado pelo controlador: ":"Proposta sem efeito confirmado: ";text+=ActionName(action);if(ok && amount>0)text+="; pickups criados $"+std::to_string(amount)+", coleta ainda não confirmada";memory_.Note(npc,text);
}
void IntelligenceRuntime::Submit(WorldFrame f){std::lock_guard<std::mutex> lock(queueMutex_);if(stopped_)return;if(frame_)++dropped_;frame_=std::move(f);cv_.notify_one();}
void IntelligenceRuntime::Boundary(){std::lock_guard<std::mutex> lock(queueMutex_);boundary_=true;++queuedEpoch_;frame_.reset();flush_=true;cv_.notify_one();}
void IntelligenceRuntime::Slot(int slot,bool saved){if(slot<0 || slot>99)return;std::lock_guard<std::mutex> lock(queueMutex_);if(!saved || queuedSlot_!=slot)++queuedEpoch_;queuedSlot_=slot;if(!saved)frame_.reset();if(slots_.size()<8)slots_.push_back({slot,saved});else slots_.back()={slot,saved};flush_=true;cv_.notify_one();}
void IntelligenceRuntime::Flush(){std::lock_guard<std::mutex> lock(queueMutex_);flush_=true;cv_.notify_one();}
void IntelligenceRuntime::Clear(){std::lock_guard<std::mutex> lock(queueMutex_);clear_=true;cv_.notify_one();}
std::optional<LocalReply> IntelligenceRuntime::TakeReply(){std::lock_guard<std::mutex> lock(queueMutex_);if(replies_.empty())return {};auto r=std::move(replies_.front());replies_.pop_front();return r;}
void IntelligenceRuntime::Process(const WorldFrame& frame){
    auto started=std::chrono::steady_clock::now();auto events=detector_.Update(frame);lastFrame_=frame;++metrics_.frames;metrics_.events+=events.size();
    for(const auto& e:events){
        if(settings_.memoryEnabled && settings_.memoryGlobal && settings_.worldEventMemory){
            auto& count=patterns_[e.kind];count=std::min(100000,count+1);
            if(count==3 && e.kind==WorldKind::TimeJump)memory_.Note("","Padrão desta sessão: mudanças abruptas de relógio se repetiram; autoria não confirmada.",true);
            if(count==3 && e.kind==WorldKind::WeaponHolstered)memory_.Note("","Padrão desta sessão: o equipamento do jogador mudou repetidamente para desarmado; intenção não confirmada.",true);
        }
        if(settings_.memoryEnabled && settings_.memoryGlobal && settings_.worldEventMemory && (e.kind==WorldKind::TimeJump || e.kind==WorldKind::RainStarted || e.kind==WorldKind::PlayerDiscontinuity))memory_.Note("",EventSummary({e,e.confidence,"engine_detector_not_npc_knowledge"}),true);
        for(const auto& observer:frame.observers){
            auto perceived=NpcPerception::Assess(e,observer);if(!perceived)continue;
            auto& mind=MindFor(observer.id,observer.personality);auto matches=mind.expectations.Observe(*perceived);metrics_.matched+=matches.size();bool matched=!matches.empty();
            if(settings_.perplexityEnabled)mind.emotion.Observe(*perceived,matched,settings_.perplexityGain);
            const auto summary=EventSummary(*perceived,matched);if(mind.recent.empty() || mind.recent.back()!=summary)mind.recent.push_back(summary);while(mind.recent.size()>8)mind.recent.pop_front();
            if(settings_.memoryEnabled && settings_.worldEventMemory){if(settings_.memoryIndividual){memory_.Note(observer.id,summary);memory_.Emotion({observer.id,false},mind.emotion.state);}if(matched && settings_.memoryGlobal)memory_.Note("",summary,true);}
            const float threshold=settings_.reactionMode=="immersive"?.5f:.75f;
            const bool important=matched || (e.intensity>threshold && perceived->confidence>.6f);
            if(important && frame.at>=mind.lastReaction+static_cast<uint64_t>(settings_.spontaneousCooldownS)*1000){
                mind.lastReaction=frame.at;LocalReply reply;reply.npc=observer.id;reply.dialogue=LocalReaction(mind.emotion.state,metrics_.events);reply.action=mind.emotion.state.fear>85 && mind.emotion.state.courage<35?Action::Flee:Action::TalkGesture;
                if(frame.at-hourStart_>=3600000){hourStart_=frame.at;spontaneousSpent_=0;}
                bool ai=settings_.spontaneousAI && settings_.reactionMode!="economy" && spontaneousSpent_<settings_.spontaneousBudgetHour;
                // Economy never schedules a network request, even if the extra toggle is set.
                reply.aiOpportunity=ai;if(ai)++spontaneousSpent_;
                {std::lock_guard<std::mutex> q(queueMutex_);if(replies_.size()<8)replies_.push_back(std::move(reply));}++metrics_.reactions;
            }
        }
    }
    for(auto& pair:minds_){pair.second.relation.Recover(frame.at,settings_.relationshipRecovery);pair.second.expectations.Expire(frame.at);if(settings_.perplexityDecay && pair.second.emotion.Decay(frame.at) && settings_.memoryEnabled && settings_.memoryIndividual)memory_.Emotion({pair.first,false},pair.second.emotion.state);}
    metrics_.perceptionUs+=frame.observerCostUs+std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count();
}
void IntelligenceRuntime::Run(){
    {std::lock_guard<std::mutex> s(stateMutex_);std::string profile=settings_.memoryProfile=="auto"?"unsaved-"+SessionId():settings_.memoryProfile;if(settings_.memoryEnabled && settings_.memoryPersistent)memory_.Load(profile);else memory_.Initialize(profile);if(settings_.memoryClear && settings_.memoryPersistent)memory_.Clear();ready_=true;readyCv_.notify_all();lastSave_=RealMilliseconds();lastLog_=lastSave_;}
    AutosaveGate saveGate(static_cast<uint64_t>(settings_.memoryAutosaveSeconds)*1000);saveGate.Reset(lastSave_);
    for(;;){
        std::optional<WorldFrame> frame;std::deque<std::pair<int,bool>> slots;std::deque<Receipt> receipts;uint64_t epoch=0;bool boundary=false,flush=false,clear=false,stop=false;
        {std::unique_lock<std::mutex> q(queueMutex_);cv_.wait_for(q,std::chrono::milliseconds(250),[&]{return stopped_ || frame_.has_value() || boundary_ || flush_ || clear_ || !slots_.empty() || !receipts_.empty();});stop=stopped_;frame=std::move(frame_);frame_.reset();slots.swap(slots_);receipts.swap(receipts_);epoch=queuedEpoch_;boundary=boundary_;boundary_=false;flush=flush_;flush_=false;clear=clear_;clear_=false;}
        {std::lock_guard<std::mutex> s(stateMutex_);
            const auto oldEpoch=appliedEpoch_;for(const auto& receipt:receipts)if(receipt.epoch==oldEpoch)ApplyReceipt(receipt);
            if(boundary){detector_.Reset();patterns_.clear();minds_.clear();histories_.clear();lastFrame_=WorldFrame{};}
            for(auto slot:slots)if(settings_.memoryProfile=="auto"){
                auto old=memory_.Snapshot();if(!slot.second){minds_.clear();histories_.clear();lastFrame_=WorldFrame{};}if(settings_.memoryPersistent && settings_.memoryEnabled)memory_.Save();if(settings_.memoryEnabled && settings_.memoryPersistent)memory_.Load("slot-"+std::to_string(slot.first));else memory_.Initialize("slot-"+std::to_string(slot.first));if(settings_.memoryClear && settings_.memoryPersistent)memory_.Clear();if(slot.second)memory_.ForkFrom(old);detector_.Reset();
            }
            appliedEpoch_=epoch;readyCv_.notify_all();if(epoch!=oldEpoch)for(const auto& receipt:receipts)if(receipt.epoch==epoch)ApplyReceipt(receipt);
            if(clear){minds_.clear();histories_.clear();if(settings_.memoryPersistent)memory_.Clear();else memory_.Initialize(memory_.Profile());}
            if(frame && settings_.worldPerception)Process(*frame);
            uint64_t now=RealMilliseconds();
            if(settings_.memoryEnabled && settings_.memoryPersistent && saveGate.Due(now,memory_.Dirty(),flush || stop)){
                bool dirty=memory_.Dirty();if(!memory_.Save())++metrics_.memoryErrors;else if(dirty)++metrics_.autosaves;lastSave_=now;
            }
            metrics_.autosaves=memory_.Saves();metrics_.memoryBytes=memory_.Bytes();metrics_.memoryErrors=std::max<uint64_t>(metrics_.memoryErrors,memory_.Corruptions());
            if(log_ && now-lastLog_>=60000){lastLog_=now;std::ostringstream line;line<<"Intelligence metrics frames="<<metrics_.frames<<" events="<<metrics_.events<<" expectations="<<metrics_.matched<<" autosaves="<<metrics_.autosaves<<" memory_bytes="<<metrics_.memoryBytes<<" perception_us="<<metrics_.perceptionUs<<" requests="<<client_.metrics.requests<<" retries="<<client_.metrics.retries<<" tokens="<<client_.metrics.tokens<<" latency_ms="<<client_.metrics.latencyMs<<" errors="<<client_.metrics.errors;log_(line.str());}
        }
        if(stop)return;
    }
}
IntelligenceMetrics IntelligenceRuntime::Metrics(){std::lock_guard<std::mutex> s(stateMutex_);auto out=metrics_;out.droppedFrames=dropped_;return out;}
JsonValue IntelligenceRuntime::DebugMemory(){std::lock_guard<std::mutex> s(stateMutex_);return memory_.Snapshot();}
void IntelligenceRuntime::Shutdown(){{std::lock_guard<std::mutex> q(queueMutex_);if(stopped_)return;stopped_=true;}cv_.notify_all();if(worker_.joinable())worker_.join();}
}
