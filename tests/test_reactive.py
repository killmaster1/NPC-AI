import json
import tempfile
import time
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from http.client import IncompleteRead
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'server'))
import server as npc
from actions import normalize,natural_speech

class Response:
    def __init__(self,text='Qual foi, parceiro?'):
        self.body={'candidates':[{'content':{'parts':[{'text':text}]}}]}
    def __enter__(self):return self
    def __exit__(self,*args):pass
    def read(self,limit):return json.dumps(self.body).encode()

class Clock:
    def __init__(self):self.now=0;self.delays=[]
    def monotonic(self):return self.now
    def sleep(self,delay):self.delays.append(delay);self.now+=delay

class Speech(unittest.TestCase):
    def test_unknown_keeps_valid_dialogue(self):
        reply={'dialogue':'Tá louco, mano?','action':'dog_bark','emotion':'angry','parameters':{}}
        self.assertEqual(normalize(reply)['dialogue'],reply['dialogue']);self.assertEqual(normalize(reply)['action'],'none')
    def test_bad_parameters_keep_dialogue(self):
        for p in ({'amount':-1},{'amount':100000},{'amount':True},{'duration_ms':'shell'}):
            r=normalize({'dialogue':'Deixa de onda!','action':'surrender_money','emotion':'afraid','parameters':p})
            self.assertEqual(r['action'],'none');self.assertEqual(r['dialogue'],'Deixa de onda!')
    def test_natural_history_and_separate_events(self):
        service=npc.Service(npc.Config(),generator=lambda c,m:dict(dialogue='Papo reto, parceiro.',action='angry',emotion='angry',parameters=dict(amount=0,duration_ms=4000)))
        reply=json.loads(service.chat(('NPC-A','c1'),'Oi',{},True,'r1'))
        entry=service.history[('NPC-A','c1')]
        self.assertEqual(entry['messages'][-1]['content'],'Papo reto, parceiro.');self.assertEqual(entry['events'][-1]['kind'],'proposal')
        service.chat(('NPC-A','c1'),'Tá bom',{'last_action_result':dict(request_id='r1',action='angry',status='failed',amount=0)},True,'r2')
        self.assertEqual(entry['events'][-2]['kind'],'engine_result');self.assertNotIn('Ação confirmada',str(entry['messages']));self.assertEqual(reply['action'],'angry')
    def test_old_structured_memory_migration(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'memory.json'
            old=dict(dialogue='Pode falar. [Ação confirmada: angry.]',action='angry',emotion='angry',parameters={})
            path.write_text(json.dumps([dict(npc_id='npcA',conversation_id='c1',at=time.time(),messages=[dict(role='assistant',content=json.dumps(old))])]))
            service=npc.Service(npc.Config(memory_file=str(path)))
            self.assertEqual(service.history[('npcA','c1')]['messages'][0]['content'],'Pode falar.')
    def test_technical_old_text_removed(self):
        self.assertEqual(natural_speech('Qual foi? [Ação confirmada: face_player.]'),'Qual foi?')
        self.assertNotIn('entrega',natural_speech('Não posso executar esse pedido nesta situação. Nenhuma entrega foi feita.'))
    def test_retry_request_idempotent(self):
        calls=[]
        def generator(c,m):calls.append(1);return 'Oi, parceiro.'
        s=npc.Service(npc.Config(),generator=generator)
        one=s.chat(('npc','c1'),'Oi',{},True,'r1');two=s.chat(('npc','c1'),'Oi',{},True,'r1')
        self.assertEqual(one,two);self.assertEqual(len(calls),1);self.assertEqual(len(s.history[('npc','c1')]['messages']),2)
        with self.assertRaises(ValueError):s.chat(('npc','c1'),'outra pergunta',{},True,'r1')

class Personality(unittest.TestCase):
    def test_stable_by_identity(self):
        a=npc.personality_for('npcA',{});self.assertEqual(a,npc.personality_for('npcA',{}));self.assertNotEqual(a,npc.personality_for('npcB',{}))
    def test_isolated_and_clear_safe(self):
        seen=[]
        s=npc.Service(npc.Config(),generator=lambda c,m:seen.append(c['personality'].copy()) or 'Oi')
        s.chat(('npcA','c1'),'Oi',{});s.chat(('npcA','c2'),'Oi',{});s.reset(('npcA','c1'));s.chat(('npcA','c1'),'Oi',{})
        self.assertTrue(all(p==seen[0] for p in seen))
    def test_variation_disabled(self):self.assertEqual(npc.personality_for('A',{'personality_variation':False}),npc.personality_for('B',{'personality_variation':False}))
    def test_trait_validation(self):
        for traits in ({'courage':1},dict(courage=True,aggression=50,kindness=50,patience=50,fear=20,trust=30)):
            with self.assertRaises(ValueError):npc.validate(dict(npc_id='npc',message='Oi',context={'personality':traits}))
    def test_different_robbery_reactions(self):
        scared=dict(courage=20,aggression=10,kindness=50,patience=50,fear=80,trust=20)
        aggressive={**scared,'aggression':90}
        msg=[dict(role='user',content='Mãos pra cima! Passa a grana!')]
        self.assertEqual(npc.generate_mock_action({'personality':scared},msg)['action'],'surrender_money')
        self.assertEqual(npc.generate_mock_action({'personality':aggressive},msg)['action'],'angry')
    def test_informal_commands(self):
        context={'personality':dict(courage=20,aggression=10,kindness=80,patience=50,fear=80,trust=60)}
        for phrase in ('Me segue','Me siga','Cola comigo','Vem cmg'):
            self.assertEqual(npc.generate_mock_action(context,[dict(role='user',content=phrase)])['action'],'follow_player')
        for phrase in ('Mãos pra cima','Levanta as mãos'):
            self.assertEqual(npc.generate_mock_action(context,[dict(role='user',content=phrase)])['action'],'hands_up')
        for phrase in ('Sai fora','Some daqui'):
            self.assertEqual(npc.generate_mock_action(context,[dict(role='user',content=phrase)])['action'],'flee')
        for phrase in ('Abaixa as mãos','Pode ir embora'):
            self.assertEqual(npc.generate_mock_action({**context,'interaction_state':'surrendering'},[dict(role='user',content=phrase)])['action'],'release_surrender')
        self.assertEqual(npc.generate_mock_action(context,[dict(role='user',content='Late')])['action'],'none')

class Retries(unittest.TestCase):
    def config(self,**kw):return npc.Config(gemini_key='fake',retry_base=.1,**kw)
    def test_503_then_success(self):
        clock=Clock()
        with patch.object(npc.time,'monotonic',clock.monotonic),patch.object(npc.time,'sleep',clock.sleep),patch.object(npc.random,'uniform',return_value=.01),patch.object(npc,'urlopen',side_effect=[HTTPError('url',503,'busy',{},None),Response()]) as request:
            self.assertEqual(npc.generate_gemini(self.config(),{},[]),'Qual foi, parceiro?');self.assertEqual(request.call_count,2);self.assertAlmostEqual(clock.delays[0],.11)
    def test_retry_produces_one_proposal(self):
        clock=Clock();decision=json.dumps(dict(dialogue='Calma, vou pegar a grana.',action='surrender_money',emotion='afraid',parameters=dict(amount=100,duration_ms=1000)))
        service=npc.Service(self.config(mode='gemini'))
        with patch.object(npc.time,'monotonic',clock.monotonic),patch.object(npc.time,'sleep',clock.sleep),patch.object(npc,'urlopen',side_effect=[HTTPError('url',503,'busy',{},None),Response(decision)]) as request:
            wire=service.chat(('npcA','c1'),'Passa a grana',{},True,'r1')
            self.assertEqual(request.call_count,2)
        self.assertEqual(json.loads(wire)['request_id'],'r1');self.assertEqual(len(service.history[('npcA','c1')]['messages']),2)
        self.assertEqual(len([e for e in service.history[('npcA','c1')]['events'] if e['kind']=='proposal']),1)
    def test_incomplete_network_read_retried(self):
        clock=Clock()
        with patch.object(npc.time,'monotonic',clock.monotonic),patch.object(npc.time,'sleep',clock.sleep),patch.object(npc,'urlopen',side_effect=[IncompleteRead(b'partial'),Response()]) as request:
            self.assertEqual(npc.generate_gemini(self.config(),{},[]),'Qual foi, parceiro?');self.assertEqual(request.call_count,2)
    def test_limited_503(self):
        clock=Clock()
        with patch.object(npc.time,'monotonic',clock.monotonic),patch.object(npc.time,'sleep',clock.sleep),patch.object(npc,'urlopen',side_effect=lambda *a,**kw:(_ for _ in ()).throw(HTTPError('url',503,'busy',{},None))) as request:
            with self.assertRaises(npc.ProviderFailure):npc.generate_gemini(self.config(),{},[])
            self.assertEqual(request.call_count,3)
    def test_credentials_model_not_retried(self):
        for code in (400,401,403,404):
            with patch.object(npc,'urlopen',side_effect=HTTPError('url',code,'bad',{},None)) as request:
                with self.assertRaises(npc.ProviderFailure):npc.generate_gemini(self.config(),{},[])
                self.assertEqual(request.call_count,1)
    def test_429_retry_after(self):
        clock=Clock()
        with patch.object(npc.time,'monotonic',clock.monotonic),patch.object(npc.time,'sleep',clock.sleep),patch.object(npc,'urlopen',side_effect=[HTTPError('url',429,'busy',{'Retry-After':'2'},None),Response()]):
            npc.generate_gemini(self.config(),{},[]);self.assertEqual(clock.delays,[2])
    def test_total_budget(self):
        clock=Clock()
        with patch.object(npc.time,'monotonic',clock.monotonic),patch.object(npc.time,'sleep',clock.sleep),patch.object(npc,'urlopen',side_effect=HTTPError('url',503,'busy',{'Retry-After':'30'},None)) as request:
            with self.assertRaises(npc.ProviderFailure):npc.generate_gemini(self.config(timeout=1),{},[])
            self.assertEqual(request.call_count,1);self.assertFalse(clock.delays)
    def test_budget_from_native_client(self):
        s=npc.Service(self.config(mode='gemini'))
        with patch.object(npc,'generate_gemini',return_value='Oi') as generate:
            s.chat(('npc','c1'),'Oi',{},True,'r1',2000)
            self.assertLessEqual(generate.call_args.args[0].timeout,.5)
    def test_failed_provider_does_not_add_npc_error_to_history(self):
        s=npc.Service(self.config(mode='gemini'))
        with patch.object(npc,'generate_gemini',side_effect=npc.ProviderFailure('Gemini')):
            with self.assertRaises(npc.ProviderFailure):s.chat(('npc','c1'),'Oi',{},True,'r1')
        self.assertEqual(s.history[('npc','c1')]['messages'],[]);self.assertFalse(s.active)
