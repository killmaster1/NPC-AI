# NPC AI 1.4.0 — diálogos e comportamentos persistentes

Alvo preservado: GTA San Andreas Android **2.10 ARM64**, AML >=1.4, Android >=8 para IME. Esta atualização continua a base 1.3.1; editor Java/DEX, teclado alternativo, servidor local, seleção geracional, projeção e balões continuam no projeto.

**Compilação e testes host não são validação no GTA.** O usuário confirmou que a base funciona no aparelho. As novas tarefas, manutenção da pose, prioridade real de eventos, retomada da rotina e coleta de dinheiro desta versão ainda precisam do roteiro abaixo. Não há aparelho conectado, executável GTA nem chamadas reais a Gemini/OpenAI neste ambiente.

## Diagnóstico e mudanças

Havia interrupções explícitas no próprio controlador: gestos eram encerrados após 700 ms, e a rendição era removida 2 s após a entrega. A rotina nativa de iniciar uma ação removia todas as tarefas pertencentes ao mod; assim, virar, parar e gesticular podiam se cancelar. O raio de validação física também era o raio inicial de 3 m, incompatível com uma soltura em 6 m.

Agora o controlador distingue comportamento persistente, postura primária e gesto secundário. O término de um gesto não remove a parada nem a rendição. Virar troca somente a tarefa primária pertencente ao mod; a pose secundária permanece. Tarefas críticas continuam prioritárias e nunca são substituídas à força.

### Rendição e seguimento

- `hands_up` e `surrender_money` entram em `SURRENDERING` quando a associação de animação mostra progresso e blending visível. Criar uma tarefa, isoladamente, não confirma efeito.
- A parada usa `CTaskSimpleStandStill(..., bForever=true, ...)`, presente no header Android.
- Mãos levantadas usam o ID simbólico Android `ANIM_STD_HANDSUP`, grupo padrão, `CTaskSimpleRunTimedAnim` e opção de manter o último frame. O prazo técnico do segmento é 60 s, separado da duração sugerida pelo modelo.
- Enquanto o NPC e as tarefas permanecem válidos, o timer **da tarefa pertencente ao mod** é renovado antes de 60 s, pelos campos do `CTaskTimer` declarado no SDK Android. A manutenção ocorre em intervalos, sem recriar o clip a cada frame. Não são escritos posição, velocidade, heading ou matriz do NPC.
- Se a pose realmente desaparecer, há no máximo três tentativas consecutivas de recuperação, espaçadas. Sem efeito seguro, o controlador libera as tarefas e registra diagnóstico; não tenta obter invulnerabilidade a eventos da engine.
- Por padrão, a rendição continua mesmo ao fechar o chat, trocar o NPC ou terminar uma fala. Só se encerra ao aceitar `release_surrender`, distância >6 m durante 2 s contínuos, morte/remoção, invalidação, interrupção física/crítica ou encerramento/reset do jogo.
- Aproximar novamente durante os 2 s reinicia a contagem de distância. A seleção inicial continua em 3 m.
- `follow_player` continua sem usar `duration_ms` como prazo obrigatório. Tem distância de liberação própria (25 m por padrão), mesma área e identidade válida; `stop_following` libera apenas o controle do mod.
- Fuga é temporária e respeita seu prazo; uma fala ou gesto não a interrompe e sair dos 3 m iniciais não a cancela automaticamente.
- Dinheiro usa o lote nativo já existente: 1..5 pickups com soma exata, limite por interação/sessão, ledger de nonces e uma entrega por identidade por padrão. Criar dinheiro não reinicia ou cancela mãos levantadas. Lote parcial é removido e não é repetido.

**Limitação importante:** `bHoldLastFrame`, timer, IDs, campos e assinaturas estão presentes no SDK Android compilado, mas a semântica visual no seu executável precisa ser confirmada. Se o clip não mantiver a pose nessa build, a recuperação é limitada e a falha aparece no diagnóstico. Não foram aplicadas flags de animação ambíguas, loops de proporções arbitrárias, endereços PC ou IFP externo. A liberação solicita o blending configurado na tarefa nativa; a suavidade real ainda deve ser observada no dispositivo.

### Gestos de fala e prioridade

A fala publicada gera um evento para o controlador na thread do jogo. Não há gesto enquanto a API está processando. A duração é calculada localmente por palavras e pausas: 180 palavras/minuto, mínimo 2500 ms e máximo 12000 ms por padrão.

Uma resposta longa agenda pequenos trechos de até 2100 ms e pausas de 650 ms. A implementação alterna os IDs Android já usados `CHAT`/`ENDCHAT2`; irritação/recusa usam os IDs existentes correspondentes. Não se promete um catálogo ilimitado de gestos. O fim de um trecho remove somente seu gesto, preservando o corpo em postura de conversa. Um clip que termina após ter sido observado é distinguido de um início sem efeito; isso permite continuar os trechos seguintes sem tratar toda finalização como erro.

A orientação de manutenção é avaliada no máximo a cada 500 ms e só inicia nova rotação quando CJ se desloca suficientemente em ângulo (~20 graus); a orientação inicial usa a confirmação mais estrita anterior. Ela não escreve heading diretamente. Gestos comuns não substituem rendição, seguimento ou fuga. Missão, ataque, veículo, dano recente, queda e respostas críticas mantêm as proteções anteriores. Fechar/trocar chat enfileira a liberação para o tick do jogo, preservando comportamentos persistentes; a worker HTTP nunca chama a engine.

A duração do balão continua em **15 segundos** por padrão e independe da duração dos gestos. Uma opção permite usar o tempo calculado de leitura no balão. Publicações consecutivas com a mesma frase também renovam a visibilidade.

### Diálogo, personalidade e falhas

- `actions.py` e o parser C++ preservam `dialogue` válido quando a ação, emoção ou parâmetros não passam na validação: ação vira `none`. JSON ambíguo/malformado continua sendo rejeitado com resposta natural segura.
- Diagnósticos do controlador não são inseridos como fala. Falha física conserva a fala natural da proposta; a confirmação física fica separada. Uma proposta verbal não é prova de que algo aconteceu. O plugin só acrescenta confirmação de dinheiro após conferir os pickups; saldo coletado ainda deve ser verificado no GTA.
- O servidor armazena somente texto natural em `messages`; propostas e recibos ficam em `events`, com tipos distintos. Memória antiga com respostas JSON/colchetes é convertida ao carregar. Os eventos não são enviados como falas anteriores. `last_action_result` acompanha a próxima pergunta, como antes, sem chamada extra à IA.
- Coragem, agressividade, simpatia e paciência são geradas uma vez por identidade efêmera. Medo/confiança começam estáveis e podem mudar a partir de ações confirmadas; não são sorteados novamente por mensagem. São características narrativas, não leituras escondidas da engine. No servidor antigo/cliente sem traços, a identidade tem fallback determinístico. Outra sessão ou entidade reciclada recebe nova identidade.
- O prompt pede português brasileiro coloquial, uma ou duas frases normalmente, gírias/ironias/palavrões conforme personalidade e emoção, sem repetir CJ ou bordões em toda fala. Gangue, polícia, taxista e civil têm estilos distintos, sem inventar bairro/missão como dado lido.
- Mock entende `me segue`, `me siga`, `cola comigo`, `vem cmg`, `solta o dinheiro`, `mãos pra cima`, `levanta as mãos`, `sai fora`, `some daqui`, `abaixa as mãos`, `pode ir embora`. A decisão varia com os traços; não há obediência obrigatória. `Late` provoca resposta verbal, sem ação inexistente.
- O contrato mantém `dialogue/action/emotion/parameters`. A única ação nova é `release_surrender`, necessária para liberar explicitamente uma pose sem confundi-la com `idle`. Somente a whitelist nativa pode ser executada.

### Retry Gemini/OpenAI

HTTP 429/500/502/503/504 e falhas de rede admitem no máximo dois retries adicionais por padrão, com backoff exponencial e jitter. `Retry-After` é respeitado; se não couber no orçamento, a tentativa termina. 400/401/403/404 e modelo inválido não são repetidos automaticamente.

O prazo total inclui requisições, leitura e esperas. O plugin informa seu timeout; o servidor usa o menor orçamento entre esse prazo (com margem) e `NPC_AI_API_TIMEOUT`. Não é feita uma nova rodada inteira para cada retry. Respostas grandes continuam limitadas. A mesma identidade/conversa/nonce tem cache de resposta limitado; uma resposta lógica gera uma proposta. O ledger nativo continua impedindo execução duplicada. Uma falha final aparece como erro de comunicação separado da fala do NPC, sem simular que o Gemini respondeu nem trocar silenciosamente para mock.

## Instalar no Android

1. Feche o GTA completamente. Guarde o plugin anterior fora de `mods` e pare o servidor com Ctrl+C.
2. Extraia `NPC_AI_1.4.0_ARM64.zip`.
3. Copie `npcai-android/libs/arm64-v8a/libnpcai.so` para a pasta efetiva de mods AML, mantendo uma única cópia. No aparelho anteriormente relatado: `/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/mods/`. Não copie DLL/ELF de outra arquitetura.
4. Acrescente as novas opções ao seu `npcai.ini` existente, preservando token e ajustes anteriores. O caminho exato é registrado como `Config path=...`.
5. Atualize **server.py e actions.py** no Termux, preservando seu `.env` privado:

```sh
# Se necessário uma única vez: termux-setup-storage; pkg install python
mkdir -p ~/npcai-server
cp ~/storage/downloads/npcai-android/server/{server,actions}.py ~/npcai-server/
cd ~/npcai-server
chmod 600 .env
set -a
. ./.env
set +a
python server.py
```

Se ainda não existir `.env`, copie `server/.env.example` e configure-o antes de carregar. Não é necessário pip. Para primeiro teste gratuito: após carregar o ambiente, execute `NPC_AI_MODE=mock python server.py`. Para Gemini: `.env` com `NPC_AI_MODE=gemini`, `GEMINI_MODEL=gemini-2.5-flash-lite` (ou modelo compatível), `GEMINI_API_KEY` privada. Para OpenAI: modo `openai`, `OPENAI_MODEL`, `OPENAI_API_KEY` privada. `NPC_AI_TOKEN` precisa coincidir com `token=` do INI. O servidor continua somente em `127.0.0.1:8765` no mesmo celular.

6. Abra GTA, carregue partida livre, aproxime CJ e toque em Conversar. Teste IME, Enviar, Voltar e o teclado alternativo. Não coloque chaves no INI, plugin, APK, logs ou repositório.

## Configurações

```ini
npc_hold_surrender=1
npc_surrender_release_distance=6.0
npc_surrender_release_delay_ms=2000
npc_follow_release_distance=25.0
npc_face_player=1
npc_dialogue_animations=1
npc_dialogue_reading_wpm=180
npc_dialogue_min_ms=2500
npc_dialogue_max_ms=12000
npc_personality_variation=1
npc_animation_debug=0
npc_bubble_dynamic_duration=0
```

Não duplique opções já existentes; preserve o restante do INI. Distâncias: 4..100 m e maiores que `radius`; delay: 500..10000 ms; leitura: 60..600 palavras/minuto; mínimos/máximos: 500..30000 ms e mínimo <= máximo. Se uma configuração antiga usa raio maior e não define distância persistente, a distância default é ampliada automaticamente para manter a separação. Com valores explícitos incompatíveis, a configuração é recusada com diagnóstico.

`npc_hold_surrender=0` permite a rendição temporária com limite configurado. `npc_dialogue_animations=0` impede gestos/pose, mantendo conversa. `npc_ai_actions=0` desativa comandos físicos da IA, preservando a conversa e a postura inicial configurada. Orçamento e opções de dinheiro da 1.3.x continuam. No `.env`, `NPC_AI_API_RETRIES=2` aceita 0..3; o timeout total não aumenta ao habilitar retries.

## Testes obrigatórios no aparelho — ainda pendentes

| Teste | Passos e resultado esperado |
|---|---|
| Saudação/IME | Enviar Oi com acentos pelo botão e IME; resposta natural, sem colchetes ou erros como fala |
| Postura inicial | Civil andando, CJ ao lado/atrás: parar por tarefa e virar suavemente, sem salto |
| Rendição >10 s | Pedir mãos levantadas; se aceitar, observar por 30 s; pose permanece e respiração/pequenos movimentos não congelam a entidade |
| Chat fechado | Fechar o editor e ficar perto; pose persiste; controles de CJ normais |
| Nova fala | Conversar com NPC rendido: gesto comum não remove as mãos levantadas |
| Soltura por distância | Sair além de 6 m por <2 s e voltar: manter; sair por >2 s: liberar suavemente |
| Comando de soltura | Reaproximar, `abaixa as mãos`/`pode ir embora`; se aceitar, liberar sem encerrar controles |
| Timer longo | Observar uma rendição por >70 s com debug: renovação sem recomeçar o clip nem abaixar os braços |
| Dinheiro | `Mãos pra cima! Passa a grana!`: se aceitar, dinheiro coletável sem reiniciar pose; conferir soma e saldo CJ |
| Repetição | Pedir dinheiro de novo, repetir nonce em teste controlado, limpar conversa: nenhuma entrega indevida |
| Seguimento | `Vem cmg`/`cola comigo`: se aceitar, seguir por >15 s sem tarefa de parada; `pare de seguir` libera |
| Fuga | `Some daqui`: fuga não cessa por uma fala/gesto nem ao ultrapassar 3 m |
| Fala curta/longa | Frase curta: gesto breve; frase longa: gestos com pausas, sem repetir corpo a cada frame; balão default continua 15 s |
| Personalidades | Mesmos NPCs em mensagens sucessivas: traços coerentes; NPCs diferentes podem recusar/obedecer; validar também gangue/polícia |
| Prioridade | Dano, combate, queda, atropelamento, veículo e missão assumem prioridade; sem tentar sustentar pose à força |
| Desaparecer/reset | NPC descarrega/morre, CJ morre, carregar save, sair do jogo: cancelar recursos sem confundir slot reciclado |
| API instável | Gemini 503/429: retries limitados, erro separado se esgotar, nenhuma ação duplicada |
| Interface | Abrir/cancelar/reabrir, trocar NPC, girar tela/teclado, diferentes resoluções; IME e touch alternativo funcionam |
| FPS/mods | Comparar FPS e log com debug desligado, depois testar outros mods individualmente |

Mock é determinístico nos traços, mas não obedece a todos os civis. Para testar um efeito específico no aparelho, procure outro NPC elegível se houver recusa narrativa. Os testes host usam cenários controlados para cobrir cada transição; não substituem este roteiro.

## Diagnóstico

Ative temporariamente `npc_animation_debug=1` e reinicie GTA:

```sh
adb logcat -c
adb logcat -v time > npcai-140-logcat.txt
# Reproduza e encerre a captura; no computador:
rg 'Animation|Action gate|Action failure|Action symbols|Actions disabled|Game state|Bubble' npcai-140-logcat.txt
```

Também consulte o log AML. `Animation start` registra tarefa, duração e slot; `effect observed` confirma progresso/blend; `owned release requested` indica remoção solicitada pelo mod; `task destroyed` informa tempo e observação anterior; `hold timer renewed` indica manutenção sem recriar o clip; `Action gate` mostra prioridade ou inelegibilidade. Nenhum log contém diálogo, token ou chave.

Um clip observado que terminou pode ter acabado ou sido substituído por uma tarefa não crítica; o SDK não fornece neste projeto uma causa completa para todo término. O agendador pausa antes de outro gesto; eventos críticos detectados impedem retomada. Não interprete `queued` como confirmação de efeito e nem pickups criados como saldo já coletado.

## Build e remoção

```sh
./tools/test.sh
ANDROID_NDK_HOME=/caminho/android-ndk-r27d ./build.sh
python3 tools/verify_elf.py libs/arm64-v8a/libnpcai.so
python3 tools/package.py
```

NDK usado: r27d / 27.3.13750724. O workflow compila, testa e publica artifacts 1.4.0. A ponte Java não mudou nesta versão; para regenerá-la: `./tools/build_keyboard.sh`. Veja [resultados reais e hashes](VALIDATION_1_4_0.md) e [auditoria do SDK](SDK_AUDIT.md).

Desinstalar: feche GTA, remova só `libnpcai.so` e opcionalmente o INI; pare Python com Ctrl+C e remova memória privada se desejar. Não descarregue a biblioteca enquanto o processo GTA estiver rodando: os hooks permanecem instalados até encerrar o processo.
