#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${NPC_AI_HOST_TTS_ROOT:?Root containing models/kokoro-pt-br and verified manifest}"
: "${NPC_AI_HOST_TTS_LIBS:?Host library directory with isolated SONAMEs; never use these Linux files on Android}"
mkdir -p build
"${CXX:-g++}" -std=c++17 -O1 -pthread -Inative/src/npcai tests/tts/test_backend_real.cpp native/src/npcai/kokoro_backend.cpp -lcrypto -ldl -o build/test_kokoro_real
./build/test_kokoro_real "$NPC_AI_HOST_TTS_ROOT" "$NPC_AI_HOST_TTS_LIBS"
