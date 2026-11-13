# Validação NPC AI 1.3.0

Esta versão foi implementada sobre os arquivos 1.2.1 presentes no workspace. Os relatos de funcionamento do usuário dizem respeito às versões anteriormente testadas no seu GTA; não substituem testes desta entrega.

## Resultados efetivamente obtidos

| Verificação | Resultado |
|---|---|
| Android NDK | r27d, revisão 27.3.13750724; compilação exit 0 |
| Saída | libs/arm64-v8a/libnpcai.so, ELF64 AArch64 DYN |
| ELF | Exports AML presentes; sem símbolos SDK/ImGui externos pendentes; segmentos LOAD alinhados a 16 KiB |
| NEEDED | libc.so, libdl.so, liblog.so, libm.so; bibliotecas padrão Android aceitas |
| Ponte Java/DEX | Recompilada com ECJ 3.37.0 / D8 8.3.37, API26, exit 0; 19252 bytes DEX |
| Dependências | 368 arquivos vendorizados conferidos por SHA-256; inclui nlohmann/json 3.11.3 e licença MIT |
| Testes C++ existentes | 13 cenários aprovados |
| Controlador/parser/configurações | 15 cenários aprovados, backend falso no host |
| Interface ImGui | 10 cenários aprovados; 640x360, 1280x720, 1920x1080 e 720x1280, além de fluxo e captura touch |
| Python/HTTP | 60 testes unittest aprovados, incluindo cliente C++ ↔ servidor real em loopback e contratos de provedores simulados |

Total: 98 cenários/testes nos grupos acima. Isso **não** significa 98 testes dentro do GTA. O SDK Android foi compilado e auditado, mas não há executável GTA nem dispositivo conectado para verificar seus efeitos.

SHA-256 do plugin: `5ae85983ad237e1ee61fc060baeb401a4a4b5237d2e6f79787743b58e88e535e`.

A primeira execução do host no sandbox passou os testes nativos/UI, mas não conseguiu abrir sockets por restrição do ambiente. A rodada final foi executada com acesso a sockets de loopback e passou completamente. Um erro de compilação host por indentação ambígua foi corrigido. O build Android ainda produz avisos de headers/qualificadores do SDK; nenhum erro de compilação permaneceu.

## Cobertura nova

- Whitelist/parser estrito, UTF-8, nonce, JSON inválido/duplicado, parâmetros incorretos e compatibilidade textual.
- Parar/orientar no início, aguardar pausa, liberar controle via backend, cancelamento por NPC inválido/prioridade de combate ou missão, timeout e animação indisponível.
- Rendição com confirmação adiada, soma/quantidade dos pickups, deduplicação por resposta e por NPC, orçamento de sessão, remoção de lote parcial e limpeza por expiração/reset/invalidação.
- Seguimento temporário e stop_following; feedback só após confirmação; nenhum diálogo de ação antecipado.
- Propostas mock de assalto respeitando civil/gangue/policial; HTTP protocolo 2, IDs dinâmicos, feedback para o provedor e fallback sem ação.
- Regressões existentes: memória isolada, servidor offline, timeout, resposta tardia, cancelamento da worker, IME mailbox e ciclo cancelar/reabrir, seleção/invalidação, balões e touch.

## Pendências reais

Todos os testes nativos da engine no aparelho: presença dos símbolos/semântica das tarefas, rotação suave, postura e animações originais, prioridade de tarefas de missão/combate, retomada da IA, coleta e valor no saldo CJ, remoção de pickups, desempenho e compatibilidade com mods. Os eventos Android reais de foco/IME/lifecycle e projeção em diferentes telas também continuam dependentes de dispositivo. Não foram feitas chamadas reais a Gemini/OpenAI nesta validação; os contratos usam respostas simuladas sem credenciais.

Consulte [o roteiro do aparelho e instalação](RELEASE_1_3_0.md) e [a auditoria do SDK](SDK_AUDIT.md).

## Arquivos principais desta atualização

| Arquivos | Mudança |
|---|---|
| native/src/npcai/actions.{h,cpp} | Contrato e parser de ações |
| native/src/npcai/npc_action_controller.{h,cpp} | FSM, limites, deduplicação, timeout, recibos e limpeza |
| native/src/npcai/engine_actions.{h,cpp} | Backend Android de tarefas, animações e pickups |
| game_bridge.cpp, registry.{h,cpp} | Integração no tick, validade, prioridades, invalidação e foco |
| chat.{h,cpp}, types.{h,cpp}, http_client.cpp | Propostas da worker, diálogo adiado e feedback |
| settings.{h,cpp}, config/npcai.ini | Novas opções com validação |
| server/actions.py, server/server.py, server/.env.example | JSON comum a Mock/Gemini/OpenAI, schemas e limites |
| tests/test_actions.cpp, test_actions_server.py, test_http.cpp | Cenários de ações e integração |
| tests/test_server.py, test_gemini.py | Verificação de schemas enviados aos provedores |
| tools/test.sh, Android.mk, native/main.cpp | Testes, fontes e versão 1.3.0 |
| tools/package.py, .github/workflows/build.yml | Distribuição e artifacts 1.3.0 |
| third_party/json e manifest.json | Parser JSON vendorizado com licença/hashes |
| README e docs | Instalação, limites, auditoria e evidências |

O código Java/IME, layout de balões e teclado touch foram preservados; o DEX gerado foi recompilado. Application.mk mantém C++17, arm64-v8a, STL estática e API26.

Evidências: [testes](validation/1.3.0/tests-host.txt), [build](validation/1.3.0/build-android.txt), [ELF](validation/1.3.0/elf.txt), [toolchain](validation/1.3.0/toolchain.txt), [hashes das fontes](validation/1.3.0/source-sha256.txt). O workflow foi configurado, mas não executado remotamente no GitHub nesta sessão.
