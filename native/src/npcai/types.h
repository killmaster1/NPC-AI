#pragma once
#include <atomic>
#include "actions.h"
#include "personality.h"
#include <cstdint>
#include <string>

namespace npcai {
struct Vec3 { float x=0, y=0, z=0; };
struct Context {
    int model=0, pedType=0, pedState=0, area=0;
    int hour=-1, minute=-1, regionLevel=-1;
    bool inVehicle=false;
    Personality personality{};bool hasPersonality=false,personalityVariation=true;
    std::string interactionState="normal";
    std::string stableCharacterId; // Empty unless a future engine integration verifies a named character.
    std::string emotion="neutral",delivery="normal",relationshipStage="neutral";float emotionIntensity=.4f,relationship=40;
    bool observedTaxiDriver=false; // Session-local evidence: verified driver of TAXI/CABBIE, not a guessed occupation.
};
struct Target {
    std::string id, name;
    int ref=-1;
    float distance=0;
    Context context;
};
struct Request {
    std::string npcId, conversationId, message;
    Context context;
    std::string requestId="";
    int timeoutMs=28000;bool spontaneous=false;
    std::optional<ActionFeedback> feedback=std::nullopt;
};
struct Result { bool ok=false; std::string text; std::optional<Decision> decision=std::nullopt; };
struct HttpConfig {
    uint16_t port=8765;
    int connectTimeoutMs=1500, requestTimeoutMs=28000;
    std::string token;
};
bool ValidUtf8(const std::string& text);
std::string Json(const Request& request);
std::string SessionId();
}
