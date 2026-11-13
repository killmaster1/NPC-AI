# Validações NPC AI 2.3.0 — 13/11/2026

## Executado no computador

- Build real do módulo Sherpa/Kokoro a partir das fontes v1.12.23, commit `7e227a529be6c383134a358c5744d0eb1cb5ae1f`, NDK r27d/27.3.13750724, arm64-v8a, API26, libc++ estática. Motor CPU; API C oficial. ONNX Android 1.22.0/API24, com headers correspondentes.
- Build real de `libs/arm64-v8a/libnpcai.so` com NDK r27d e geração do DEX Java por ECJ/D8/API26. Há warnings preexistentes do SDK e de depreciação Java; build encerrou com sucesso, sem erros de link.
- ELF dos três binários: AArch64, notas Android, dependências resolvíveis, ausência de libc++ compartilhada, LOAD >=16 KiB, SONAMEs TTS isolados. O plugin tem somente dependências nativas Android padrão; voz é carregada opcionalmente.
- O runtime pronto do Sherpa trazia ORT/API27: foi rejeitado pela verificação e substituído pelo build descrito acima. Não se alterou a nota API para mascarar incompatibilidade.
- Testes determinísticos C++: perfis/configuração/estéreo/cache/idle unload (15 cenários), worker de voz (21, incluindo cancelamento de reprodução antes de inferência cooperativa retornar), configuração/diagnóstico (23), inteligência nativa (22), núcleo (13), animações (22), ações (18), UI (12 verificações nos cenários existentes). São 146 verificações/cenários C++, mais 86 testes Python/JVM aprovados. O teste UI foi recompilado após a última mudança da interface de áudio.
- Teste real de síntese com **a implementação KokoroTtsEngine**, Sherpa Linux v1.12.23 e pacote FP32 brasileiro verificado: arquivos ausentes/corrompidos e modelo não registrado rejeitados; speakers 42/43/44 produziram áudio finito, RMS não nulo, 24 kHz; cancelamento no callback, unload e reload aprovados. Bibliotecas Linux usadas somente nesse teste, nunca copiadas ao instalador Android. Esse teste usa o runtime host da distribuição Sherpa, não executa o ELF Android/ORT1.22.

Resultados observados do teste real Linux (máquina compartilhada com compilações):

| Speaker | Amostras | Áudio | Síntese | RMS |
|---|---:|---:|---:|---:|
| pf_dora (42) | 122400 | 5,10 s | 3803 ms | 0,0549 |
| pm_alex (43) | 124800 | 5,20 s | 6621 ms | 0,0588 |
| pm_santa (44) | 126600 | 5,275 s | 6926 ms | 0,0581 |

Inicialização observada: 2387 ms; pico de RSS do processo de teste: 771160 KiB (~753 MiB). Não mede RAM do GTA ou desempenho Android. Esses tempos não são promessa de FPS nem de latência no celular. Prévias WAV são amostras reais desse teste, sem avaliação humana de naturalidade.

## Ainda não validado no aparelho

Carregamento e namespace privado pelo AML, AudioTrack estéreo real, qualidade percebida e pronúncia, latência/FPS/PSS dentro do GTA, comportamento do teclado/pausa/retomada sob síntese, animações sincronizadas na engine, uso em Android 8/API26 real e interoperabilidade com outros mods. Também não se usou uma chave Gemini privada nesta entrega. Os testes HTTP/JVM são controlados.

Leia `docs/ANDROID_KOKORO_TESTS.md`. Aprovação de testes host e compilação não equivalem a aprovação no GTA.
