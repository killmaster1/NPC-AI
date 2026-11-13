# Validação real da entrega 2.4.0 — 13/11/2026

## Executado neste ambiente

- Build real de `libnpcai.so` com Android NDK r27d/27.3.13750724, C++17, arm64-v8a, API26. DEX incorporado gerado por ECJ/D8 para API26. Warnings de macros do SDK e depreciação Java não foram tratados como sucesso de teste nem ocultados; o build terminou sem erros.
- Verificação ELF do plugin: ELF64/AArch64, exports AML, LOAD de 16 KiB e dependências Android padrão `libc`, `libdl`, `liblog`, `libm`. As bibliotecas Kokoro opcionais continuam sendo ELF Android ARM64, API <=26, sem libc++ compartilhada, com fechamento de dependências e hashes conferidos.
- **29 cenários C++ novos** de ElevenLabs/core: v3 e Flash, construção/whitelist, vozes por modelo/instância/identificador verificado, distribuição e retenção, catálogo simulado, cache persistente/RAM/configurações/corrupção/LRU/limpeza, orçamento/reinício/corrupção, chave ausente, ausência de retries em 401/402/403/429/503, cancelamento, textos técnicos/vazios, UTF-8/limites, relação/recuperação/mira não atribuída, palavrões/repetição, contrato emocional, SHA-256, INI duplicado, traços estáveis, teste de voz com o modelo real selecionado e troca de motor com trabalho pendente.
- Regressões C++ aprovadas: perfis Kokoro (15), worker de voz (22, incluindo serial repetido e interrupção antes da inferência retornar), configuração/diagnóstico (23), inteligência (22), núcleo, animações, ações e UI. O cenário UI foi executado para Kokoro e ElevenLabs indisponíveis, preservando balão/IME/diagnóstico sem NPC.
- **87 testes Python/JVM aprovados**, incluindo NativeEleven real com transporte HTTPS substituído por mocks: headers/URL, TLS e hostname padrão, redirects bloqueados, limites PCM, credencial ausente, erros, sem retries, cancelamento e AES-GCM com chave de teste. O teste separado de TLS rejeitou certificado não confiável no host. Nenhuma chave real foi usada.
- INI de exemplo completo, sem valores secretos, verificado pelo parser real. Seleção explícita de Gemini não muda para Mock quando a rede/configuração falha. Erros de voz permanecem independentes da UI.
- O workflow de publicação foi testado de fato na Release 2.3.0: run `43462737317`, conclusão success, dois ZIPs permanentes com estado `uploaded`. Isso valida a rota GitHub-hosted para upload de assets, não a API ElevenLabs. A publicação 2.4.0 será verificada separadamente após upload.

## Não executado / pendente

**Não houve chamada real à ElevenLabs, geração paga, validação perceptiva de tags/emoções, importação AndroidKeyStore num aparelho, AudioTrack real, GTA executando ou medição de FPS/PSS no telefone.** Não usamos a chave Gemini do usuário nos testes. Testes de transporte e conta são mocks, e o build ARM64 não confirma runtime Android.

A fonte atual compila e as partes independentes da engine foram exercitadas. Um teste de síntese Kokoro em Linux da versão 2.3.0 permanece documentado no histórico, mas não é prova de ElevenLabs nem de execução GTA. A ponte continua sem identificação verificável de personagens nomeados e sem atribuição nova de dano/mira direta ao CJ.

Leia `docs/ANDROID_ELEVENLABS_TESTS.md` para a validação ainda necessária. Consumo de créditos é estimativa local conservadora; custos efetivos, suporte a voz/tag/modelo e retenção dependem da conta e do provedor.
