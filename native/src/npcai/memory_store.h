#pragma once
#include "world_types.h"
#include "../../../third_party/json/json.hpp"
#include <filesystem>
#include <mutex>
#include <optional>
namespace npcai {
using JsonValue=nlohmann::json;
struct MemoryIdentity {std::string key;bool stableVerified=false;};
class NpcMemoryManager {
public:
    NpcMemoryManager(std::string directory,size_t maxBytes=10*1024*1024);
    void Initialize(const std::string& profile);
    bool Load(const std::string& profile);bool Save();bool Clear();
    bool Note(const std::string& npc,const std::string& text,bool global=false);
    void Emotion(const MemoryIdentity&,const EmotionalState&);
    std::optional<EmotionalState> Restore(const MemoryIdentity&)const;
    std::vector<std::string> Notes(const MemoryIdentity&,bool includeGlobal)const;
    void ForkFrom(const JsonValue&);
    JsonValue Snapshot()const{return data_;}
    std::string Profile()const{return profile_;}
    bool Dirty()const{return dirty_;}size_t Saves()const{return saves_;}size_t Bytes()const{return bytes_;}size_t Corruptions()const{return corruptions_;}
private:
    void Prune();bool Read(const std::filesystem::path&);std::filesystem::path File()const;
    std::string directory_,profile_;size_t maxBytes_,saves_=0,bytes_=0,corruptions_=0;bool dirty_=false,readOnly_=false;JsonValue data_;
};
std::string IntegrityHash(const std::string&);
}
