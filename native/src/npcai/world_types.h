#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "personality.h"
namespace npcai {
struct WorldPoint {float x=0,y=0,z=0;};
enum class WorldKind { TimeJump,TimeReversed,Night,RainStarted,RainStopped,PedAppeared,VehicleAppeared,PedGone,VehicleGone,WeaponHolstered,PlayerLeft,PlayerDiscontinuity,Gunshot,Explosion,Combat,Death,VehicleDamage };
const char* WorldName(WorldKind);
bool ParseWorldKind(const std::string&,WorldKind&);
bool Monitorable(WorldKind);
struct ExpectationSpec {WorldKind event=WorldKind::Night;int count=1,timeoutSeconds=60;float radius=15,minConfidence=.7f;bool requireVisible=false;};
struct WorldEntity {std::string id;WorldPoint position;bool vehicle=false,alive=true,combat=false,ctorSeen=false;};
struct WorldObserverSample {std::string id;WorldPoint position,forward;int area=0;std::vector<std::string> visible;bool losAvailable=false,playerVisible=false;uint64_t at=0;Personality personality{};};
struct WorldEvent {std::string id,subject,source,observer;WorldKind kind=WorldKind::TimeJump;WorldPoint position;uint64_t at=0;float confidence=0,intensity=0;bool audible=false,visual=false,causeConfirmed=false;float magnitude=0;};
struct WorldFrame {
    uint64_t at=0,observerCostUs=0;int minuteOfDay=0,weather=-1,area=0,region=-1,weapon=-1,wanted=-1;float rain=0,speed=0;
    bool inVehicle=false,aiming=false,weatherAvailable=false,vehiclesAvailable=false;
    WorldPoint player;std::vector<WorldEntity> entities;std::vector<WorldObserverSample> observers;std::vector<WorldEvent> nativeEvents;
};
struct EmotionalState {float perplexity=0,fear=20,curiosity=40,trust=30,aggression=50,courage=50,skepticism=65;};
struct PerceivedEvent {WorldEvent event;float confidence=0;std::string channel;float distance=0;};
}
