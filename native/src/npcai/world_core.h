#pragma once
#include "world_types.h"
#include "personality.h"
#include <optional>
#include <map>
#include <set>
namespace npcai {
float WorldDistance(WorldPoint,WorldPoint);
class WorldEventDetector {
public:std::vector<WorldEvent> Update(const WorldFrame&);void Reset(){previous_.reset();}
private:std::optional<WorldFrame> previous_;uint64_t serial_=0;
};
class NpcPerception {public:static std::optional<PerceivedEvent> Assess(const WorldEvent&,const WorldObserverSample&);};
struct ActiveExpectation {std::string id;ExpectationSpec spec;uint64_t created=0,deadline=0;std::set<std::string> evidence;};
struct ExpectationMatch {ActiveExpectation expectation;PerceivedEvent evidence;};
class ExpectationManager {
public:bool Register(const std::string& nonce,const std::vector<ExpectationSpec>&,uint64_t now,int maxActive,const WorldFrame&);
    std::vector<ExpectationMatch> Observe(const PerceivedEvent&);size_t Expire(uint64_t);const std::vector<ActiveExpectation>& Active()const{return active_;}
private:std::vector<ActiveExpectation> active_;std::set<std::string> registered_;
};
class NpcPerplexitySystem {
public:explicit NpcPerplexitySystem(const Personality& p={});
    bool Observe(const PerceivedEvent&,bool expectationSatisfied,float gain=1.f);bool Decay(uint64_t realMs);
    EmotionalState state;
private:std::map<WorldKind,int> habituation_;std::set<std::string> seen_;uint64_t lastDecay_=0,lastHabituation_=0;
};
std::string EventSummary(const PerceivedEvent&,bool challenge=false);
std::string LocalReaction(const EmotionalState&,uint64_t variety);
}
