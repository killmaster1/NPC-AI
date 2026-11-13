#pragma once
#include "chat.h"
#include "diagnostics.h"
#include "tts.h"
namespace NpcUi {
void Init(npcai::Chat& chat,float bubbleScreenOffsetY=6,std::shared_ptr<npcai::Diagnostics> diagnostics={},std::shared_ptr<npcai::VoiceManager> voice={});
void Draw();
void Shutdown();
}
