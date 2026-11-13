#!/usr/bin/env python3
"""Dependências já vendorizadas, verificadas por SHA-256; nenhum HEAD móvel no CI."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / "third_party/manifest.json").read_text())
for name, expected in manifest["files"].items():
    p = root / name
    if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest() != expected:
        raise SystemExit(f"Dependência ausente ou modificada: {name}")
print(f"Dependências verificadas: {len(manifest['files'])} arquivos vendorizados")
