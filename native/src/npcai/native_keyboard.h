#pragma once
#include <string>
namespace npcai::NativeKeyboard {
// Android UI thread owns the editor; only UTF-8 values cross back to the game thread.
bool Init(std::string& version);
bool Show(const std::string& initial,const std::string& history="");
bool Take(std::string& text);
void Invalidate();
bool Active();
void Heartbeat();
}
