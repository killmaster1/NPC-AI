import unittest,subprocess,os,hashlib,tempfile
from pathlib import Path
class NativeAudioJVM(unittest.TestCase):
 def test_pcm_validation_and_integrity_without_android_playback(self):
  root=Path(__file__).resolve().parents[1]
  candidates=[Path(os.environ.get('NPC_AI_JAVA_TOOLS',root/'build/java-tools')),root.parent/'npcai-java-tools']
  tools=next(p for p in candidates if (p/'ecj.jar').exists())
  target=root/'build/audio-jvm';target.mkdir(parents=True,exist_ok=True)
  compile=subprocess.run(['java','-jar',str(tools/'ecj.jar'),'-1.8','-proc:none','-classpath',str(tools/'android.jar'),'-d',str(target),str(root/'android/br/com/npcai/NativeAudio.java'),str(root/'tests/java/NativeAudioJvmTest.java')],capture_output=True,text=True,timeout=60)
  self.assertEqual(compile.returncode,0,compile.stdout+compile.stderr)
  with tempfile.TemporaryDirectory() as temp:
   file=Path(temp)/'integrity.bin';data='ação e coração'.encode();file.write_bytes(data)
   run=subprocess.run(['java','-cp',str(target)+os.pathsep+str(tools/'android.jar'),'NativeAudioJvmTest',str(file),hashlib.sha256(data).hexdigest()],capture_output=True,text=True,timeout=15)
   self.assertEqual(run.returncode,0,run.stdout+run.stderr)
