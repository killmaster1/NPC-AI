# NPC AI 2.4.1 — ajustes de fluidez Kokoro

Esta atualização mantém os pesos FP32 e os speakers brasileiros verificados: pm_alex (43), pm_santa (44), pf_dora (42). Não há modelo novo, re-treinamento, clonagem ou tags emocionais Kokoro. Não prometemos qualidade equivalente ao ElevenLabs nem naturalidade avaliada por teste automático.

## O que mudou

O worker dividia texto em blocos de 180 bytes, inclusive no meio da frase. Agora mantém frases até 320 bytes, reconhece reticências, abreviaturas comuns e números decimais. Frases maiores continuam limitadas para manter cancelamento e memória; prioriza pausas de oração e depois espaços. UTF-8 e todo o texto são preservados na segmentação.

A fonemização offline recebe palavras completas no lugar de abreviações restritas de chat: vc/vcs/tb/tbm/cmg/hj/blz. Acentos, gírias, números e o significado continuam presentes. Diretivas entre colchetes não viram fala. O balão mantém o texto original, e expansões representam as mesmas palavras. Não interpretamos instruções como comandos de áudio.

A velocidade não recebe mais variação arbitrária pela identidade do NPC. A emoção já existente permite variação de duração discreta (até -4% ou +3%, proporcional à intensidade), sem alterar pitch ou aplicar efeitos. Isso é controle de ritmo, não um modelo expressivo de emoção. Voz por identidade, FP32, 24 kHz, estéreo aproximado, cache, worker, cancelamento e sincronização existentes permanecem.

## Instalar e configurar

Instale `mods/libnpcai.so` atualizado e preserve seu INI. As bibliotecas e o modelo Kokoro 2.3.0/2.4.0 podem ser reutilizados. O instalador inclui dependências Android; o modelo opcional permanece na [Release 2.4.0](https://github.com/killmaster1/NPC-AI/releases/tag/v2.4.0). Nenhuma chave é necessária para Kokoro. Gemini continua independente.

Edite as linhas existentes, sem criar duplicatas:

```ini
npc_tts_enabled=1
npc_tts_engine=kokoro
npc_tts_language=pt-BR
npc_tts_model=kokoro-pt-br
npc_tts_voice=pm_alex
npc_tts_voice_variation=0
npc_tts_speed=0.96
npc_tts_volume=0.85
npc_tts_threads=2
npc_tts_kokoro_prosody=1
```

A nova opção `npc_tts_kokoro_prosody` aceita 0/1, padrão 1. Com 0, desativa expansões de chat e o ajuste emocional de velocidade. A correção de segmentação permanece. Não altera ElevenLabs ou Gemini. Velocidade 0.96 é um ponto inicial para comparação, não melhora garantida.

A pasta `PREVIAS_KOKORO` do instalador contém WAVs dos três timbres, gerados no Linux com a mesma frase e velocidade 1.0. Servem para escolher a voz; não são uma comparação antes/depois nem áudio capturado no Android.

No menu, desligue “Variar a voz por NPC” durante a comparação e teste Alex, Santa e Dora com a mesma mensagem. Depois reative se desejar. Os ajustes do menu só duram a sessão. Santa e Alex são timbres diferentes; escolha pela escuta, não por um rótulo de qualidade. Não alteramos gênero/identidade da engine para escolher vozes.

## Roteiro no aparelho

1. Teste as três vozes com a mesma frase e sem música alta.
2. Compare velocidades 0.96 e 1.0. Não acelere para compensar demora de inferência.
3. Envie uma frase longa sem ponto no meio e confira se não corta palavras.
4. Teste “O Dr. Silva pagou 2,50. Tá tudo certo?” e uma frase com acentos e gírias.
5. Alterne NPC, afaste-se, pause e cancele durante a geração.
6. Confira tempo de síntese no diagnóstico e observe FPS, aquecimento e uso de RAM no Mi 11 Lite.
7. Compare o controle de ritmo com a opção nova em 0/1 e a mesma velocidade.

O Mi 11 Lite não foi usado nesta implementação. Síntese Linux e testes host não comprovam naturalidade, latência ou FPS no Android. O timbre e a expressividade continuam limitados pelo modelo Kokoro e pelas três vozes disponíveis.
