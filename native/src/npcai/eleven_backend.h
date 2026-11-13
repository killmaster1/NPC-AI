#pragma once
#include "tts.h"
#include "eleven_core.h"
namespace npcai {
std::unique_ptr<VoiceBackend> MakeElevenBackend(const Settings&,const std::string& privateDirectory,ElevenTransport,std::function<std::string(const std::string&,const std::string&)> control);
}
