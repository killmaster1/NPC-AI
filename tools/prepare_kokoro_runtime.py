#!/usr/bin/env python3
"""Finalize NDK-built Sherpa; never substitute an official API27 prebuilt."""
import hashlib,json,pathlib,shutil,subprocess,os,sys
root=pathlib.Path(__file__).resolve().parents[1];lock=json.loads((root/'tts/dependencies.json').read_text())
source=pathlib.Path(sys.argv[1]);assert source.is_file(),'Compile Sherpa with tools/build_tts.sh first'
ort=root/'third_party/onnxruntime/android/libonnxruntime.so'
assert hashlib.file_digest(ort.open('rb'),'sha256').hexdigest()==lock['ort_android_sha256']
patchelf=os.environ.get('NPC_AI_PATCHELF','patchelf');runtime=root/'runtime/arm64-v8a';runtime.mkdir(parents=True,exist_ok=True)
shutil.copy(ort,runtime/'libnpcai_kokoro_ort.so');shutil.copy(source,runtime/'libnpcai_kokoro.so')
strip=os.environ.get('NPC_AI_STRIP') or str(pathlib.Path(os.environ['ANDROID_NDK_HOME'])/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip')
subprocess.run([strip,'--strip-unneeded',str(runtime/'libnpcai_kokoro.so')],check=True)
subprocess.run([patchelf,'--set-soname','libnpcai_kokoro_ort.so',str(runtime/'libnpcai_kokoro_ort.so')],check=True)
subprocess.run([patchelf,'--remove-rpath','--set-soname','libnpcai_kokoro.so','--replace-needed','libonnxruntime.so','libnpcai_kokoro_ort.so',str(runtime/'libnpcai_kokoro.so')],check=True)
(root/'native/generated/tts_runtime_hashes.h').write_text('// Generated from verified NDK Android binaries\n'+''.join('static constexpr const char* '+key+'="'+hashlib.file_digest((runtime/name).open('rb'),'sha256').hexdigest()+'";\n' for name,key in [('libnpcai_kokoro_ort.so','npcaiOrtHash'),('libnpcai_kokoro.so','npcaiKokoroHash')]))
print('NDK Sherpa '+lock['sherpa_version']+' + Android ONNX 1.22.0; isolated SONAMEs')
