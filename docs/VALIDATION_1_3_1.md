# Validação NPC AI 1.3.1

## Resultados reais desta entrega

- Build concluído com exit 0: Android NDK r27d, revisão 27.3.13750724, C++17, arm64-v8a, API26.
- ELF64 AArch64 DYN aprovado pelo verificador: exports AML, alinhamento de segmentos 16 KiB, dependências libc.so, libdl.so, liblog.so, libm.so. Os avisos de headers do SDK continuam; não há erro de build.
- 13 cenários nativos existentes aprovados.
- 18 cenários do controlador/parser/integração Chat aprovados, usando engine falsa no host.
- 10 cenários ImGui/touch aprovados, incluindo quatro resoluções e fluxo editor/chat.
- 62 testes Python/HTTP aprovados, incluindo cliente C++ em loopback e contratos simulados de Gemini/OpenAI.
- Total: **103 cenários/testes host**, nenhum executado dentro do GTA neste ambiente.
- ZIP verificado por CRC, presença dos arquivos necessários e igualdade do plugin empacotado com o ELF validado.

SHA-256 libnpcai.so: `1259ce4ebcc8ed2f91d294e62bed640967a151afedf55b1ded7979497894e6db`.

## Regressões adicionadas

1. Fala de talk_gesture aparece antes de animação; recusa por prioridade da engine não apaga nem duplica a fala. Feedback ainda registra failed.
2. Sucesso/falha de orientação não coloca sufixos técnicos ou referência a entrega de dinheiro na saudação.
3. Falhas de surrender_money, follow_player, hands_up, flee e cower não mostram a declaração de ação concluída do modelo.
4. Mock usa none para Oi.
5. Prompt dos provedores separa conversa de orientação e parada inicial.

As proteções monetárias, testes de timeout, resposta tardia, shutdown, invalidação, missão/combate, cancelamento, parser e memória continuam passando. Java/DEX e layout não foram alterados nesta correção; sua interação real com o aparelho continua dependente de teste manual. APIs externas não foram chamadas com credenciais: contratos HTTP dos provedores usam respostas simuladas.

## Causa confirmada e pendência

Confirmado no código: ReportAction substituía diálogo por mensagens internas; o controlador anexava nomes técnicos; o servidor induzia ações em conversa comum. Esses pontos foram corrigidos.

Não confirmado pelas imagens: causa exata de inelegibilidade/interrupção das tarefas do pedestre no executável do usuário. Não foi relaxado nenhum bloqueio de missão/combate nem inventada confirmação de efeito. Novos logs permitem distinguir esses casos. É necessário reproduzir no aparelho com a nova versão e consultar Action gate / Action failure; confira [o roteiro](RELEASE_1_3_1.md).

## Arquivos alterados

- actions.{h,cpp}: classificação de ações de fala.
- chat.{h,cpp}: publicação de diálogo opcional e apresentação de resultados sem diagnóstico técnico.
- npc_action_controller.{h,cpp}: motivos específicos de rejeição e remoção do sufixo de confirmação.
- engine_actions.cpp: causas de elegibilidade e falhas de tarefa no log.
- game_bridge.cpp: diagnóstico interno de falhas separado da fala.
- server/server.py: mock e instruções para conversa sem ação obrigatória.
- tests/test_actions.cpp, tests/test_actions_server.py: regressões.
- native/main.cpp, workflow e package.py: versão 1.3.1.
- README e documentação desta entrega.

Fontes completas e os dois módulos Python estão no ZIP; configure sua chave somente no ambiente privado. Evidências: [testes](validation/1.3.1/tests-host.txt), [build](validation/1.3.1/build-android.txt), [ELF](validation/1.3.1/elf.txt), [hashes](validation/1.3.1/source-sha256.txt). O workflow foi atualizado, mas não foi executado remotamente nesta sessão.
