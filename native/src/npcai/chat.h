#pragma once
#include "types.h"
#include "settings.h"
#include <deque>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace npcai {
struct Message { bool fromPlayer; std::string text; };
struct Snapshot {
    std::optional<Target> target;
    std::vector<Message> messages;
    std::string error;
    bool waiting=false;
    std::string conversationId;
    int bubbleDurationMs=15000;uint64_t replySerial=0;
};
struct ActionRequest {Target target;std::string conversationId;Decision decision;bool dialoguePublished=false;};
struct SpeechEvent {Target target;std::string conversationId,text;uint64_t serial=0;};
class Chat {
public:
    using Transport=std::function<Result(const Request&, const std::atomic_bool&)>;
    explicit Chat(Transport transport, size_t historyLimit=24);
    ~Chat();
    void Select(std::optional<Target>);
    void Retain(const std::vector<std::string>& liveIds);
    bool Send(const std::string&);
    void Clear();
    Snapshot Get() const;
    void Shutdown();
    std::optional<ActionRequest> TakeAction();
    std::optional<SpeechEvent> TakeSpeech();
    void ConfigureDialogue(const Settings&);
    void SetDecisionObserver(std::function<void(const Request&,const Decision&)>);
    bool LocalReply(const Target&,const std::string&);
    bool React(const std::string&);
    void ReportAction(const ActionRequest&,bool success,const std::string& text,int amount=0);
    Chat(const Chat&)=delete;
    Chat& operator=(const Chat&)=delete;
private:
    struct Conversation { std::string token; std::vector<Message> messages; uint64_t touched=0; std::optional<ActionFeedback> feedback=std::nullopt; bool actionWaiting=false;int bubbleDurationMs=15000;uint64_t replySerial=0; };
    struct Job { Request request; std::shared_ptr<std::atomic_bool> cancelled; };
    bool Enqueue(const std::string&,bool);
    void Run();
    Conversation& ConversationFor(const std::string&);
    void Trim(Conversation&);
    void Published(Conversation&,const Target&,const std::string&);
    std::function<void(const Request&,const Decision&)> observer_;

    Settings settings_;
    std::deque<SpeechEvent> speech_;
    uint64_t replySerial_=0;
    Transport transport_;
    size_t historyLimit_;
    mutable std::mutex mutex_;
    std::mutex shutdownMutex_;
    std::condition_variable cv_;
    std::map<std::string,Conversation> conversations_;
    std::optional<Target> target_;
    std::optional<Job> pending_;
    std::optional<ActionRequest> action_;
    uint64_t requestSerial_=0;
    std::shared_ptr<std::atomic_bool> active_;
    std::string error_, activeId_, activeToken_;
    bool stopping_=false, busy_=false;
    uint64_t serial_=0, clock_=0;
    std::thread worker_;
};
}
