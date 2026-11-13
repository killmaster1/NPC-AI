#!/usr/bin/env python3
import re
import subprocess
import sys
from pathlib import Path

path = Path(sys.argv[1])
header = subprocess.check_output(["readelf", "-h", str(path)], text=True)
dynamic = subprocess.check_output(["readelf", "-d", str(path)], text=True)
exports = subprocess.check_output(["readelf", "--dyn-syms", "--wide", str(path)], text=True)
assert "ELF64" in header and "AArch64" in header and re.search(r"Type:\s+DYN", header), header
allowed = {"libc.so", "libm.so", "liblog.so", "libdl.so", "libEGL.so", "libGLESv2.so", "libandroid.so", "libz.so"}
needed = set(re.findall(r"Shared library: \[(.*?)\]", dynamic))
assert needed <= allowed, f"Dependências não empacotadas: {needed - allowed}"
for name in ("OnModLoad", "OnModUnload", "__GetModInfo", "__GetDepsList"):
    assert re.search(r"\b" + name + r"$", exports, re.M), f"Export AML ausente: {name}"
undefined = [line for line in exports.splitlines() if " UND " in line]
assert not any("_ZN" in line and any(tag in line for tag in ("CPed", "CPools", "ImGui")) for line in undefined), undefined
segments = subprocess.check_output(["readelf", "-lW", str(path)], text=True)
for line in segments.splitlines():
    if line.strip().startswith("LOAD"):
        assert int(line.split()[-1], 16) >= 16384, "Segmento não alinhado para páginas de 16 KiB"
print(f"ELF OK: ARM64, AML exports, páginas 16 KiB; NEEDED={', '.join(sorted(needed))}")
