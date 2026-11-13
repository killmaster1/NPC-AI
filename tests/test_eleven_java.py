import unittest,subprocess,os
from pathlib import Path
class ElevenJava(unittest.TestCase):
 def test_native_eleven_https_mocked(self):
  r=Path(__file__).resolve().parents[1]
  candidates=[Path(os.environ.get('NPC_AI_JAVA_TOOLS',r/'build/java-tools')),r.parent/'npcai-java-tools']
  tools=next(p for p in candidates if (p/'ecj.jar').exists())
  target=r/'build/eleven-jvm';target.mkdir(parents=True,exist_ok=True)
  sources=[r/'android/br/com/npcai'/n for n in ['NativeEleven.java','NativeEditor.java','NativeAudio.java']]+[r/'tests/java/br/com/npcai/NativeElevenJvmTest.java']
  c=subprocess.run(['java','-jar',str(tools/'ecj.jar'),'-1.8','-proc:none','-classpath',str(tools/'android.jar'),'-d',str(target)]+list(map(str,sources)),capture_output=True,text=True,timeout=60)
  self.assertEqual(c.returncode,0,c.stdout+c.stderr)
  run=subprocess.run(['java','-cp',str(target)+os.pathsep+str(tools/'android.jar'),'br.com.npcai.NativeElevenJvmTest'],capture_output=True,text=True,timeout=20)
  self.assertEqual(run.returncode,0,run.stdout+run.stderr)
