# NPC AI 2.3.0 — Kokoro offline

Atualização do projeto existente para GTA San Andreas Android 2.10 ARM64, AML >=1.4, Android >=8/API26. Gemini e Mock continuam gerando texto; a síntese é totalmente local e opcional. O APK precisa de INTERNET apenas para Gemini. Não requer Termux, Python no telefone, root, outro APK ou Android TextToSpeech.

## Instalação manual

1. Feche o GTA. Guarde uma cópia de `mods/libnpcai.so` e do seu INI.
2. Extraia **NPC_AI_2.3.0_Install_ARM64.zip** na pasta do jogo que contém `mods` e `files`. Ela costuma ser `Android/data/com.rockstargames.gtasa/` ou o caminho `Android_unprotected/data/com.rockstargames.gtasa/` informado pelo AML. Use o mesmo diretório real da instalação anterior; o plugin não procura em todo o armazenamento.
3. Instale também o pacote **NPC_AI_Kokoro_PTBR_FP32.zip**, extraindo na mesma pasta. O resultado precisa conter:

```
mods/libnpcai.so
mods/npcai-tts/lib/libnpcai_kokoro.so
mods/npcai-tts/lib/libnpcai_kokoro_ort.so
files/npcai-tts/models/kokoro-pt-br/model.onnx
files/npcai-tts/models/kokoro-pt-br/voices.bin
files/npcai-tts/models/kokoro-pt-br/tokens.txt
files/npcai-tts/models/kokoro-pt-br/manifest.json
files/npcai-tts/models/kokoro-pt-br/espeak-ng-data/...
```

O instalador fornece `files/npcai.ini.example`: **não substitui seu INI nem sua chave**. Mescle as opções TTS do exemplo em `files/npcai.ini`, uma ocorrência por parâmetro. Não misture vozes Piper com o novo modelo. O plugin migra `npc_tts_engine=piper` e nomes `pt_BR-...` para Kokoro com aviso; as opções antigas de modelos por classe são ignoradas com aviso. Valores TTS inválidos ou duplicados conflitantes desativam apenas a voz e aparecem no diagnóstico. Não há fallback para serviço de voz online.

Depois da atualização você pode remover **somente** os arquivos antigos `mods/npcai-tts/lib/libnpcai_piper.so`, `libnpcai_ort.so`, modelos Piper e dados antigos em `files/npcai-tts/espeak-ng-data/`. O plugin não os carrega nem os apaga automaticamente. Não remova bibliotecas de outros mods.

## Uso

Abra o GTA, aproxime CJ de um NPC e converse normalmente. Em **NPC AI → Diagnóstico → Voz dos NPCs**, habilite Kokoro e use **Testar voz**, mesmo sem NPC selecionado e sem Gemini. O teste usa exatamente:

> Qual foi, parceiro! Agora eu consigo falar de verdade. Que loucura é essa, mano?

A primeira fala inicializa e verifica os arquivos na worker. As seguintes reutilizam a sessão. O status mostra speaker, modelo, fonemização, latência, duração PCM e estimativa mínima de memória. O texto e o botão Conversar continuam disponíveis se faltarem arquivos, memória ou bibliotecas.

Speakers registrados **apenas para este pacote**: `pm_alex=43` (padrão), `pm_santa=44`, `pf_dora=42`. `npc_tts_voice_variation=1` distribui essas três vozes por hash da identidade de sessão, sem inferir gênero ou outras características pelo modelo do pedestre. A voz fica estável enquanto aquela identidade estiver viva. Não recupera voz/identidade de um pedestre aleatório de outra sessão. Para todos usarem a voz escolhida, desmarque **Variar a voz por NPC** no menu ou configure `npc_tts_voice_variation=0`. A troca manual nesse modo aplica a voz escolhida às próximas falas, inclusive do NPC atual. O teste sempre usa a seleção do menu. Os ajustes do menu valem só na sessão; o INI persiste preferências.

## Áudio e desempenho

- Kokoro FP32 + Sherpa-ONNX 1.12.23 + eSpeak NG, idioma `pt`, CPU; uma sessão para todos os speakers. Arquivos e bibliotecas são verificados por SHA-256. FP16/INT8 não são aceitos.
- Uma worker neural, 1..4 threads ONNX (padrão 2), uma fala ativa e uma pendente substituível, textos até 1.200 bytes UTF-8, segmentos de aproximadamente 180 bytes, cache até 4 MiB/4 entradas e áudio total até 40 s. Não há inferência no tick do GTA.
- PCM mono 24 kHz convertido em frames estéreo para AudioTrack; progresso medido em frames, não em amostras intercaladas. Atenuação suave pela distância até CJ e pan pela projeção de câmera já usada pelos balões. Se o NPC não puder ser projetado, pan central. É aproximação estéreo, sem HRTF, reverberação, oclusão acústica ou integração no mixer do GTA.
- O NPC deve estar válido, na mesma identidade/conversa e no alcance. Troca de NPC, fechamento do chat, pausa e invalidação cancelam a fala. Resposta antiga não toca. Não existe sobreposição de vozes.
- Animações recebem início/fim real do playback e heartbeat; continuam respeitando combate, missão, fuga, acompanhamento e rendição. RMS é apenas amplitude aproximada por segmento, **não lip-sync fonético**.
- Cancelamento e timeout são **cooperativos entre blocos/frases**. A API C pública não oferece interrupção de um ONNX Run em andamento. A síntese pode terminar internamente antes de perceber o cancelamento, mas seu resultado antigo não é reproduzido. Shutdown espera essa chamada retornar; não mata a thread nem libera a sessão durante inferência.
- O pacote ocupa cerca de 354 MiB de arquivos necessários. O teste real Linux atingiu aproximadamente 753 MiB de RSS; o processo GTA acrescenta seu próprio consumo. O INI permite uma checagem preventiva `npc_tts_min_available_mb=900` via MemAvailable antes de abrir o modelo; não é um teto rígido de RSS. Falhas alocáveis são tratadas, mas o Android ainda pode encerrar um processo sob pressão extrema. Reduza concorrência ou desative TTS em aparelhos com pouca RAM.
- `npc_tts_idle_unload_seconds=60` descarrega a sessão após 60 s ociosa; 0 mantém carregada. Reabrir requer nova inicialização. Não carregamos três cópias para três vozes.

## Carregamento AML e build

O AML auditado copia plugins individualmente para a pasta privada e não prepara recursivamente suas dependências. Por isso o mod copia apenas as duas bibliotecas conhecidas, com SHA-256 esperado, para `GetDataPath()/npcai-tts-lib/`, usando arquivo temporário, fsync e rename; depois faz preload com caminho absoluto/RTLD_LOCAL. Nomes SONAME próprios reduzem colisões: `libnpcai_kokoro_ort.so` e `libnpcai_kokoro.so`. A alternativa restrita é `GetInternalModsPath()/npcai-tts/lib`, sem buscas arbitrárias.

O Sherpa pronto para Android 1.12.23 traz ORT que exige API27; **não foi usado no instalador**. O Sherpa desta versão foi compilado das fontes com NDK **r27d**, API26 e libc++ estática, ligado ao ORT Android oficial **1.22.0/API24**. O script verifica ELF AArch64 real, notas Android, fechamento de dependências e LOAD de 16 KiB. Não há binário Linux no instalador. Somente API C cruza a fronteira dos módulos.

No computador Linux, instale CMake/Ninja, compilador C++17, Python3 e Java, e execute:

```
python3 -m pip install patchelf
export ANDROID_NDK_HOME=/caminho/android-ndk-r27d
./tools/build_keyboard.sh
./tools/test.sh
./tools/build_tts.sh
./build.sh
python3 tools/verify_tts_elf.py runtime/arm64-v8a/*.so
python3 tools/package_install.py
python3 tools/package.py
```

Fontes Sherpa e eSpeak exatas são incluídas; outras dependências são baixadas pelos CMake oficiais com SHA fixado. ORT Android, headers e licenças estão em `third_party/onnxruntime/`. O modelo é distribuído separadamente e não é necessário compilar para instalá-lo.

## Desinstalação

Feche o GTA, remova `mods/libnpcai.so`, as duas bibliotecas próprias e o diretório `files/npcai-tts/models/kokoro-pt-br`. Remova opcionalmente o INI e memórias próprias conforme a documentação de privacidade. Não remova o AML, bibliotecas do jogo ou arquivos de outros mods. Para voltar à versão anterior, restaure o plugin, bibliotecas, modelos e INI do mesmo pacote anterior.

## Diagnóstico

O menu mostra erros sem chave Gemini nem conteúdo privado. Falha de voz não vira fala do personagem. Se tiver ADB futuramente: `adb logcat | grep -iE 'npcai|kokoro|sherpa|onnx|AndroidRuntime'`. Veja `ANDROID_KOKORO_TESTS.md` para a validação que continua pendente em GTA real.
