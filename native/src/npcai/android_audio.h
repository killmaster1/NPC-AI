#pragma once
#include <jni.h>
#include <cstdint>
#include <cstddef>
#include <string>
namespace npcai::AndroidAudio {
bool Init(JNIEnv*,jclass);
bool Open(int,float);
int Write(const int16_t*,size_t);
int64_t Head();
void Close();
void Gains(float,float);
std::string Hash(const std::string&);
}
