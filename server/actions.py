"""Provider-independent protocol. Decisions are proposals; only the game confirms effects."""
import json
import re

ACTIONS = ('none','idle','face_player','talk_gesture','hands_up','surrender_money','flee','angry','refuse','follow_player','stop_following','cower','release_surrender')
EMOTIONS = ('neutral','afraid','angry','friendly','suspicious')
SCHEMA = {'type':'object','additionalProperties':False,'required':['dialogue','action','emotion','parameters'],
          'properties':{'dialogue':{'type':'string'},'action':{'type':'string','enum':list(ACTIONS)},
                        'emotion':{'type':'string','enum':list(EMOTIONS)},
                        'parameters':{'type':'object','additionalProperties':False,'required':['amount','duration_ms'],
                                      'properties':{'amount':{'type':'integer','minimum':0,'maximum':10000},
                                                    'duration_ms':{'type':'integer','minimum':100,'maximum':10000}}}}}


def gemini_schema(value):
    if isinstance(value,dict):
        return {k:(v.upper() if k=='type' else gemini_schema(v)) for k,v in value.items() if k!='additionalProperties'}
    if isinstance(value,list): return [gemini_schema(v) for v in value]
    return value


def _unique(pairs):
    result={}
    for k,v in pairs:
        if k in result: raise ValueError('duplicate key')
        result[k]=v
    return result


def natural_speech(text):
    text=re.sub(r'\[Ação confirmada:[^\]]*\]','',text,flags=re.IGNORECASE).strip()
    if text.startswith(('Não posso executar esse pedido nesta situação.', 'A tarefa foi interrompida ou não produziu efeito.', 'Interação interrompida:')):
        return 'Peraí, vamos conversar de boa.'
    return text or 'Peraí, fala de novo?'


def dialogue_only(text):
    return {'dialogue':text,'action':'none','parameters':{'amount':0,'duration_ms':4000},'emotion':'neutral'}


def normalize(raw,money_max=200):
    if isinstance(raw,str):
        if not raw.lstrip().startswith(('{','[')):
            return dialogue_only(natural_speech(raw.encode('utf-8')[:4000].decode('utf-8','ignore')))
        try: raw=json.loads(raw,object_pairs_hook=_unique)
        except (ValueError,RecursionError): return dialogue_only('Peraí, não entendi. Fala de novo?')
    fallback=dialogue_only('Peraí, não entendi. Fala de novo?')
    if isinstance(raw,dict) and isinstance(raw.get('dialogue'),str) and raw['dialogue'].strip():
        text=raw['dialogue'].encode('utf-8')[:4000].decode('utf-8','ignore')
        fallback=dialogue_only(natural_speech(text))
    if not isinstance(raw,dict) or set(raw)!={'dialogue','action','emotion','parameters'}: return fallback
    if not isinstance(raw['dialogue'],str) or not raw['dialogue'].strip() or raw['action'] not in ACTIONS or raw['emotion'] not in EMOTIONS: return fallback
    p=raw['parameters']
    if not isinstance(p,dict) or set(p)-{'amount','duration_ms'}: return fallback
    amount=p.get('amount',0);duration=p.get('duration_ms',4000)
    if type(amount) is not int or type(duration) is not int or not 100<=duration<=10000: return fallback
    if raw['action']=='surrender_money':
        if not 1<=amount<=min(money_max,10000): return fallback
    elif amount!=0: return fallback
    return {'dialogue':natural_speech(raw['dialogue'].encode('utf-8')[:4000].decode('utf-8','ignore')),
            'action':raw['action'],'emotion':raw['emotion'],'parameters':{'amount':amount,'duration_ms':duration}}


def validate_feedback(value):
    if value is None: return None
    if not isinstance(value,dict) or set(value)!={'request_id','action','status','amount'}: raise ValueError('action_result')
    if not isinstance(value['request_id'],str) or not re.fullmatch(r'[A-Za-z0-9_.:-]{1,160}',value['request_id']):raise ValueError('request_id')
    if value['action'] not in ACTIONS or value['status'] not in ('executed','failed','cancelled'):raise ValueError('action_result')
    if type(value['amount']) is not int or not 0<=value['amount']<=10000:raise ValueError('amount')
    return value.copy()


def encode(decision,request_id):
    result={**decision,'request_id':request_id}
    while True:
        wire=json.dumps(result,ensure_ascii=False,separators=(',',':'))
        if len(wire.encode('utf-8'))<=6000:return wire
        result['dialogue']=result['dialogue'][:max(1,len(result['dialogue'])//2)]
