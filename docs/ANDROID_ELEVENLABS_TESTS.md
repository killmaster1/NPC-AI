# Validação no Android — NPC AI 2.4.0

**Todos os itens abaixo continuam pendentes em aparelho real nesta entrega.** Não foi usada chave ElevenLabs real nem executado GTA/Android por ADB. Testes JVM simulam transporte e verificam algoritmos; AndroidKeyStore, Activity e AudioTrack reais são outra etapa.

1. Atualize somente plugin/arquivos próprios, preserve o INI Gemini e remova blocos TTS duplicados. Confirme 2.4.0 e caminho real no diagnóstico.
2. Sem internet/chave/Voice ID, abra Conversar, balão, editor e menus; texto e controles não podem desaparecer. Gemini explicitamente escolhido deve informar falha, sem virar Mock.
3. Importe uma chave no campo de senha da Activity atual; teste enviar e Voltar/cancelar. Não deve entrar no chat/histórico/log nem pausar o GTA por uma segunda Activity. Feche/reabra e confirme a credencial protegida. Remova e reabra; não pode ressurgir do INI sem nova importação explícita.
4. Consulte vozes da conta, confirme IDs e permissões do plano. Mapeie modelos 105/106/107/280 com seus IDs reais, se desejar. Dois pedestres da mesma skin continuam tendo memórias/identidades distintas. Trocar modelo da mesma instância não deve mudar o timbre.
5. Teste primeiro uma fala curta com custo consciente. Abrir o menu, consultar status, falhar sem chave ou mudar volume não deve sintetizar nada. Confirme aumento do contador somente em tentativas de síntese.
6. Com v3, peça conversa normal, sussurro e ironia em contextos coerentes; confira fala natural e balão sem tags. Avalie se a voz escolhida realmente executa cada tag. Isso não é garantido pelo teste host.
7. Com Flash v2.5, confirme ausência de tags pronunciadas, aplicação de velocidade/estabilidade e texto igual ao balão. Não espere o mesmo controle expressivo do v3.
8. Repita a mesma fala com o mesmo ID, modelo e parâmetros: cache hit, sem nova tentativa paga. Reinicie o jogo e repita com cache persistente. Mude voz/modelo/emoção e confirme chave distinta. Limpar cache não deve zerar o orçamento.
9. Configure tetos baixos e tarifa conhecida; chegue a 80%, depois tente exceder. Deve avisar/bloquear antes do POST. Reinicie no mesmo período UTC e confira manutenção do consumo estimado. Não compare esse contador a uma auditoria exata da conta.
10. Teste 401/402/403/429 e perda de rede: sem retries automáticos e sem mensagens técnicas na fala. Uma resposta perdida pode ter sido cobrada no provedor.
11. Troque NPC, feche chat, provoque desaparecimento e pause o Android durante geração/playback. Resposta antiga não pode tocar no novo NPC; não deve haver áudio sobreposto. Reabra e continue usando os controles.
12. Rode a câmera e afaste CJ até o limite: testar pan/atenuação, invalidação e cancelamento. É estéreo aproximado, sem HRTF/oclusão.
13. Confirme gestos enquanto AudioTrack realmente toca e encerramento ao fim. Missão, combate, fuga e rendição têm prioridade; não há lip-sync fonético.
14. Cumprimente NPCs comuns e faça perguntas estranhas sem ameaçar. Devem começar neutros; teste provocação gradual, ameaça verbal, pedido de desculpa e recuperação com tempo. Não devem esquecer imediatamente um episódio sério. Investigue falsos positivos da heurística de linguagem.
15. Teste níveis 0/1/2/3 de palavrões, diversidade de aberturas e diferentes personalidades. O limite não deve provocar palavrões obrigatórios nem censura sem sentido.
16. Teste falas longas: reduzir por frases completas; frase única acima do teto fica em texto sem cobrança TTS. Preserve contexto/ações e confira correspondência entre fala efetiva e balão.
17. Alterne explicitamente para Kokoro com seus pesos instalados e volte ao ElevenLabs. Nunca deve mudar automaticamente o motor para esconder falhas. Faça isso com requisição pendente para testar propriedade do backend na worker.
18. Meça FPS, PSS/RSS, latência, armazenamento e 20 conversas sucessivas no telefone. Modelo ElevenLabs não exige carregar pesos Kokoro. Cache e fila devem permanecer limitados.

Registre aparelho, Android/API, AML, versão GTA, engine/modelo/Voice IDs usados (sem chave), HTTPs, cache hits, estimativas e métricas de desempenho. Não publique cache, chaves, memórias ou logs privados.
