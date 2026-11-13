import copy
import json
from pathlib import Path
import sys
import threading
import subprocess
import unittest
from http.client import HTTPConnection
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'server'))
import server as npc
from actions import normalize, encode, validate_feedback, SCHEMA, gemini_schema


def proposal(**changes):
    result={'dialogue':'Calma, vou tentar ajudar, CJ!','action':'surrender_money','emotion':'afraid','parameters':{'amount':100,'duration_ms':8000}}
    result.update(changes)
    return result


class Validation(unittest.TestCase):
    def test_valid_utf8(self):
        d=normalize(json.dumps(proposal(),ensure_ascii=False));self.assertEqual(d['action'],'surrender_money');self.assertIn('CJ',d['dialogue'])
    def test_unknown_action(self):
        for action in ('os.system','give_weapon','_ZN5CPed',None,{},True):self.assertEqual(normalize(proposal(action=action))['action'],'none')
    def test_money_bounds(self):
        for amount in (-1,0,201,10001,True,3.5,'100'):
            self.assertEqual(normalize(proposal(parameters={'amount':amount}))['action'],'none')
    def test_duplicate_and_extra_fields(self):
        self.assertEqual(normalize(json.dumps(proposal())[:-1]+',"action":"flee"}')['action'],'none')
        self.assertEqual(normalize({**proposal(),'native_command':'arbitrary'})['action'],'none')
    def test_schema_contract(self):
        self.assertFalse(SCHEMA['additionalProperties']);self.assertIn('surrender_money',SCHEMA['properties']['action']['enum']);self.assertEqual(gemini_schema(SCHEMA)['type'],'OBJECT')
    def test_legacy_safe(self):
        self.assertEqual(normalize('Olá, você é real?')['action'],'none');self.assertEqual(normalize('{invalid')['action'],'none')
    def test_feedback(self):
        f={'request_id':'s1.c2.r3','action':'hands_up','status':'executed','amount':0}
        self.assertEqual(validate_feedback(f),f)
        for key,value in (('status','shell'),('amount',-1),('request_id','../bad'),('action','eval')):
            bad={**f,key:value}
            with self.assertRaises(ValueError):validate_feedback(bad)
    def test_wire_bound(self):
        p=proposal(dialogue='\n'*4000);encoded=encode(normalize(p),'r1');self.assertLessEqual(len(encoded.encode()),6000);self.assertEqual(json.loads(encoded)['request_id'],'r1')
    def test_mock_personality(self):
        messages=[{'role':'user','content':'Passa a grana!'}]
        self.assertEqual(npc.generate_mock_action({},messages)['action'],'surrender_money')
        self.assertEqual(npc.generate_mock_action({'ped_type':6},messages)['action'],'refuse')
        self.assertIn(npc.generate_mock_action({'model':102},messages)['action'],('angry','flee','refuse'))
    def test_ordinary_greeting_without_physical_action(self):
        messages=[{'role':'user','content':'Oi'}]
        decision=npc.generate_mock_action({},messages)
        self.assertEqual(decision['action'],'none')
        self.assertIn('Oi',decision['dialogue'])
    def test_provider_prompt_separates_speech_from_posture(self):
        prompt=npc.instructions_for({})
        self.assertIn('conversa comum use action=none',prompt)
        self.assertIn('Não use face_player ou idle em toda resposta',prompt)
    def test_instructions_receipts(self):
        prompt=npc.instructions_for({'last_action_result':{'status':'failed'}});self.assertIn('não a executa',prompt);self.assertIn('last_action_result',prompt);self.assertIn('failed',prompt)


class Protocol(unittest.TestCase):
    def setUp(self):
        self.service=npc.Service(npc.Config());self.http=npc.Server(('127.0.0.1',0),self.service)
        self.thread=threading.Thread(target=self.http.serve_forever,kwargs={'poll_interval':.01},daemon=True);self.thread.start()
    def tearDown(self):self.http.shutdown();self.http.server_close();self.thread.join(1)
    def req(self,**changes):
        data={'npc_id':'dynamic-live-824','conversation_id':'c1','message':'Passa a grana!','context':{},'protocol':2,'request_id':'dynamic-live-824.c1.r1'};data.update(changes)
        conn=HTTPConnection('127.0.0.1',self.http.server_port,timeout=2);conn.request('POST','/chat',json.dumps(data,ensure_ascii=False).encode(),{'Content-Type':'application/json'});response=conn.getresponse();status=response.status;body=response.read().decode();conn.close();return status,body
    def test_structured_http(self):
        status,raw=self.req();self.assertEqual(status,200);d=json.loads(raw);self.assertEqual(d['action'],'surrender_money');self.assertEqual(d['request_id'],'dynamic-live-824.c1.r1');self.assertEqual(d['parameters']['amount'],100)
    def test_nonce_validation(self):
        for value in ('',None,'../bad',True):self.assertEqual(self.req(request_id=value)[0],400)
    def test_feedback_passed_to_provider(self):
        seen=[]
        def generator(context,messages):seen.append(copy.deepcopy(context));return proposal(action='refuse',parameters={'amount':0})
        self.service.generator=generator
        self.assertEqual(self.req(action_result={'request_id':'r0','action':'surrender_money','status':'failed','amount':0})[0],200);self.assertEqual(seen[0]['last_action_result']['status'],'failed')
    def test_invalid_model_reply_is_safe(self):
        self.service.generator=lambda c,m:proposal(action='run_shell')
        status,raw=self.req();self.assertEqual(status,200);self.assertEqual(json.loads(raw)['action'],'none')
    def test_provider_plaintext_compatibility(self):
        self.service.generator=lambda c,m:'Olá, CJ! Ação e coração.'
        status,raw=self.req();self.assertEqual(status,200);self.assertEqual(json.loads(raw)['action'],'none');self.assertIn('coração',raw)
    def test_cpp_structured_roundtrip(self):
        binary=Path(__file__).resolve().parents[1]/'build/test_http'
        run=subprocess.run([str(binary),str(self.http.server_port),'3000','','structured'],capture_output=True,text=True,timeout=5)
        self.assertEqual(run.returncode,0,run.stdout+run.stderr);self.assertIn('surrender_money',run.stdout)
    def test_isolated_actions(self):
        self.req();self.req(npc_id='other-174',request_id='other.c1.r1',message='me siga')
        self.assertNotIn('surrender_money',str(self.service.history[('other-174','c1')]))
    def test_money_config_rejects_excess(self):
        self.service.config.money_max=50
        self.service.generator=lambda c,m:proposal()
        status,raw=self.req();self.assertEqual(status,200);self.assertEqual(json.loads(raw)['action'],'none')
