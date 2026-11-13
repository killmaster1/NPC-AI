#!/usr/bin/env python3
"""Ponte loopback para Termux. Python 3.10+, somente biblioteca padrão."""
from __future__ import annotations

from actions import ACTIONS, SCHEMA, gemini_schema, normalize, validate_feedback, encode, natural_speech
import hmac
import hashlib
import random
import unicodedata
import email.utils
import json
import os
import re
import socket
import threading
import time
from collections import OrderedDict
from dataclasses import dataclass, replace
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from http.client import IncompleteRead
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

ID = re.compile(r"[A-Za-z0-9_.:-]{1,160}\Z")
MAX_BODY = 8192
MAX_MESSAGE = 1200  # bytes UTF-8, igual ao plugin
MAX_REPLY = 6000
MAX_MEMORY_FILE = 8 * 1024 * 1024


def bounded_int(name, default, low, high):
    value = int(os.getenv(name, str(default)))
    if not low <= value <= high:
        raise ValueError(f"{name} deve estar entre {low} e {high}")
    return value


@dataclass
class Config:
    mode: str = "mock"
    model: str = "gpt-4.1-mini"
    token: str = ""
    api_key: str = ""
    gemini_key: str = ""
    gemini_model: str = "gemini-2.5-flash-lite"
    timeout: float = 20.0
    history_limit: int = 24
    max_conversations: int = 128
    max_inflight: int = 4
    ttl: float = 3600.0
    memory_file: str = ""
    money_max: int = 200
    structured_output: bool = True
    retry_attempts: int = 2
    retry_base: float = 0.35

    @classmethod
    def env(cls):
        return cls(
            retry_attempts=bounded_int("NPC_AI_API_RETRIES",2,0,3),
            money_max=bounded_int("NPC_AI_MONEY_MAX",200,0,10000),
            structured_output=os.getenv("NPC_AI_STRUCTURED_OUTPUT","1")=="1",
            mode=os.getenv("NPC_AI_MODE", "mock").strip().lower(),
            model=os.getenv("OPENAI_MODEL", "gpt-4.1-mini"),
            gemini_key=os.getenv("GEMINI_API_KEY", ""),
            gemini_model=os.getenv("GEMINI_MODEL", "gemini-2.5-flash-lite"),
            token=os.getenv("NPC_AI_TOKEN", ""), api_key=os.getenv("OPENAI_API_KEY", ""),
            timeout=bounded_int("NPC_AI_API_TIMEOUT", 20, 1, 25),
            history_limit=bounded_int("NPC_AI_HISTORY", 24, 2, 100) // 2 * 2,
            max_conversations=bounded_int("NPC_AI_MAX_CONVERSATIONS", 128, 1, 1024),
            max_inflight=bounded_int("NPC_AI_MAX_INFLIGHT", 4, 1, 16),
            ttl=bounded_int("NPC_AI_MEMORY_TTL", 3600, 30, 86400),
            memory_file=os.getenv("NPC_AI_MEMORY_FILE", ""))


def validate(data, chat=True):
    if not isinstance(data, dict):
        raise ValueError("Esperado objeto JSON")
    npc = data.get("npc_id")
    conversation = data.get("conversation_id", "default")
    if not isinstance(npc, str) or not ID.fullmatch(npc):
        raise ValueError("npc_id inválido (1–160 caracteres ASCII)")
    if not isinstance(conversation, str) or not ID.fullmatch(conversation):
        raise ValueError("conversation_id inválido")
    if not chat:
        return (npc, conversation), "", {}
    message = data.get("message")
    if not isinstance(message, str) or not message.strip() or len(message.encode("utf-8")) > MAX_MESSAGE:
        raise ValueError("Mensagem deve conter 1–1200 bytes UTF-8")
    if any(ord(c) < 32 and c not in "\n\t" for c in message):
        raise ValueError("Mensagem contém controles inválidos")
    context = data.get("context", {})
    if not isinstance(context, dict):
        raise ValueError("context deve ser objeto")
    clean = {}
    for key, low, high in (("model", 0, 65535), ("ped_type", 0, 255), ("ped_state", 0, 255),
                           ("hour", 0, 23), ("minute", 0, 59), ("area", 0, 255)):
        if key in context:
            val = context[key]
            if type(val) is not int or not low <= val <= high:
                raise ValueError(f"context.{key} inválido")
            clean[key] = val
    if "in_vehicle" in context:
        if type(context["in_vehicle"]) is not bool:
            raise ValueError("context.in_vehicle inválido")
        clean["in_vehicle"] = context["in_vehicle"]
    if "observed_taxi_driver" in context:
        if type(context["observed_taxi_driver"]) is not bool:
            raise ValueError("context.observed_taxi_driver inválido")
        clean["observed_taxi_driver"] = context["observed_taxi_driver"]
    if "region_level" in context:
        if type(context["region_level"]) is not int or not 0 <= context["region_level"] <= 4:
            raise ValueError("region_level inválido")
        clean["region_level"] = context["region_level"]
    if 'personality_variation' in context:
        if type(context['personality_variation']) is not bool:raise ValueError('personality_variation')
        clean['personality_variation']=context['personality_variation']
    if 'personality' in context:
        traits=context['personality'];names={'courage','aggression','kindness','patience','fear','trust'}
        if not isinstance(traits,dict) or set(traits)!=names or any(type(v) is not int or not 0<=v<=100 for v in traits.values()):raise ValueError('personality')
        clean['personality']=traits.copy()
    if 'interaction_state' in context:
        if context['interaction_state'] not in ('normal','conversing','surrendering','following','fleeing'):raise ValueError('interaction_state')
        clean['interaction_state']=context['interaction_state']
    return (npc, conversation), message.strip(), clean


def persona(context):
    # Modelos originais: Families 105..107 e Ballas 102..104. 261/262 são crupiês, não taxistas.
    # Classificação narrativa inferida; o tipo/modelo numérico é o dado da engine.
    model, pedtype = context.get("model"), context.get("ped_type")
    if pedtype == 6:
        return "Policial", "Policial autoritário e cauteloso; aborda CJ com firmeza."
    if model in (105, 106, 107):
        return "Membro das Families", "Grove Street Families, linguagem informal e conhecimento do bairro."
    if model in (102, 103, 104):
        return "Membro dos Ballas", "Ballas desconfiado e provocativo diante de CJ."
    if context.get("observed_taxi_driver") is True:
        return "Taxista", "Taxista conversador, conhece Los Santos e reclama do trânsito."
    return "Pedestre", "Morador comum de San Andreas, vida cotidiana e opiniões próprias."


def generate_mock(context, messages):
    name, _ = persona(context)
    msg = messages[-1]["content"]
    lower = msg.lower()
    if "simula" in lower or "jogo" in lower:
        return f"{name}: Uma simulação? Pra mim essa rua é bem real, CJ. O que te fez pensar nisso?"
    if "nome" in lower:
        return f"Pode me chamar de {name}, CJ."
    if "lembra" in lower and len(messages) > 1:
        return f"Você tinha dito: {messages[-3]['content'][:160]}. Eu lembro, CJ."
    return f"{name}: Você disse “{msg[:180]}”. Me conta mais, CJ."


def personality_for(npc_id,context):
    if context.get('personality_variation') is False:
        return dict(courage=50,aggression=50,kindness=50,patience=50,fear=20,trust=30)
    if 'personality' in context:return context['personality'].copy()
    data=hashlib.sha256(npc_id.encode()).digest()
    values=[15+b%71 for b in data[:6]]
    traits=dict(zip(('courage','aggression','kindness','patience','fear','trust'),values))
    traits['fear']//=2;traits['trust']//=2
    return traits


def generate_mock_action(context,messages):
    text=''.join(c for c in unicodedata.normalize('NFKD',messages[-1]['content'].lower()) if not unicodedata.combining(c))
    model=context.get('model',0);cop=context.get('ped_type')==6;gang=model in (102,103,104,105,106,107)
    traits=context.get('personality',dict(courage=25,aggression=20,kindness=60,patience=50,fear=60,trust=40))
    action='none';emotion='friendly';amount=0
    dialogue=generate_mock(context,messages).split(': ',1)[-1].replace(', CJ','')
    if text.strip() in ('oi','ola','e ai','salve'):dialogue='Oi! Qual foi, parceiro?'
    if any(x in text for x in ('abaixa as maos','abaixe as maos','pode baixar','pode abaixar','baixa as maos')):
        action='release_surrender';dialogue='Tá, sem onda então.'
    elif any(x in text for x in ('pare de seguir','para de seguir','nao me segue','pode ir embora')):
        action='release_surrender' if context.get('interaction_state')=='surrendering' else 'stop_following';dialogue='Fechou, vou ficar na minha.'
    elif any(x in text for x in ('passa a grana','assalto','me da dinheiro','entrega o dinheiro','solta o dinheiro')):
        if cop:action='refuse';emotion='angry';dialogue='Tá de sacanagem? Baixa essa bola agora.'
        elif gang or traits['aggression']>=70:action='angry';emotion='angry';dialogue='Tá achando que tá falando com quem, cuzão?'
        elif traits['courage']>70 and traits['fear']<35:action='refuse';emotion='suspicious';dialogue='Mano do céu, olha esse maluco! Sai fora.'
        elif traits['courage']>55 and traits['fear']<60:action='flee';emotion='afraid';dialogue='Que porra é essa? Vou sair daqui!'
        else:action='surrender_money';emotion='afraid';amount=min(100,context.get('money_limit',200));dialogue='Calma, mano! Vou pegar a grana, não me machuca.'
        if action=='surrender_money' and amount<1:action='refuse';amount=0;dialogue='Tô sem grana, pô.'
    elif any(x in text for x in ('me segue','me siga','cola comigo','vem cmg','vem comigo')):
        action='refuse' if cop or gang or traits['kindness']<35 or traits['trust']<20 else 'follow_player'
        dialogue='Não vou nessa, não.' if action=='refuse' else 'Bora, parceiro. Vou colar contigo.'
    elif any(x in text for x in ('maos pra cima','maos para cima','levanta as maos','maos ao alto')):
        action='refuse' if cop or gang or traits['aggression']>75 else 'hands_up';emotion='afraid' if action=='hands_up' else 'angry';dialogue='Calma, pô! Sem confusão.' if action=='hands_up' else 'Tá tirando com a minha cara?'
    elif any(x in text for x in ('sai fora','some daqui','fuja','corra')):action='flee';emotion='afraid';dialogue='Já tô saindo, se liga!'
    elif 'abaixe' in text or 'medo' in text:action='cower';emotion='afraid';dialogue='Vou me proteger aqui, mano.'
    elif 'olhe' in text or 'olha pra mim' in text:action='face_player';dialogue='Qual foi? Pode falar.'
    elif 'parado' in text:action='idle';dialogue='Tá, tô por aqui.'
    elif text.strip()=='late':action='none';dialogue='Tá me achando com cara de cachorro, é? Se liga!'
    return dict(dialogue=dialogue,action=action,emotion=emotion,parameters=dict(amount=amount,duration_ms=8000 if amount else 4000))


def instructions_for(context):
    name, style = persona(context)
    instructions = (
        "Interprete um personagem fictício de GTA San Andreas falando com CJ, em português brasileiro. "
        f"Identificação narrativa: {name}. Personalidade: {style} "
        "Fale português brasileiro coloquial de rua, espontâneo, com gírias, ironia e palavrões quando fizerem sentido. "
        "Referências de tom (não copie nem repita como bordões): Qual foi, parceiro? Tá louco, mano? "
        "Se liga, irmão! Tá de sacanagem? Sai fora, cuzão! Papo reto! Bora, carai! "
        "Não xingue em toda fala: ajuste à simpatia, paciência, confiança, agressividade e medo individuais. "
        "Families: descontraído e leal; Ballas: provocativo e desconfiado; policial: direto, impaciente e autoritário; "
        "taxista: bem-humorado e reclamão; civil: use os traços individuais para variar timidez, deboche ou simpatia. "
        "Use normalmente uma ou duas frases; alongue somente se o contexto justificar. "
        "Não use CJ em toda resposta nem tom de assistente, sermão ou formalidade. "
        "Os traços de personality são características narrativas estáveis por identidade, não medições da engine; "
        "não revele seus valores numéricos. Medo e confiança podem mudar com efeitos confirmados. "
        "Mantenha o papel. Converse sobre assuntos variados; "
        "se CJ falar em simulação, reaja como personagem com curiosidade ou desconfiança. "
        "Não responda como assistente genérico. Não diga que executou ações na engine. "
        "Os dados numéricos abaixo vieram do jogo; a personalidade é inferida do modelo original e pode "
        "não corresponder a skins modificadas. Não invente bairro, missão, profissão ou situação como "
        "se fossem leituras da engine. in_vehicle só informa que está em veículo, não que dirige. "
        "region_level é uma região ampla, não um bairro. Contexto observado: "
        + json.dumps(context, ensure_ascii=False))
    return instructions + (
        "\nRetorne somente um objeto JSON com dialogue, action, emotion, parameters. "
        "action deve ser uma destas: " + ", ".join(ACTIONS) + ". "
        "Para saudações, perguntas e conversa comum use action=none. Gestos são opcionais: "
        "se desejar, use talk_gesture, angry ou refuse; dialogue deve continuar sendo uma fala, sem narrar gestos concluídos. "
        "Não use face_player ou idle em toda resposta: o plugin já cuida da postura inicial. "
        "parameters contém amount (0 se não entregar dinheiro) e duration_ms (100..10000). "
        "emotion: neutral, afraid, angry, friendly ou suspicious. "
        "Você propõe a ação, não a executa. Nunca declare entrega, fuga, rendição ou acompanhamento já concluídos: "
        "use intenção futura. Somente last_action_result do cliente confirma o que aconteceu. "
        "Pedidos de assalto não exigem obediência: civis podem se render, gangues podem provocar/recusar/fugir, "
        "policiais devem recusar. Não prometa prisão, reforços ou transporte, ainda não implementados. "
        "Dinheiro: amount de 1 até " + str(context.get('money_limit',200)) + ". "
        "Interprete abreviações e intenção no contexto: vem cmg, cola comigo, solta o dinheiro, abaixa as mãos. "
        "Use release_surrender quando aceitar abaixar as mãos; stop_following para deixar de seguir. "
        "Hands_up/surrender_money e follow_player são persistentes: duration_ms não os encerra. "
        "Nunca sugira uma animação de cachorro para Late; pode provocar ou recusar verbalmente. "
        "Não inclua diagnósticos técnicos, colchetes de confirmação, JSON ou nomes de ações na fala. "
        "Histórico de propostas JSON não é confirmação física; se houve falha respeite o resultado da engine."
    )


class ProviderFailure(RuntimeError):
    def __init__(self,provider,status=503):
        self.status=status
        super().__init__(f'{provider} temporariamente indisponível. Tente novamente em instantes.' if status in (429,503) else f'Confira a configuração do provedor {provider} (HTTP {status}).')


def provider_payload(req,config,provider):
    deadline=time.monotonic()+config.timeout
    for attempt in range(config.retry_attempts+1):
        remaining=deadline-time.monotonic()
        if remaining<=0:raise ProviderFailure(provider)
        retry_after=0.0
        try:
            with urlopen(req,timeout=config.timeout if attempt==0 else remaining) as response:
                if hasattr(response,'read1'):
                    chunks=[];total=0
                    while total<=262144:
                        remaining=deadline-time.monotonic()
                        if remaining<=0:raise ProviderFailure(provider)
                        sock=getattr(getattr(getattr(response,'fp',None),'raw',None),'_sock',None)
                        if sock is not None:sock.settimeout(remaining)
                        part=response.read1(min(8192,262145-total))
                        if not part:break
                        chunks.append(part);total+=len(part)
                    raw=b''.join(chunks)
                else:raw=response.read(262145)
            if time.monotonic()>deadline:raise ProviderFailure(provider)
            if len(raw)>262144:raise RuntimeError('Resposta da API excedeu o limite')
            return json.loads(raw)
        except HTTPError as exc:
            code=exc.code
            value=(exc.headers or {}).get('Retry-After','0')
            try:retry_after=max(0,float(value))
            except (ValueError,TypeError):
                try:retry_after=max(0,email.utils.parsedate_to_datetime(value).timestamp()-time.time())
                except (ValueError,TypeError,OverflowError):retry_after=0
            exc.close()
            if code not in (429,500,502,503,504):raise ProviderFailure(provider,code) from exc
        except (URLError,TimeoutError,OSError,IncompleteRead):code=503
        except (ValueError,TypeError) as exc:raise RuntimeError(f'Resposta inválida do {provider}') from exc
        if attempt>=config.retry_attempts:raise ProviderFailure(provider,429 if code==429 else 503)
        delay=max(retry_after,config.retry_base*(2**attempt)+random.uniform(0,0.2))
        if delay+0.05>=deadline-time.monotonic():raise ProviderFailure(provider,429 if code==429 else 503)
        time.sleep(delay)
    raise ProviderFailure(provider)


def generate_openai(config, context, messages):
    instructions = instructions_for(context)
    req = Request("https://api.openai.com/v1/responses", data=json.dumps({
        "model": config.model, "instructions": instructions, "input": messages,
        "max_output_tokens": 600, "store": False,
        **({"text":{"format":{"type":"json_schema","name":"npc_action","strict":True,"schema":SCHEMA}}} if config.structured_output else {})}, ensure_ascii=False).encode("utf-8"),
        headers={"Authorization": f"Bearer {config.api_key}", "Content-Type": "application/json"}, method="POST")
    payload=provider_payload(req,config,"OpenAI")
    chunks = [c["text"] for item in payload.get("output", []) if isinstance(item, dict)
              for c in item.get("content", []) if isinstance(c, dict)
              and c.get("type") == "output_text" and isinstance(c.get("text"), str)]
    result = "\n".join(chunks).strip()
    if not result:
        raise RuntimeError("OpenAI não retornou texto")
    return result.encode("utf-8")[:MAX_REPLY].decode("utf-8", "ignore")


def generate_gemini(config, context, messages):
    if not re.fullmatch(r"[A-Za-z0-9._-]{1,100}", config.gemini_model):
        raise RuntimeError("GEMINI_MODEL inválido; use somente o nome do modelo")
    contents = [{"role": "model" if m["role"] == "assistant" else "user",
                 "parts": [{"text": m["content"]}]} for m in messages]
    body = {"systemInstruction": {"parts": [{"text": instructions_for(context)}]},
            "contents": contents, "generationConfig": {"maxOutputTokens": 800, **({"responseMimeType":"application/json","responseSchema":gemini_schema(SCHEMA)} if config.structured_output else {})}}
    req = Request("https://generativelanguage.googleapis.com/v1beta/models/" +
                  config.gemini_model + ":generateContent",
                  data=json.dumps(body, ensure_ascii=False).encode("utf-8"),
                  headers={"x-goog-api-key": config.gemini_key, "Content-Type": "application/json"},
                  method="POST")
    payload=provider_payload(req,config,"Gemini")
    candidates = payload.get("candidates", []) if isinstance(payload, dict) else []
    if not isinstance(candidates, list):
        raise RuntimeError("Resposta inválida do Gemini")
    content = candidates[0].get("content", {}) if candidates and isinstance(candidates[0], dict) else {}
    parts = content.get("parts", []) if isinstance(content, dict) else []
    if not isinstance(parts, list):
        raise RuntimeError("Resposta inválida do Gemini")
    result = "\n".join(p["text"] for p in parts if isinstance(p, dict) and
                       isinstance(p.get("text"), str) and not p.get("thought")).strip()
    if not result:
        raise RuntimeError("Gemini não retornou texto; confira modelo, limite ou bloqueio da resposta")
    return result.encode("utf-8")[:MAX_REPLY].decode("utf-8", "ignore")


class Busy(Exception):
    pass


class Stale(Exception):
    pass


class Service:
    def __init__(self, config=None, generator=None):
        self.config = config or Config.env()
        if self.config.mode not in ("mock", "openai", "gemini"):
            raise ValueError("NPC_AI_MODE deve ser mock, openai ou gemini")
        if self.config.mode == "openai" and not self.config.api_key and generator is None:
            raise ValueError("OPENAI_API_KEY não configurada")
        if self.config.mode == "gemini" and not self.config.gemini_key and generator is None:
            raise ValueError("GEMINI_API_KEY não configurada")
        self.generator = generator
        self.lock = threading.RLock()
        self.history = OrderedDict()
        self.active = set()
        self.load()

    def load(self):
        path = self.config.memory_file
        if not path or not Path(path).exists():
            return
        if Path(path).stat().st_size > MAX_MEMORY_FILE:
            raise ValueError("Arquivo de memória excede 8 MiB")
        rows = json.loads(Path(path).read_text(encoding="utf-8"))
        if not isinstance(rows, list):
            raise ValueError("Arquivo de memória inválido")
        now = time.time()
        for row in rows[-self.config.max_conversations:]:
            if not isinstance(row, dict):
                continue
            try:
                key, _, _ = validate(row, False)
                at = float(row["at"])
                messages = row["messages"][-self.config.history_limit:]
                if not all(isinstance(m, dict) and m.get("role") in ("user", "assistant") and
                           isinstance(m.get("content"), str) and
                           len(m["content"].encode("utf-8")) <= MAX_REPLY for m in messages):
                    continue
                if 0 <= now - at < self.config.ttl:
                    normalized=[]
                    for item in messages:
                        content=item['content']
                        if item['role']=='assistant':
                            if content.lstrip().startswith('{'):content=normalize(content,self.config.money_max)['dialogue']
                            content=natural_speech(content)
                        normalized.append(dict(role=item['role'],content=content))
                    self.history[key] = {"messages": normalized, "at": at,"events":[],"cache":OrderedDict()}
            except (ValueError, KeyError, TypeError):
                continue

    def save_locked(self):
        if not self.config.memory_file:
            return
        path = Path(self.config.memory_file)
        # Snapshot dos registros mais recentes, limitado também em bytes no disco.
        # Evita produzir um arquivo que o próprio loader recusaria no próximo início.
        chunks = []
        size = 2
        for key, entry in reversed(self.history.items()):
            row = json.dumps({"npc_id": key[0], "conversation_id": key[1], "messages":entry["messages"],"at":entry["at"],"events":entry.get("events",[])},
                             ensure_ascii=False).encode("utf-8")
            if size + len(row) + bool(chunks) > MAX_MEMORY_FILE:
                break
            chunks.append(row)
            size += len(row) + 1
        data = b"[" + b",".join(reversed(chunks)) + b"]"
        tmp = path.with_name(path.name + ".tmp")
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        os.replace(tmp, path)

    def reset(self, key):
        with self.lock:
            self.history.pop(key, None)
            self.save_locked()

    def chat(self, key, message, context, structured=False, request_id="",client_timeout_ms=28000):
        context={**context,"money_limit":self.config.money_max,"personality":personality_for(key[0],context)}
        provider_config=replace(self.config,timeout=min(self.config.timeout,max(0.001,client_timeout_ms/1000-1.5)))
        fingerprint=hashlib.sha256(message.encode()).hexdigest()
        with self.lock:
            if key in self.active or len(self.active) >= self.config.max_inflight:
                raise Busy("NPC ocupado; tente novamente")
            existing=self.history.get(key)
            if structured and request_id and existing:
                cached=existing.get('cache',{}).get(request_id)
                if cached:
                    if cached[0]!=fingerprint:raise ValueError('request_id reutilizado com outra mensagem')
                    return cached[1]
            now = time.time()
            for old in list(self.history):
                if old not in self.active and now - self.history[old]["at"] > self.config.ttl:
                    del self.history[old]
            if key not in self.history:
                while len(self.history) >= self.config.max_conversations:
                    victim = next((k for k in self.history if k not in self.active), None)
                    if victim is None:
                        raise Busy("Memória ocupada")
                    del self.history[victim]
                self.history[key] = {"messages": [], "at": now,"events":[],"cache":OrderedDict()}
            entry = self.history[key]
            self.history.move_to_end(key)
            if context.get('last_action_result'):
                result=context['last_action_result']
                if not any(event.get('kind')=='engine_result' and event.get('request_id')==result['request_id'] for event in entry.get('events',[])):
                    entry.setdefault('events',[]).append({'kind':'engine_result',**result})
            messages = entry["messages"] + [{"role": "user", "content": message}]
            self.active.add(key)
        try:
            if self.generator:
                result = self.generator(context, messages)
            elif self.config.mode == "mock":
                result = generate_mock_action(context, messages) if structured else generate_mock(context, messages)
            elif self.config.mode == "gemini":
                result = generate_gemini(provider_config, context, messages)
            else:
                result = generate_openai(provider_config, context, messages)
            if isinstance(result,str) and not result.strip(): raise RuntimeError("Resposta vazia")
            decision=normalize(result,self.config.money_max)
            result=decision['dialogue']
            saved=result
            wire=encode(decision,request_id) if structured else result
            with self.lock:
                if self.history.get(key) is not entry:
                    raise Stale("Conversa foi limpa durante a resposta")
                entry["messages"] = (messages + [{"role": "assistant", "content": saved}])[-self.config.history_limit:]
                if structured and request_id:
                    entry.setdefault('events',[]).append({'kind':'proposal','request_id':request_id,'action':decision['action'],'parameters':decision['parameters'],'emotion':decision['emotion']})
                    cache=entry.setdefault('cache',OrderedDict());cache[request_id]=(fingerprint,wire)
                    while len(cache)>32:cache.popitem(last=False)
                entry['events']=entry.get('events',[])[-self.config.history_limit:]
                entry["at"] = time.time()
                self.save_locked()
            return wire
        finally:
            with self.lock:
                self.active.discard(key)


class Server(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, service=None):
        self.service = service or Service()
        self.slots = threading.BoundedSemaphore(12)
        super().__init__(address, Handler)

    def process_request(self, request, address):
        if not self.slots.acquire(blocking=False):
            request.close()
            return
        try:
            super().process_request(request, address)
        except BaseException:
            self.slots.release()
            raise

    def process_request_thread(self, request, address):
        try:
            super().process_request_thread(request, address)
        finally:
            self.slots.release()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    server_version = "NPCAI/1.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def log_message(self, fmt, *args):
        pass  # Não registrar mensagens, token ou chave.

    def reply(self, code, text, content_type="text/plain; charset=utf-8"):
        data = text.encode("utf-8")[:MAX_REPLY]
        data = data.decode("utf-8", "ignore").encode("utf-8")
        try:
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            pass

    def authorized(self):
        token = self.server.service.config.token
        return not token or hmac.compare_digest(self.headers.get("Authorization", "").encode(),
                                                ("Bearer " + token).encode())

    def do_GET(self):
        if self.path != "/health":
            return self.reply(404, "Rota não encontrada")
        if not self.authorized():
            return self.reply(401, "Token local inválido")
        self.reply(200, json.dumps({"ok": True, "mode": self.server.service.config.mode}),
                   "application/json; charset=utf-8")

    def do_POST(self):
        if not self.authorized():
            return self.reply(401, "Token local inválido")
        if self.path not in ("/chat", "/reset"):
            return self.reply(404, "Rota não encontrada")
        if self.headers.get_content_type() != "application/json":
            return self.reply(415, "Use application/json")
        if self.headers.get("Transfer-Encoding") or len(self.headers.get_all("Content-Length", [])) != 1:
            return self.reply(400, "Exigido Content-Length único")
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= MAX_BODY:
                return self.reply(413, "Corpo vazio ou grande demais")
            raw = self.rfile.read(length)
            if len(raw) != length:
                return self.reply(400, "Corpo incompleto")
            data=json.loads(raw.decode("utf-8"))
            key, message, context = validate(data, self.path == "/chat")
            protocol=data.get('protocol',1)
            if type(protocol) is not int or protocol not in (1,2): raise ValueError('protocol')
            structured=protocol==2
            request_id=data.get('request_id','')
            if structured and (not isinstance(request_id,str) or not ID.fullmatch(request_id)): raise ValueError('request_id')
            client_timeout=data.get('client_timeout_ms',28000)
            if type(client_timeout) is not int or not 50<=client_timeout<=60000:raise ValueError('client_timeout_ms')
            feedback=validate_feedback(data.get('action_result'))
            if feedback: context['last_action_result']=feedback
            if self.path == "/reset":
                self.server.service.reset(key)
                return self.reply(200, "Conversa limpa")
            self.reply(200, self.server.service.chat(key, message, context,structured,request_id,client_timeout), "application/json; charset=utf-8" if structured else "text/plain; charset=utf-8")
        except (ValueError, TypeError, UnicodeError):
            self.reply(400, "JSON, UTF-8 ou campos inválidos")
        except (TimeoutError, socket.timeout):
            self.reply(408, "Timeout ao ler requisição")
        except Busy as exc:
            self.reply(429, str(exc))
        except Stale as exc:
            self.reply(409, str(exc))
        except ProviderFailure as exc:
            self.reply(exc.status,str(exc))
        except RuntimeError as exc:
            self.reply(502, str(exc))
        except Exception:
            self.reply(500, "Erro interno; verifique configuração e arquivo de memória")


def main():
    config = Config.env()
    with Server(("127.0.0.1", 8765), Service(config)) as http:
        print(f"NPC AI: http://127.0.0.1:8765 | modo={config.mode} | modelo={config.gemini_model if config.mode == 'gemini' else config.model}", flush=True)
        if not config.token:
            print("Autenticação local desativada (NPC_AI_TOKEN vazio).", flush=True)
        try:
            http.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
