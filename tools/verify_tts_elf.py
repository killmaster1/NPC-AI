#!/usr/bin/env python3
"""Verify actual Android ELF ABI, min API, dependency closure and page alignment."""
import re,subprocess,sys,pathlib,struct
for file in sys.argv[1:]:
 h=subprocess.check_output(['readelf','-h',file],text=True);d=subprocess.check_output(['readelf','-d',file],text=True);s=subprocess.check_output(['readelf','-lW',file],text=True);n=subprocess.check_output(['readelf','-n',file],text=True)
 assert 'ELF64' in h and 'AArch64' in h and re.search(r'Type:\s+DYN',h),file
 deps=set(re.findall(r'Shared library: \[(.*?)\]',d));platform={'libandroid.so','libc.so','libm.so','libdl.so','liblog.so'}
 assert deps<=platform|{'libnpcai_kokoro_ort.so'},deps
 for name in deps-platform:assert (pathlib.Path(file).parent/name).is_file(),name
 assert not re.search(r'\((?:RUNPATH|RPATH)\).*?\[/',d),'Absolute build RPATH leaked'
 assert 'libc++_shared.so' not in deps,'Must isolate static libc++'
 for line in s.splitlines():
  if line.strip().startswith('LOAD'):assert int(line.split()[-1],16)>=16384,'16 KiB alignment: '+file
 assert 'Android' in n,'Expected real Android build note'
 desc=re.search(r'description data: ((?:[0-9a-f]{2} )+)',n)
 assert desc and struct.unpack('<I',bytes.fromhex(desc[1])[:4])[0]<=26,'min API >26'
 if 'kokoro_ort' not in file:
  symbols=subprocess.check_output(['readelf','--dyn-syms','--wide',file],text=True)
  for name in ['SherpaOnnxCreateOfflineTts','SherpaOnnxOfflineTtsGenerateWithCallbackWithArg','SherpaOnnxDestroyOfflineTts']:assert name in symbols,name
 print(file,'Android ARM64; min API <=26; static C++; 16 KiB LOAD; closure OK:',','.join(sorted(deps)))
