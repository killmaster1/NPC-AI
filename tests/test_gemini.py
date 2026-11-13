import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
from urllib.error import HTTPError, URLError
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'server'))
import server as npc

class Response:
    def __init__(self,body): self.body=body
    def __enter__(self): return self
    def __exit__(self,*args): pass
    def read(self,limit): return json.dumps(self.body,ensure_ascii=False).encode()

class Gemini(unittest.TestCase):
    def test_contract_and_history(self):
        messages=[{'role':'user','content':'Olá, você é real?'},{'role':'assistant','content':'Sou, CJ.'},{'role':'user','content':'Lembra de mim?'}]
        def fake(req,timeout):
            self.assertEqual(req.full_url,'https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-lite:generateContent')
            self.assertNotIn('fake-key',req.full_url)
            self.assertIn('fake-key',req.headers.values())
            self.assertEqual(timeout,20)
            data=json.loads(req.data)
            self.assertEqual([m['role'] for m in data['contents']],['user','model','user'])
            self.assertEqual(data['contents'][0]['parts'][0]['text'],messages[0]['content'])
            self.assertIn('Grove Street',data['systemInstruction']['parts'][0]['text'])
            self.assertEqual(data['generationConfig']['responseMimeType'],'application/json')
            self.assertIn('surrender_money',data['generationConfig']['responseSchema']['properties']['action']['enum'])
            return Response({'candidates':[{'content':{'parts':[{'text':'pensamento','thought':True},{'text':'Lembro, CJ! Ação e coração.'}]}}]})
        with patch.object(npc,'urlopen',side_effect=fake):
            self.assertEqual(npc.generate_gemini(npc.Config(gemini_key='fake-key'),{'model':105},messages),'Lembro, CJ! Ação e coração.')

    def test_requires_key(self):
        with self.assertRaisesRegex(ValueError,'GEMINI_API_KEY'): npc.Service(npc.Config(mode='gemini'))

    def test_mode_routes_and_saves(self):
        service=npc.Service(npc.Config(mode='gemini',gemini_key='test'))
        with patch.object(npc,'generate_gemini',return_value='Olá, CJ!') as generate:
            self.assertEqual(service.chat(('dynamic-ped-999','c1'),'Oi',{}),'Olá, CJ!')
            self.assertEqual(len(service.history[('dynamic-ped-999','c1')]['messages']),2)
            generate.assert_called_once()

    def test_timeout_and_http_errors(self):
        for error in (URLError('timeout'),TimeoutError(),HTTPError('url',429,'rate limit',{},None)):
            with self.subTest(error=type(error).__name__),patch.object(npc,'urlopen',side_effect=error):
                with self.assertRaises(RuntimeError): npc.generate_gemini(npc.Config(gemini_key='test'),{},[])

    def test_blocked_empty_response(self):
        with patch.object(npc,'urlopen',return_value=Response({'promptFeedback':{'blockReason':'SAFETY'}})):
            with self.assertRaisesRegex(RuntimeError,'não retornou texto'): npc.generate_gemini(npc.Config(gemini_key='test'),{},[])

    def test_reply_bound_utf8(self):
        with patch.object(npc,'urlopen',return_value=Response({'candidates':[{'content':{'parts':[{'text':'ação 😀'*3000}]}}]})):
            result=npc.generate_gemini(npc.Config(gemini_key='test'),{},[])
            self.assertLessEqual(len(result.encode()),6000)
            result.encode().decode('utf-8')

    def test_invalid_model(self):
        with self.assertRaisesRegex(RuntimeError,'GEMINI_MODEL'):
            npc.generate_gemini(npc.Config(gemini_key='test',gemini_model='../unsafe'),{},[])

    def test_malformed_payload(self):
        for payload in ({'candidates':{}},{'candidates':[{'content':None}]},{'candidates':[{'content':{'parts':None}}]}):
            with self.subTest(payload=payload),patch.object(npc,'urlopen',return_value=Response(payload)):
                with self.assertRaises(RuntimeError): npc.generate_gemini(npc.Config(gemini_key='test'),{},[])

    def test_environment(self):
        with patch.dict('os.environ',{'NPC_AI_MODE':'gemini','GEMINI_API_KEY':'private-test','GEMINI_MODEL':'gemini-example'}):
            config=npc.Config.env()
        self.assertEqual(config.mode,'gemini')
        self.assertEqual(config.gemini_key,'private-test')
        self.assertEqual(config.gemini_model,'gemini-example')
