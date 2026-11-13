#pragma once
#include "eleven_core.h"
#include <jni.h>
namespace npcai::AndroidEleven {
bool Init(JNIEnv*,jclass,jobject);std::string Configure(const Settings&);std::string Control(const std::string&,const std::string&);
ElevenReply Https(const std::string&,const std::string&,int,const std::atomic_bool&);
void ImportEditor();
}
