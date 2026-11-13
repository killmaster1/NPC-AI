#pragma once
#include "chat.h"
#include "settings.h"
#include <memory>
namespace npcai {class IntelligenceRuntime;class VoiceManager;}
namespace npcai::GameBridge {
void AttachVoice(std::shared_ptr<VoiceManager>);
void AttachRuntime(std::shared_ptr<IntelligenceRuntime>);
bool Init(Chat& chat, const Settings& settings);
void Open(bool value);
void Switch();
void Sent();
bool IsOpen();
// Só thread de renderização; projeta snapshot, sem acessar CPed.
bool Bubble(float& x,float& y,float uiWidth,float uiHeight);
void Stop();
const char* Status();
}
