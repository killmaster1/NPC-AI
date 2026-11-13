#pragma once
#include "world_core.h"
#include "expressive.h"
#include "gemini_client.h"
#include <condition_variable>
#include <thread>
#include <deque>
#include <memory>
namespace npcai {
struct LocalReply {std::string npc,dialogue;Action action=Action::None;bool aiOpportunity=false;};
struct IntelligenceMetrics {uint64_t frames=0,events=0,matched=0,autosaves=0,memoryBytes=0,memoryErrors=0,droppedFrames=0,perceptionUs=0,reactions=0;};
class IntelligenceRuntime {
public:
    using Log=std::function<void(const std::string&)>;
    IntelligenceRuntime(Settings,std::string privateDirectory,GeminiClient::HttpsTransport,Log={});~IntelligenceRuntime();
    Result Chat(const Request&,const std::atomic_bool&);
    void Accepted(const Request&,const Decision&);
    void Confirmed(const std::string&,Action,bool,int);
    void Submit(WorldFrame);
    std::vector<std::string> Interests();
    std::optional<LocalReply> TakeReply();
    void Boundary();void Slot(int,bool saved);void Flush();void Clear();void Shutdown();
    IntelligenceMetrics Metrics();
    JsonValue DebugMemory();
private:
    struct Mind {SocialState relation;NpcPerplexitySystem emotion;ExpectationManager expectations;std::deque<std::string> recent;uint64_t lastReaction=0,interestUntil=0;explicit Mind(const Personality& p):emotion(p){}};
    struct Receipt {std::string npc;Action action;bool ok;int amount;uint64_t epoch;};
    void ApplyReceipt(const Receipt&);uint64_t CurrentEpoch();
    Mind& MindFor(const std::string&,const Personality&);
    JsonValue Context(const Request&);void Run();void Process(const WorldFrame&);void Interest(const std::string&);
    Settings settings_;GeminiClient client_;NpcMemoryManager memory_;Log log_;
    std::mutex queueMutex_,stateMutex_;std::condition_variable cv_,readyCv_;std::atomic<uint64_t> dropped_{0};std::optional<WorldFrame> frame_;
    bool stopped_=false,boundary_=false,flush_=false,clear_=false,ready_=false;std::deque<std::pair<int,bool>> slots_;
    std::deque<Receipt> receipts_;uint64_t queuedEpoch_=0,appliedEpoch_=0;int queuedSlot_=-1;std::map<std::string,uint64_t> pendingEpochs_;
    std::deque<std::string> interests_;std::deque<LocalReply> replies_;
    std::map<WorldKind,int> patterns_;
    std::map<std::string,Mind> minds_;std::map<std::string,JsonValue> histories_;WorldFrame lastFrame_;WorldEventDetector detector_;
    IntelligenceMetrics metrics_;uint64_t lastSave_=0,lastLog_=0,hourStart_=0;int spontaneousSpent_=0;
    std::thread worker_;
};
uint64_t RealMilliseconds();
}
