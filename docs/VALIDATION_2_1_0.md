# Validação NPC AI 2.1.0 — Piper offline

## Resultados reais

| Verificação | Resultado |
|---|---|
| libnpcai.so | NDK r27d 27.3.13750724, C++17/API26, arm64-v8a, build concluído |
| libnpcai_piper.so | Compilado para Android arm64-v8a/API26, r27d; Piper/eSpeak estáticos, ORT via API C explícita |
| libnpcai_ort.so | ELF do AAR oficial onnxruntime-android 1.22.0; minSdk24, nota Android indica NDK r28 do fornecedor; não foi recompilado aqui |
| ELF dos três | AArch64 ELF64 DYN, LOAD 16 KiB; somente libc/libm/libdl/liblog do Android, conforme cada arquivo |
| C++ runtime | Não há dependência de libc++_shared.so nem de runtime C++ do jogo |
| Java/DEX | NativeAudio, editor e HTTPS combinados no DEX; ECJ/D8 API26 concluídos |
| Dependências existentes | Manifesto dos 368 arquivos anteriores conferido |
| Fontes/voz | Commits e hashes fixados, MODEL_CARD e base/atribuições preservados |

O build C++ continua emitindo avisos de headers do SDK. A compilação Java registra avisos de depreciação de APIs legadas de áudio/URL no JDK atual, sem erro. Não houve execução remota do workflow no GitHub; ele agora compila Piper antes do plugin para embutir os hashes corretos das bibliotecas que distribuirá.

## Testes automatizados

**216 cenários/testes de host/JVM aprovados**, suíte final completa com exit 0:

| Grupo | Quantidade |
|---|---:|
| Voz: worker, cache, timeout, fila, identidade, pausa, distância, falha e staging privado | 20 |
| Configuração/diagnóstico | 23 |
| Inteligência/memória/percepção/expectativas | 22 |
| Núcleo nativo/seleção/HTTP worker | 13 |
| Animações persistentes e novos testes de áudio/prioridade | 22 |
| Ações/chat | 18 |
| UI/touch/IME | 12 |
| unittest Python/HTTP/JVM | 86 |

Os testes JVM já estão incluídos nos 86. O novo teste Java valida SHA-256 real, arquivo ausente, parâmetros PCM e track fechado; **não reproduz áudio Android no JVM**. Os testes de eSpeak/ONNX reais abaixo são programas adicionais e não foram somados artificialmente aos 216.

## Síntese real no host

Foram compilados libpiper/eSpeak para o host Linux e usados o ONNX Runtime **host** e o modelo real Faber. Essas bibliotecas Linux ficam em build/ ou fora do projeto e **não entram nos ZIPs Android**.

Texto fixo do teste de voz e texto com “mano”, “tá ligado”, pontuação e acentos produziram PCM mono 22050 Hz finito/não silencioso. A sessão foi reutilizada para várias falas, cancelada via RunOptions e reutilizada depois do cancelamento. O teste confirmou os cinco clusters de vogais e ausência de corrupção do mapa da vogal simples.

Registro desta execução: `HOST REAL Piper/Faber: model_load=0.408s chunks=5 audio=9.451s total=1.542s PCM finite/nonzero, UTF-8, accents, cancel/reuse OK. Android not tested.`

Um segundo executável usa o **backend C++ de produção** com hashes SHA-256 reais OpenSSL no lugar da função Java e bibliotecas host: testou modelo ausente, manifesto/hash incorreto, modelo verificado, reutilização e consumo do último chunk DONE com áudio. Resultado: `REAL HOST backend: missing voice, SHA256 mismatch, verified model/session reuse and final DONE audio chunk passed. samples=43520 rate=22050; Android playback pending`

Os tempos medidos são deste host e não estimativas de latência/FPS de celular. A qualidade/pronúncia perceptual não foi avaliada por uma pessoa nesta execução. A amostra WAV contém apenas as frases públicas de teste, sem histórico de jogador.

## Auditoria e adaptações

A API C é oficial do OHF-Voice/piper1-gpl; revisão base e backport de vowel_clusters estão em tts/dependencies.json. O adaptador próprio expõe C ABI para cancelamento/threads/ORT manual. Foi corrigido o consumo da chamada final DONE que ainda contém amostras. Verifica-se saída mono float32 com dimensões dinâmicas permitidas na metadata e validadas na inferência; Faber produz saída de rank 4 com eixos unitários antes das amostras. Modelos fp16/multicanal/split não são prometidos.

O espeak-ng é cross-compilado para Android; dados são gerados pela mesma revisão no host. Os headers compat C de eSpeak não vazam para o compilador C++/libc++ do Piper. A biblioteca ONNX é obtida do AAR Android, não do tarball Linux selecionado pelo CMake upstream.

O loader AML oficial foi auditado: copia o mod para GetDataPath, chama dlopen e remove a cópia. A integração não depende de dladdr apontar para o arquivo original nem de subpastas carregarem automaticamente. Staging próprio de dois arquivos usa fontes limitadas, hashes compilados, cópia privada atômica/readonly e reutilização de cópias idênticas. A lógica portátil passou testes com funções de hash injetadas; o SHA-256 real é testado separadamente no JVM e no backend real host. **Isso não valida a política SELinux/namespace do APK em aparelho.**

## Pendências reais no Android

- Carregar o plugin, suas duas bibliotecas copiadas para a pasta privada e a API C ONNX dentro do processo GTA SA 2.10 ARM64.
- Reproduzir PCM pelo AudioTrack do aparelho, volume/mistura com GTA, pequeno áudio já bufferizado no cancelamento e consumo de bateria.
- Confirmar a animação visível acompanhando início/fim real de áudio, sem substituir rendição, missão, combate, fuga e seguimento.
- Validar IME, balões, cancelamento ao trocar/fechar/limpar conversa, NPC inválido/morto, pausa/retorno Android e encerramento durante carga/inferência.
- Medir FPS, RSS e latência em aparelhos com diferentes RAM/CPUs. Os buffers/fila/cache e tamanho de modelo têm limites; não há teto rígido para a RAM interna do ONNX ou do GTA.

**Nenhuma execução desta atualização no GTA real, nenhum áudio AudioTrack real e nenhuma chamada Gemini autenticada foram feitos neste ambiente.** Gemini não é necessário ao teste de voz. Falhas de voz preservam o texto; o roteiro completo está em TTS_2_1_0.md.

## Arquivos principais

- Novos tts.h/cpp, piper_backend.cpp, tts_staging.h, android_audio.h/cpp e NativeAudio.java.
- GameBridge, main, NativeKeyboard, NpcActionController, UI, Settings e INI integrados ao novo controlador, preservando os módulos de Gemini/memória/mundo.
- Fontes e licenças em third_party/tts, incluindo o tarball upstream ONNX v1.22.0 integral (não é instalado no Android); CMake/adaptador/patch/hashes em tts; scripts de build, download fixado, testes e três formas de pacote em tools.
- Testes novos em tests/tts, tests/test_native_audio.py, NativeAudioJvmTest.java; testes de prioridade/sincronização adicionados ao controlador existente.

Hashes e tamanhos dos binários: [binaries.json](validation/2.1.0/binaries.json). Logs, ELF, toolchain, lista exata de alterações e hashes das fontes estão em [validation/2.1.0/](validation/2.1.0/). Nenhuma credencial foi incorporada.
