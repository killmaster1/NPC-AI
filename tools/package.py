#!/usr/bin/env python3
"""ZIP instalável com binário e fontes correspondentes (incluindo licenças)."""
from pathlib import Path
import hashlib
import json
import zipfile

root = Path(__file__).resolve().parents[1]
so = root / 'libs/arm64-v8a/libnpcai.so'
if not so.is_file():
    raise SystemExit('Compile primeiro: ./build.sh')
dist = root / 'dist'
dist.mkdir(exist_ok=True)
zip_path = dist / 'NPC_AI_2.4.0_ARM64.zip'
items = []
for name in ('android', 'native', 'server', 'config', 'tests', 'tools', 'docs', '.github', 'third_party', 'libs', 'tts', 'runtime'):
    items.extend(p for p in (root / name).rglob('*') if p.is_file())
items.extend(root / name for name in ('Android.mk', 'Application.mk', 'build.sh', 'Probe.mk', 'README.md', 'CHANGELOG.md', 'LICENSE', 'NOTICE', '.gitignore'))
with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for p in sorted(items):
        relative = p.relative_to(root)
        if '__pycache__' in relative.parts or p.suffix in ('.pyc', '.o') or (p.name.startswith('.env') and p.name != '.env.example'):
            continue
        archive.write(p, Path('npcai-android') / relative, compress_type=zipfile.ZIP_STORED if p.suffix=='.gz' else zipfile.ZIP_DEFLATED)
checksums = {'libs/arm64-v8a/libnpcai.so': hashlib.sha256(so.read_bytes()).hexdigest(),
             zip_path.name: hashlib.sha256(zip_path.read_bytes()).hexdigest()}
(dist / 'SHA256SUMS').write_text(''.join(f'{sha}  {name}\n' for name, sha in checksums.items()))
print(f'{zip_path} ({zip_path.stat().st_size} bytes)')
print(json.dumps(checksums, indent=2))
