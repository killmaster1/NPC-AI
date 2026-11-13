# NPC AI 1.3.0 — instalação e validação

Alvo: GTA San Andreas Android **2.10 ARM64**, AML >=1.4, Android >=8 para o editor Java. Não instale este ELF em ARM32, GTA PC ou outro executável Android. As funções novas são chamadas pelo SDK Android vendorizado; não foram adicionados endereços PC.

Esta entrega contém código e binário compilado. **Nenhum aparelho, executável libGTASA.so nem API com credenciais está disponível neste ambiente. A presença e o comportamento das funções no seu executável, as animações e a coleta real de dinheiro ainda precisam do teste abaixo.** O usuário confirmou Gemini/Termux na versão anterior; isso não valida o protocolo e as ações desta atualização.

## Instalar e executar no Android

1. Feche completamente o GTA. Guarde a versão anterior fora de `mods`. Extraia `NPC_AI_1.3.0_ARM64.zip`.
2. Copie `npcai-android/libs/arm64-v8a/libnpcai.so` para a pasta de mods efetivamente usada pelo AML. No log fornecido anteriormente: `/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/mods/`. Mantenha somente uma cópia do plugin. Não substitua outras bibliotecas.
3. Acrescente as opções abaixo ao `npcai.ini` já existente. Preserve `token`, ajustes de balão e timeouts. O log `Config path=...` informa o caminho exato lido; normalmente fica no diretório `files` do pacote usado pelo GTA.
4. No Termux, atualize **os dois arquivos Python**. Exemplos após extrair o ZIP em Downloads:

```sh
termux-setup-storage
pkg install python
mkdir -p ~/npcai-server
cp ~/storage/downloads/npcai-android/server/{server,actions}.py ~/npcai-server/
cd ~/npcai-server
# Preserve seu .env privado existente. Em uma instalação nova:
# cp ~/storage/downloads/npcai-android/server/.env.example .env
# Edite .env: NPC_AI_MODE=mock para testar; depois gemini e a chave privada.
chmod 600 .env
set -a
. ./.env
set +a
python server.py
```

Se ainda não houver `.env`, copie o exemplo antes de executar `chmod`. Não ponha GEMINI_API_KEY/OPENAI_API_KEY no INI, plugin, APK ou arquivos públicos. `NPC_AI_TOKEN` deve coincidir com `token=` no INI. O servidor escuta somente em `127.0.0.1:8765`, no mesmo celular. Nenhum pacote pip é necessário. Para Gemini: `NPC_AI_MODE=gemini`, `GEMINI_MODEL=gemini-2.5-flash-lite` (ou modelo compatível) e sua `GEMINI_API_KEY` privada. OpenAI: `NPC_AI_MODE=openai`, `OPENAI_MODEL` e `OPENAI_API_KEY` privada.

5. Abra o GTA, carregue uma partida livre e aproxime CJ a menos de 3 m de um pedestre civil vivo. Toque em `Conversar...`; o primeiro tick da simulação solicita orientação/parada, sem esperar a IA. Digite acima do teclado Android e use Enviar ou a ação do IME. Voltar cancela. O teclado touch continua disponível como alternativa. O balão mostra espera durante HTTP/execução e a resposta por 15 segundos após confirmação.
6. Comece em mock, sem outros mods que alterem tarefas/controle/render. Depois teste Gemini e reintroduza seus mods individualmente.

## Configuração

```ini
npc_face_player=1
npc_stop_on_talk=1
npc_ai_actions=1
npc_money_drop=1
npc_money_max=200
npc_money_pickups_max=5
npc_action_timeout_ms=10000
npc_money_session_max=5000
npc_money_repeat=0
```

`npc_money_max`: 0..10000 por entrega; `npc_money_pickups_max`: 1..5; timeout: 500..10000 ms da simulação; orçamento de sessão: 0..100000. Valores inválidos produzem erro de configuração. Dinheiro: uma entrega por identidade por processo, salvo `npc_money_repeat=1`; mesmo nesse modo cada resposta só pode executar uma vez e o orçamento continua valendo. Reservas de tentativas parciais não são devolvidas ao orçamento para impedir repetição. Reiniciar o processo inicia outra sessão; não há promessa de identidade permanente de pedestres aleatórios.

No servidor: `NPC_AI_MONEY_MAX=200` limita propostas e `NPC_AI_STRUCTURED_OUTPUT=1` pede schema JSON ao provedor. Se o modelo não suportar schema, use `0`; a instrução JSON e os validadores locais permanecem. O plugin aplica seu próprio limite, mesmo que o servidor permita mais. Opções anteriores continuam: `npc_stand_still=0` desativa parada de conversa, `npc_dialogue_animations=0` desativa ações de gestos/rendição; `npc_ai_actions=0` rejeita ações da IA, preservando diálogo e orientação inicial. Para não orientar/parar, desative também as duas opções correspondentes.

## O que é executado

| Ação | Implementação solicitada à engine | Confirmação observada |
|---|---|---|
| idle | CTaskSimpleStandStill temporária | Velocidade baixa |
| face_player | CTaskComplexTurnToFaceEntityOrCoord; fallback CTaskSimpleAchieveHeading | Vetor frontal alinhado a CJ |
| talk_gesture | CTaskSimpleRunTimedAnim / CHAT | Associação de animação iniciada |
| hands_up | Mesma tarefa / HANDSUP | Associação de animação iniciada |
| surrender_money | Parada, orientação, HANDSUP; espera >=900 ms, CPickups::GenerateNewOne / PICKUP_MONEY | Geração, tipo, modelo e valor dos pickups conferidos |
| flee | CTaskComplexSmartFleePoint | Distância a CJ aumentou |
| angry | Animação PARTIAL_FUCKU temporária | Associação iniciada; não inicia combate |
| refuse | Animação ENDCHAT2 temporária | Associação iniciada |
| follow_player | CTaskSimpleGoToPoint, replaneja para CJ | Aproximação observada ou já próximo |
| stop_following | Retira apenas tarefas pertencentes ao mod | Controle temporário liberado |
| cower | CTaskSimpleDuck se CanPedDuck permitir | Flag de abaixado observada |

Gestos secundários mantêm uma tarefa primária de parada. Não há teleporte, fixação de coordenadas, escrita direta de orientação, IFP externo ou comando arbitrário do modelo. Animação indisponível, símbolo ausente, missão, combate, dano recente, queda, veículo, identidade inválida, saída do raio ou timeout interrompem/rejeitam a ação. O controlador remove somente tarefas cuja propriedade e vida útil ainda são válidas; os slots originais de rotina não são substituídos. A engine pode retomar a rotina após a liberação; não há restauração artificial de uma caminhada antiga.

O seguimento é temporário, limitado pelo timeout e **pelo raio configurado** (3 m por padrão). Não é um recrutamento persistente. NPCs de missão e policiais em atendimento podem ser inelegíveis. Não há prisão, reforços, transporte ou ataque implementados. `angry` é gesto hostil; fuga/seguimento/parada/dinheiro usam tarefas físicas reais, quando disponíveis.

O modelo de dinheiro é buscado pelo nome original `money`, carregado de modo assíncrono e posicionado no chão com a API do mundo. Cada entrega cria 1..5 pickups nativos, não objetos decorativos, com soma exata do valor autorizado. A confirmação significa **pickups criados**, não saldo já creditado a CJ. Sem modelo, chão, APIs ou rendição confirmada, nada é anunciado como entregue. Pickups parciais são removidos; os restantes expiram após 60 s da simulação e são removidos em reset, invalidação ou interrupção prioritária. No máximo 32 pickups do mod ativos.

## Contrato e threads

O plugin envia protocolo 2 com `request_id` exclusivo e recebe `dialogue`, `action`, `parameters`, `emotion`, `request_id`. Gemini, Responses API e mock convergem para o mesmo validador. Ações fora da whitelist, chaves duplicadas, campos extras, tipos incorretos, dinheiro negativo/excessivo ou nonce errado viram diálogo seguro sem ação. Servidor antigo com resposta textual continua funcionando, sem ações.

Texto de uma proposta física fica retido até confirmação do controlador. Se falhar, aparece a falha, não uma declaração de sucesso do modelo. O resultado local (`action_result`) acompanha a **próxima mensagem** enviada ao servidor; não há requisição HTTP extra para confirmar. O histórico do servidor distingue propostas e resultados locais. Uma fala textual legada não constitui confirmação física.

HTTP roda na worker; a worker apenas valida e entrega dados. A criação/atualização das tarefas ocorre em `gameProcessEvent`. A orientação é enfileirada no toque e executada no próximo tick; pausas reais do GTA são respeitadas e não há desativação global de pausa. Encerrar, trocar/limpar conversa, sumir/morrer, combate/missão e encerramento de pools invalidam controle temporário. O ledger de 8192 respostas e 4096 NPCs pagos tem limite sem expulsão insegura: atingir o limite recusa novas ações/entregas até reiniciar o processo.

## Roteiro obrigatório no aparelho

Anote APK/AML, resolução, teclado, outros mods e horário; guarde logcat/AML crashlog em caso de falha. Nenhum item abaixo foi aprovado em dispositivo neste ambiente.

| Teste | Passos | Critério |
|---|---|---|
| Parada/orientação | Civil caminhando, tocar Conversar com CJ ao lado/atrás | Para por tarefa e gira suavemente, sem salto; respiração preservada |
| Liberação | Cancelar com Voltar, fechar chat; repetir 20 vezes | Teclado some, CJ controla normalmente, NPC retoma IA |
| IME | Digitar `Olá! Ação, ç, 123?`, enviar por botão e por IME | Campo/Enviar acima do teclado, nenhuma nova Activity, uma requisição |
| Balão | Testar alturas, distâncias, orientações/resoluções | Âncora próxima à cabeça, resposta 15 s; limites do modelo e fallback preservados |
| Assalto mock | Civil: `Passa a grana!` | Mãos levantadas antes dos pickups; valor total até 200 |
| Coleta real | Anotar saldo CJ, coletar todos os pickups antes de 60 s | Saldo aumenta exatamente pelo valor anunciado; registrar qualquer divergência |
| Repetição | Pedir dinheiro novamente ao mesmo NPC; limpar conversa e tentar | Nenhuma segunda entrega por identidade; nenhum dinheiro duplicado |
| Personalidades | Repetir com gangue/policial, depois Gemini | Recusa/fuga/gesto conforme contexto, sem obediência automática |
| Ações | `me siga`, `pare de seguir`, `abaixe`, `mãos ao alto`, `olhe pra mim` | Efeito nativo observado ou erro explícito, sem travamento |
| Prioridade | Durante edição/ação, NPC entra em combate, cai, morre ou some; CJ afasta >raio | Sem acesso a entidade inválida; controle do mod cancelado |
| Missão/veículo | Tentar NPC de missão ou embarcado | Não sobrescrever tarefa crítica; diálogo pode continuar sem ação |
| Resposta atrasada | Servidor lento; trocar NPC, limpar, sair do raio | Nenhuma ação/resposta atribuída ao novo pedestre |
| Pausa/segundo plano | Alternar Termux/GTA e girar teclado | Pausa real respeitada, solicitação retomada ou cancelada com segurança |
| Offline | Desligar servidor no meio e voltar a ligar | Erro visível, render não bloqueia, novo envio funciona |
| Limpeza | Deixar dinheiro sem coletar 60 s; carregar save | Apenas recursos do mod removidos, sem reutilizar handles antigos |

O teste automatizado usa uma engine falsa para provar transições, limites e cancelamento; não prova efeitos do SDK. Falha de uma animação deve terminar com mensagem e limpeza. Verifique ainda FPS e compatibilidade com os demais mods.

## Logs, build e remoção

```sh
adb logcat -c
adb logcat -v time > npcai-130-logcat.txt
# Reproduza, interrompa a captura e procure localmente:
rg 'NPC|Action|Orientation|Keyboard|Editor|Game state|Bubble|Activity|Window' npcai-130-logcat.txt
```

AML também pode salvar log em sua pasta de dados. Logs novos incluem `Action symbols ...`, `Actions disabled: task lifetime hooks unavailable`, `Action native task=... queued`, `Action result=confirmed/failed ... amount=...`, pausa e orientação pendente. Eles não registram mensagem ou chave. `confirmed` comprova somente o observável local descrito na tabela; confira a coleta real no jogo. Se algum símbolo faltar, envie o log para diagnosticar a capacidade recusada. Não informe chaves nos relatos.

```sh
./tools/build_keyboard.sh
./tools/test.sh
ANDROID_NDK_HOME=/caminho/android-ndk-r27d ./build.sh
python3 tools/verify_elf.py libs/arm64-v8a/libnpcai.so
python3 tools/package.py
```

O workflow instala NDK 27.3.13750724, compila Java/DEX/C++, executa testes, valida ELF e publica binário/ZIP. Bibliotecas padrão Android não são rejeitadas. Resultados desta entrega: [VALIDATION_1_3_0.md](VALIDATION_1_3_0.md).

Para desinstalar: feche GTA, remova apenas `libnpcai.so` e opcionalmente `npcai.ini`; pare Python com Ctrl+C, remova memória privada se desejar. Não descarregue a biblioteca durante o processo GTA: hooks permanecem instalados até seu encerramento.
