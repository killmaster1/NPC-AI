#pragma once
#include "npc_action_controller.h"
#include <memory>
struct CPed;
namespace npcai {
class EngineActions final:public ActionEngine {
public:
    using Resolver=std::function<CPed*(const Target&,bool cleanup)>;
    explicit EngineActions(Resolver,bool debug=false);
    ~EngineActions();
    bool Init();
    void ForgetPed(int refSlot);
    ActionContext Inspect(const Target&) override;
    bool Start(const Target&,Action,int) override;
    Effect Poll(const Target&,Action) override;
    void Stop(const Target&) override;
    void StopAction(const Target&,Action) override;
    bool PrepareMoney(const Target&) override;
    int SpawnMoney(const Target&,int,int) override;
    bool PickupAlive(int) override;
    void RemovePickup(int) override;
    void TaskDestroyed(uintptr_t);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
