#pragma once
#include <string>
#include <optional>
#include "world_types.h"
namespace npcai {
enum class Action { None, Idle, FacePlayer, TalkGesture, HandsUp, SurrenderMoney, Flee, Angry, Refuse, FollowPlayer, StopFollowing, Cower, ReleaseSurrender };
struct Decision {
    Action action=Action::None;
    std::string dialogue,emotion="neutral",requestId,delivery="normal",relationshipStage="neutral";
    int amount=0,durationMs=4000;float emotionIntensity=.5f,relationship=40;
    std::vector<ExpectationSpec> expectations;
};
struct ActionFeedback {std::string requestId,action,status;int amount=0;};
std::string NaturalDialogue(std::string);
bool IsSpeechAction(Action);
const char* ActionName(Action);
std::optional<Action> ActionFromName(const std::string&);
Decision ParseDecision(const std::string&,const std::string& expectedRequestId);
}
