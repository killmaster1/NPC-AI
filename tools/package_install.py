#!/usr/bin/env python3
from pathlib import Path
import zipfile,shutil
r=Path(__file__).resolve().parents[1];s=r/'build/install-stage-240';s.mkdir(parents=True,exist_ok=True)
items=[(r/'libs/arm64-v8a/libnpcai.so','mods/libnpcai.so'),(r/'runtime/arm64-v8a/libnpcai_kokoro.so','mods/npcai-tts/lib/libnpcai_kokoro.so'),(r/'runtime/arm64-v8a/libnpcai_kokoro_ort.so','mods/npcai-tts/lib/libnpcai_kokoro_ort.so'),(r/'config/npcai.ini','files/npcai.ini.example'),(r/'docs/ELEVENLABS_2_4_0.md','INSTALACAO.md'),(r/'docs/KOKORO_LICENSES.md','LICENCAS.md'),(r/'docs/ANDROID_ELEVENLABS_TESTS.md','TESTES_ANDROID.md'),(r/'LICENSE','LICENSE'),(r/'NOTICE','NOTICE'),(r/'third_party/onnxruntime/LICENSE','LICENSES/ONNX-Runtime-MIT'),(r/'third_party/onnxruntime/ThirdPartyNotices.txt','LICENSES/ONNX-ThirdPartyNotices.txt'),(r/'third_party/sherpa/LICENSE','LICENSES/Sherpa-Apache-2.0')]
items.extend([(r/'third_party/picosha2/LICENSE','LICENSES/PicoSHA2-MIT'),(r/'CHANGELOG.md','CHANGELOG.md'),(r/'docs/ARCHITECTURE_2_4_0.md','ARQUITETURA.md')])
items.extend((p,'LICENSES/'+p.name) for p in (r/'tts/licenses').iterdir() if p.is_file())
for source,dest in items:
 p=s/dest;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,p)
(s/'FONTES.txt').write_text('Corresponding Source GPL: NPC_AI_2.4.0_ARM64.zip entregue junto com este instalador. Inclui plugin, Sherpa e fork eSpeak exatos, scripts de build e licenças. Não contém GTA nem APK. ElevenLabs usa HTTPS Android e chave própria, sem modelo local. Kokoro FP32 é opcional, em pacote separado.\n')
a=r/'dist/NPC_AI_2.4.0_Install_ARM64.zip';a.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(a,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for p in sorted(s.rglob('*')):
  if p.is_file():z.write(p,p.relative_to(s))
with zipfile.ZipFile(a) as z:assert z.testzip() is None
print(a,a.stat().st_size,'bytes; ZIP CRC verified')
