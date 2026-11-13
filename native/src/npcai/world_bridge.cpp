#include "world_bridge.h"
#include <mod/logger.h>
#include <mod/amlmod.h>
#include <plugin-sdk/game_sa/Events.h>
#include <plugin-sdk/game_sa/other/Pools.h>
#include <plugin-sdk/game_sa/other/Clock.h>
#include <plugin-sdk/game_sa/other/Weather.h>
#include <plugin-sdk/game_sa/other/PlayerData.h>
#include <plugin-sdk/game_sa/other/Wanted.h>
#include <plugin-sdk/game_sa/other/Explosion.h>
#include <plugin-sdk/game_sa/engine/World.h>
#include <plugin-sdk/game_sa/entity/PlayerPed.h>
#include <algorithm>
#include <thread>
#include <set>
#include <cmath>
namespace npcai::WorldBridge {
namespace {
std::weak_ptr<IntelligenceRuntime> runtime;Settings options;uint64_t next=0;thread_local bool gameThread=false;
std::vector<WorldEvent> pending;std::set<int> born,dead,bornVehicles;int pendingSlot=-1;
bool(*originalOpen)(char*,u32)=nullptr;bool(*originalLoad)(bool&)=nullptr;bool(*originalSave)(int)=nullptr;
WorldPoint Point(const CVector& p){return {p.x,p.y,p.z};}
bool Finite(WorldPoint p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool OnThread(){return gameThread;}
void Add(WorldKind kind,WorldPoint position,float intensity,bool audible,const char* source){if(!OnThread() || !Finite(position) || pending.size()>=16)return;WorldEvent e;e.kind=kind;e.position=position;e.confidence=.85f;e.intensity=intensity;e.at=RealMilliseconds();e.audible=audible;e.source=source;pending.push_back(std::move(e));}
// This callback signals the weapon-fire method invocation, not necessarily a successful shot.
void Fire(CWeapon*,CEntity* owner,CVector* start,CVector* barrel,CEntity*,CVector*,CVector*){if(!OnThread() || !owner)return;auto pos=Point(owner->GetPosition());if(start)pos=Point(*start);else if(barrel)pos=Point(*barrel);Add(WorldKind::Gunshot,pos,.25f,false,"weapon_fire_method_invoked_projectile_not_confirmed");}
void Explosion(CEntity*,CEntity*,eExplosionType type,CVector p,u32,u8 sound,float,u8){
    if(!OnThread() || !CExplosion::TestForExplosionInArea)return;
    if(CExplosion::TestForExplosionInArea(static_cast<int>(type),p.x-2,p.x+2,p.y-2,p.y+2,p.z-2,p.z+2))Add(WorldKind::Explosion,Point(p),.9f,sound!=0,"native_explosion_activity_query_no_cause_attribution");
}
void VehicleBorn(CVehicle* v){if(OnThread() && v && CPools::ms_pVehiclePool && CPools::GetVehicleRef && bornVehicles.size()<256)bornVehicles.insert(CPools::GetVehicleRef(v));}
void Damage(CVehicle*,CEntity*,eWeaponType,float damage,CVector p){if(damage>20)Add(WorldKind::VehicleDamage,Point(p),.45f,true,"vehicle_damage_not_proof_of_collision");}
bool Open(char* name,u32 slot){bool ok=originalOpen(name,slot);if(ok && slot<=99)pendingSlot=static_cast<int>(slot);return ok;}
bool Load(bool& wrong){if(auto r=runtime.lock())r->Boundary();pendingSlot=-1;bool ok=originalLoad(wrong);if(ok && !wrong && pendingSlot>=0)if(auto r=runtime.lock())r->Slot(pendingSlot,false);pending.clear();born.clear();next=0;return ok;}
bool Save(int slot){bool ok=originalSave(slot);if(ok && slot>=0 && slot<=99)if(auto r=runtime.lock())r->Slot(slot,true);return ok;}
template<class F>void Hook(const char* symbol,F replacement,F* original){auto address=aml->GetSym(GetMainLibrary(),symbol);if(address)aml->Hook(reinterpret_cast<void*>(address),reinterpret_cast<void*>(replacement),reinterpret_cast<void**>(original));}
bool Visible(CPed* observer,WorldPoint p,CEntity* target=nullptr){
    if(!CWorld::GetIsLineOfSightClear && !CWorld::ProcessLineOfSight)return false;const auto pos=observer->GetPosition();auto f=observer->GetTopDirection();float x=p.x-pos.x,y=p.y-pos.y;float distance=std::sqrt(x*x+y*y);if(distance>40 || distance<.05f)return distance<.05f;
    float n=std::sqrt(f.x*f.x+f.y*f.y);if(n<.001f || (f.x*x+f.y*y)/(n*distance)<.5f)return false;
    CVector start{pos.x,pos.y,pos.z+.8f},end{p.x,p.y,p.z+.6f};
    if(CWorld::ProcessLineOfSight){CColPoint point{};CEntity* hit=nullptr;bool blocked=CWorld::ProcessLineOfSight(start,end,point,hit,true,true,false,true,true,false,false,false);return !blocked || (target && hit==target);}
    return CWorld::GetIsLineOfSightClear(start,end,true,true,false,true,true,false,false);
}
}
void Init(std::shared_ptr<IntelligenceRuntime> r,const Settings& s){runtime=r;options=s;
    Hook("_ZN19CGenericGameStorage18OpenFileForReadingEPcj",&Open,&originalOpen);Hook("_ZN19CGenericGameStorage11GenericLoadERb",&Load,&originalLoad);Hook("_ZN19CGenericGameStorage11GenericSaveEi",&Save,&originalSave);
    logger->Info("World capabilities los=%d vehicles=%d save_open=%d save_load=%d save_write=%d",(CWorld::GetIsLineOfSightClear || CWorld::ProcessLineOfSight),CPools::GetVehicle && CPools::GetVehicleRef,originalOpen!=nullptr,originalLoad!=nullptr,originalSave!=nullptr);
    Events::weaponFireEvent += Fire;Events::addExplosionEvent += Explosion;Events::vehicleDamageEvent += Damage;Events::vehicleCtorEvent += VehicleBorn;
    logger->Info("World event hooks weapon=%d explosion=%d vehicle_damage=%d",Events::weaponFireEvent.m_pOriginalFunc!=nullptr,Events::addExplosionEvent.m_pOriginalFunc!=nullptr,Events::vehicleDamageEvent.m_pOriginalFunc!=nullptr);
}
void GameThread(){gameThread=true;}
void Dead(int ref,WorldPoint point){if(dead.count(ref) || dead.size()>=512)return;dead.insert(ref);Add(WorldKind::Death,point,.65f,false,"native_health_zero");}
void Boundary(){next=0;born.clear();dead.clear();bornVehicles.clear();pending.clear();if(auto r=runtime.lock())r->Boundary();}
void PedBorn(int ref){dead.erase(ref);if(born.size()<256 && ref>=0)born.insert(ref);}
void Tick(Registry& registry,CPed* player,const std::string& selected){
    auto r=runtime.lock();if(!r || !options.worldPerception || !player)return;gameThread=true;uint64_t now=RealMilliseconds();if(now<next)return;next=now+options.worldIntervalMs;
    const auto captureStarted=std::chrono::steady_clock::now();
    WorldFrame frame;frame.at=now;frame.minuteOfDay=static_cast<int>(CClock::ms_nGameClockHours)*60+CClock::ms_nGameClockMinutes;frame.area=player->m_nAreaCode;frame.player=Point(player->GetPosition());frame.inVehicle=player->bInVehicle;frame.aiming=player->bIsAimingGun;frame.speed=player->m_vecMoveSpeed.Magnitude();frame.weatherAvailable=true;frame.weather=CWeather::NewWeatherType;frame.rain=CWeather::Rain;
    int weapon=player->m_nSelectedWepSlot;if(weapon>=0 && weapon<13)frame.weapon=static_cast<int>(player->m_aWeapons[weapon].m_eWeaponType);
    if(player->m_pPlayerData && player->m_pPlayerData->m_pWanted)frame.wanted=static_cast<int>(player->m_pPlayerData->m_pWanted->m_nWantedLevel);
    for(const auto& nearby:registry.Nearby({frame.player.x,frame.player.y,frame.player.z},frame.area,60)){
        const auto* t=&nearby;const auto& id=t->id;
        auto pos=registry.Position(id);if(!pos)continue;WorldEntity e;e.id=id;e.position={pos->x,pos->y,pos->z};e.ctorSeen=born.count(t->ref)!=0;e.combat=t->context.pedState==PEDSTATE_ATTACK || t->context.pedState==PEDSTATE_FIGHT;frame.entities.push_back(std::move(e));if(frame.entities.size()>=128)break;
    }
    std::map<std::string,CEntity*> collisionTargets;
    auto* vehicles=CPools::ms_pVehiclePool;
    frame.vehiclesAvailable=vehicles && vehicles->m_byteMap && CPools::GetVehicle && CPools::GetVehicleRef && vehicles->m_nSize>0 && vehicles->m_nSize<=4096;
    if(frame.vehiclesAvailable)for(int i=0;i<vehicles->m_nSize && frame.entities.size()<192;++i){
        if(vehicles->IsFreeSlotAtIndex(i))continue;int ref=(i<<8)|vehicles->m_byteMap[i].IntValue();auto* v=CPools::GetVehicle(ref);if(!v || CPools::GetVehicleRef(v)!=ref || v->bRemoveFromWorld || !v->m_pRwObject || v->m_nAreaCode!=frame.area)continue;
        auto p=Point(v->GetPosition());if(!Finite(p) || WorldDistance(p,frame.player)>60)continue;WorldEntity e;e.id="vehicle-"+std::to_string(ref);e.position=p;e.vehicle=true;e.ctorSeen=bornVehicles.count(ref)!=0;collisionTargets[e.id]=v;frame.entities.push_back(std::move(e));
    }
    std::stable_sort(frame.entities.begin(),frame.entities.end(),[](const auto& a,const auto& b){return a.ctorSeen && !b.ctorSeen;});
    auto ids=r->Interests();if(!selected.empty()){ids.erase(std::remove(ids.begin(),ids.end(),selected),ids.end());ids.insert(ids.begin(),selected);}
    int visibilityBudget=12;
    for(const auto& id:ids){if(frame.observers.size()>=4)break;auto t=registry.Resolve(id,{frame.player.x,frame.player.y,frame.player.z},frame.area,80);if(!t)continue;
        auto* ped=CPools::GetPed(t->ref);if(!ped || CPools::GetPedRef(ped)!=t->ref || !registry.Contains(id,t->ref) || ped->m_fHealth<=0)continue;
        WorldObserverSample observer;observer.id=id;observer.at=now;observer.position=Point(ped->GetPosition());observer.forward=Point(ped->GetTopDirection());observer.area=ped->m_nAreaCode;if(!Finite(observer.position) || !Finite(observer.forward))continue;observer.personality=t->context.personality;observer.losAvailable=(CWorld::GetIsLineOfSightClear || CWorld::ProcessLineOfSight) && visibilityBudget>0;
        if(observer.losAvailable && visibilityBudget>0){observer.playerVisible=Visible(ped,frame.player,player);--visibilityBudget;}
        for(const auto& e:frame.entities){if(visibilityBudget<=0)break;if(e.id==id || WorldDistance(e.position,observer.position)>25)continue;if(Visible(ped,e.position,e.vehicle?collisionTargets[e.id]:nullptr))observer.visible.push_back(e.id);--visibilityBudget;}
        frame.observers.push_back(std::move(observer));
    }
    frame.nativeEvents.swap(pending);born.clear();bornVehicles.clear();frame.observerCostUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-captureStarted).count();r->Submit(std::move(frame));
}
}
