# 2.4.1 — Fluidez Kokoro

- Frases mantidas inteiras ate 320 bytes, preservando abreviaturas, decimais, reticencias e UTF-8; corte de seguranca prioriza pausas.
- Abreviacoes de chat expandidas somente na sintese offline; metadados nao sao pronunciados.
- Ritmo emocional discreto e opcional; removida variacao arbitraria de velocidade por identidade. Sem pitch artificial.
- FP32 e speakers oficiais 42/43/44 preservados; nenhuma nova copia do modelo. Backend Kokoro descarta tambem a credencial ElevenLabs da copia de configuracao.
- Nao ha promessa de qualidade equivalente ao ElevenLabs nem validacao Android por testes host.

# Changelog

## 2.4.0

- ElevenLabs HTTPS nativo com v3 como padrão solicitado e Flash v2.5 alternativo, credenciais e fila independentes do Gemini.
- Tags de delivery/emocionais controladas somente no v3; Flash usa parâmetros compatíveis e texto limpo. PCM 24 kHz decodificado corretamente no AudioTrack existente.
- NpcVoiceManager por identidade de sessão, mapeamentos explícitos de modelos/IDs verificados, consulta de vozes da conta e variedade sem promessa de exclusividade ilimitada.
- Cache local SHA-256, LRU/limites e persistência opcional; orçamento hora/dia persistente e conservador, sem retry automático de síntese.
- Perfil individual ampliado, relação inicialmente neutra, recuperação gradual, emoções/contexto compacto, menos palavrões e repetição. Ações e memória existentes preservadas.
- Diagnóstico Voz e personalidade, importação de chave em editor de senha na mesma Activity, remoção, status, teste com aviso de custo e limpeza de cache.
- Diagnóstico claro de INI duplicado e floats independentes de locale; seleção de provedores e inicialização da UI seguem independentes.
- Build Android ARM64/r27d e testes host/JVM; validação real ElevenLabs/Keystore/áudio/GTA pendente.

## 2.3.0

Kokoro FP32/Sherpa-ONNX, três speakers brasileiros, perfis efêmeros estáveis e áudio estéreo aproximado. Publicação anterior por ZIPs no repositório; workflow posterior habilitou assets permanentes de Releases.
