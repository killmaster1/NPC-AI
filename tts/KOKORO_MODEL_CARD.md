---
license: apache-2.0
tags:
- tts
- kokoro
- portuguese
- pt-br
- letrinhas
---
# kokoro-pt-br

Bundle ZIP do **Kokoro-82M (multi-lang v1_0, fp32)** pronto para o app Letrinhas
(TTS neural on-device pt-BR via sherpa-onnx).

## Conteúdo do `letrinhas-kokoro-pt.zip` (~329MB)

- `model.onnx` — Kokoro-82M fp32 (349MB descompactado)
- `voices.bin` — 53 speakers (pt-BR: pf_dora=42, pm_alex=43, pm_santa=44)
- `tokens.txt` — mapa de fonemas
- `espeak-ng-data/` — dados de fonemização espeak-ng (lang=pt)

## Por que fp32?

Os exports int8 (oficial sherpa-onnx e quantização dinâmica) e fp16 produzem
**NaN no gráfico** (verificado em sherpa-onnx Python e iOS). fp32 é o único
caminho que gera áudio válido em pt-BR hoje.

## Uso (sherpa-onnx Swift)

```swift
let kokoro = sherpaOnnxOfflineTtsKokoroModelConfig(
    model: "<dir>/model.onnx", voices: "<dir>/voices.bin",
    tokens: "<dir>/tokens.txt", dataDir: "<dir>/espeak-ng-data", lang: "pt")
// sid 42 = pf_dora (voz feminina pt-BR)
```

Fontes: hexgrad/Kokoro-82M (Apache-2.0) + k2-fsa/sherpa-onnx (Apache-2.0).
