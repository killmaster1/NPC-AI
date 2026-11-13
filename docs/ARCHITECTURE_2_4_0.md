# Arquitetura 2.4.0

GameBridge (thread do jogo) publica IDs geracionais, posição/projeção e contexto observado → Chat/IntelligenceRuntime (contexto compacto, relação, memória, Gemini) → SpeechEvent com serial e emoção/delivery → VoiceManager (uma worker + watchdog) → ElevenLabsTtsEngine ou KokoroTtsEngine → AudioTrack JNI.

- **eleven_core.h**: NpcVoiceManager, CreditBudget e PcmCache; independentes da engine, testados em host. JSON de orçamento tem schema/checksum e gravação atômica; PCM de cache tem versão e SHA-256. PicoSHA2 MIT vendorizado, sem dependência Android extra.
- **expressive.h**: SocialState, recuperação/heurísticas, whitelist de emoções/delivery, normalização de texto, redução por frases completas e moderação leve de repetição/palavrões.
- **eleven_backend.cpp**: construção do contrato REST, parâmetros por modelo, tags somente v3, contadores, orçamento, cache e decodificação PCM normalizada. Um POST por fala, sem retries; cache e orçamento não dependem do Gemini.
- **NativeEleven.java / android_eleven.cpp**: JNI de rede com fila própria limitada e cancelamento, HTTPS padrão, key alias/arquivo separados, importação/remoção e consulta de vozes. Editor secreto reutiliza o contêiner da Activity existente, com callback que nunca chega ao histórico de chat.
- **VoiceManager**: todos os acessos e trocas de backend ocorrem na worker. Threads externas cancelam via flags e o watchdog interrompe o AudioTrack. Um evento só é anunciado como fala depois de o playback head avançar. Retorno atrasado não muda a voz do NPC atual nem recebe autorização para executar tarefas.
- **Engine SDK**: nenhuma nova estrutura, offset ou animação de GTA foi inventada. Novos estados emocionais não são gravados diretamente no comportamento original da engine; ações continuam passando pelo controlador e pelas validações existentes.

Identidades nomeadas continuam sem fonte verificável na ponte atual; a interface de mapeamento está preparada, mas não associa automaticamente uma skin a Sweet/Ryder ou a um indivíduo de sessão anterior. O campo de mira próxima também não prova alvo direto. Falas locais não afirmam autoria de fenômenos extraordinários sem evidência.
