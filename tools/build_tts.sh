#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
npcai_cmake="${NPC_AI_CMAKE:-cmake}"
npcai_ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
[[ -f "$npcai_ndk/build/cmake/android.toolchain.cmake" ]] || { echo 'Defina ANDROID_NDK_HOME para r27d';exit 1; }
npcai_source="${NPC_AI_SHERPA_SOURCE:-$PWD/build/sherpa-source}"
if [[ ! -f "$npcai_source/CMakeLists.txt" ]];then
 python3 - "$npcai_source" <<'PY'
import pathlib,sys,json,hashlib,tarfile
r=pathlib.Path('.');lock=json.loads((r/'tts/dependencies.json').read_text());p=r/'third_party/sherpa/source/sherpa-v1.12.23.tar.gz'
assert hashlib.file_digest(p.open('rb'),'sha256').hexdigest()==lock['sherpa_source_sha256']
with tarfile.open(p) as t:t.extractall(sys.argv[1],filter='data')
PY
fi
export SHERPA_ONNXRUNTIME_INCLUDE_DIR="$PWD/third_party/onnxruntime/include"
export SHERPA_ONNXRUNTIME_LIB_DIR="$PWD/third_party/onnxruntime/android"
# Exact GPL eSpeak source can be used from the included corresponding-source archive.
mkdir -p build/kokoro-android
for npcai_archive in third_party/sherpa/source/espeak-ng-*.zip; do
 [[ ! -f "$npcai_archive" ]] || cp "$npcai_archive" build/kokoro-android/
done
"$npcai_cmake" -S tts -B build/kokoro-android -G Ninja \
 -DNPC_AI_SHERPA_SOURCE="$npcai_source" \
 -DCMAKE_TOOLCHAIN_FILE="$npcai_ndk/build/cmake/android.toolchain.cmake" \
 -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=26 -DANDROID_STL=c++_static \
 -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release
"$npcai_cmake" --build build/kokoro-android --target npcai_kokoro -j"${JOBS:-2}"
NPC_AI_STRIP="$npcai_ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" python3 tools/prepare_kokoro_runtime.py build/kokoro-android/libnpcai_kokoro.so
python3 tools/verify_tts_elf.py runtime/arm64-v8a/*.so
