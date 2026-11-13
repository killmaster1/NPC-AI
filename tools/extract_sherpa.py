#!/usr/bin/env python3
"""Extract pinned native-build sources without following upstream example symlinks."""
import hashlib,json,sys,tarfile
from pathlib import Path

def extract(archive:Path,destination:Path,expected:str):
    with archive.open('rb') as handle:
        actual = hashlib.file_digest(handle,'sha256').hexdigest()
    if actual != expected:
        raise ValueError('Sherpa source archive SHA-256 mismatch')
    destination.mkdir(parents=True,exist_ok=True)
    with tarfile.open(archive) as source:
        # Go/Kotlin examples contain relative and absolute links; native C/C++ sources are regular files.
        members=[m for m in source.getmembers() if m.isfile() or m.isdir()]
        source.extractall(destination,members=members,filter='data')
    return len(members)

if __name__=='__main__':
    root=Path(__file__).resolve().parents[1]
    lock=json.loads((root/'tts/dependencies.json').read_text())
    count=extract(root/'third_party/sherpa/source/sherpa-v1.12.23.tar.gz',Path(sys.argv[1]),lock['sherpa_source_sha256'])
    print('Pinned Sherpa native sources safely extracted:',count,'regular files/directories; example links skipped')
