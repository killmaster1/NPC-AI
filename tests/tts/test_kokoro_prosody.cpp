#include "kokoro_prosody.h"
#include <iostream>
#include <stdexcept>
using namespace npcai;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
int main(){
 std::string sentence="Rapaz, eu estava passando pela rua e encontrei um amigo que não via há muito tempo, então a gente resolveu conversar um pouco sobre as histórias antigas do bairro e sobre como tudo mudou desde aquela época.";
 CHECK(sentence.size()>180 && sentence.size()<320);CHECK(KokoroSegments(sentence).size()==1);
 auto decimal=KokoroSegments("O Dr. Silva pagou R$ 2.50. Tá tudo certo!");CHECK(decimal.size()==2);CHECK(decimal[0]=="O Dr. Silva pagou R$ 2.50.");
 CHECK(KokoroSegments("Sério... Como assim?! Tá de brincadeira?").size()==3);
 std::string many;for(int i=0;i<100;++i)many+="Ação, coração e café na rua ";std::string joined;for(auto&s:KokoroSegments(many)){CHECK(s.size()<=320 && ValidUtf8(s));joined+=s;}CHECK(joined==many);
 std::string longWord;for(int i=0;i<300;++i)longWord+="á";joined.clear();for(auto&s:KokoroSegments(longWord)){CHECK(ValidUtf8(s) && s.size()<=320);joined+=s;}CHECK(joined==longWord);
 CHECK(KokoroSpokenText("[whispers] Vc vem cmg hj? Blz!")=="você vem comigo hoje? beleza!");CHECK(KokoroSpokenText("cvc, tá ligado? Café 2,50.")=="cvc, tá ligado? Café 2,50.");
 CHECK(KokoroSpokenText("[angry]").empty());
 Context c;c.emotion="sad";c.emotionIntensity=1;CHECK(KokoroProsodySpeed(1,c,true)==.96f);CHECK(KokoroProsodySpeed(1,c,false)==1);
 c.emotion="excited";CHECK(KokoroProsodySpeed(1,c,true)>1 && KokoroProsodySpeed(1.5,c,true)<=1.5f);
 c.emotion="neutral";CHECK(KokoroProsodySpeed(1,c,true)==1);
 std::cout<<"Kokoro prosody: 12 deterministic scenarios passed; perceived naturalness pending listening/device.\n";
}
