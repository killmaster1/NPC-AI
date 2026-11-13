import os
from pathlib import Path
import subprocess
import unittest

class AndroidServicesJVM(unittest.TestCase):
    def test_services_on_jvm(self):
        root=Path(__file__).resolve().parents[1]
        candidates=[Path(os.environ.get('NPC_AI_JAVA_TOOLS',root/'build/java-tools')),root.parent/'npcai-java-tools']
        tools=next((p for p in candidates if (p/'ecj.jar').exists() and (p/'android.jar').exists()),None)
        self.assertIsNotNone(tools,'Execute ./tools/build_keyboard.sh para preparar os jars')
        target=root/'build/java-host-tests';target.mkdir(parents=True,exist_ok=True)
        run=subprocess.run(['java','-jar',str(tools/'ecj.jar'),'-1.8','-proc:none','-classpath',str(tools/'android.jar'),'-d',str(target),str(root/'android/br/com/npcai/NativeServices.java'),str(root/'tests/java/br/com/npcai/NativeServicesJvmTest.java')],capture_output=True,text=True,timeout=60)
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        run=subprocess.run(['java','-cp',str(target)+os.pathsep+str(tools/'android.jar'),'br.com.npcai.NativeServicesJvmTest'],capture_output=True,text=True,timeout=20)
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        self.assertIn('passed',run.stdout)

    def test_default_tls_rejects_untrusted_certificate(self):
        import ssl
        import tempfile
        import threading
        from http.server import HTTPServer,BaseHTTPRequestHandler
        root=Path(__file__).resolve().parents[1]
        candidates=[Path(os.environ.get('NPC_AI_JAVA_TOOLS',root/'build/java-tools')),root.parent/'npcai-java-tools']
        tools=next(p for p in candidates if (p/'ecj.jar').exists())
        target=root/'build/java-tls-tests';target.mkdir(parents=True,exist_ok=True)
        compile=subprocess.run(['java','-jar',str(tools/'ecj.jar'),'-1.8','-proc:none','-d',str(target),str(root/'tests/java/TlsPolicyTest.java')],capture_output=True,text=True,timeout=60)
        self.assertEqual(compile.returncode,0,compile.stdout+compile.stderr)
        with tempfile.TemporaryDirectory() as tmp:
            cert=Path(tmp)/'cert.pem';key=Path(tmp)/'key.pem'
            subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-keyout',str(key),'-out',str(cert),'-days','1','-subj','/CN=localhost'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            server=HTTPServer(('127.0.0.1',0),BaseHTTPRequestHandler)
            context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(cert,key)
            server.socket=context.wrap_socket(server.socket,server_side=True)
            thread=threading.Thread(target=server.serve_forever,kwargs={'poll_interval':.01},daemon=True);thread.start()
            try:
                run=subprocess.run(['java','-cp',str(target),'TlsPolicyTest',f'https://127.0.0.1:{server.server_port}/'],capture_output=True,text=True,timeout=10)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr);self.assertIn('rejected',run.stdout)
            finally:server.shutdown();server.server_close();thread.join(1)
