#pragma once
#include "types.h"
#include <vector>
namespace npcai {
struct ConfigurationReport {
 std::string path; bool found=false,valid=true; size_t accepted=0;
 std::vector<std::string> errors,warnings,candidates;
};
struct Settings { ConfigurationReport configuration; std::string providerConfigError; HttpConfig http; float radius=3; float bubbleHeight=1.75f, bubbleScreenOffsetY=6; bool npcStandStill=true, npcDialogueAnimations=true; bool facePlayer=true, stopOnTalk=true, aiActions=true, moneyDrop=true, moneyRepeat=false; int moneyMax=200,moneyPickupsMax=5,actionTimeoutMs=10000,moneySessionMax=5000; bool holdSurrender=true, personalityVariation=true, animationDebug=false, dynamicBubble=false;
float surrenderReleaseDistance=6.f,followReleaseDistance=25.f;
int surrenderReleaseDelayMs=2000,readingWpm=180,dialogueMinMs=2500,dialogueMaxMs=12000;
bool geminiEnabled=false,legacyHttp=false,memoryEnabled=true,memoryPersistent=true,memoryGlobal=true,memoryIndividual=true,memorySend=false,memoryClear=false,perplexityEnabled=true,perplexityDecay=true,worldPerception=true,worldEventMemory=true,worldExpectations=true,spontaneousAI=false;
int geminiTimeoutMs=25000,geminiRetries=2,memoryAutosaveSeconds=10,memoryMaxMb=10,worldIntervalMs=500,maxExpectations=3,contextTokens=2048,spontaneousCooldownS=60,spontaneousBudgetHour=4;
float perplexityGain=1.f;
std::string geminiModel="gemini-2.5-flash-lite",geminiApiKey,credentialStorage="keystore",credentialAction="none",memoryProfile="auto",reactionMode="economy";
bool ttsKokoroProsody=true;
bool ttsEnabled=true,ttsSync=true,ttsStopDistance=true,ttsCache=true,ttsSpatial=true,ttsVariation=true;
float ttsVolume=.85f,ttsSpeed=1.f,ttsMaxDistance=15.f;int ttsThreads=2,ttsTimeoutMs=45000,ttsIdleSeconds=0,ttsMinMemoryMb=900;
std::string ttsEngine="kokoro",elevenModel="eleven_v3",elevenKey,elevenStorage="keystore",elevenKeyAction="none",elevenVoices,elevenModelVoices,elevenCharacterVoices,elevenAssignment="character_id";
bool elevenVariation=true,elevenCache=true,elevenCachePersistent=true,emotionsEnabled=true,relationshipRecovery=true,profanityRepetition=true,personalityPersistent=true;
int elevenTarget=150,elevenMax=200,elevenCacheMb=150,elevenTimeout=25000,profanityLevel=1;
float elevenRate=1.f,elevenDay=1000.f,elevenHour=250.f,emotionIntensity=.7f,aggressionMultiplier=.65f;
std::string ttsModel="kokoro-pt-br",ttsVoice="pm_alex",ttsLanguage="pt-BR",ttsConfigError;
size_t historyLimit=24; };
Settings LoadSettings(const std::string& path);
Settings LoadSettingsFromLoader(const std::string& dataPath,const std::string& configPath);
std::string SelectedTransport(const Settings&);
}
