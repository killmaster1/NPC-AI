#pragma once
#include "types.h"
namespace npcai {
// Somente worker. Socket não bloqueante, prazo total, cancelamento em até um poll de 50 ms.
Result HttpChat(const Request&, const HttpConfig&, const std::atomic_bool& cancelled);
}
