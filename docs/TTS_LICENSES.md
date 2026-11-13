# Licenças da integração Piper / Faber

O código novo do NPC AI e do adaptador é distribuído sob **GPL-3.0-or-later**. O projeto já traz o texto GPL em LICENSE. A integração é tratada como conjunto GPL, incluindo o plugin principal, e não como forma de evitar copyleft usando `dlopen`.

## Componentes

| Componente | Origem / licença |
|---|---|
| Piper libpiper | OHF-Voice/piper1-gpl, GPL-3.0-or-later; revisão fixada em tts/dependencies.json |
| eSpeak NG / dados compilados | espeak-ng/espeak-ng, GPL-3.0-or-later e avisos de componentes internos, preservados em third_party/tts/espeak-src |
| ONNX Runtime Android 1.22.0 | Microsoft, MIT; LICENSE e ThirdPartyNotices.txt incluídos |
| Faber, dataset próprio | MODEL_CARD informa CC0, https://github.com/OHF-Voice/voice-datasets |
| Base hifi_tts_92 | Hi-Fi Multi-Speaker English TTS Dataset, NVIDIA Corporation, CC BY 4.0; voz Cori Samuel, reader 92 |

Atribuição da base: Bakhturina, Lavrukhin, Ginsburg e Zhang, **Hi-Fi Multi-Speaker English TTS Dataset**, Interspeech 2021, https://arxiv.org/abs/2104.01497, https://www.openslr.org/109/. A base foi treinada a partir das gravações de Cori Samuel. O pacote retém o MODEL_CARD da Faber, BASE_MODEL_CARD e o texto CC BY 4.0. A integração faz síntese e conversão PCM; não altera pesos do modelo. Não implica endosso dos autores/locutora.

A licença declarada no MODEL_CARD da Faber é a do dataset próprio, não uma nova licença concedida pelo motor sobre todos os pesos. A cadeia da cópia obtida é Faber → hifi_tts_92, conforme cards preservados; não se deve rotular toda a cadeia como CC0. Os esclarecimentos oficiais do Piper estão copiados em tts/upstream/LICENSING.md e COMMERCIAL_USE.md. A revisão e hashes distinguem esta cópia de versões anteriores com outro treinamento.

## Ao redistribuir

Distribua os avisos e textos de licença e o **Corresponding Source** da versão modificada: fontes Piper/eSpeak/NPC AI, adaptador, patch e scripts de compilação estão no arquivo **NPC_AI_2.2.0_ARM64.zip**, entregue junto do instalador. O arquivo também inclui o tarball upstream integral ONNX Runtime v1.22.0 em third_party/tts/ort/source, com scripts de build originais; dependências externas de build são referenciadas pelos scripts/hash upstream. Por isso o ZIP de fontes é grande e não é necessário baixá-lo para jogar. Mantenha essa fonte acessível aos destinatários; não redistribua apenas o instalador fechando ou restringindo a fonte GPL. Preserve a atribuição CC BY da base e os MODEL_CARDs ao compartilhar o pacote de voz.

O instalador contém referência ao arquivo de fontes que o acompanha. Ao oferecer os binários para download, ofereça a fonte correspondente na mesma distribuição/local. Não é necessário publicar sua chave Gemini, INI privado, saves ou memórias. Nenhum arquivo do GTA/APK é distribuído. A licença do plugin não relicencia o jogo; permissões sobre uso/distribuição do GTA permanecem separadas.

Não há garantia do software nem promessa de direitos de voz além do que seus autores/datasets concedem. Para uma distribuição comercial ou diferente desta, confira as condições das dependências e da cadeia de treinamento, em vez de presumir que a licença GPL do motor autoriza qualquer modelo.
