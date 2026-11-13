#pragma once
#include <cstdint>
#include <string>
namespace npcai {
class FocusLease {
    std::string id_;
    uint64_t until_=0;
    bool awaiting_=false;
public:
    void Begin(const std::string& id,uint64_t now){id_=id;until_=now+15000;awaiting_=false;}
    void Sent(uint64_t now){if(!id_.empty()){until_=now+45000;awaiting_=true;}}
    void Tick(uint64_t now,bool editing,bool waiting){
        if(id_.empty()) return;
        if(editing || waiting) until_=now+15000;
        if(awaiting_ && !waiting){until_=now+15000;awaiting_=false;}
        if(!editing && !waiting && now>=until_) Release();
    }
    void Release(){id_.clear();awaiting_=false;until_=0;}
    const std::string& Id() const{return id_;}
};
}
