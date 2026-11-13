# Validação NPC AI 1.4.0

## Resultados efetivamente obtidos

| Verificação | Resultado |
|---|---|
| Compilação | Android NDK r27d / 27.3.13750724, C++17, API26, arm64-v8a, exit 0 |
| Build completo | Recompilação forçada de todas as unidades concluída; ajuste final do controlador recompilado e religado em seguida |
| ELF | ELF64 AArch64 DYN, exports AML verificados, LOAD alinhado a 16 KiB |
| NEEDED | libc.so, libdl.so, liblog.so, libm.so; bibliotecas padrão Android aceitas |
| Dependências | 368 arquivos vendorizados conferidos por SHA-256 |
| Nativos existentes | 13 cenários aprovados |
| Controlador/parser/Chat | 18 cenários aprovados |
| Animações persistentes/leitura/traços | 20 cenários aprovados, backend simulado independente do GTA |
| ImGui/touch | 10 cenários aprovados, quatro resoluções e ciclo do editor |
| Python/HTTP | 83 testes unittest aprovados; sockets reais em loopback e cliente C++ |
| Java/DEX | Fontes do editor e DEX embutido idênticos byte a byte à distribuição 1.3.1 |

Total: **144 cenários/testes de host**. Os testes de engine usam mocks; nenhum dos 144 foi executado dentro do GTA por este ambiente. Os provedores foram simulados nos testes: não houve chamada autenticada real ao Gemini/OpenAI nem gasto de quota.

SHA-256 libnpcai.so: `4cca2a27973ab5cd4415b7a8cf3850777806bdb26d1df65daab4233a78269bc4`.

O build do SDK continua emitindo avisos de headers/qualificadores; não restaram erros de compilação. A ponte Java não foi alterada e não precisou de nova geração; a comparação de bytes com o ZIP anterior comprovou sua preservação. O workflow 1.4.0 foi atualizado, mas não executado remotamente no GitHub nesta sessão.

## O que os testes novos comprovam

- Uma rendição confirmada permanece por 30 s no estado SURRENDERING, sem reiniciar a tarefa a cada frame.
- Expirar um gesto/fechar chat/nova fala não encerra a rendição.
- Distância >6 m por 2 s contínuos libera; reaproximação reinicia a histerese.
- Morte/remoção e contexto de combate/missão inelegível cancelam as tarefas do mod.
- Seguimento continua além do prazo de duration_ms e do raio de 3 m, sem parada/gesto incompatíveis.
- Orientação usa uma tarefa independente, preservando a pose secundária no backend falso.
- Entrega preserva a pose, soma exatamente 103 no caso com resto de divisão, limita/deduplica e exige que a pose esteja confirmada no momento do lote.
- Recuperação de pose perdida é limitada; ausência de animação não cria loop de tentativas ilimitadas.
- Duração curta/longa responde à velocidade de leitura; fala longa tem vários gestos e pausas, fala curta tem um gesto, sem recriar a parada.
- Publicação de fala acontece após o retorno da worker; nenhuma animação é agendada enquanto HTTP está pendente. Balão dinâmico opcional recebe a estimativa, default permanece separado em 15 s.
- Clip que acaba cedo não gera uma sequência acelerada: respeita a janela planejada antes do próximo trecho. Esse teste inicialmente expôs a cadência excessiva e o agendador foi corrigido.
- Traços são distintos entre identidades e estáveis durante observações; medo pode mudar somente a partir de confirmações locais. Configurações e raio antigo maior têm validação/compatibilidade.
- Fala válida sobrevive a ação desconhecida ou parâmetros ruins; propostas/resultados ficam fora de messages; memória JSON antiga é convertida em fala.
- Variações informais são reconhecidas em mock, Late não cria ação fictícia, personalidades podem produzir reações diferentes ao mesmo assalto.
- Gemini 503 tem retry limitado, 429 respeita Retry-After, credenciais/modelos não são repetidos, orçamento é respeitado, leitura incompleta pode ser repetida e uma resposta lógica gera uma única proposta.
- Erro de provedor não aparece como fala do NPC; replay do mesmo nonce não chama o provedor novamente enquanto estiver no cache limitado.

Também passaram as regressões de UTF-8, identidade geracional/reuso de pool, troca/limpeza, resposta atrasada, cancelamento/encerramento da worker, memória isolada, servidor offline, timeouts, parser, IME mailbox, balões, layout e captura touch.

## O que ainda depende do aparelho

A manutenção visual real de HANDSUP, efeito de bHoldLastFrame/timer no executável 2.10 específico, blending ao liberar, animações CHAT/ENDCHAT2, giro suave, associação observada versus pose final, retomada da IA original, presença/semântica dos símbolos, prioridades reais de eventos e desempenho com outros mods. Também é necessário conferir a coleta e o valor no saldo de CJ, o seguimento prolongado, descarregamento real de entidades e IME/lifecycle Android.

Não foi liberado controle de tarefas primárias desconhecidas para contornar recusas. Um NPC pode continuar inelegível por prioridade real; Action gate informa o motivo. A recuperação é limitada e uma falha física não é anunciada como sucesso. Uma fala do modelo é uma proposta verbal; a engine só confirma seu efeito observável separadamente.

Execute [o roteiro no Android](RELEASE_1_4_0.md). A auditoria de headers, timers e vida de tarefas está em [SDK_AUDIT.md](SDK_AUDIT.md).

## Arquivos principais alterados/criados

| Caminho | Finalidade |
|---|---|
| native/src/npcai/npc_action_controller.{h,cpp} | FSM persistente, prioridade, distância/histerese, agendamento e dinheiro sem reiniciar pose |
| native/src/npcai/engine_actions.{h,cpp} | Tarefas primárias/secundárias separadas, hold/timer, observação de progresso, término e logs |
| native/src/npcai/dialogue_timing.h | Estimativa local de leitura |
| native/src/npcai/personality.h | Traços estáveis por identidade |
| registry.{h,cpp}, types.{h,cpp} | Vida dos traços e contexto serializado |
| game_bridge.cpp | Eventos de fala, detach no tick, contexto de comportamento e validade além do raio inicial |
| chat.{h,cpp}, actions.{h,cpp}, http_client.cpp | Fala preservada, recibos separados, whitelist de soltura, timeout informado |
| ui.cpp | Balão opcional dinâmico e renovação de frase idêntica; default 15 s |
| settings.{h,cpp}, config/npcai.ini | Opções persistentes/leitura/debug e validação |
| server/server.py, server/actions.py, server/.env.example | Prompt, traits, mock, histórico natural, cache e retries com prazo total |
| tests/test_animation.cpp, tests/test_reactive.py | Regressões novas |
| tests/test_actions.cpp, test_http.cpp, tools/test.sh | Adequação às semânticas persistentes e suíte ampliada |
| native/main.cpp, workflow, tools/package.py | Versão/artifacts 1.4.0 |
| README e docs | Instalação, limites, roteiro e evidências |

Evidências: [testes](validation/1.4.0/tests-host.txt), [recompilação completa](validation/1.4.0/build-complete.txt), [ajuste final compilado](validation/1.4.0/build-final.txt), [ELF](validation/1.4.0/elf.txt), [toolchain](validation/1.4.0/toolchain.txt), [hashes das fontes](validation/1.4.0/source-sha256.txt).
