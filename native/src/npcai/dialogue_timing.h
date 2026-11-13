#pragma once
#include "settings.h"
#include <algorithm>
#include <cctype>
namespace npcai {
inline int DialogueDuration(const std::string& text,const Settings& s){
    int words=0,pauses=0;bool word=false;
    for(unsigned char c:text){bool separator=std::isspace(c)!=0;if(!separator && !word)++words;word=!separator;if(c=='.'||c==','||c=='!'||c=='?'||c==';')++pauses;}
    return std::clamp(words*60000/std::max(60,s.readingWpm)+std::min(pauses,12)*120,s.dialogueMinMs,s.dialogueMaxMs);
}
}
