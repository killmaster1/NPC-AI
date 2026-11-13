LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := npcai_probe
LOCAL_SRC_FILES := native/probe.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/third_party/base
LOCAL_CPPFLAGS := -std=c++17 -O2 -fvisibility=hidden -fvisibility-inlines-hidden
LOCAL_LDLIBS := -llog -ldl
LOCAL_LDFLAGS := -Wl,--exclude-libs,ALL -Wl,-z,max-page-size=16384 -Wl,--no-undefined
include $(BUILD_SHARED_LIBRARY)
