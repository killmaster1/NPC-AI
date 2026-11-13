#pragma once
#include "settings.h"
#include <jni.h>
#include <functional>
namespace npcai::AndroidServices {
void SetPauseObserver(std::function<void()>);
bool Init(JNIEnv*,jclass,jobject);
std::string Status();
std::string Configure(const Settings&,std::string& diagnostic);
std::string Https(const std::string& model,const std::string& body,int timeoutMs,const std::atomic_bool& cancel);
}
