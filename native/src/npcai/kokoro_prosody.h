#pragma once
#include "expressive.h"
#include <cctype>
namespace npcai {
// Expand only whole chat abbreviations. Preserve accents, slang, numbers and meaning.
inline std::string KokoroSpokenText(const std::string& input) {
 auto text=PlainSpeech(input);std::string result;
 const std::map<std::string,std::string> words={{"vc","você"},{"vcs","vocês"},{"tb","também"},{"tbm","também"},{"cmg","comigo"},{"hj","hoje"},{"blz","beleza"}};
 for(size_t i=0;i<text.size();) {
  unsigned char c=text[i];
  if(std::isalpha(c) || c>=128){size_t end=i+1;while(end<text.size()){auto x=static_cast<unsigned char>(text[end]);if(!std::isalpha(x) && x<128)break;++end;}auto word=text.substr(i,end-i);auto it=words.find(LowerSpeech(word));result+=it==words.end()?word:it->second;i=end;}
  else {result+=text[i++];}
 }
 return result;
}
inline float KokoroProsodySpeed(float base,const Context& context,bool enabled) {
 if(!enabled)return std::clamp(base,.6f,1.5f);
 // Gentle duration control, not an emotion model, pitch shift or time-stretch effect.
 float change=0;if(context.emotion=="sad" || context.emotion=="suspicious" || context.emotion=="sarcastic")change=-.04f;
 else if(context.emotion=="excited" || context.emotion=="happy")change=.03f;
 float intensity=std::isfinite(context.emotionIntensity)?std::clamp(context.emotionIntensity,0.f,1.f):0.f;
 return std::clamp(base*(1+change*intensity),.6f,1.5f);
}
inline bool KokoroSentenceEnd(const std::string& text,size_t i) {
 char c=text[i];if(c=='\n')return true;if(c!='.' && c!='!' && c!='?')return false;
 if(i+1<text.size() && std::string(" \t\r\n").find(text[i+1])==std::string::npos)return false;
 if(c=='.') {size_t start=i;while(start && text[start-1]!=' ' && text[start-1]!='\n')--start;auto token=LowerSpeech(text.substr(start,i-start));
  if(token=="sr" || token=="sra" || token=="dr" || token=="dra" || token=="prof" || token=="av" || token=="ex" || (token.size()==1 && std::isalpha(static_cast<unsigned char>(token[0]))))return false;
 }
 return true;
}
inline std::vector<std::string> KokoroSegments(const std::string& text) {
 std::vector<std::string> result;size_t first=0;
 while(first<text.size()) {
  size_t end=text.size(),space=first,clause=first;bool boundary=false;
  for(size_t i=first;i<text.size();++i) {
   if(text[i]==' ' || text[i]=='\n')space=i+1;
   if((text[i]==',' || text[i]==';' || text[i]==':') && i-first>=100 && i+1<text.size() && text[i+1]==' ')clause=i+2;
   if(KokoroSentenceEnd(text,i)){end=i+1;boundary=true;break;}
   if(i-first>=319) {end=clause>first?clause:space>first?space:i;while(end>first && end<text.size() && (static_cast<unsigned char>(text[end])&192)==128)--end;if(end==first)end=i+1;boundary=true;break;}
  }
  if(!boundary)end=text.size();
  result.push_back(text.substr(first,end-first));first=end;
 }
 return result;
}
}
