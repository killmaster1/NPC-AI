#include "engine_actions.h"
#include <mod/logger.h>
#include <plugin-sdk/game_sa/anim/AnimBlendAssociation.h>
#include <plugin-sdk/game_sa/entity/PlayerPed.h>
#include <plugin-sdk/game_sa/other/Pools.h>
#include <plugin-sdk/game_sa/other/Pickups.h>
#include <plugin-sdk/game_sa/ai/tasks/TaskTypes.h>
#include <plugin-sdk/game_sa/enum/AnimGroup.h>
#include <plugin-sdk/game_sa/enum/TaskEnums.h>
#include <plugin-sdk/game_sa/base/Timer.h>
#include <plugin-sdk/game_sa/engine/World.h>
#include <plugin-sdk/game_sa/engine/ModelInfo.h>
#include <plugin-sdk/game_sa/engine/Streaming.h>
#include <algorithm>
#include <cmath>
#include <map>
namespace npcai {
namespace {
EngineActions* hooked=nullptr;
void(*originalTaskDtor)(CTask*)=nullptr;
void(*originalTaskDelete)(void*)=nullptr;
void TaskDelete(void* task){if(hooked)hooked->TaskDestroyed(reinterpret_cast<uintptr_t>(task));originalTaskDelete(task);}
void TaskDtor(CTask* task){if(hooked)hooked->TaskDestroyed(reinterpret_cast<uintptr_t>(task));originalTaskDtor(task);}
float Distance(CPed* a,CPed* b){return a->GetPosition().Dist(b->GetPosition());}
CPed* Player(){return FindPlayerPed?FindPlayerPed(-1):nullptr;}
bool Facing(CPed* p,CPed* player){
    auto d=player->GetPosition()-p->GetPosition();float n=std::sqrt(d.x*d.x+d.y*d.y);
    if(n<.05f)return true;
    auto f=p->GetTopDirection();float len=std::sqrt(f.x*f.x+f.y*f.y);
    return len>.001f && (d.x*f.x+d.y*f.y)/(n*len)>.992f;
}
}
struct EngineActions::Impl {
    explicit Impl(Resolver r):resolve(std::move(r)){}
    struct Owner {Target target;Action action;int slot;bool secondary,animation;Vec3 goal;float initialDistance;uint32_t started;bool observed=false;float lastTime=0;bool stopping=false;};
    struct Money {int model,amount;};
    Resolver resolve;
    bool ready=false,debug=false;
    std::map<uintptr_t,Owner> tasks;
    std::map<int,Money> money;
    std::map<std::string,std::string> gates;
    std::map<std::string,unsigned> gestureSequence;
    std::map<std::pair<std::string,Action>,int> ended;
    bool Own(uintptr_t ptr,const Target& t)const{auto it=tasks.find(ptr);return it!=tasks.end() && it->second.target.id==t.id && it->second.target.ref==t.ref;}
};
EngineActions::EngineActions(Resolver r,bool debug):impl_(new Impl(std::move(r))){impl_->debug=debug;}
EngineActions::~EngineActions(){if(hooked==this)hooked=nullptr;}
bool EngineActions::Init(){
    if(originalTaskDtor && originalTaskDelete){hooked=this;impl_->ready=true;return true;}
    auto symbol=GetMainLibrarySymbol("_ZN5CTaskD2Ev");
    auto deletion=GetMainLibrarySymbol("_ZN5CTaskdlEPv");
    if(!symbol || !deletion ||
       (!originalTaskDtor && !aml->Hook(symbol,reinterpret_cast<void*>(&TaskDtor),reinterpret_cast<void**>(&originalTaskDtor))) ||
       (!originalTaskDelete && !aml->Hook(deletion,reinterpret_cast<void*>(&TaskDelete),reinterpret_cast<void**>(&originalTaskDelete)))){
        logger->Error("Actions disabled: task lifetime hooks unavailable");return false;
    }
    hooked=this;impl_->ready=true;
    logger->Info("Action symbols face=%d stand=%d flee=%d follow=%d timed_anim=%d pickups=%d",
        CTaskComplexTurnToFaceEntityOrCoord::FuncProxy_ctorCTaskComplexTurnToFaceEntityOrCoord1!=nullptr,
        CTaskSimpleStandStill::FuncProxy_ctorCTaskSimpleStandStill!=nullptr,CTaskComplexSmartFleePoint::FuncProxy_ctorCTaskComplexSmartFleePoint!=nullptr,
        CTaskSimpleGoToPoint::FuncProxy_ctorCTaskSimpleGoToPoint!=nullptr,CTaskSimpleRunTimedAnim::FuncProxy_ctorCTaskSimpleRunTimedAnim!=nullptr,
        CPickups::GenerateNewOne && CPickups::RemovePickUp && CPickups::GetActualPickupIndex);
    return true;
}
void EngineActions::TaskDestroyed(uintptr_t p){
    auto it=impl_->tasks.find(p);
    if(it!=impl_->tasks.end() && it->second.animation && it->second.observed && !it->second.stopping){
        if(impl_->ended.size()>=64)impl_->ended.erase(impl_->ended.begin());
        impl_->ended[{it->second.target.id,it->second.action}]=it->second.target.ref;
    }
    if(it!=impl_->tasks.end() && impl_->debug)logger->Info("Animation task destroyed action=%s observed=%d elapsed_ms=%u slot=%d secondary=%d",ActionName(it->second.action),it->second.observed,static_cast<unsigned>(CTimer::GetTimeMS()-it->second.started),it->second.slot,it->second.secondary);
    impl_->tasks.erase(p);
}
void EngineActions::ForgetPed(int slot){for(auto it=impl_->ended.begin();it!=impl_->ended.end();)if((it->second>>8)==slot)it=impl_->ended.erase(it);else ++it;for(auto it=impl_->tasks.begin();it!=impl_->tasks.end();)if((it->second.target.ref>>8)==slot)it=impl_->tasks.erase(it);else ++it;}
ActionContext EngineActions::Inspect(const Target& t){
    ActionContext c;c.paused=CTimer::IsPaused();
    auto* ped=impl_->resolve(t,false);auto* player=Player();
    auto finish=[&](const char* reason){
        c.reason=reason;
        auto old=impl_->gates.find(t.id);
        if(old==impl_->gates.end() || old->second!=reason){
            if(impl_->gates.size()>=32 && old==impl_->gates.end())impl_->gates.erase(impl_->gates.begin());
            impl_->gates[t.id]=reason;
            unsigned primary=0,secondary=0;
            if(ped && ped->m_pIntelligence){
                auto& m=ped->m_pIntelligence->m_TaskMgr;
                for(int i=0;i<TASK_PRIMARY_MAX;++i)if(m.m_aPrimaryTasks[i])primary|=1u<<i;
                for(int i=0;i<TASK_SECONDARY_MAX;++i)if(m.m_aSecondaryTasks[i])secondary|=1u<<i;
            }
            logger->Info("Action gate=%s model=%d state=%d primary_mask=%u secondary_mask=%u",reason,
                ped?ped->m_nModelIndex:-1,ped?static_cast<int>(ped->m_ePedState):-1,primary,secondary);
        }
        return c;
    };
    if(!ped || !player)return finish("invalid_identity_or_range");
    c.valid=true;c.facing=Facing(ped,player);c.distance=Distance(ped,player);
    auto delta=player->GetPosition()-ped->GetPosition();auto forward=ped->GetTopDirection();
    float magnitude=std::sqrt((delta.x*delta.x+delta.y*delta.y)*(forward.x*forward.x+forward.y*forward.y));
    c.needsTurn=magnitude>.001f && (delta.x*forward.x+delta.y*forward.y)/magnitude<.94f;
    c.stationary=ped->m_vecMoveSpeed.Magnitude()<.015f;
    if(!impl_->ready)return finish("lifetime_hooks_unavailable");
    if(!ped->m_pIntelligence)return finish("no_intelligence");
    if(ped->bInVehicle)return finish("in_vehicle");
    if(ped->m_nCreatedBy==PED_MISSION)return finish("mission_ped");
    if(ped->bIsInTheAir)return finish("airborne");
    if((ped->m_ePedState>=PEDSTATE_STATES_NO_AI && ped->m_ePedState!=PEDSTATE_HANDS_UP) ||
       ped->m_ePedState==PEDSTATE_PURSUE || ped->m_ePedState==PEDSTATE_MUG ||
       ped->m_ePedState==PEDSTATE_SNIPER_MODE || ped->m_ePedState==PEDSTATE_ROCKETLAUNCHER_MODE ||
       ped->m_ePedState==PEDSTATE_ATTACK || ped->m_ePedState==PEDSTATE_FIGHT || ped->m_ePedState==PEDSTATE_AIMGUN ||
       ped->m_ePedState==PEDSTATE_FALL || ped->m_ePedState==PEDSTATE_GETUP || ped->m_ePedState==PEDSTATE_ON_FIRE || ped->m_ePedState==PEDSTATE_STAGGER)return finish("priority_ped_state");
    if(ped->m_nLastDamagedTime && static_cast<uint32_t>(CTimer::GetTimeMS()-ped->m_nLastDamagedTime)<2000)return finish("recent_damage");
    auto& m=ped->m_pIntelligence->m_TaskMgr;
    if(m.m_aPrimaryTasks[TASK_PRIMARY_PHYSICAL_RESPONSE])return finish("physical_response");
    if(m.m_aPrimaryTasks[TASK_PRIMARY_PRIMARY])return finish("primary_task");
    if(m.m_aPrimaryTasks[TASK_PRIMARY_EVENT_RESPONSE_NONTEMP])return finish("nontemporary_event");
    if(m.m_aPrimaryTasks[TASK_PRIMARY_EVENT_RESPONSE_TEMP] && !impl_->Own(reinterpret_cast<uintptr_t>(m.m_aPrimaryTasks[TASK_PRIMARY_EVENT_RESPONSE_TEMP]),t))return finish("foreign_temporary_event");
    if(m.m_aSecondaryTasks[TASK_SECONDARY_ATTACK])return finish("secondary_attack");
    c.eligible=true;return finish("eligible");
}
void EngineActions::Stop(const Target& t){StopAction(t,Action::None);}
void EngineActions::StopAction(const Target& t,Action action){
    // Dtor notifications invalidate ownership before any address can be reused.
    std::vector<std::pair<uintptr_t,Impl::Owner>> owned;
    for(const auto& p:impl_->tasks)if(p.second.target.id==t.id && (action==Action::None || p.second.action==action))owned.push_back(p);
    for(auto it=impl_->ended.begin();it!=impl_->ended.end();)if(it->first.first==t.id && (action==Action::None || it->first.second==action))it=impl_->ended.erase(it);else ++it;
    if(owned.empty())return;
    auto* ped=impl_->resolve(t,true);
    if(!ped || !ped->m_pIntelligence){for(const auto& p:owned)impl_->tasks.erase(p.first);return;}
    auto& m=ped->m_pIntelligence->m_TaskMgr;
    for(const auto& p:owned){
        if(!impl_->Own(p.first,t))continue;
        auto* current=p.second.secondary?m.m_aSecondaryTasks[p.second.slot]:m.m_aPrimaryTasks[p.second.slot];
        if(reinterpret_cast<uintptr_t>(current)!=p.first){impl_->tasks.erase(p.first);continue;}
        impl_->tasks.at(p.first).stopping=true;
        if(impl_->debug)logger->Info("Animation owned release requested action=%s",ActionName(p.second.action));
        if(p.second.secondary){if(CTaskManager::FuncProxy_SetTaskSecondary)m.SetTaskSecondary(nullptr,p.second.slot);}
        else if(CTaskManager::FuncProxy_SetTask)m.SetTask(nullptr,p.second.slot,true);
        impl_->tasks.erase(p.first);
    }
}
bool EngineActions::Start(const Target& t,Action a,int duration){
    auto c=Inspect(t);if(!c.valid || !c.eligible || c.paused)return false;
    impl_->ended.erase({t.id,a});
    auto* ped=impl_->resolve(t,false);auto* player=Player();if(!ped || !player)return false;
    auto& m=ped->m_pIntelligence->m_TaskMgr;
    for(auto it=impl_->tasks.begin();it!=impl_->tasks.end();){
        const auto& o=it->second;
        if(o.target.id!=t.id){++it;continue;}
        auto* current=o.secondary?m.m_aSecondaryTasks[o.slot]:m.m_aPrimaryTasks[o.slot];
        if(reinterpret_cast<uintptr_t>(current)!=it->first){it=impl_->tasks.erase(it);continue;}
        if(o.action==a){
            if(a==Action::HandsUp && o.observed && static_cast<uint32_t>(CTimer::GetTimeMS()-o.started)>=55000){
                // Refresh only the owned task's documented Android timer. Do not replay the clip.
                auto* held=reinterpret_cast<CTaskSimpleRunTimedAnim*>(it->first);
                held->m_iDuration=60000;held->m_timer=CTaskTimer(static_cast<int>(CTimer::GetTimeMS()),60000);
                it->second.started=CTimer::GetTimeMS();
                if(impl_->debug)logger->Info("Animation hold timer renewed without restarting clip");
            }
            return true;
        }
        ++it;
    }
    const bool stationaryAction=a==Action::TalkGesture || a==Action::HandsUp || a==Action::Angry || a==Action::Refuse || a==Action::Cower;
    if(stationaryAction && !Start(t,Action::Idle,duration))return false;
    const bool primary=a==Action::Idle || a==Action::FacePlayer || a==Action::FollowPlayer || a==Action::Flee;
    if(primary){
        // Primary orientation may coexist with the owned secondary pose/gesture.
        if(a==Action::Idle){for(const auto& pair:impl_->tasks)if(pair.second.target.id==t.id && pair.second.action==Action::FacePlayer)return true;}
        for(auto old:{Action::Idle,Action::FacePlayer,Action::FollowPlayer,Action::Flee})StopAction(t,old);
    }else{
        for(auto old:{Action::TalkGesture,Action::HandsUp,Action::Angry,Action::Refuse,Action::Cower})StopAction(t,old);
    }
    CTask* task=nullptr;bool secondary=false,animation=false;int slot=TASK_PRIMARY_EVENT_RESPONSE_TEMP;
    auto goal=player->GetPosition();duration=std::clamp(duration,100,60000);
    switch(a){
    case Action::FacePlayer:
        if(c.facing)return true;
        if(CTaskComplexTurnToFaceEntityOrCoord::FuncProxy_ctorCTaskComplexTurnToFaceEntityOrCoord1 && CTaskComplexTurnToFaceEntityOrCoord::FuncProxy_opnewCTaskComplexTurnToFaceEntityOrCoord)
            task=new CTaskComplexTurnToFaceEntityOrCoord(goal,.5f,.12f);
        else if(CTaskSimpleAchieveHeading::FuncProxy_ctorCTaskSimpleAchieveHeading && CTaskSimpleAchieveHeading::FuncProxy_opnewCTaskSimpleAchieveHeading){
            auto d=goal-ped->GetPosition();task=new CTaskSimpleAchieveHeading(std::atan2(-d.x,d.y),.5f,.12f);
        }
        break;
    case Action::Idle:
        if(CTaskSimpleStandStill::FuncProxy_ctorCTaskSimpleStandStill && CTaskSimpleStandStill::FuncProxy_opnewCTaskSimpleStandStill)task=new CTaskSimpleStandStill(duration,true,false,2.f);break;
    case Action::Flee:
        if(CTaskComplexSmartFleePoint::FuncProxy_ctorCTaskComplexSmartFleePoint && CTaskComplexSmartFleePoint::FuncProxy_opnewCTaskComplexSmartFleePoint)
            task=new CTaskComplexSmartFleePoint(goal,false,12.f,duration);
        break;
    case Action::FollowPlayer:
        if(CTaskSimpleGoToPoint::FuncProxy_ctorCTaskSimpleGoToPoint && CTaskSimpleGoToPoint::FuncProxy_opnewCTaskSimpleGoToPoint)
            task=new CTaskSimpleGoToPoint(Distance(ped,player)>2.4f?PEDMOVE_RUN:PEDMOVE_WALK,goal,1.2f,true,false);
        break;
    case Action::Cower:
        secondary=true;slot=TASK_SECONDARY_DUCK;
        if(!m.m_aSecondaryTasks[slot] && CTaskSimpleDuck::FuncProxy_ctorCTaskSimpleDuck && CTaskSimpleDuck::FuncProxy_opnewCTaskSimpleDuck && CTaskSimpleDuck::CanPedDuck && CTaskSimpleDuck::CanPedDuck(ped))
            task=new CTaskSimpleDuck(DUCK_TASK_CONTROLLED,static_cast<u16>(duration),-1);
        break;
    case Action::TalkGesture:case Action::HandsUp:case Action::Angry:case Action::Refuse:{
        secondary=true;animation=true;slot=TASK_SECONDARY_PARTIAL_ANIM;
        if(m.m_aSecondaryTasks[slot]){logger->Info("Action start rejected reason=foreign_partial_animation action=%s",ActionName(a));return false;}
        if(impl_->gestureSequence.size()>=32 && !impl_->gestureSequence.count(t.id))impl_->gestureSequence.erase(impl_->gestureSequence.begin());
        const bool alternate=(impl_->gestureSequence[t.id]++%2)!=0;
        AnimationId anim=a==Action::HandsUp?ANIM_STD_HANDSUP:a==Action::Angry?ANIM_STD_PARTIAL_FUCKU:a==Action::Refuse || alternate?ANIM_STD_ENDCHAT2:ANIM_STD_CHAT;
        if(CTaskSimpleRunTimedAnim::FuncProxy_ctorCTaskSimpleRunTimedAnim && CTaskSimpleRunTimedAnim::FuncProxy_opnewCTaskSimpleRunTimedAnim)
            task=new CTaskSimpleRunTimedAnim(ANIM_GROUP_DEFAULT,anim,2.f,2.f,a==Action::HandsUp?60000:duration,a==Action::HandsUp);
        break;}
    case Action::StopFollowing:Stop(t);return true;
    default:return false;
    }
    if(!task){logger->Info("Action start rejected reason=constructor_or_slot_unavailable action=%s",ActionName(a));return false;}
    if((secondary && !CTaskManager::FuncProxy_SetTaskSecondary) || (!secondary && !CTaskManager::FuncProxy_SetTask)){
        if(CTask::FuncProxy_dtorCTask && CTask::FuncProxy_opdelCTask)delete task;return false;
    }
    const auto address=reinterpret_cast<uintptr_t>(task);
    impl_->tasks.emplace(address,Impl::Owner{t,a,slot,secondary,animation,{goal.x,goal.y,goal.z},Distance(ped,player),CTimer::GetTimeMS()});
    if(secondary)m.SetTaskSecondary(task,slot);else m.SetTask(task,slot,false);
    logger->Info("Action native task=%s queued",ActionName(a));
    if(impl_->debug)logger->Info("Animation start action=%s duration=%d slot=%d secondary=%d hold_last=%d",ActionName(a),duration,slot,secondary,a==Action::HandsUp);
    return impl_->tasks.count(address)!=0;
}
Effect EngineActions::Poll(const Target& t,Action a){
    auto c=Inspect(t);if(!c.valid || !c.eligible)return Effect::Failed;if(c.paused)return Effect::Pending;
    auto* ped=impl_->resolve(t,false);auto* player=Player();if(!ped || !player)return Effect::Failed;
    if(a==Action::FacePlayer)return c.facing?Effect::Applied:Effect::Pending;
    auto found=std::find_if(impl_->tasks.begin(),impl_->tasks.end(),[&](const auto& p){return p.second.target.id==t.id && p.second.action==a;});
    if(a==Action::FollowPlayer){
        float distance=Distance(ped,player);
        if(found==impl_->tasks.end())return Start(t,a,1000)?(distance<1.8f?Effect::Applied:Effect::Pending):Effect::Failed;
        const auto owner=found->second;auto position=player->GetPosition();
        float moved=(position.x-owner.goal.x)*(position.x-owner.goal.x)+(position.y-owner.goal.y)*(position.y-owner.goal.y);
        if(moved>.56f && static_cast<uint32_t>(CTimer::GetTimeMS()-owner.started)>500){Stop(t);if(!Start(t,a,1000))return Effect::Failed;}
        return distance<1.8f || distance<owner.initialDistance-.2f?Effect::Applied:Effect::Pending;
    }
    if(found==impl_->tasks.end()){
        auto ended=impl_->ended.find({t.id,a});
        if(ended!=impl_->ended.end() && ended->second==t.ref){impl_->ended.erase(ended);return Effect::Finished;}
        logger->Info("Action effect failed reason=task_gone action=%s",ActionName(a));return Effect::Failed;
    }
    auto& manager=ped->m_pIntelligence->m_TaskMgr;
    const auto& recorded=found->second;
    const auto* current=recorded.secondary?manager.m_aSecondaryTasks[recorded.slot]:manager.m_aPrimaryTasks[recorded.slot];
    if(reinterpret_cast<uintptr_t>(current)!=found->first){impl_->tasks.erase(found);return Effect::Failed;}
    auto& owner=found->second;
    if(a==Action::Idle)return c.stationary?Effect::Applied:Effect::Pending;
    if(a==Action::Flee)return Distance(ped,player)>owner.initialDistance+.25f?Effect::Applied:Effect::Pending;
    if(a==Action::Cower)return ped->bIsDucking?Effect::Applied:Effect::Pending;
    if(owner.animation){
        // Base task destructor invalidates the entry before its address can be reused.
        auto* anim=reinterpret_cast<CTaskSimpleAnim*>(found->first);
        if(anim->m_pAnim){
            const float time=anim->m_pAnim->m_fCurrentTime;
            const bool progressed=time>.001f || std::abs(time-owner.lastTime)>.001f;
            owner.lastTime=time;
            if(anim->m_pAnim->m_fBlendAmount>.05f && progressed && !owner.observed){
                owner.observed=true;
                if(impl_->debug)logger->Info("Animation effect observed action=%s time=%.3f blend=%.3f",ActionName(a),time,anim->m_pAnim->m_fBlendAmount);
            }
            if(owner.observed && a!=Action::HandsUp && anim->m_bIsFinished)return Effect::Finished;
            if(owner.observed && anim->m_pAnim->m_fBlendAmount>.05f)return Effect::Applied;
        }
        return static_cast<uint32_t>(CTimer::GetTimeMS()-owner.started)>2000?Effect::Failed:Effect::Pending;
    }
    return Effect::Failed;
}
bool EngineActions::PrepareMoney(const Target& t){
    auto c=Inspect(t);if(!c.valid || !c.eligible || c.paused || !CPickups::GenerateNewOne || !CPickups::RemovePickUp ||
       !CPickups::GetActualPickupIndex || !CPickups::GetUniquePickupIndex || !CPickups::aPickUps || !CModelInfo::GetModelInfo1 || !CWorld::FindGroundZFor3DCoord)return false;
    int model=-1;auto* info=CModelInfo::GetModelInfo("money",&model);
    if(!info || model<0)return false;
    if(!info->m_pRwObject){if(CStreaming::RequestModel)CStreaming::RequestModel(model,0);return false;}
    return true;
}
int EngineActions::SpawnMoney(const Target& t,int amount,int ordinal){
    if(amount<1 || amount>10000 || !PrepareMoney(t))return -1;
    auto* ped=impl_->resolve(t,false);if(!ped)return -1;
    int model=-1;auto* info=CModelInfo::GetModelInfo("money",&model);if(!info || model<0)return -1;
    auto pos=ped->GetPosition();
    const float angle=static_cast<float>(ordinal)*1.256637f;
    pos.x+=.22f*std::cos(angle);pos.y+=.22f*std::sin(angle);
    bool ground=false;CEntity* entity=nullptr;
    float z=CWorld::FindGroundZFor3DCoord(pos.x,pos.y,pos.z+1,&ground,&entity);
    if(!ground || !std::isfinite(z) || std::abs(z-pos.z)>5)return -1;
    pos.z=z+.12f;
    int handle=CPickups::GenerateNewOne(pos,static_cast<u32>(model),PICKUP_MONEY,static_cast<u32>(amount),0,false,nullptr);
    if(handle<0)return -1;
    impl_->money[handle]={model,amount};
    if(!PickupAlive(handle)){
        int index=CPickups::GetActualPickupIndex(handle);
        if(index>=0 && CPickups::GetUniquePickupIndex(index)==handle)CPickups::RemovePickUp(handle);
        impl_->money.erase(handle);return -1;
    }
    return handle;
}
bool EngineActions::PickupAlive(int handle){
    auto saved=impl_->money.find(handle);if(saved==impl_->money.end() || !CPickups::GetActualPickupIndex || !CPickups::GetUniquePickupIndex)return false;
    int index=CPickups::GetActualPickupIndex(handle);
    if(index<0 || CPickups::GetUniquePickupIndex(index)!=handle){impl_->money.erase(saved);return false;}
    const auto& pickup=CPickups::aPickUps[index];
    if(pickup.m_nPickupType!=PICKUP_MONEY || pickup.m_nModelIndex!=saved->second.model || pickup.m_nAmmo!=saved->second.amount){impl_->money.erase(saved);return false;}
    return true;
}
void EngineActions::RemovePickup(int handle){if(PickupAlive(handle) && CPickups::RemovePickUp)CPickups::RemovePickUp(handle);impl_->money.erase(handle);}
}
