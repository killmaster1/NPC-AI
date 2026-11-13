#pragma once
#include <array>
#include <string>
#include <cstdint>
namespace npcai {
struct Personality {int courage=50,aggression=50,kindness=50,patience=50,fear=20,trust=30;int sociability=50,curiosity=50,humor=50,skepticism=50,empathy=50,slang=40,profanity=15;};
inline Personality MakePersonality(const std::string& id,int model,int type){
    uint64_t seed=1469598103934665603ull;for(unsigned char c:id){seed^=c;seed*=1099511628211ull;}
    std::array<int,6> values{};
    for(auto& n:values){seed^=seed>>12;seed^=seed<<25;seed^=seed>>27;n=15+static_cast<int>((seed*2685821657736338717ull)%71);}
    Personality p{values[0],values[1],values[2],values[3],values[4]/2,values[5]/2};
    if(type==6){p.courage=80;p.aggression=65;p.patience=25;}
    else if(model>=102 && model<=104){p.aggression=70;p.trust=10;}
    else if(model>=105 && model<=107){p.kindness=70;p.trust=60;}
    p.sociability=values[2];p.curiosity=values[3];p.humor=values[0];p.skepticism=values[1];p.empathy=values[2];p.slang=values[4];p.profanity=values[5]/3;
    return p;
}
}
