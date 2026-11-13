#!/usr/bin/env python3
"""Split any delivery archive into <=20,000,000-byte parts and verify the reconstructed hash."""
import hashlib,json,sys
from pathlib import Path
source=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True);limit=20_000_000
parts=[];whole=hashlib.sha256();reconstructed=hashlib.sha256()
with source.open('rb') as f:
 n=1
 while True:
  b=f.read(limit)
  if not b:break
  whole.update(b);p=out/(source.name+f'.part{n:03d}');p.write_bytes(b);p.chmod(0o644)
  checked=p.read_bytes();assert checked==b;reconstructed.update(checked)
  parts.append({'file':p.name,'bytes':len(b),'sha256':hashlib.sha256(checked).hexdigest()});n+=1
assert whole.hexdigest()==reconstructed.hexdigest()
(out/(source.name+'.partes.sha256')).write_text(''.join(p['sha256']+'  '+p['file']+'\n' for p in parts))
(out/(source.name+'.sha256')).write_text(whole.hexdigest()+'  '+source.name+'\n')
(out/(source.name+'.reconstruir.txt')).write_text('Na pasta com todas as partes deste arquivo:\nsha256sum -c '+source.name+'.partes.sha256\ncat '+source.name+'.part[0-9][0-9][0-9] > '+source.name+'\nsha256sum -c '+source.name+'.sha256\nunzip -t '+source.name+'\n')
print(json.dumps({'source':str(source),'sha256':whole.hexdigest(),'parts':parts},indent=2))
