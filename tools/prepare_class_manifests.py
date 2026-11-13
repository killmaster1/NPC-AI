#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,shutil,sys
root=Path(__file__).resolve().parents[1];source=Path(sys.argv[1]);shared=Path(sys.argv[2]);destination=Path(sys.argv[3])
(destination/'models').mkdir(parents=True,exist_ok=True);shutil.copytree(shared,destination/'espeak-ng-data',dirs_exist_ok=True)
common={str(p.relative_to(destination)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (destination/'espeak-ng-data').rglob('*') if p.is_file()}
for model in json.loads((root/'tts/class-voices.json').read_text())['models']:
 target=destination/'models'/model;shutil.copytree(source/model,target,dirs_exist_ok=True);files=dict(common)
 for p in target.rglob('*'):
  if p.is_file() and p.name!='manifest.json':files[str(p.relative_to(destination))]=hashlib.sha256(p.read_bytes()).hexdigest()
 (target/'manifest.json').write_text(json.dumps({'schema':1,'voice':model,'language':'pt-BR','revision':'2f358b6a560e3a913ebf509b2f3344f20b367f94','files':files},indent=2)+'\n')
 print(model,len(files),'files')
