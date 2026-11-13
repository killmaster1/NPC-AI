LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := npcai
LOCAL_CPP_EXTENSION := .cpp
LOCAL_SRC_FILES := native/main.cpp \
 native/src/npcai/native_keyboard.cpp \
 native/src/npcai/actions.cpp \
 native/src/npcai/world_types.cpp \
 native/src/npcai/world_core.cpp \
 native/src/npcai/memory_store.cpp \
 native/src/npcai/gemini_client.cpp \
 native/src/npcai/intelligence_runtime.cpp \
 native/src/npcai/world_bridge.cpp \
 native/src/npcai/android_services.cpp \
 native/src/npcai/android_audio.cpp \
 native/src/npcai/android_eleven.cpp \
 native/src/npcai/eleven_backend.cpp \
 native/src/npcai/tts.cpp \
 native/src/npcai/kokoro_backend.cpp \
 native/src/npcai/npc_action_controller.cpp \
 native/src/npcai/engine_actions.cpp \
 native/src/npcai/types.cpp \
 native/src/npcai/settings.cpp \
 native/src/npcai/diagnostics.cpp \
 native/src/npcai/registry.cpp \
 native/src/npcai/game_bridge.cpp \
 native/src/npcai/chat.cpp \
 native/src/npcai/http_client.cpp \
 native/src/npcai/ui.cpp \
 native/renderware_backend.cpp \
 third_party/base/mod/logger.cpp \
 third_party/base/mod/thirdparty/imgui/imgui.cpp \
 third_party/base/mod/thirdparty/imgui/imgui_draw.cpp \
 third_party/base/mod/thirdparty/imgui/imgui_tables.cpp \
 third_party/base/mod/thirdparty/imgui/imgui_widgets.cpp \
 third_party/base/plugin-sdk/renderware/imgui/imgui_shared.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/native/compat $(LOCAL_PATH)/native $(LOCAL_PATH)/third_party/base $(LOCAL_PATH)/third_party/base/plugin-sdk $(LOCAL_PATH)/third_party/base/mod/thirdparty
LOCAL_CPPFLAGS := -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Wno-unused-parameter -fvisibility=hidden -fvisibility-inlines-hidden -fexceptions
LOCAL_LDLIBS := -llog -ldl
LOCAL_LDFLAGS := -Wl,--exclude-libs,ALL -Wl,-z,max-page-size=16384 -Wl,--no-undefined
include $(BUILD_SHARED_LIBRARY)
