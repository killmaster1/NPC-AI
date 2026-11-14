#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
npcai_cxx="${CXX:-g++}"
npcai_flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -pthread -Inative/src/npcai)
"$npcai_cxx" "${npcai_flags[@]}" tests/tts/test_kokoro_prosody.cpp native/src/npcai/types.cpp -o build/test_kokoro_prosody
./build/test_kokoro_prosody
"$npcai_cxx" "${npcai_flags[@]}" -c native/src/npcai/actions.cpp -o build/actions.o
"$npcai_cxx" "${npcai_flags[@]}" -c native/src/npcai/world_types.cpp -o build/world_types.o
"$npcai_cxx" "${npcai_flags[@]}" tests/test_native.cpp native/src/npcai/{types,npc_action_controller,settings,registry,chat}.cpp build/actions.o build/world_types.o -o build/test_native
"$npcai_cxx" "${npcai_flags[@]}" tests/test_http.cpp native/src/npcai/{types,http_client}.cpp build/actions.o build/world_types.o -o build/test_http
"$npcai_cxx" -std=c++17 -O1 -g -pthread -Inative/src/npcai -Ithird_party/base tests/test_ui.cpp native/src/npcai/{types,chat,ui,diagnostics,gemini_client,settings,tts}.cpp build/actions.o build/world_types.o third_party/base/mod/thirdparty/imgui/{imgui,imgui_draw,imgui_tables,imgui_widgets}.cpp third_party/base/plugin-sdk/renderware/imgui/imgui_shared.cpp -o build/test_ui
"$npcai_cxx" "${npcai_flags[@]}" tests/test_actions.cpp native/src/npcai/{types,npc_action_controller,settings,chat}.cpp build/actions.o build/world_types.o -o build/test_actions
"$npcai_cxx" "${npcai_flags[@]}" tests/test_animation.cpp native/src/npcai/{types,npc_action_controller,registry,settings,chat}.cpp build/actions.o build/world_types.o -o build/test_animation
for npcai_unit in world_core memory_store gemini_client intelligence_runtime; do
  "$npcai_cxx" "${npcai_flags[@]}" -O0 -c "native/src/npcai/$npcai_unit.cpp" -o "build/$npcai_unit.o"
done
"$npcai_cxx" "${npcai_flags[@]}" -O0 tests/test_intelligence.cpp native/src/npcai/{types,settings}.cpp build/{actions,world_types,world_core,memory_store,gemini_client,intelligence_runtime}.o -o build/test_intelligence
"$npcai_cxx" "${npcai_flags[@]}" tests/test_configuration.cpp native/src/npcai/{types,settings,diagnostics,gemini_client}.cpp build/actions.o build/world_types.o -o build/test_configuration
"$npcai_cxx" "${npcai_flags[@]}" tests/tts/test_voice.cpp native/src/npcai/{types,settings,tts}.cpp build/actions.o build/world_types.o -o build/test_voice
"$npcai_cxx" "${npcai_flags[@]}" tests/tts/test_voice_profiles.cpp native/src/npcai/{types,settings,tts,registry}.cpp build/actions.o build/world_types.o -o build/test_voice_profiles
"$npcai_cxx" "${npcai_flags[@]}" -O0 tests/tts/test_eleven.cpp native/src/npcai/{eleven_backend,actions,types,world_types,settings,tts}.cpp -o build/test_eleven
./build/test_eleven
./build/test_voice_profiles
./build/test_voice
./build/test_configuration
./build/test_intelligence
./build/test_native
./build/test_animation
./build/test_actions
./build/test_ui
python3 -m unittest discover -s tests -v
