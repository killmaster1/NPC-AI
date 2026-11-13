#pragma once
#include "chat.h"
#include "settings.h"
#include <functional>
#include <map>
#include <set>
#include <vector>
namespace npcai {
enum class InteractionState { IDLE,SELECTED,FACE_PLAYER,CONVERSING,EXECUTING_ACTION,ENDING,RELEASED,
    SURRENDERING,FOLLOWING_PLAYER,FLEEING,TALKING_GESTURE,AFRAID,ANGRY };
struct ActionContext {bool valid=false,eligible=false,paused=false,facing=false,stationary=false;const char* reason="unknown";float distance=0;bool needsTurn=false;};
enum class Effect { Pending,Applied,Failed,Finished };
class ActionEngine {
public:
    virtual ~ActionEngine()=default;
    virtual ActionContext Inspect(const Target&)=0;
    virtual bool Start(const Target&,Action,int durationMs)=0;
    virtual Effect Poll(const Target&,Action)=0;
    virtual void Stop(const Target&)=0;
    virtual void StopAction(const Target& t,Action){Stop(t);}
    virtual bool PrepareMoney(const Target&)=0;
    virtual int SpawnMoney(const Target&,int amount,int ordinal)=0;
    virtual bool PickupAlive(int)=0;
    virtual void RemovePickup(int)=0;
};
class NpcActionController {
public:
    using Reporter=std::function<void(const ActionRequest&,bool,const std::string&,int)>;
    NpcActionController(ActionEngine&,Settings,Reporter);
    bool Begin(const Target&,uint64_t);
    bool Submit(ActionRequest,uint64_t);
    void Speak(const Target&,const std::string&,uint64_t);
    void VoicePlayback(const Target&,bool,uint64_t);
    void Tick(uint64_t);
    void Detach(const std::string&);
    void Cancel(const std::string&,const std::string& reason="Conversa encerrada.",bool cleanupMoney=false);
    void Reset();
    bool Executing(const std::string&)const;
    bool Persistent(const std::string&)const;
    InteractionState State(const std::string&)const;
    std::map<std::string,InteractionState> States()const;
    size_t PickupCount()const{return pickups_.size();}
private:
    struct Entry {
        Target target;InteractionState state=InteractionState::SELECTED;Action active=Action::None;
        uint64_t deadline=0,stageAt=0,speechUntil=0,nextGesture=0,gestureUntil=0,nextFace=0,farSince=0,nextMaintain=0;
        int stage=0,recoveries=0,durationMs=4000;bool reported=false,detached=false,holding=false,audioDriven=false;
        Action gesture=Action::None;std::optional<ActionRequest> request;
    };
    struct Pickup {int handle;uint64_t expires;std::string owner;};
    void Report(Entry&,bool,const std::string&,int amount=0);
    void Fail(Entry&,const std::string&);
    bool Money(Entry&,uint64_t);
    void Posture(Entry&,const ActionContext&,uint64_t);
    void Gestures(Entry&,uint64_t);
    ActionEngine& engine_;Settings settings_;Reporter reporter_;
    std::map<std::string,Entry> entries_;
    std::set<std::string> seen_,paid_;
    int sessionMoney_=0;
    std::vector<Pickup> pickups_;
};
}
