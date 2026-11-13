#!/usr/bin/env python3
"""Offline Kokoro package, retaining original model files and all phonemizer data."""
import pathlib,zipfile,sys,hashlib,json
root=pathlib.Path(__file__).resolve().parents[1];source=pathlib.Path(sys.argv[1]);target=pathlib.Path(sys.argv[2])
manifest=json.loads((root/'tts/kokoro-manifest.json').read_text())
for name,digest in manifest['files'].items():
 p=source/name;assert hashlib.file_digest(p.open('rb'),'sha256').hexdigest()==digest,name
target.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name in manifest['files']:z.write(source/name,'files/npcai-tts/models/kokoro-pt-br/'+name)
 z.write(root/'tts/kokoro-manifest.json','files/npcai-tts/models/kokoro-pt-br/manifest.json')
 z.write(root/'docs/KOKORO_2_3_0.md','INSTALACAO.md')
 z.write(root/'docs/KOKORO_LICENSES.md','LICENCAS.md')
 z.write(root/'tts/KOKORO_MODEL_CARD.md','MODEL_CARD.md')
 for p in (root/'tts/licenses').glob('*espeak*'):z.write(p,'LICENSES/'+p.name)
with zipfile.ZipFile(target) as z:assert z.testzip() is None
print(target,target.stat().st_size,'bytes; ZIP CRC verified')
