#pragma once
#include "intelligence_runtime.h"
#include "registry.h"
struct CPed;
namespace npcai::WorldBridge {
void Init(std::shared_ptr<IntelligenceRuntime>,const Settings&);
void GameThread();void Dead(int,WorldPoint);
void Boundary();void PedBorn(int ref);void Tick(Registry&,CPed*,const std::string& selected);
}
