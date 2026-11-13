#!/usr/bin/env python3
"""Development download of pinned class voices; never runs in the GTA process."""
from pathlib import Path
import hashlib,json,urllib.request,sys
root=Path(__file__).resolve().parents[1];catalog=json.loads((root/'tts/class-voices.json').read_text());destination=Path(sys.argv[1] if len(sys.argv)>1 else 'build/class-voice-download')
for model,item in catalog['models'].items():
 folder=destination/model;folder.mkdir(parents=True,exist_ok=True)
 for name,record in item['files'].items():
  target=folder/name
  if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest()!=record['sha256']:
   temp=target.with_suffix(target.suffix+'.part')
   with urllib.request.urlopen(record['url'],timeout=120) as response,temp.open('wb') as out:
    while True:
     b=response.read(65536)
     if not b:break
     out.write(b)
   assert hashlib.sha256(temp.read_bytes()).hexdigest()==record['sha256'],'Checksum mismatch: '+model+'/'+name
   temp.replace(target)
  print(model,name,'OK')
