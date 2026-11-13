# NPC AI 1.3.1 — diálogo preservado quando uma ação falha

## Diagnóstico das capturas

A captura Android mostra `[Ação confirmada: face_player.]` misturado à fala. A captura ImGui mostra mensagens como `Não posso executar esse pedido nesta situação. Nenhuma entrega foi feita.` mesmo após `Oi`, e `A tarefa foi interrompida ou não produziu efeito.`

No código 1.3.0, toda resposta estruturada ficava retida aguardando a engine, inclusive conversa com gesto opcional. O ReportAction substituía a fala pela string interna de erro, e o controlador acrescentava nomes técnicos às confirmações. Isso explica a perda de diálogo e a referência a dinheiro fora de contexto. As imagens não identificam qual condição da engine rejeitou a tarefa do pedestre modelo 22. Não há confirmação de missão, combate ou símbolo ausente apenas pela aparência do NPC.

## Correções implementadas

- Conversa com `talk_gesture`, irritação verbal ou recusa verbal aparece sem esperar uma animação. Falha da animação mantém a fala, sem duplicar histórico. O resultado físico continua sendo `failed`, nunca sucesso fictício.
- `face_player` e `idle` ainda aguardam confirmação; se falharem, preservam a fala comum. Não há sufixo `[Ação confirmada: ...]` nem diagnóstico de SDK dentro da conversa.
- Dinheiro, seguir, fugir, mãos levantadas e abaixar continuam com confirmação física adiada. Falhas usam uma fala curta apropriada ao pedido; jamais mostram uma falsa entrega ou execução concluída.
- Mock usa `action=none` para conversa comum. Gemini/OpenAI recebem a mesma instrução: postura inicial já é controlada pelo plugin; não devem propor `face_player` em toda saudação nem narrar um gesto como concluído.
- Logs distinguem a causa de inelegibilidade: identidade/raio, hooks ausentes, missão, veículo, dano, estado físico, tarefa prioritária, resposta temporária estrangeira e ataque. Registram máscaras de slots, modelo e estado, sem mensagens/chaves. Configurações e limites monetários também têm motivos próprios.
- Nenhuma proteção de missão, combate, validade ou dinheiro foi removida. Não foram adicionados offsets, símbolos ou animações novos nesta correção.

## Atualização exata

1. Feche GTA e pare o servidor Termux com Ctrl+C.
2. Extraia `NPC_AI_1.3.1_ARM64.zip`. Substitua **somente uma cópia** de `libnpcai.so` na pasta efetiva `mods` do AML pela versão em `npcai-android/libs/arm64-v8a/`. No aparelho anteriormente relatado: `/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/mods/`.
3. Copie `npcai-android/server/server.py` e `actions.py` para `~/npcai-server` no Termux. Preserve seu `.env` privado e `npcai.ini`; nenhuma opção nova é necessária.
4. Execute:

```sh
cd ~/npcai-server
set -a
. ./.env
set +a
python server.py
```

Use seu `NPC_AI_MODE=gemini` habitual. Para primeiro teste gratuito, após carregar o ambiente, execute `NPC_AI_MODE=mock python server.py`. Não publique sua chave.

5. Abra GTA de novo e inicie conversa nova (ou use Limpar conversa). Teste `Oi`, uma pergunta comum, gesto, assalto e seguimento. Não deve haver nomes de funções/ações nem erros de engine como se fossem falas. Uma tarefa indisponível pode continuar sendo recusada fisicamente; isso deve estar no log, sem destruir a conversa.

## Como verificar no aparelho

| Caso | Resultado esperado |
|---|---|
| `Oi` e assunto cotidiano em mock | Resposta normal, sem ação física adicional por mensagem |
| `Oi` em Gemini | Resposta normal; modelo instruído a usar none, porém qualquer gesto opcional falho não apaga o texto |
| Recusa/irritação com animação indisponível | Uma única fala, mesmo se o gesto não ocorrer |
| Orientação aplicada | Fala sem `[Ação confirmada: face_player.]` |
| Pedido de dinheiro negado ou repetido | Nenhuma falsa entrega; frase sobre dinheiro somente nesse pedido |
| Seguimento/rendição/fuga realmente executados | Backend ainda exige efeito observado; conferir visualmente e verificar saldo ao coletar |
| Trocar, limpar, sair do raio, fechar, Voltar | Descarte de proposta antiga e liberação dos controles conforme versão anterior |
| IME e alternativa touch | Enviar e cancelamento continuam funcionando; balão e histórico mostram a mesma fala |

Se as tarefas físicas ainda forem recusadas, capture o log durante uma tentativa:

```sh
adb logcat -c
adb logcat -v time > npcai-131-logcat.txt
# Reproduza o erro, encerre a captura e filtre no computador:
rg 'Action gate=|Action failure|Action start|Action effect|Action symbols|Actions disabled|Game state' npcai-131-logcat.txt
```

`Action gate=foreign_temporary_event` indica uma tarefa temporária da engine que o mod preservou; `primary_task`, `physical_response` e `nontemporary_event` indicam prioridade protegida; `lifetime_hooks_unavailable` indica integração indisponível. `Action failure reason=...` informa limites ou falhas do controlador; `task_gone` indica fim/substituição da tarefa. Os motivos são diagnósticos internos, não falas do NPC. Consulte também o log AML. Não é necessário enviar mensagens privadas ou chave de API.

## Validação e limites

Veja [os resultados desta entrega](VALIDATION_1_3_1.md). Testes host usam uma engine falsa para falhas e confirmações; não comprovam animações, tarefas, IME real ou dinheiro no GTA. Esta versão corrige o acoplamento entre fala e execução e acrescenta evidência para diagnosticar a recusa física. **A causa específica das recusas no seu aparelho continua pendente do log e de novo teste real.**

As instruções gerais, whitelist, configurações, servidor e desinstalação estão em [RELEASE_1_3_0.md](RELEASE_1_3_0.md). Para remover: feche o GTA, retire o plugin, pare Python; não descarregue a biblioteca durante uma sessão.
