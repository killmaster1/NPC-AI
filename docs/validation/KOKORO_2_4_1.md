# Validação 2.4.1 — 14/11/2026

- Regressões completas aprovadas: 90 testes Python/JVM, 29 cenários ElevenLabs, 15 perfis Kokoro, 22 worker, configuração/inteligência/ações/animações/UI existentes. Nenhuma chamada paga.
- Testes novos: 12 cenários determinísticos de segmentação/prosódia, UTF-8, decimal, abreviaturas, reticências, expansão restrita e limites de velocidade. Parser da nova opção incluído nas regressões Kokoro.
- Síntese neural real Linux com os pesos FP32 já distribuídos, speakers 42/43/44, 24 kHz: áudio finito e não silencioso; arquivos ausentes/corrompidos rejeitados; cancelamento cooperativo e descarregamento/recarga verificados.
- Build Android NDK r27d/27.3.13750724 e ELF ARM64/API26, exportações AML e alinhamento 16 KiB verificados.
- Não houve teste no Mi 11 Lite, avaliação perceptiva controlada, benchmark Android/FPS nem uso de APIs pagas. RMS/amostras válidas não comprovam naturalidade da voz. A mudança mantém o modelo e não promete expressividade de outro motor.

Logs desta entrega acompanham os fontes em docs/validation/. As regressões host devem terminar com exit 0 antes de empacotar/publicar.
