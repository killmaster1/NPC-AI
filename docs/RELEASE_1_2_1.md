# NPC AI 1.2.1 — correções e teste no aparelho

## Resultado e alcance

Esta versão corrige o posicionamento calculado do balão, substitui o editor em Dialog por uma View na Activity atual e trata envio/cancelamento como conclusões distintas. Inclui o pedido adicional de manter o NPC parado e usar gestos curtos de diálogo quando as tarefas da engine permitem.

O usuário confirmou Gemini/Termux e execução de 1.2.0 em GTA SA Android 2.10 ARM64. **A versão 1.2.1 ainda não foi executada em aparelho por este ambiente.** Compilar Java/DEX/C++ e aprovar testes host não demonstra que foco, IME, SurfaceView e tarefas do NPC já passaram no dispositivo. O roteiro abaixo deve ser executado antes de considerar a correção validada.

## Diagnóstico

1. A âncora anterior usava sempre `posição + 2,15 m`, sem considerar os limites do modelo. Além disso, a ponta triangular avançava abaixo da borda inferior; não havia separação explícita entre ponto projetado, margem e ponta. Isso podia aumentar a distância visual em relação à cabeça.
2. O Java de 1.2.0 criava `android.app.Dialog`: uma janela adicional com foco e política de teclado próprios. Não criava uma segunda Activity. Um Dialog normalmente não produz `onPause` por si só; sem logs do aparelho não é possível afirmar se o relato foi pausa real do jogo, reação à perda de foco, interrupção de render ou captura de input.
3. O retorno JNI com `null` (cancelamento) não era entregue à UI como uma conclusão. O editor podia fechar deixando `GameBridge::opened` ativo e o filtro de CPad suprimindo os comandos. Agora cancelamento chega explicitamente ao C++ e libera o estado do chat.
4. A UI grande ImGui permanecia atrás do editor nativo, ocupando o jogo. Agora ela fica oculta durante a edição nativa e a captura é liberada mesmo se a resolução ficar temporariamente muito pequena.

O código não escreve nos flags de pausa do CTimer ou do menu. O filtro existente de entrada continua usando `CPad::Clear(false,false)` na thread do jogo enquanto o chat está aberto; não fixa `DisablePlayerControls`. As leituras/logs dos flags CTimer servem apenas para diagnóstico. Eventos reais de pausa do Android continuam sendo respeitados, com cancelamento da edição; não há unpause global ou supressão de eventos de ciclo de vida.

## Balões e configuração

Acrescente estas opções ao seu `npcai.ini` existente, preservando token e demais valores:

```ini
bubble_height=1.75
bubble_screen_offset_y=6
npc_stand_still=1
npc_dialogue_animations=1
```

O caminho lido é registrado como `Config path=.../npcai.ini`. O template completo está em `config/npcai.ini`. Reinicie o GTA após editar.

- `bubble_height`: 0,50..3,50 metros. Fallback absoluto acima da origem do pedestre quando os limites não são utilizáveis. Quando há limites válidos, funciona como ajuste de calibração **valor − 1,75** sobre o topo calculado; por exemplo, 1,60 reduz esse topo em 0,15 m.
- `bubble_screen_offset_y`: 0..200 pixels ImGui entre a ponta triangular e a âncora. Positivo sobe o balão. O padrão 6 deixa uma pequena separação; use 0 para aproximar a ponta. A borda inferior fica acima da ponta, respeitando sua altura.
- `npc_stand_still`: 0/1, padrão 1. Ativa tarefas curtas de espera para NPCs elegíveis durante a conversa.
- `npc_dialogue_animations`: 0/1, padrão 1. Com o NPC sob controle de espera, permite gestos temporizados quando há uma resposta no histórico.

### Fonte da posição

O SDK Android expõe `CEntity::GetColModel` e `CColModel::m_boundBox`. Para o pedestre selecionado e validado, a thread do jogo lê limites finitos/coerentes e transforma o centro superior pelo `CMatrix::TransformPoint`. Isso considera a geometria de colisão do modelo e sua matriz/escala, sem escrever na posição do NPC. Limites ausentes, incoerentes ou fora do intervalo defensivo usam o fallback de 1,75 m.

`GetBonePosition` existe nos headers, mas não foi encontrada uma enumeração de cabeça e um contrato de atualização de esqueleto suficientemente verificados para adicionar essa leitura neste executável. Não foram usados IDs de colisão como se fossem bones nem estruturas/offsets de PC. O topo da colisão é uma aproximação e pode diferir da cabeça visual em skins modificadas, agachamento ou veículo.

`CSprite::CalcScreenCoors` continua usando clipping próximo/distante. A saída é convertida do tamanho de render `RsGlobal.maximumWidth/Height` para `ImGuiIO::DisplaySize`. A conversão, profundidade, valores não finitos e borda inferior/ponta têm testes host, incluindo resoluções distintas. Logs mostram `Bubble projection render=... imgui=...` e `Bubble anchor model=... source=bounds/fallback above_origin=...`. A correspondência dessa convenção com o executável e a resolução configurada precisa de confirmação visual no aparelho.

Se não houver espaço acima da âncora, o balão é ocultado; não é deslocado verticalmente para longe da cabeça. Identidade, raio de conversa, pool, remoção do mundo e descarte de respostas tardias continuam validados. Permanece sem teste de oclusão por paredes.

## Editor Android na Activity existente

- Adiciona um `FrameLayout` à ViewGroup `android.R.id.content` da Activity obtida pelo AML. Não instancia Dialog, Activity ou Window adicional.
- Toda operação de View/IME ocorre em `Handler(Looper.getMainLooper())`.
- O contêiner consome toques fora do painel; os filhos recebem texto e cliques normalmente. Isso evita repassar esses gestos ao SurfaceView do jogo.
- O painel se ancora à parte inferior visível. Ajusta a margem quando a IME cobre a tela e reduz o histórico para preservar campo e botão Enviar em áreas menores.
- Salva e restaura o `softInputMode` anterior; usa `ADJUST_RESIZE` durante a edição. Não modifica flags fullscreen, pause ou menu. A reação específica do SurfaceView do APK a resize permanece parte do teste real.
- O EditText recebe foco e solicita a IME. O botão Enviar permanece junto do campo; `IME_ACTION_SEND` é configurada inclusive para o campo multilinha quando o teclado a respeitar.
- Ao enviar: valida texto, oculta a IME, remove o contêiner, restaura foco/modo da janela, entrega bytes UTF-8 ao C++ e enfileira pela worker HTTP existente. O jogo volta a receber input; o balão passa a pensar e depois exibe a resposta por 15 s.
- Voltar é tratado antes da IME e também pela observação do teclado oculto/gesto. Em Android 11+, a visibilidade é consultada pelos WindowInsets públicos, inclusive para IMEs flutuantes; versões anteriores usam o retângulo visível. O observador periódico é encerrado com o editor.
- Cancelar entrega uma conclusão sem texto, sem chamada HTTP. Tickets descartam callbacks duplicados/antigos. Troca, remoção do NPC, segundo plano ou shutdown invalidam a edição e solicitam remoção da View pela UI thread.

Se a abertura JNI falhar, o chat ImGui e o teclado touch permanecem como alternativa. Texto vazio não é enviado. Gemini, mock, OpenAI, memória por identidade, limites, token local e servidor Termux foram preservados; quem já usa o servidor 1.2.0 não precisa reconfigurar a chave para este teste.

## NPC parado e animações

A identidade selecionada fica associada à conversa enquanto há edição ou requisição e durante a fala final. Não muda automaticamente para um vizinho só porque este ficou mais perto. Distância e validade continuam encerrando a associação.

A implementação usa `CTaskSimpleStandStill(1000,false,false,4)` no slot temporário disponível da IA, com renovação enquanto a conversa está ativa. Não altera velocidade, posição, colisão ou flags de física. Pedestres de missão (`PED_MISSION`), em veículos, ou com tarefas de missão/combate/resposta física/temporária em andamento não têm essas tarefas substituídas.

Para gestos, usa `CTaskSimpleRunTimedAnim` com o grupo padrão do pedestre e `ANIM_STD_CHAT`, por 1200 ms, somente se o slot secundário de animação parcial estiver livre. Há intervalo de 6 s entre gestos. Não são carregados grupos de animação externos.

Não se guardam ponteiros de CPed ou tarefas para liberação posterior. A engine detém as tarefas curtas e temporizadas; ao cancelar, sair do raio, trocar NPC ou expirar a conversa, a renovação para. A última espera pode levar até aproximadamente 1 s e o último gesto até aproximadamente 1,2 s para terminar após a retomada normal do tempo do jogo. Não há promessa de restauração de uma pose exata. Se os símbolos não existirem no APK, o recurso fica inativo e a conversa permanece disponível. Logs de inicialização indicam disponibilidade; tarefas efetivamente enfileiradas também são registradas.

## Instalação

1. Feche o GTA completamente. Guarde a versão anterior fora da pasta mods.
2. Copie apenas o novo `libs/arm64-v8a/libnpcai.so`, sem cópias duplicadas, para a pasta efetiva de mods AML. No log anterior ela era `/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/mods/`.
3. Acrescente as quatro opções acima ao INI no caminho informado pelo AML. Caso não exista INI, os novos padrões já são aplicados.
4. Mantenha seu servidor Termux e token local. Para os primeiros testes, pode executar `NPC_AI_MODE=mock python server.py` sem gastar quota; depois retome `NPC_AI_MODE=gemini` e a chave privada já configurada.
5. Primeiro teste com a combinação de mods que estava estável. Depois reintroduza os demais individualmente, pois houve relato anterior de interação entre mods.

Requisitos: GTA SA Android 2.10 ARM64, AML >=1.4, Android >=8/API26. DEX e fontes estão no pacote; nenhum APK adicional precisa ser instalado.

## Roteiro no aparelho — todos os itens ainda precisam de execução real nesta versão

| Caso | Procedimento | Resultado esperado |
| --- | --- | --- |
| 1. Abrir/digitar | Aproxime CJ de um pedestre comum e toque no balão; digite acentos/emoji | GTA visível atrás do painel, campo focado, texto acima da IME; sem menu de pausa |
| 2. Botão Enviar | Envie pergunta em mock | Botão visível/clicável; editor/IME fecham; controles retornam; aparece pensando e resposta |
| 3. Ação IME | Use a ação Enviar do teclado quando disponível | Mesmo fluxo do botão; uma mensagem por acionamento |
| 4. Voltar | Digite e pressione Voltar/gesto sem enviar | Editor fecha; não há mensagem vazia nem requisição; CJ pode andar normalmente |
| 5. Repetição | Abra/cancele 10 vezes e depois envie | Sem View duplicada, foco retido, teclado preso ou bloqueio de controles |
| 6. Troca de NPC | Feche/troque pelo chat; inicie com outro pedestre | Editor antigo fecha, histórico separado e nenhum callback antigo enviado ao novo NPC |
| 7. Gemini | Execute seu modo Gemini já funcional e envie pelo editor | Mesma worker/servidor; resposta sobre o NPC por 15 s; nenhum segredo nos logs |
| 8. NPC removido | Durante a edição, remova/mate/descarregue o NPC pelo jogo | Editor é invalidado; nenhum acesso inválido nem envio tardio; controles liberados |
| 9. Resolução/IME | Teste orientações suportadas pelo GTA, escala de render, teclado normal/flutuante e paisagem | Enviar acessível, campo acima da IME, margens coerentes e balão junto da cabeça |
| 10. Retomada | Após enviar/cancelar, mova CJ, use controles e alterne Termux/GTA | Interação normal; pausa real de segundo plano respeitada; sem bloqueio persistente |
| 11. Altura | Teste skins pequenas/grandes, agachado e em veículo; varie height 1,60/1,75 e gap 0/6 | Marcador acompanha o modelo; fallback/calibração documentados; nenhuma alteração da posição real |
| 12. Postura | Converse com ped comum parado/andando; aguarde resposta; feche e afaste CJ | NPC elegível para; gesto curto na fala; retoma comportamento após expirar a tarefa. Missão/combate/veículo preservados |

## Logs e diagnóstico de pausa

No computador com adb, capture desde a abertura do processo:

```sh
adb logcat -c
adb logcat -v threadtime > npcai-1.2.1-logcat.txt
```

Abra GTA, reproduza abertura → digitação → Enviar/Voltar e encerre a captura com Ctrl+C. Use depois a busca por `NPC_AI`, `NPC_AI_IME`, `AndroidRuntime`, `ActivityTaskManager` e `WindowManager`; mantenha o log completo ao investigar rejeição pelo loader ou eventos fora da tag.

Sequência típica:

```text
IME show queued
editor_attached same_activity=true ...
edit_focus=true
ime_show_requested=...
ime_visible=true ...
send_button                 (ou ime_send)
editor_close reason=submit  (ou back_pre_ime / ime_hidden_back_or_gesture)
IME callback: send accepted (ou cancel accepted)
IME completion consumed on game thread: send (ou cancel)
Message queued to worker; controls released ...
```

`Conversation closed; controls released` identifica cancelamento. `native_invalidation` e `stale_ignored` são esperados quando o NPC deixa de ser válido. Não incluem a mensagem digitada.

- `activity_paused/resumed/stopped` vêm dos callbacks da Application para a Activity host; permitem distinguir ciclo de vida real de uma janela de teclado.
- `window_focus` registra foco da janela host recebido pela View; `edit_focus` é apenas foco local do campo. Perder foco local não prova pausa da Activity.
- `Game state userPause=... codePause=... inputSuppressed=...` observa flags e supressão quando o processo da engine executa. Não os escreve.
- `IME render heartbeat` a cada cerca de 2 s durante a edição indica que o callback de render do plugin continua rodando. Se logs Java continuam mas esses heartbeats param, é uma pista de interrupção do loop/render; não prova sozinho a causa.
- `Bubble projection render=... imgui=...` registra escalas; `Bubble anchor ... source=...` mostra se foi usada colisão ou fallback.
- Nenhuma chave, token, mensagem ou conteúdo do histórico é registrado por esses diagnósticos. Revise o restante do logcat do aparelho antes de compartilhá-lo.

## Código alterado e compilação

- `android/br/com/npcai/NativeEditor.java`: overlay no content host, layout IME, foco, Voltar, cleanup e observadores.
- `native/generated/editor_dex.h`: Java recompilado e DEX embutido.
- `native/src/npcai/native_keyboard.{h,cpp}`, `editor_state.h`: ponte/cancelamento/tickets/fechamento/diagnóstico.
- `game_bridge.{h,cpp}`, `focus_lease.h`: âncora por bounds, projeção, logs, postura e duração da conversa.
- `bubble_layout.h`, `ui.{h,cpp}`: conversão/posicionamento, ponta, estado nativo e liberação de input.
- `settings.{h,cpp}`, `config/npcai.ini`, `native/main.cpp`: novos parâmetros e versão 1.2.1.
- `tests/test_native.cpp`, `tests/test_ui.cpp`: regressões de geometria, cancelamento, vida útil e fluxo.
- `tools/package.py`, `.github/workflows/build.yml`, README e documentação: distribuição 1.2.1.

Com Java, Python, g++ e NDK r27d disponíveis:

```sh
./tools/build_keyboard.sh
./tools/test.sh
ANDROID_NDK_HOME=/caminho/android-ndk-r27d ./build.sh
python3 tools/package.py
```

Os logs reais desta entrega estão em `docs/validation/1.2.1/`. Os testes host cobrem projeção em resoluções diferentes, posição da ponta, parâmetros inválidos, cancelamento/reabertura/tickets em threads, expiração do foco, estado da UI durante edição, troca/destruição do NPC com mocks e todos os testes anteriores de servidor/HTTP/Gemini. Eles não executam a Activity Android, a IME, o renderer GTA ou as tarefas de IA do jogo. Nenhum resultado desses testes deve ser apresentado como validação no dispositivo.

## Resultado real da entrega

- Build Java/DEX e NDK **r27d / 27.3.13750724** concluídos com código 0, C++17, **arm64-v8a**, API26.
- **65 verificações aprovadas**: 13 cenários nativos, 10 de UI/touch/fluxo e 42 testes Python/HTTP/compatibilidade/Gemini.
- ELF64 AArch64 DYN, exports AML conferidos e segmentos LOAD alinhados a 16 KiB.
- NEEDED: libc.so, libdl.so, liblog.so e libm.so; são dependências padrão Android permitidas. STL estática.
- Plugin: **2240464 bytes**, SHA-256 `87d20ef00660ca039c57451359a9b15ba5188deb3d897bd6b9a7dccac4502177`.
- Os headers SDK mantêm avisos de qualificadores const sem efeito. Nenhum erro no build final.
- Nenhum teste desta versão em Android/GTA/IME real foi realizado por este ambiente; não houve chamada nova à API Gemini real. O funcionamento do servidor/Gemini na versão anterior foi informado pelo usuário.

Distribuição: `NPC_AI_1.2.1_ARM64.zip`, com binário, fontes correspondentes, configuração, servidor, testes, workflow e licenças.
