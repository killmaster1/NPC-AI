#pragma once
#include <cstdint>
namespace npcai {
class AutosaveGate {
public:explicit AutosaveGate(uint64_t intervalMs):interval_(intervalMs){}
    void Reset(uint64_t at){last_=at;}
    bool Due(uint64_t now,bool dirty,bool force=false){if(!force && now-last_<interval_)return false;last_=now;return dirty;}
private:uint64_t interval_,last_=0;
};
}
