#pragma once
#include "settings.h"
#include <array>
namespace npcai {
inline constexpr std::array<const char*,3> KokoroVoices={"pm_alex","pm_santa","pf_dora"};
inline int KokoroSpeaker(const std::string& name){return name=="pm_alex"?43:name=="pm_santa"?44:name=="pf_dora"?42:-1;}
struct VoiceProfile {std::string model,voice;float speed=1;};
inline VoiceProfile SelectVoiceProfile(const Settings& s,const std::string& id){
 uint32_t hash=2166136261u;for(unsigned char c:id)hash=(hash^c)*16777619u;
 if(s.ttsEngine=="elevenlabs")return {s.elevenModel,"",s.ttsSpeed};
 return {s.ttsModel,s.ttsVariation && !id.empty()?KokoroVoices[hash%3]:s.ttsVoice,s.ttsSpeed};
}
}
