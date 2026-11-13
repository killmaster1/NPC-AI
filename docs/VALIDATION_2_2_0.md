# Validação NPC AI 2.2.0 — classes de voz

## Resultado real

- Plugin e adaptador Piper compilados com NDK r27d 27.3.13750724, C++17/API26, ARM64. ONNX Android 1.22.0 oficial foi preservado.
- Três ELF AArch64/DYN verificados, LOAD alinhado a 16 KiB, exports AML/adaptador e dependências padrão Android. Nenhuma dependência libc++_shared.so foi acrescentada.
- DEX do editor/HTTPS/AudioTrack preservado byte a byte da 2.1.0 confirmada pelo usuário. Não houve alteração em Activity/IME ou transporte Java.
- Manifesto dos 368 arquivos de dependências anteriores conferido.
- **233 testes/cenários host e JVM aprovados:** 17 novos de classes + 20 de TTS + 23 configuração + 22 inteligência + 13 núcleo + 22 animações + 18 ações + 12 UI + 86 unittest. Os testes JVM já estão incluídos nos 86.
- Suíte completa executada com exit 0. Com o perfil por identidade incorporado, a suíte completa foi repetida e os 17 testes de classe foram também recompilados/repetidos e o plugin recompilado, ambos aprovados.
- Há avisos de headers/qualificadores do SDK, sem erros finais. O workflow foi atualizado, mas não foi executado remotamente no GitHub nesta sessão.

## O que os novos testes demonstram

Classificação de Ballas/Grove/COP, taxista por evidência, outros grupos e defaults; 261/262 não viram taxista por aparência; configuração/validação e herança; seleção de modelos por job; prévia manual; cache PCM isolado por modelo; fallback com aviso e opção de desativá-lo; falha numa voz sem desligar as demais; cancelamento ao trocar NPC/classe; perfil estável por identidade e liberação ao invalidar entidades; evidência de motorista retida só na mesma identidade geracional; nenhuma inferência na thread do chamador.

Os cenários de engine usam dados simulados. A leitura real de CModelInfo/CPools/CVehicle no Android não foi exercitada neste ambiente.

## Síntese real de quatro modelos no host

O backend C++ de produção foi exercitado com Piper/ONNX host, modelos reais e SHA-256 real via OpenSSL no lugar da função Java. Foi testada uma sequência de oito seleções/falas, incluindo retorno a sessões em cache, eviction e continuidade do eSpeak ao liberar outra sessão. Depois o mesmo teste refez trocas com apenas uma sessão residente.

Todos os modelos produziram PCM mono finito/não silencioso: Faber/Cadu/Jeff a 22050 Hz; Edresson a 16000 Hz. Limites de até duas sessões e 128 MiB de arquivos de pesos somados foram conferidos. Esse limite não representa RSS total, tensores ou memória do GTA.

O teste Faber anterior com cancelamento/reutilização também passou, assim como modelo ausente, SHA-256 inválido e consumo do último chunk DONE com áudio. As amostras comparáveis contêm somente uma frase pública de teste. Não houve avaliação perceptual humana da naturalidade; não se afirma que todos os timbres soem melhores que Faber.

Bibliotecas Linux e executáveis desses testes ficam em build/ ou fora do projeto. Não estão nas pastas instaláveis Android. Os resultados/tempos do host não são medições de desempenho Android.

## Verificado e ainda pendente no aparelho

O usuário confirmou a base 2.1.0 no GTA real, falando respostas Gemini. **A atualização 2.2.0 ainda não foi executada em aparelho por este ambiente.**

Pendentes: carregar o novo plugin/adaptador pareados; roteamento de classes nos NPCs reais; identificação de motorista de taxi/cabbie via símbolo/pool; timbre/qualidade/pronúncia; PCM 16 kHz no AudioTrack; alternância de modelos e consumo de RAM/FPS no celular; regressões de balões, IME, Gemini, memória, percepção, gestos, rendição e ações físicas. Nenhuma chamada Gemini autenticada foi necessária aos testes de voz.

Padrão conservador: uma sessão residente; duas são opt-in. Se uma voz estiver ausente, o fallback padrão é explícito no diagnóstico. A voz low opcional Edresson e a cadeia RyanSpeech estão documentadas com licenças/cards, sem promessa de uso comercial irrestrito.

## Alterações principais

voice_profiles.h; Settings/INI; VoiceManager e cache PCM; PiperBackend e LRU; lease eSpeak no adaptador; GameBridge/Registry/Context e evidência de motorista; prévia ImGui por classe; identificação/prompt nativo e validação/contexto do servidor legado. Animações/tarefas físicas anteriores foram preservadas.

Modelo/JSON/cards dos novos pacotes são fixados por revisão/SHA-256, com dados eSpeak compartilhados comprovadamente iguais aos do pacote 2.1.0. Não é necessário baixar Faber ou eSpeak de novo para atualizar a instalação já funcional.

## Evidências

Logs completos, ELF, hashes/tamanhos, lista de alterações e fonte correspondente estão em validation/2.2.0/. `multivoice-real-host.txt` contém as oito seleções reais; `class-tests-final.txt` contém a repetição do grupo novo após a validação final dos vínculos por identidade. `tts/android-adaptations.patch`, `tts/dependencies.json` e `tts/class-voices.json` registram adaptações, commits e arquivos de modelo. Fontes e licenças acompanham os binários, sem credenciais ou conversas privadas.
