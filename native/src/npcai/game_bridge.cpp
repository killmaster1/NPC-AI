#include "game_bridge.h"
#include "registry.h"
#include "native_keyboard.h"
#include "bubble_layout.h"
#include "focus_lease.h"
#include "engine_actions.h"
#include "world_bridge.h"
#include "tts.h"
#include <chrono>
#include <plugin-sdk/game_sa/ai/tasks/TaskTypes.h>
#include <plugin-sdk/game_sa/enum/AnimGroup.h>
#include <plugin-sdk/game_sa/engine/RsGlobal.h>
#include <plugin-sdk/game_sa/base/Timer.h>
#include <mod/logger.h>
#include <plugin-sdk/game_sa/Events.h>
#include <plugin-sdk/game_sa/entity/PlayerPed.h>
#include <plugin-sdk/game_sa/entity/Vehicle.h>
#include <plugin-sdk/game_sa/engine/ModelInfo.h>
#include <plugin-sdk/game_sa/other/Pools.h>
#include <plugin-sdk/game_sa/other/Clock.h>
#include <plugin-sdk/game_sa/engine/Pad.h>
#include <plugin-sdk/game_sa/engine/Sprite.h>
#include <plugin-sdk/game_sa/engine/OS.h>
#include <plugin-sdk/game_sa/engine/Zones.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <set>

#if !defined(__aarch64__)
#error O GameBridge só foi preparado para GTA SA 2.10 ARM64.
#endif

namespace npcai::GameBridge {
namespace {
Chat* chat=nullptr;
std::shared_ptr<IntelligenceRuntime> intelligence;
uint64_t nextRegistryScan=0;
std::unique_ptr<Registry> registry;
std::shared_ptr<VoiceManager> voice;
std::atomic_bool blocked{false};
bool running=false, opened=false, switchNext=false;
float radius=3;
Settings options;
FocusLease focus;
std::unique_ptr<EngineActions> actionEngine;
std::unique_ptr<NpcActionController> actions;
std::string actionConversation;
uint64_t nextGesture=0;
std::mutex commandMutex;
std::set<std::string> pendingDetach;
uint64_t Now(){return CTimer::GetTimeMS();}
void ReleaseFocus(){if(!focus.Id().empty()){std::lock_guard<std::mutex> lock(commandMutex);pendingDetach.insert(focus.Id());}focus.Release();nextGesture=0;if(intelligence)intelligence->Flush();}

uintptr_t poolStorage=0;
std::string selected;
int selectedSlot=-1;
std::optional<Vec3> anchor;
const char* status="Aguardando CJ e pool de pedestres";
void (*originalUpdatePads)()=nullptr;

CPedPool* Pool() {
    // .got 0x84B968 provém do aml-psdk SA 2.10 ARM64, nunca da versão PC.
    // O main valida versão do APK e tamanho da biblioteca antes de qualquer leitura.
    CPedPool* pool=CPools::ms_pPedPool;
    if(!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize<=0 || pool->m_nSize>4096) return nullptr;
    return pool;
}
int Slot(CPed* p) {
    auto* pool=Pool();if(!pool || !p) return -1;
    auto begin=reinterpret_cast<uintptr_t>(pool->m_pObjects), address=reinterpret_cast<uintptr_t>(p);
    auto bytes=static_cast<size_t>(pool->m_nSize)*sizeof(*pool->m_pObjects);
    if(address<begin || address-begin>=bytes || (address-begin)%sizeof(*pool->m_pObjects)) return -1;
    return static_cast<int>((address-begin)/sizeof(*pool->m_pObjects));
}
CPed* ActionPed(const Target& t,bool cleanup){
    auto* pool=Pool();if(!pool || t.ref<0)return nullptr;
    int slot=t.ref>>8;if(slot<0 || slot>=pool->m_nSize || pool->IsFreeSlotAtIndex(slot) || pool->m_byteMap[slot].IntValue()!=(t.ref&255))return nullptr;
    auto* ped=CPools::GetPed(t.ref);if(!ped || Slot(ped)!=slot || CPools::GetPedRef(ped)!=t.ref)return nullptr;
    if(cleanup)return ped;
    auto currentPosition=ped->GetPosition();if(!std::isfinite(currentPosition.x) || !std::isfinite(currentPosition.y) || !std::isfinite(currentPosition.z))return nullptr;
    if(!registry || !registry->Contains(t.id,t.ref) || ped->m_nModelIndex!=t.context.model || !ped->m_pRwObject || ped->bRemoveFromWorld || ped->m_fHealth<=0)return nullptr;
    auto* player=FindPlayerPed(-1);int playerSlot=Slot(player);
    if(playerSlot<0 || pool->IsFreeSlotAtIndex(playerSlot) || player->m_fHealth<=0 || ped->m_nAreaCode!=player->m_nAreaCode)return nullptr;
    return ped;
}
void Invalidate(CPed* p,bool dying=false) {
    if(!running || !registry)return;
    int slot=Slot(p);if(slot<0)return;
    if(dying && actionEngine)actionEngine->ForgetPed(slot);
    const auto id=registry->IdAt(slot);
    if(actions && !id.empty())actions->Cancel(id,"NPC removido; ação cancelada.",true);
    if(actionEngine)actionEngine->ForgetPed(slot);
    registry->Destroy(slot);
    if(slot==selectedSlot){ReleaseFocus();NativeKeyboard::Invalidate();selected.clear();selectedSlot=-1;anchor.reset();opened=false;blocked.store(false);if(chat)chat->Select({});}
}
void Reset() {
    if(voice && !selected.empty())voice->Cancel();
    if(!registry) return;
    ReleaseFocus();NativeKeyboard::Invalidate();if(actions)actions->Reset();actionConversation.clear();
    {std::lock_guard<std::mutex> lock(commandMutex);pendingDetach.clear();}
    if(intelligence)WorldBridge::Boundary();nextRegistryScan=0;
    registry->Reset();if(voice)voice->Retain({});poolStorage=0;selected.clear();selectedSlot=-1;anchor.reset();opened=false;blocked.store(false);
    if(chat) {chat->Select({});chat->Retain({});}
}
void OnCtor(CPed* p){Invalidate(p,true);if(CPools::GetPedRef && p)WorldBridge::PedBorn(CPools::GetPedRef(p));} // garante serial novo mesmo após wrap dos 7 bits do pool.
void OnDtor(CPed* p){Invalidate(p,true);}
void OnRemove(CEntity* e){if(running && e && e->IsPed()) Invalidate(static_cast<CPed*>(e));}
void OnPools(){if(voice)voice->Cancel();Reset();}
void OnBackground(OSEventType type,void*) {
    if(type==OSET_Pause || type==OSET_Resume || type==OSET_SystemUIShowing || type==OSET_SystemUIClosed)
        logger->Info("Android OS event=%d editor=%d",static_cast<int>(type),NativeKeyboard::Active());
    if(type==OSET_Pause || type==OSET_RequestExit) {
        if(intelligence)intelligence->Flush();
        if(voice)voice->Cancel();
        const bool deferred=type==OSET_Pause && NativeKeyboard::Active() && !focus.Id().empty();
        opened=false;blocked.store(false);NativeKeyboard::Invalidate();
        if(!deferred){ReleaseFocus();if(chat)chat->Select({});}
        else logger->Info("Orientation retained until simulation resumes");
    }
}
std::optional<Vec3> Anchor(CPed* ped) {
    const auto pos=ped->GetPosition();
    Vec3 result{pos.x,pos.y,pos.z+options.bubbleHeight};
    bool fromBounds=false;
    if(CEntity::FuncProxy_GetColModel) {
        const auto& box=ped->GetColModel().m_boundBox;
        const auto& lo=box.m_vecMin;const auto& hi=box.m_vecMax;
        bool valid=true;for(float n:{lo.x,lo.y,lo.z,hi.x,hi.y,hi.z})valid=valid && std::isfinite(n);
        if(valid && lo.x<=hi.x && lo.y<=hi.y && lo.z<hi.z && hi.z-lo.z>=.2f && hi.z-lo.z<=6.f && std::abs(hi.z)<4.f) {
            CVector top{(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,hi.z};
            auto* matrix=ped->GetMatrix();
            CVector world=matrix?matrix->TransformPoint(top):CVector{pos.x,pos.y,pos.z+hi.z};
            const float delta=options.bubbleHeight-1.75f;
            const float dx=world.x-pos.x,dy=world.y-pos.y,dz=world.z-pos.z;
            if(std::isfinite(world.x) && std::isfinite(world.y) && std::isfinite(world.z) && dx*dx+dy*dy+dz*dz<36.f)
                {result={world.x,world.y,world.z+delta};fromBounds=true;}
        }
    }
    static int model=-1;static bool source=false;
    if(model!=ped->m_nModelIndex || source!=fromBounds){model=ped->m_nModelIndex;source=fromBounds;
        logger->Info("Bubble anchor model=%d source=%s above_origin=%.2f",model,fromBounds?"bounds":"fallback",result.z-pos.z);}
    return result;
}
void FilterPad() {
    if(!running || !blocked.load() || !CPad::GetPad || !CPad::FuncProxy_Clear) return;
    if(auto* p=CPad::GetPad(0)) p->Clear(false,false);
}
void UpdatePads(){originalUpdatePads();FilterPad();}
void Tick() {
    if(!running || !chat || !registry) return;
    WorldBridge::GameThread();
    FilterPad();
    const auto snapshot=chat->Get();
    const auto previousFocus=focus.Id();
    if(voice && snapshot.target && voice->BusyFor(snapshot.target->id))focus.Sent(Now());
    focus.Tick(Now(),opened,snapshot.waiting);
    if(actions && !previousFocus.empty() && focus.Id().empty())actions->Detach(previousFocus);
    static int lastPause=-1;
    const int pause=(CTimer::m_UserPause?1:0)|(CTimer::m_CodePause?2:0)|(blocked.load()?4:0);
    if(pause!=lastPause){lastPause=pause;logger->Info("Game state userPause=%d codePause=%d inputSuppressed=%d",pause&1,(pause>>1)&1,(pause>>2)&1);}
    auto* pool=Pool();
    if(!pool || !FindPlayerPed) {Reset();status="Pool indisponível";return;}
    auto storage=reinterpret_cast<uintptr_t>(pool->m_pObjects);
    if(poolStorage!=storage) {Reset();poolStorage=storage;}
    CPlayerPed* player=FindPlayerPed(-1);
    int playerSlot=Slot(player);
    if(playerSlot<0 || pool->IsFreeSlotAtIndex(playerSlot) || CPools::GetPed(pool->GetRef(player))!=player) {
        Reset();status="CJ ainda não foi carregado";return;
    }
    const auto p=player->GetPosition();
    if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(player->m_fHealth) || player->m_fHealth<=0) {
        Reset();status="CJ indisponível";return;
    }
    bool registryScanned=false;
    if(RealMilliseconds()>=nextRegistryScan){registryScanned=true;
    nextRegistryScan=RealMilliseconds()+static_cast<uint64_t>(options.worldIntervalMs);
    registry->Begin();
    std::set<const CPed*> observedTaxiDrivers;
    static int taxiModel=-1,cabbieModel=-1;
    if(CModelInfo::GetModelInfo1){if(taxiModel<0)CModelInfo::GetModelInfo1("taxi",&taxiModel);if(cabbieModel<0)CModelInfo::GetModelInfo1("cabbie",&cabbieModel);}
    auto* vehicles=CPools::ms_pVehiclePool;
    if(vehicles && vehicles->m_byteMap && CPools::GetVehicle && CPools::GetVehicleRef && vehicles->m_nSize>0 && vehicles->m_nSize<=4096){
        for(int index=0;index<vehicles->m_nSize;++index){
            if(vehicles->IsFreeSlotAtIndex(index))continue;
            int vehicleRef=(index<<8)|vehicles->m_byteMap[index].IntValue();auto* v=CPools::GetVehicle(vehicleRef);
            if(v && CPools::GetVehicleRef(v)==vehicleRef && !v->bRemoveFromWorld && v->m_pRwObject && ((taxiModel>=0 && v->m_nModelIndex==taxiModel) || (cabbieModel>=0 && v->m_nModelIndex==cabbieModel)) && v->m_pDriver)observedTaxiDrivers.insert(v->m_pDriver);
        }
    }
    for(int i=0;i<pool->m_nSize;++i) {
        if(pool->IsFreeSlotAtIndex(i)) continue;
        // Resolver pela função nativa evita supor stride para buscar o pedestre.
        int ref=(i<<8)|pool->m_byteMap[i].IntValue();
        CPed* ped=CPools::GetPed(ref);
        if(!ped || ped==player || Slot(ped)!=i || CPools::GetPedRef(ped)!=ref) continue;
        if(!ped->IsPed() || ped->bRemoveFromWorld || !ped->m_pRwObject ||
           ped->m_nPedType<=static_cast<int>(PED_TYPE_PLAYER_UNUSED) || ped->m_nModelIndex<0) continue;
        const auto pos=ped->GetPosition();
        Observation o;o.slot=i;o.ref=ref;o.alive=std::isfinite(ped->m_fHealth) && ped->m_fHealth>0 &&
            ped->m_ePedState!=PEDSTATE_DIE && ped->m_ePedState!=PEDSTATE_DEAD && ped->m_ePedState!=PEDSTATE_DIE_BY_STEALTH;
        o.position={pos.x,pos.y,pos.z};
        if(ped->m_fHealth<=0)WorldBridge::Dead(ref,{pos.x,pos.y,pos.z});
        o.context.model=ped->m_nModelIndex;o.context.pedType=ped->m_nPedType;
        o.context.pedState=static_cast<int>(ped->m_ePedState);o.context.area=ped->m_nAreaCode;
        o.context.observedTaxiDriver=observedTaxiDrivers.count(ped)>0;o.context.inVehicle=ped->bInVehicle;o.context.personalityVariation=options.personalityVariation;
        o.context.hour=CClock::ms_nGameClockHours;o.context.minute=CClock::ms_nGameClockMinutes;
        if(CTheZones::GetLevelFromPosition) o.context.regionLevel=static_cast<int>(CTheZones::GetLevelFromPosition(&pos));
        registry->Observe(o);
    }
    registry->End();
    }
    Vec3 position{p.x,p.y,p.z};
    const float resolveRadius=voice && voice->BusyFor(selected)?std::max(radius,options.ttsMaxDistance):radius;
    auto t=registry->Resolve(selected,position,player->m_nAreaCode,resolveRadius);
    if(switchNext) {t=registry->Nearest(position,player->m_nAreaCode,radius,selected);switchNext=false;}
    if(!opened && focus.Id().empty()) t=registry->Nearest(position,player->m_nAreaCode,radius);
    if(t && !ActionPed(*t,false))t.reset();
    if(opened && !t) {opened=false;blocked.store(false);status="NPC saiu do alcance ou foi removido";}
    else status=t?"NPC disponível":"Aproxime CJ de um NPC vivo (3 m por padrão)";
    if(opened && t && focus.Id()!=t->id) focus.Begin(t->id,Now());
    anchor.reset();
    if(t) {
        auto* candidate=CPools::GetPed(t->ref);
        if(candidate && Slot(candidate)==(t->ref>>8) && CPools::GetPedRef(candidate)==t->ref) {
            anchor=Anchor(candidate);

        }
    } else {ReleaseFocus();NativeKeyboard::Invalidate();}
    selected=t?t->id:"";selectedSlot=t?(t->ref>>8):-1;
    if(registryScanned){auto liveIds=registry->Ids();chat->Retain(liveIds);if(voice)voice->Retain(liveIds);}
    chat->Select(std::move(t));
    const auto latest=chat->Get();
    if(voice)voice->Bind(latest.target?latest.target->id:"",latest.conversationId,latest.target?latest.target->distance:0,latest.target && ActionPed(*latest.target,true),(pause&3)!=0,latest.target?latest.target->context:Context{});
    if(voice){float pan=0; // Safe camera-relative approximation from the existing verified projection API.
        if(anchor && CSprite::CalcScreenCoors && RsGlobal.maximumWidth>0){RwV3d point{anchor->x,anchor->y,anchor->z},projected{};float sx=0,sy=0;
            if(CSprite::CalcScreenCoors(point,&projected,&sx,&sy,true,true) && projected.z>0 && std::isfinite(projected.x))pan=std::clamp(projected.x*2.f/RsGlobal.maximumWidth-1.f,-1.f,1.f);
        }
        voice->Spatial(pan,latest.target?latest.target->distance:options.ttsMaxDistance);
    }
    if(intelligence)WorldBridge::Tick(*registry,player,selected);
    if(actions){
        std::set<std::string> detached;
        {std::lock_guard<std::mutex> lock(commandMutex);detached.swap(pendingDetach);}
        for(const auto& id:detached)actions->Detach(id);
        if(latest.target && focus.Id()==latest.target->id){
            if(actionConversation!=latest.conversationId){actions->Detach(latest.target->id);actionConversation=latest.conversationId;}
            actions->Begin(*latest.target,Now());
        }
        if(intelligence){
            while(auto reaction=intelligence->TakeReply()){
                if(!latest.target || reaction->npc!=latest.target->id || latest.waiting || opened)continue;
                if(focus.Id().empty()){focus.Begin(latest.target->id,Now());actionConversation=latest.conversationId;}
                chat->LocalReply(*latest.target,reaction->dialogue);
                if(reaction->action==Action::Flee && options.aiActions){Decision d;d.action=Action::Flee;d.dialogue=reaction->dialogue;d.requestId="local-"+std::to_string(RealMilliseconds());actions->Submit({*latest.target,latest.conversationId,d,true},Now());}
                if(reaction->aiOpportunity && chat->React("Comente brevemente o acontecimento percebido mais recente. Isto não é uma fala do jogador."))focus.Sent(Now());
            }
        }
        if(auto proposal=chat->TakeAction()){
            if(latest.target && focus.Id()==latest.target->id && proposal->target.id==latest.target->id && proposal->conversationId==latest.conversationId)actions->Submit(std::move(*proposal),Now());
            else chat->ReportAction(*proposal,false,"Ação descartada: o NPC ou a conversa mudou.");
        }
        while(auto speech=chat->TakeSpeech()){
            if(latest.target && focus.Id()==latest.target->id && speech->target.id==latest.target->id && speech->conversationId==latest.conversationId){
                const bool queued=voice && voice->Speak(speech->target.id,speech->conversationId,speech->text,false,speech->target.context,speech->serial);
                if(!queued || !options.ttsSync)actions->Speak(speech->target,speech->text,Now());
            }
        }
        if(voice){
            while(auto event=voice->TakeEvent())if(options.ttsSync && latest.target && event->npc==latest.target->id && event->conversation==latest.conversationId){if(event->failed)actions->Speak(*latest.target,event->text,Now());else actions->VoicePlayback(*latest.target,event->started,Now());}
            const auto state=voice->Status();if(options.ttsSync && state.playing && latest.target && voice->BusyFor(latest.target->id))actions->VoicePlayback(*latest.target,true,Now());
        }
        actions->Tick(Now());
        std::map<std::string,std::string> observed;
        const auto states=actions->States();
        static std::map<std::string,InteractionState> previousStates;
        for(const auto& item:states){
            const auto state=item.second;
            observed[item.first]=state==InteractionState::SURRENDERING?"surrendering":state==InteractionState::FOLLOWING_PLAYER?"following":state==InteractionState::FLEEING?"fleeing":"conversing";
            if(options.animationDebug && (!previousStates.count(item.first) || previousStates[item.first]!=state))logger->Info("Animation state=%d",static_cast<int>(state));
        }
        previousStates=states;registry->Behaviors(observed);
    }
}
}
void AttachVoice(std::shared_ptr<VoiceManager> instance){voice=std::move(instance);}
void AttachRuntime(std::shared_ptr<IntelligenceRuntime> runtime){intelligence=std::move(runtime);}
bool Init(Chat& instance,const Settings& settings) {
    if(running) return true;
    const auto session=SessionId();if(session.empty()) {logger->Error("Falha de entropia; ponte desativada");return false;}
    if(!FindPlayerPed || !CPools::GetPed || !CPools::GetPedRef || !CPad::GetPad || !CPad::FuncProxy_Clear) {
        logger->Error("Símbolos de jogador, pool ou controles ausentes");return false;
    }
    chat=&instance;chat->ConfigureDialogue(settings);options=settings;if(intelligence)WorldBridge::Init(intelligence,settings);radius=settings.radius;registry=std::make_unique<Registry>(session);
    actionEngine=std::make_unique<EngineActions>(ActionPed,settings.animationDebug);actionEngine->Init();
    actions=std::make_unique<NpcActionController>(*actionEngine,settings,[](const ActionRequest& request,bool ok,const std::string& text,int amount){
        if(chat)chat->ReportAction(request,ok,text,amount);
        if(ok && registry)registry->ActionConfirmed(request.target.id,request.decision.action);
        if(intelligence)intelligence->Confirmed(request.target.id,request.decision.action,ok,amount);
        logger->Info("Action result=%s action=%s amount=%d",ok?"confirmed":"failed",ActionName(request.decision.action),amount);
        // text is an internal fixed controller diagnostic on failure, never the user/model text.
        if(!ok)logger->Info("Action failure reason=%s",text.c_str());
    });
    Events::pedCtorEvent += OnCtor;Events::pedDtorEvent += OnDtor;
    Events::entityRemoveEvent += OnRemove;
    Events::initPoolsEvent += OnPools;Events::shutdownPoolsEvent.before += OnPools;
    Events::restartGameEvent.before += Reset;
    Events::gameProcessEvent.before += Tick;
    Events::applicationEvent += OnBackground;
    if(!Events::pedCtorEvent.m_pOriginalFunc || !Events::pedDtorEvent.m_pOriginalFunc ||
       !Events::initPoolsEvent.m_pOriginalFunc || !Events::shutdownPoolsEvent.m_pOriginalFunc ||
       !Events::restartGameEvent.m_pOriginalFunc || !Events::gameProcessEvent.m_pOriginalFunc ||
       !Events::entityRemoveEvent.m_pOriginalFunc) {
        logger->Error("Hook de ciclo de vida falhou; seleção desativada");return false;
    }
    auto address=aml->GetSym(GetMainLibrary(),"_ZN4CPad10UpdatePadsEv");
    if(!address || !aml->Hook(reinterpret_cast<void*>(address),reinterpret_cast<void*>(&UpdatePads),
                             reinterpret_cast<void**>(&originalUpdatePads))) {
        logger->Error("Hook dos controles falhou; chat desativado");return false;
    }
    running=true;
    logger->Info("NPC posture stand_symbol=%d gesture_symbol=%d",CTaskSimpleStandStill::FuncProxy_ctorCTaskSimpleStandStill!=nullptr,
                 CTaskSimpleRunTimedAnim::FuncProxy_ctorCTaskSimpleRunTimedAnim!=nullptr);
    logger->Info("GameBridge 2.10 ARM64 pronto; raio=%.1f; identidade efêmera por ciclo de vida",radius);
    return true;
}
void Open(bool value){
    opened=value && running && chat && chat->Get().target.has_value();blocked.store(opened);
    if(opened){focus.Begin(selected,Now());logger->Info("Conversation opened; orientation queued for game tick; controls suppressed");}
    else {if(voice)voice->Cancel();ReleaseFocus();logger->Info("Conversation closed; controls released");}
}
void Sent(){opened=false;blocked.store(false);focus.Sent(Now());logger->Info("Message queued to worker; controls released; NPC focus lease retained");}
void Switch(){if(voice)voice->Cancel();ReleaseFocus();NativeKeyboard::Invalidate();switchNext=true;}
bool IsOpen(){return opened;}
bool Bubble(float& x,float& y,float uiWidth,float uiHeight) {
    if(!running || !anchor || selected.empty() || !CSprite::CalcScreenCoors) return false;
    RwV3d point{anchor->x,anchor->y,anchor->z}, screen{};float sx=0,sy=0;
    if(!CSprite::CalcScreenCoors(point,&screen,&sx,&sy,true,true)) return false;
    const int rw=RsGlobal.maximumWidth,rh=RsGlobal.maximumHeight;
    auto converted=ProjectToUi(screen.x,screen.y,screen.z,static_cast<float>(rw),static_cast<float>(rh),uiWidth,uiHeight);
    static int lastW=0,lastH=0;static float lastUiW=0,lastUiH=0;
    if(lastW!=rw || lastH!=rh || lastUiW!=uiWidth || lastUiH!=uiHeight){
        lastW=rw;lastH=rh;lastUiW=uiWidth;lastUiH=uiHeight;
        logger->Info("Bubble projection render=%dx%d imgui=%.0fx%.0f",rw,rh,uiWidth,uiHeight);
    }
    if(!converted) return false;
    x=converted->x;y=converted->y;return true;
}
void Stop(){if(voice)voice->Cancel();running=false;Reset();chat=nullptr;}
const char* Status(){return status;}
}
