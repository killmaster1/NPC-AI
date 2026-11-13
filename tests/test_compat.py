"""Execute the actual SDK guard with a fake AML interface before any engine calls."""
from pathlib import Path
import subprocess
import tempfile
import unittest

class Compatibility(unittest.TestCase):
    def test_sdk_early_guard(self):
        root=Path(__file__).resolve().parents[1]
        text=(root/'native/compat/plugin-sdk/sdk_base.h').read_text()
        body=text[text.index('inline bool NpcAiSdkRuntimeAllowed()'):text.index('template <class Owner>')]
        source=r'''
#include <cassert>
#include <cstdint>
using uptr=uintptr_t;
#define BASE_GAME_LIBRARY "libGTASA.so"
struct Fake {
 bool compatible=false; int calls=0;
 bool HasModOfVersion(const char*,const char*) {return compatible;}
 void* GetLibHandle(const char*) {++calls; return (void*)0x1234;}
 uptr GetLib(const char*) {++calls; return 0x1000;}
 uptr GetSym(void*,const char*) {++calls; return 0x4321;}
};
Fake* aml=nullptr;
void* g_hMainGameLibrary=nullptr;
uptr g_pMainGameAddress=0;
'''+body+r'''
int main(){
 assert(GetMainLibrary()==nullptr); assert(GetMainLibraryAddress()==0);
 assert(GetMainLibrarySymbol<void*>("test")==nullptr);
 Fake old; aml=&old;
 assert(GetMainLibrary()==nullptr); assert(GetMainLibraryAddress()==0);
 assert(GetMainLibrarySymbol<void*>("test")==nullptr); assert(old.calls==0);
 Fake modern; modern.compatible=true; aml=&modern;
 assert(GetMainLibrary()==(void*)0x1234); assert(GetMainLibraryAddress()==0x1000);
 assert(GetMainLibrarySymbol<void*>("test")== (void*)0x4321); assert(modern.calls==3);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'guard.cpp'; path.write_text(source)
            binary=Path(tmp)/'guard'
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(path),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
