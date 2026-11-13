#include "npc_action_controller.h"
#include "dialogue_timing.h"
#include <algorithm>
namespace npcai {
NpcActionController::NpcActionController(ActionEngine& e,Settings s,Reporter r):engine_(e),settings_(s),reporter_(std::move(r)){}
bool NpcActionController::Begin(const Target& t,uint64_t now){
    auto it=entries_.find(t.id);
    if(it!=entries_.end()){it->second.detached=false;return true;}
    auto c=engine_.Inspect(t);
    if(!c.valid || !c.eligible || c.distance>settings_.radius || entries_.size()>=32)return false;
    Entry e;e.target=t;e.deadline=now+settings_.actionTimeoutMs;entries_.emplace(t.id,std::move(e));return true;
}
void NpcActionController::Report(Entry& e,bool ok,const std::string& why,int amount){if(e.request && !e.reported){e.reported=true;reporter_(*e.request,ok,why,amount);}}
void NpcActionController::Fail(Entry& e,const std::string& why){engine_.Stop(e.target);Report(e,false,why);e.request.reset();e.holding=false;e.state=InteractionState::RELEASED;}
bool NpcActionController::Persistent(const std::string& id)const{auto it=entries_.find(id);return it!=entries_.end() && (it->second.holding || it->second.state==InteractionState::FOLLOWING_PLAYER);}
bool NpcActionController::Submit(ActionRequest request,uint64_t now){
    const auto d=request.decision;
    if(d.action==Action::None || d.requestId.empty() || seen_.count(d.requestId))return false;
    auto reject=[&](const std::string& reason){reporter_(request,false,reason,0);return false;};
    if(seen_.size()>=8192)return reject("nonce_ledger_full");
    seen_.insert(d.requestId);
    auto c=engine_.Inspect(request.target);
    if(!settings_.aiActions)return reject("actions_disabled");
    if(!c.valid || !c.eligible)return reject(c.reason);
    if(d.durationMs<100 || d.durationMs>10000)return reject("invalid_duration");
    if((IsSpeechAction(d.action) || d.action==Action::HandsUp || d.action==Action::SurrenderMoney || d.action==Action::Cower) && !settings_.npcDialogueAnimations)return reject("animations_disabled");
    auto found=entries_.find(request.target.id);
    if(found!=entries_.end()){
        auto& previous=found->second;
        const bool posture=d.action==Action::Idle || d.action==Action::FacePlayer || IsSpeechAction(d.action);
        if(posture && (previous.holding || previous.state==InteractionState::FOLLOWING_PLAYER || previous.state==InteractionState::FLEEING))return reject("higher_priority_behavior");
    }
    if(d.action==Action::SurrenderMoney){
        if(!settings_.moneyDrop)return reject("money_disabled");
        if(d.amount<1 || d.amount>settings_.moneyMax)return reject("money_amount_limit");
        if(!settings_.moneyRepeat && paid_.count(request.target.id))return reject("money_already_paid");
        if(paid_.size()>=4096 || sessionMoney_+d.amount>settings_.moneySessionMax)return reject("money_session_limit");
    }
    if(!Begin(request.target,now))return reject("npc_unavailable_or_out_of_initial_range");
    auto& e=entries_.at(request.target.id);
    if(d.action==Action::ReleaseSurrender || d.action==Action::StopFollowing){
        const bool matches=d.action==Action::ReleaseSurrender?e.holding:e.active==Action::FollowPlayer;
        if(!matches)return reject("requested_behavior_not_active");
        Report(e,false,"explicit_release");engine_.Stop(e.target);
        Entry fresh;fresh.target=e.target;e=std::move(fresh);e.state=InteractionState::CONVERSING;e.request=std::move(request);Report(e,true,"released");e.request.reset();return true;
    }
    Report(e,false,"superseded");e.request=std::move(request);e.reported=false;e.recoveries=0;e.durationMs=std::min(settings_.actionTimeoutMs,d.durationMs);
    e.deadline=now+static_cast<uint64_t>(settings_.actionTimeoutMs);
    if(e.holding && (d.action==Action::HandsUp || d.action==Action::SurrenderMoney)){
        e.active=d.action;e.stage=1;e.stageAt=now;e.state=InteractionState::SURRENDERING;return true;
    }
    engine_.Stop(e.target);e.holding=false;e.gesture=Action::None;e.speechUntil=0;
    e.active=d.action;e.state=InteractionState::SELECTED;e.stage=0;e.stageAt=now;
    if(IsSpeechAction(d.action))e.speechUntil=now+DialogueDuration(d.dialogue,settings_);
    return true;
}
void NpcActionController::Speak(const Target& t,const std::string& text,uint64_t now){
    if(!settings_.npcDialogueAnimations || !Begin(t,now))return;
    auto& e=entries_.at(t.id);
    if(e.holding || e.active==Action::FollowPlayer || e.active==Action::Flee || (e.request && !IsSpeechAction(e.active)))return;
    // Speech timing starts on publication, not on HTTP dispatch or while waiting.
    e.speechUntil=now+DialogueDuration(text,settings_);e.nextGesture=now;e.recoveries=0;
}
void NpcActionController::VoicePlayback(const Target& t,bool playing,uint64_t now){
    if(playing){
        if(!settings_.npcDialogueAnimations || !Begin(t,now))return;
        auto& e=entries_.at(t.id);
        if(e.holding || e.active==Action::FollowPlayer || e.active==Action::Flee || (e.request && !IsSpeechAction(e.active)))return;
        if(!e.audioDriven){e.audioDriven=true;e.nextGesture=now;e.recoveries=0;}
        e.speechUntil=now+1500;
    }else{auto it=entries_.find(t.id);if(it!=entries_.end() && it->second.audioDriven){it->second.audioDriven=false;it->second.speechUntil=now;}}
}
void NpcActionController::Posture(Entry& e,const ActionContext& c,uint64_t now){
    if(now<e.nextFace)return;
    e.nextFace=now+500;
    if(settings_.facePlayer && c.needsTurn){
        engine_.Start(e.target,Action::FacePlayer,settings_.actionTimeoutMs);
    }else{
        engine_.StopAction(e.target,Action::FacePlayer);
        if((settings_.stopOnTalk && settings_.npcStandStill) || e.holding)engine_.Start(e.target,Action::Idle,60000);
    }
}
void NpcActionController::Gestures(Entry& e,uint64_t now){
    if(e.gesture!=Action::None){
        auto effect=engine_.Poll(e.target,e.gesture);
        if(effect==Effect::Applied)Report(e,true,"gesture_observed");
        if(now>=e.gestureUntil || effect==Effect::Finished || effect==Effect::Failed){
            engine_.StopAction(e.target,e.gesture);e.gesture=Action::None;e.nextGesture=std::max(now+650,e.gestureUntil+300);
            if(effect==Effect::Failed){++e.recoveries;Report(e,false,"gesture_interrupted");}
        }
    }
    if(now>=e.speechUntil){
        if(e.gesture!=Action::None)engine_.StopAction(e.target,e.gesture);
        e.gesture=Action::None;Report(e,false,"speech_window_ended_without_effect");e.request.reset();e.active=Action::None;e.state=InteractionState::CONVERSING;return;
    }
    if(e.gesture==Action::None && now>=e.nextGesture && e.speechUntil-now>=700 && e.recoveries<3){
        const auto gesture=IsSpeechAction(e.active)?e.active:Action::TalkGesture;
        const int duration=static_cast<int>(std::min<uint64_t>(2100,e.speechUntil-now));
        if(engine_.Start(e.target,gesture,duration)){
            e.gesture=gesture;e.gestureUntil=now+duration;e.state=gesture==Action::Angry?InteractionState::ANGRY:InteractionState::TALKING_GESTURE;
        }else{++e.recoveries;e.nextGesture=now+1000;Report(e,false,"gesture_start_unavailable");}
    }
}
bool NpcActionController::Money(Entry& e,uint64_t now){
    if(!e.request || e.reported)return true;
    if(now-e.stageAt<900)return false;
    if(engine_.Poll(e.target,Action::HandsUp)!=Effect::Applied){
        if(now>=e.deadline){Report(e,false,"money_pose_unconfirmed");e.request.reset();}
        return false;
    }
    if(!engine_.PrepareMoney(e.target)){
        if(now>=e.deadline){Report(e,false,"money_model_or_api_unavailable");e.request.reset();}
        return false;
    }
    const int amount=e.request->decision.amount,count=std::min({settings_.moneyPickupsMax,5,amount});
    if(pickups_.size()+static_cast<size_t>(count)>32 || (!settings_.moneyRepeat && paid_.count(e.target.id)) || sessionMoney_+amount>settings_.moneySessionMax){Report(e,false,"money_budget_or_duplicate");e.request.reset();return false;}
    paid_.insert(e.target.id);sessionMoney_+=amount;
    std::vector<int> batch;bool ok=true;
    for(int n=0;n<count;++n){
        auto c=engine_.Inspect(e.target);if(!c.valid || !c.eligible || c.paused){ok=false;break;}
        int h=engine_.SpawnMoney(e.target,amount/count+(n<amount%count?1:0),n);
        if(h<0){ok=false;break;}batch.push_back(h);
    }
    if(!ok){for(int h:batch)engine_.RemovePickup(h);Report(e,false,"money_partial_batch_rolled_back");e.request.reset();return false;}
    for(int h:batch)pickups_.push_back({h,now+60000,e.target.id});
    Report(e,true,"money_pickups_verified",amount);e.request.reset();return true;
}
void NpcActionController::Tick(uint64_t now){
    for(auto it=pickups_.begin();it!=pickups_.end();){
        if(!engine_.PickupAlive(it->handle))it=pickups_.erase(it);
        else if(now>=it->expires){engine_.RemovePickup(it->handle);it=pickups_.erase(it);}else ++it;
    }
    for(auto& pair:entries_){
        auto& e=pair.second;auto c=engine_.Inspect(e.target);
        if(!c.valid || !c.eligible){Fail(e,std::string("priority_or_invalid: ")+c.reason);continue;}
        if(c.paused)continue;
        const bool follows=e.state==InteractionState::FOLLOWING_PLAYER;
        if(e.holding || follows){
            const float distance=follows?settings_.followReleaseDistance:settings_.surrenderReleaseDistance;
            if(c.distance>distance){if(!e.farSince)e.farSince=now?now:1;if(now>=e.farSince+settings_.surrenderReleaseDelayMs){Fail(e,"distance_release");continue;}}
            else e.farSince=0;
        }else if(e.state!=InteractionState::FLEEING && c.distance>settings_.radius){Fail(e,"conversation_range_exit");continue;}
        if(e.state==InteractionState::SELECTED){
            if(settings_.facePlayer){
                if(!engine_.Start(e.target,Action::FacePlayer,settings_.actionTimeoutMs)){Fail(e,"face_start_unavailable");continue;}
                e.state=InteractionState::FACE_PLAYER;
            }else e.state=e.active==Action::None?InteractionState::CONVERSING:InteractionState::EXECUTING_ACTION;
        }
        if(e.state==InteractionState::FACE_PLAYER){
            const auto face=engine_.Poll(e.target,Action::FacePlayer);
            if(face==Effect::Failed || now>=e.deadline){Fail(e,"face_unconfirmed");continue;}
            if(face!=Effect::Applied)continue;
            engine_.StopAction(e.target,Action::FacePlayer);
            e.state=e.active==Action::None?InteractionState::CONVERSING:InteractionState::EXECUTING_ACTION;
        }
        if(e.state==InteractionState::CONVERSING || e.state==InteractionState::TALKING_GESTURE || e.state==InteractionState::ANGRY){
            Posture(e,c,now);if(e.speechUntil)Gestures(e,now);continue;
        }
        if(e.state==InteractionState::EXECUTING_ACTION){
            if(IsSpeechAction(e.active)){e.state=InteractionState::CONVERSING;Posture(e,c,now);Gestures(e,now);continue;}
            if(!e.stage){
                auto first=e.active==Action::SurrenderMoney?Action::HandsUp:e.active;
                if(!engine_.Start(e.target,first,e.active==Action::HandsUp || e.active==Action::SurrenderMoney?60000:e.durationMs)){Fail(e,"action_start_unavailable");continue;}
                e.stage=1;e.stageAt=now;
            }
            auto first=e.active==Action::SurrenderMoney?Action::HandsUp:e.active;
            auto effect=engine_.Poll(e.target,first);
            if(effect==Effect::Failed || now>=e.deadline){Fail(e,"action_unconfirmed");continue;}
            if(effect!=Effect::Applied)continue;
            if(e.active==Action::HandsUp || e.active==Action::SurrenderMoney){
                e.holding=settings_.holdSurrender;e.state=InteractionState::SURRENDERING;e.nextMaintain=now+500;
                if(e.active==Action::HandsUp){Report(e,true,"hands_observed");e.request.reset();}
            }else if(e.active==Action::FollowPlayer){Report(e,true,"follow_movement_observed");e.request.reset();e.state=InteractionState::FOLLOWING_PLAYER;}
            else if(e.active==Action::Flee){Report(e,true,"flee_movement_observed");e.request.reset();e.deadline=now+e.durationMs;e.state=InteractionState::FLEEING;}
            else{Report(e,true,"effect_observed");e.request.reset();e.deadline=now+e.durationMs;e.state=InteractionState::AFRAID;}
        }
        if(e.state==InteractionState::SURRENDERING){
            Posture(e,c,now);
            if(now>=e.nextMaintain){
                e.nextMaintain=now+500;auto effect=engine_.Poll(e.target,Action::HandsUp);
                if(effect==Effect::Failed || effect==Effect::Finished){
                    if(++e.recoveries>3 || !engine_.Start(e.target,Action::HandsUp,60000)){Fail(e,"surrender_pose_lost");continue;}
                    e.nextMaintain=now+1000;
                }else{e.recoveries=0;engine_.Start(e.target,Action::HandsUp,60000);}
            }
            if(e.active==Action::SurrenderMoney)Money(e,now);
            else if(e.request && engine_.Poll(e.target,Action::HandsUp)==Effect::Applied){Report(e,true,"hands_still_observed");e.request.reset();}
            if(!settings_.holdSurrender && now-e.stageAt>=static_cast<uint64_t>(settings_.actionTimeoutMs)){engine_.Stop(e.target);e.state=InteractionState::RELEASED;}
        }else if(e.state==InteractionState::FOLLOWING_PLAYER){
            auto effect=engine_.Poll(e.target,Action::FollowPlayer);if(effect==Effect::Failed)Fail(e,"follow_interrupted");
        }else if(e.state==InteractionState::FLEEING || e.state==InteractionState::AFRAID){
            if(now>=e.deadline)Fail(e,"temporary_behavior_completed");
        }
    }
    std::vector<std::string> ended;
    for(const auto& pair:entries_)if(pair.second.state==InteractionState::RELEASED)ended.push_back(pair.first);
    for(const auto& id:ended)Cancel(id,"released",true);
}
void NpcActionController::Detach(const std::string& id){auto it=entries_.find(id);if(it==entries_.end())return;if(Persistent(id) || it->second.state==InteractionState::FLEEING){it->second.detached=true;return;}Cancel(id);}
void NpcActionController::Cancel(const std::string& id,const std::string& why,bool cleanup){
    auto it=entries_.find(id);if(it!=entries_.end()){engine_.Stop(it->second.target);Report(it->second,false,why);entries_.erase(it);}
    if(cleanup)for(auto p=pickups_.begin();p!=pickups_.end();){if(p->owner==id){engine_.RemovePickup(p->handle);p=pickups_.erase(p);}else ++p;}
}
void NpcActionController::Reset(){for(auto& p:entries_){engine_.Stop(p.second.target);Report(p.second,false,"game_reset");}entries_.clear();for(auto& p:pickups_)engine_.RemovePickup(p.handle);pickups_.clear();}
std::map<std::string,InteractionState> NpcActionController::States()const{std::map<std::string,InteractionState> states;for(const auto& e:entries_)states[e.first]=e.second.state;return states;}
bool NpcActionController::Executing(const std::string& id)const{auto it=entries_.find(id);return it!=entries_.end() && (it->second.request || Persistent(id));}
InteractionState NpcActionController::State(const std::string& id)const{auto it=entries_.find(id);return it==entries_.end()?InteractionState::IDLE:it->second.state;}
}
