#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 tools/prepare_deps.py
npcai_ndk_path="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
if [[ -z "$npcai_ndk_path" ]] && [[ -f NDKPath.txt ]]; then read -r npcai_ndk_path < NDKPath.txt || true; fi
if [[ ! -x "$npcai_ndk_path/ndk-build" ]]; then
    echo 'Defina ANDROID_NDK_HOME para o NDK r27d (27.3.13750724).' >&2
    exit 1
fi
"$npcai_ndk_path/ndk-build" NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk NDK_APPLICATION_MK=Application.mk APP_ABI=arm64-v8a NDK_DEBUG=0 -j"${JOBS:-2}" "$@"
python3 tools/verify_elf.py libs/arm64-v8a/libnpcai.so
