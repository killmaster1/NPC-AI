# NPC AI 2.4.0 — ElevenLabs e Kokoro

GTA San Andreas Android 2.10 ARM64 / AML >=1.4 / Android >=8. Gemini gera diálogos e metadados numa chamada. Eleven v3 usa tags controladas; Flash v2.5 funciona sem tags; Kokoro continua offline. Texto, balões, IME, ações, percepção e memória permanecem ativos se voz/rede falharem.

- [Instalação, vozes, custos, configurações e privacidade](docs/ELEVENLABS_2_4_0.md)
- [Arquitetura e limites de identidade](docs/ARCHITECTURE_2_4_0.md)
- [Roteiro Android ainda pendente](docs/ANDROID_ELEVENLABS_TESTS.md)
- [Changelog](CHANGELOG.md)
- [Validações da entrega](docs/validation/ELEVENLABS_2_4_0.md)

Instale o ZIP ARM64 da Release e preserve seu INI privado: o pacote traz apenas um exemplo sem chaves. Importe a chave ElevenLabs em **Voz e personalidade**, consulte vozes reais da sua conta e teste conscientemente: geração não encontrada no cache pode consumir créditos. Nenhuma requisição paga é feita apenas por abrir o menu. Fontes correspondentes são distribuídos na mesma Release. Kokoro requer o pacote de pesos opcional; ElevenLabs não exige pesos, Termux, Python ou outro app em execução.

Build: NDK r27d, `./tools/build_keyboard.sh`, `./tools/test.sh`, `./tools/build_tts.sh` (se recompilar dependências Kokoro), `./build.sh`. Só o `.so` principal é exigido para ElevenLabs; o instalador também inclui bibliotecas Kokoro opcionais. Testes host usam mocks e não confirmam API/aparelho real.

Documentos de versões anteriores abaixo são históricos; a documentação atual está nos links acima.

---

# NPC AI 2.3.0 — Kokoro-82M offline

Atualização do projeto existente para GTA San Andreas Android 2.10 ARM64, AML >=1.4 e Android >=8/API26. Piper foi substituído por Kokoro FP32 + Sherpa-ONNX/eSpeak NG em CPU, com três speakers brasileiros, sessão reutilizada, áudio estéreo aproximado e sincronização pelo AudioTrack. Gemini/Mock, teclado, balões, ações, memória e percepção foram preservados.

- [Instalação, configuração, arquitetura de voz e limites](docs/KOKORO_2_3_0.md)
- [Testes no aparelho ainda necessários](docs/ANDROID_KOKORO_TESTS.md)
- [Licenças e fontes correspondentes](docs/KOKORO_LICENSES.md)
- [Validações reais desta entrega](docs/validation/KOKORO_2_3_0.md)

Use o instalador e o pacote Kokoro FP32 separado; o ZIP completo contém fontes para desenvolvimento. Leia as diferenças entre testes host e validação GTA. Não há Python em execução, Termux, servidor ou aplicativo auxiliar exigido no telefone. Outros documentos antigos abaixo são históricos e não descrevem o motor atual.

---

# NPC AI 2.2.0 — GTA SA Android 2.10 ARM64 / AML

A arquitetura principal agora é **GTA → libnpcai.so → JNI/HttpsURLConnection → Gemini**. O DEX Java vai embutido no plugin. **Não é necessário abrir Termux, executar Python nem instalar aplicativo auxiliar.** O modo Mock também é nativo e gratuito.

**[Instalação, configuração e roteiro Android](docs/TTS_2_2_0.md)** · **[Arquitetura](docs/ARCHITECTURE_2_0_0.md)** · **[Privacidade e identidade](docs/PRIVACY_2_0_0.md)** · **[Resultados reais](docs/VALIDATION_2_2_0.md)**

Esta entrega contém implementação, testes de host/JVM, Java/DEX e ELF ARM64 compilados. **Não foi executada no GTA real por este ambiente.** HTTPS/JNI/Keystore, permissões efetivas do APK e semântica dos novos hooks de save/percepção precisam de validação no aparelho. Não considere teste JVM como aprovação Android.

A 2.0.2 aceita credenciais com ponto no C++ e no Java e aspas externas opcionais. Mantém a correção da 2.0.1 no parser INI e impede que falhas da configuração/Gemini interrompam a inicialização local. O botão fixo **NPC AI · Diagnóstico** permite consultar caminho, erros, HTTPS e credencial, testar Gemini em worker e exportar relatório sem ADB. Valores inválidos são recuperados por parâmetro; não há fallback silencioso de Gemini para Mock.

A 2.1.0 adiciona Piper TTS offline: Faber pt-BR, modelo/sessão reutilizados, worker, AudioTrack PCM, cancelamento e sincronização de gestos. Instale as dependências Android e o pacote separado da voz conforme o guia. Modelo ausente não desliga chat, balões ou Gemini. **[Licenças GPL/voz](docs/TTS_LICENSES.md)**.

A 2.2.0 adiciona vozes por classe: Jeff/Ballas, Cadu/Grove, Faber/policiais e Edresson/taxistas observados. Os perfis são configuráveis, com prévia no diagnóstico, cache PCM isolado por modelo, fallback explícito e uma sessão residente por padrão (duas opcionais). A base 2.1.0 foi confirmada pelo usuário no aparelho falando respostas Gemini; a nova seleção multiclasses ainda precisa dessa validação. Edresson é low e seu pacote separado tem atribuições/restrições da cadeia RyanSpeech, conforme o guia.

## Começar sem servidor

1. Feche GTA e substitua uma única cópia de `mods/libnpcai.so` pela biblioteca em `libs/arm64-v8a/`.
2. Atualize o `npcai.ini` efetivamente lido pelo AML (caminho mostrado no log), preservando seus ajustes. O exemplo está em `config/npcai.ini`.
3. Para teste gratuito, use `gemini_enabled=0`: o Mock roda dentro do plugin.
4. Para Gemini, configure explicitamente uma das opções descritas no guia. Exemplo de desenvolvimento em texto puro:

```ini
npc_ai_transport=native
gemini_enabled=1
gemini_model=gemini-2.5-flash-lite
gemini_key_storage=ini
gemini_key_action=none
gemini_api_key=SUA_CHAVE_PRIVADA
```

O modelo deve existir na sua conta; não é garantida a disponibilidade de nomes novos. **INI em texto puro não protege a chave de apps/mods capazes de ler o arquivo.** Android Keystore é a modalidade recomendada; o guia explica importação, substituição e remoção.

## Principais mudanças

- Percepção periódica com proveniência: relógio, chuva, CJ, entidades próximas, linha de visão aproximada e eventos do SDK; entrada/streaming não é tratada como criação comprovada.
- Expectativas tipadas, prazos, contagens distintas, raio, confiança e exigência de visibilidade; nenhuma expressão ou função arbitrária do modelo é executada.
- Perplexidade e emoções calculadas localmente, com habituação, limites e recuperação gradual.
- Memória JSON privada, transacional por rename/fsync, checksum, backup, esquema e autosave de 10 s somente quando há alterações. Separação por slot confirmado ou perfil manual.
- **NPCs aleatórios continuam com identidade de sessão.** Memória individual antiga não é atribuída a outro pedestre do mesmo modelo. Continuidade entre sessões é global/narrativa por save quando não há identificação individual estável.
- Contexto compacto, histórico natural, Gemini com JSON estruturado, retries limitados, TLS padrão verificado e métricas sem chave/diálogo nos logs.
- Animações, rendição/seguimento persistentes, dinheiro coletável, teclado Android e alternativa touch da 1.4 foram mantidos.

## Desenvolvimento

```sh
./tools/build_keyboard.sh
./tools/test.sh
ANDROID_NDK_HOME=/caminho/android-ndk-r27d ./build.sh
python3 tools/verify_elf.py libs/arm64-v8a/libnpcai.so
python3 tools/package.py
```

NDK r27d: `27.3.13750724`. O workflow instala dependências, gera DEX, compila, testa, valida o ELF e publica biblioteca/ZIP. Java/ECJ/D8 e Python são ferramentas de desenvolvimento/teste, **não dependências de uso no Android**.

Os arquivos `server/server.py` e `server/actions.py` permanecem para testes e transporte legado opcional (`npc_ai_transport=legacy_http`). OpenAI continua disponível nesse caminho legado; não foi adicionado cliente OpenAI nativo nesta entrega. Os documentos 1.x em `docs/` são históricos.
