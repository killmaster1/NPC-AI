#pragma once
#include "settings.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <map>
#include <sstream>
namespace npcai {
inline size_t Characters(const std::string& s){size_t n=0;for(unsigned char c:s)if((c&192)!=128)++n;return n;}
inline std::string LowerSpeech(std::string s){for(char& c:s)if(c>='A' && c<='Z')c+=32;return s;}
inline std::string TrimSpeech(std::string s){auto a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
inline std::string PlainSpeech(std::string s){ // Never speak provider directives or receipts.
 for(size_t p=0;(p=s.find('[',p))!=std::string::npos;){auto end=s.find(']',p);if(end==std::string::npos){s.erase(p);break;}s.erase(p,end-p+1);}
 return TrimSpeech(s);
}
inline std::string ShortSpeech(const std::string& text,size_t max){
 auto s=PlainSpeech(text);if(Characters(s)<=max)return s;
 size_t count=0,last=0;for(size_t i=0;i<s.size();++i){if((static_cast<unsigned char>(s[i])&192)!=128)++count;if(count>max)break;if(s[i]=='.' || s[i]=='!' || s[i]=='?' || s[i]=='\n')last=i+1;}
 // A complete sentence is preferred. An overlong single sentence stays text-only rather than being mutilated.
 return last?TrimSpeech(s.substr(0,last)):std::string{};
}
inline const std::vector<std::string>& Emotions(){static const std::vector<std::string> v={"neutral","happy","excited","curious","surprised","suspicious","afraid","sad","annoyed","angry","furious","sarcastic","embarrassed","impressed","friendly"};return v;}
inline std::string EmotionName(const std::string& s){return std::find(Emotions().begin(),Emotions().end(),s)!=Emotions().end()?s:"neutral";}
struct SocialState {
 float irritation=0,trust=40,fear=0,severity=0;uint64_t updated=0;std::string emotion="neutral";float intensity=.4f;
 void Recover(uint64_t now,bool enabled){if(!updated){updated=now;return;}if(enabled && now>updated){float seconds=std::min(300.f,(now-updated)/1000.f);irritation=std::max(severity*.2f,irritation-seconds*.08f);fear=std::max(0.f,fear-seconds*.04f);if(trust<40-severity*.25f)trust=std::min(40-severity*.25f,trust+seconds*.015f);}updated=now;}
 void Observe(const std::string& input,const Personality& p,bool aimingNearby,uint64_t now,const Settings& s){Recover(now,s.relationshipRecovery);auto t=LowerSpeech(input);
  bool negated=t.find("não vou")!=std::string::npos || t.find("nao vou")!=std::string::npos || t.find("não quero")!=std::string::npos;
  bool threat=!negated && (t.find("vou te matar")!=std::string::npos || t.find("passa a grana")!=std::string::npos || t.find("mãos pra cima")!=std::string::npos || t.find("maos pra cima")!=std::string::npos);
  bool insult=t.find("idiota")!=std::string::npos || t.find("seu merda")!=std::string::npos;
  if(threat){irritation=std::min(100.f,irritation+20*s.aggressionMultiplier);fear=std::min(100.f,fear+25*(1-p.courage/150.f));trust=std::max(0.f,trust-12);severity=std::min(100.f,severity+10);}
  else if(insult){irritation=std::min(100.f,irritation+12*s.aggressionMultiplier);trust=std::max(0.f,trust-3);}
  else if(t.find("obrigad")!=std::string::npos || t.find("desculp")!=std::string::npos){irritation=std::max(0.f,irritation-4);trust=std::min(100.f,trust+2);}
  if(aimingNearby)fear=std::max(fear,15.f); // Aiming nearby is not verified aiming AT this NPC.
  emotion=fear>45?"afraid":irritation>80?"furious":irritation>60?"angry":irritation>35?"annoyed":threat || irritation>15?"suspicious":"neutral";
  intensity=std::clamp(std::max(fear,irritation)/100.f,.2f,1.f)*s.emotionIntensity;
 }
 std::string Stage()const{return irritation>85?"hostile":irritation>60?"angry":irritation>35?"annoyed":irritation>15?"suspicious":"neutral";}
};
inline std::string CleanLanguage(std::string text,int level,bool repetition,const std::vector<std::string>& recent){
 const std::vector<std::pair<std::string,std::string>> strong={{"cuzão","cara"},{"caralho","caramba"},{"porra","poxa"},{"merda","droga"},{"vai se ferrar","me deixa em paz"}};
 if(level<2)for(const auto& item:strong){size_t p=0;auto folded=LowerSpeech(text);while((p=folded.find(item.first,p))!=std::string::npos){const bool left=p==0 || std::string(" ,.!?\n").find(text[p-1])!=std::string::npos;size_t end=p+item.first.size();const bool right=end==text.size() || std::string(" ,.!?\n").find(text[end])!=std::string::npos;if(left && right){text.replace(p,item.first.size(),item.second);folded=LowerSpeech(text);p+=item.second.size();}else ++p;}}
 if(level==0){for(const auto& word:{std::string("droga"),std::string("poxa")}){auto p=LowerSpeech(text).find(word);if(p==0 && text.size()>word.size()+2)text=TrimSpeech(text.substr(word.size()+2));}}
 if(repetition && recent.size()>=2){for(const std::string prefix:{"Mano, ","Mano... ","Papo reto, "})if(text.rfind(prefix,0)==0){int count=0;for(const auto& old:recent)if(LowerSpeech(old).rfind(LowerSpeech(prefix),0)==0)++count;if(count>=2)text=TrimSpeech(text.substr(prefix.size()));}}
 return text;
}
inline std::string DeliveryTag(const std::string& s){return s=="whisper"?"[whispers] ":s=="giggle"?"[giggles] ":s=="sarcastic"?"[sarcastically] ":s=="tense"?"[tense] ":"";}
inline std::string VoiceTag(const std::string& emotion){if(emotion=="happy" || emotion=="friendly")return "[cheerfully] ";if(emotion=="surprised" || emotion=="impressed")return "[surprised] ";if(emotion=="sarcastic")return "[sarcastically] ";if(emotion=="afraid")return "[nervously] ";if(emotion=="angry" || emotion=="furious")return "[angry] ";if(emotion=="sad")return "[sad] ";if(emotion=="excited")return "[excited] ";if(emotion=="embarrassed")return "[hesitantly] ";return {};}
}
