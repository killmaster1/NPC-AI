#!/usr/bin/env python3
"""Build-time download only. No Python or network is used for voice inference on Android."""
from pathlib import Path
import json,hashlib,urllib.request,sys
root=Path(__file__).resolve().parents[1];meta=json.loads((root/'tts/voice-faber.json').read_text());out=Path(sys.argv[1] if len(sys.argv)>1 else 'build/voice-download');out.mkdir(parents=True,exist_ok=True)
for name,item in meta['files'].items():
 target=out/name
 if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest()!=item['sha256']:
  temp=target.with_suffix(target.suffix+'.part')
  with urllib.request.urlopen(item['url'],timeout=120) as response,temp.open('wb') as f:
   while True:
    block=response.read(65536)
    if not block:break
    f.write(block)
  assert hashlib.sha256(temp.read_bytes()).hexdigest()==item['sha256'],'Unexpected voice file checksum: '+name
  temp.replace(target)
 print(name,'SHA256 OK')
