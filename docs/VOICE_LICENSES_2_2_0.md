# Licenças das vozes por classe — 2.2.0

O código novo e o adaptador Piper permanecem GPL-3.0-or-later. Fontes correspondentes, patch, commits e scripts acompanham o arquivo de fontes, com ONNX Runtime MIT e seus avisos. As vozes são dados/modelos separados: o motor não concede licença própria sobre elas. Mantenha MODEL_CARD, BASE_MODEL_CARD, textos de licença e atribuições ao redistribuir cada pacote.

| Modelo | Dataset próprio conforme card | Base conforme card |
|---|---|---|
| Faber (já instalado) | OHF-Voice/voice-datasets, CC0 | hifi_tts_92, Hi-Fi TTS/NVIDIA/Cori Samuel, CC BY 4.0 |
| Cadu | OHF-Voice/voice-datasets, CC0 | rdh, https://github.com/r-dh/dutch-vl-tts, CC0 |
| Jeff | OHF-Voice/voice-datasets, CC0 | rdh, https://github.com/r-dh/dutch-vl-tts, CC0 |
| Edresson low | https://github.com/Edresson/TTS-Portuguese-Corpus, CC BY 4.0 | Ryan low, https://www.kaggle.com/datasets/roholazandie/ryanspeech, CC BY-NC-SA 4.0 |

Edresson é fornecido em pacote separado para uso não comercial, preservando atribuição aos autores do corpus TTS-Portuguese-Corpus e da base RyanSpeech, identificados nas origens, e o texto CC BY-NC-SA 4.0. Não se afirma que a cadeia seja CC0 ou comercialmente irrestrita. A relação jurídica entre pesos e corpus não é decidida pelo Piper; consulte as fontes/cards para o seu uso. As restrições do dataset/base não são impostas ao código GPL do plugin.

Faber mantém sua atribuição: Bakhturina, Lavrukhin, Ginsburg e Zhang, Hi-Fi Multi-Speaker English TTS Dataset, Interspeech 2021; NVIDIA Corporation; voz Cori Samuel (reader 92), https://www.openslr.org/109/, https://arxiv.org/abs/2104.01497. O pacote antigo já contém seu BASE_MODEL_CARD e CC BY 4.0.

Não foram alterados os pesos dos modelos. Eles foram obtidos do repositório público rhasspy/piper-voices na revisão fixada nos manifestos. Identificadores de classe são associações configuráveis de timbres; não há gravações ou cópias de dublagem do GTA nos pacotes. Nenhuma chave, save, memória ou diálogo privado é distribuído.

As condições gerais da integração estão em TTS_LICENSES.md. Ao redistribuir os binários modificados GPL, ofereça a fonte correspondente da 2.2.0 e preserve os avisos dos componentes. O instalador não exige baixar/instalar o arquivo de fontes no celular.
