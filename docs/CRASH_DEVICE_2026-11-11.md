# Atualização posterior do teste de isolamento

O usuário confirmou que o jogo também não travou com apenas NPC AI instalado. Portanto, a falha sugere interação com outros mods, sem identificar o responsável. O botão continuou ausente. A versão 1.1.0 trata a compatibilidade AML 1.4 e adiciona editor Android; veja RELEASE_1_1_0.md. A análise abaixo registra a evidência inicial e suas hipóteses naquele momento.

# Análise do crashlog enviado após instalação

Relato: botão NPC AI ausente; usuário tocou diretamente no pedestre e o jogo fechou. Esta análise usa o `aml_crashlog.txt` fornecido pelo usuário. Não houve reprodução no aparelho nem modificação do código com base apenas em uma hipótese.

## Evidências

- Linhas 60–77: lista de oito mods carregados não contém `br.com.npcai.android`/NPC AI/libnpcai.so.
- Linha 61: AML Core reporta versão 1.4. O binário entregue declara dependência mínima 1.4.1 e utiliza a API GetAppVersionName introduzida em 1.4.1. Não se deve simplesmente baixar essa dependência para 1.4.
- Linhas 7–11: SIGSEGV em libGTASA.so + 0x2BEFB8, `_rpMaterialListAppendMaterial`, endereço inválido 0xDAD5DADAD5DAFE.
- Linhas 713–720: pilha passa por UseReducedMaterialListCB, CVehicleModelInfo::ReduceMaterialsInVehicle, SetClump, CFileLoader::LoadClumpFile e CStreaming::ConvertBufferToObject, seguida por libilusionloader.so + 0x9D9F8.
- O padrão DADA no ponteiro é compatível com memória inválida/preenchida; não prova sozinho quem corrompeu a memória ou qual modelo está defeituoso.
- O caminho efetivo dos mods neste aparelho é `/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/mods/`, diferente do caminho Android/data convencional usado como exemplo no README.

## Conclusões limitadas

O log documenta uma falha durante processamento de materiais de veículo, não uma chamada de chat. A presença do loader ILUSION nessa pilha é uma pista de investigação, não prova de culpa. O relato de toque pode coincidir com streaming de um veículo. A implementação NPC AI responde ao botão ImGui; tocar diretamente no pedestre não inicia uma conversa.

A ausência do NPC AI na lista e a versão reportada do AML justificam investigar primeiro rejeição de dependência ou instalação na pasta errada. O crashlog sozinho não informa a razão pela qual o plugin não consta na lista, nem demonstra que ele jamais foi mapeado ou inicializado parcialmente. É necessário o log de carregamento AML e confirmar que este arquivo corresponde à tentativa atual.

## Recuperação e isolamento reversíveis

1. Fechar totalmente o processo GTA. Mover temporariamente libnpcai.so para fora da pasta mods efetiva, sem apagar saves. Repetir a mesma situação para verificar se a falha persiste sem o plugin.
2. Se persistir, manter NPC AI fora e testar temporariamente sem ILUSION DFF Loader e seus modelos substituídos, restaurando os modelos originais. Mudar uma condição por tentativa e conservar backup. Não remover arquivos comerciais de dados do jogo.
3. Se deixar de falhar somente com NPC AI removido, coletar um crashlog novo e logcat completo da reprodução para investigar eventual inicialização parcial/interação entre hooks.
4. Antes de reinstalar NPC AI, verificar loader compatível >=1.4.1 para esse APK e a pasta usada de fato pelo AML. Não contornar o requisito alterando só o número da dependência.
5. Coletar o log de inicialização desde abrir o processo: `adb logcat -c`, iniciar `adb logcat -v threadtime > npcai-inicializacao.txt`, abrir o jogo e interromper a captura após observar o problema. Localizar mensagens de dependência/erro de libnpcai.so e tag NPC_AI. Revisar dados privados antes de compartilhar.

Nenhum novo binário é apresentado como correção comprovada deste crash. O diagnóstico depende do teste de isolamento e dos logs de carregamento.
