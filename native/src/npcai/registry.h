#pragma once
#include "types.h"
#include <map>
#include <optional>
#include <vector>

namespace npcai {
struct Observation {
    int slot=0, ref=-1;
    bool player=false, alive=false;
    Vec3 position;
    Context context;
};
// Só thread do jogo. Nenhum ponteiro de entidade é armazenado.
class Registry {
public:
    explicit Registry(std::string session):session_(std::move(session)){}
    void Begin();
    void Observe(const Observation&);
    void End();
    void Destroy(int slot);
    void Reset();
    void ActionConfirmed(const std::string&,Action);
    void Behaviors(const std::map<std::string,std::string>&);
    bool Contains(const std::string&,int ref) const;
    std::string IdAt(int slot) const;
    std::optional<Target> Nearest(Vec3 player, int area, float radius, const std::string& exclude={}) const;
    std::optional<Target> Resolve(const std::string&, Vec3 player, int area, float radius) const;
    std::vector<std::string> Ids() const;
    std::vector<Target> Nearby(Vec3,int,float,size_t limit=128) const;
    std::optional<Vec3> Position(const std::string& id) const;
private:
    struct Entry { Observation value; std::string id; bool seen=false; };
    std::map<int,Entry> entries_;
    std::string session_;
    uint64_t serial_=0;
};
std::string Identify(const Context&);
}
