# Adendo 1.2.1

Foram consultados `game_sa/entity/Entity.h` (`GetColModel`, `_ZN7CEntity11GetColModelEv`), `game_sa/base/ColModel.h` (`CColModel::m_boundBox`, CHECKSIZE ARM64 0x38), `game_sa/base/Matrix.h` (`TransformPoint`), `game_sa/engine/RsGlobal.h` e `CSprite::CalcScreenCoors` para âncora/projeção. A leitura ocorre com CPed temporário revalidado na thread do jogo; o render recebe apenas valores copiados.

Para postura foram consultados `ai/TaskManager.h`, `ai/tasks/TaskTypes.h`, `enum/AnimationId.h` e `enum/AnimGroup.h`: CTaskSimpleStandStill (`_ZN21CTaskSimpleStandStillC2Eibbf`), CTaskSimpleRunTimedAnim (`_ZN23CTaskSimpleRunTimedAnimC2E12AssocGroupId11AnimationIdffib`), `ANIM_STD_CHAT` e `ANIM_GROUP_DEFAULT`. São tarefas nativas temporizadas alocadas pelo operador new do SDK/jogo e entregues ao TaskManager; não há retenção de ponteiro de ped/task para limpeza. Não se inserem tarefas em slots ocupados e não se alteram física ou posição.

Não foi adivinhado ID de bone de cabeça. O balão usa bounds finitos transformados ou fallback configurável 1,75 m. O editor deixou de criar Dialog e é uma View na Activity host. Não se força unpause de CTimer ou menu. A validação runtime de todos esses recursos no APK do usuário continua pendente. Detalhes em RELEASE_1_2_1.md; os adendos seguintes são históricos.

# Atualização da auditoria — versão 1.2.0

A versão 1.2.0 usa `game_sa/engine/Sprite.h`, `CSprite::CalcScreenCoors`, símbolo `_ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb`, para projetar o snapshot de posição do candidato. Não adiciona offsets. A âncora é posição + 2,15 m, sem fingir leitura do osso da cabeça. O teclado atual usa editor Java/JNI embutido (ver RELEASE_1_2_0.md), superando a ausência de contrato para eventos OSET_KeyboardSoft. A verificação de versão é Android PackageManager; não usa mais GetAppVersionName de AML 1.4.1. A auditoria abaixo descreve a primeira versão.

# Auditoria do alvo Android 2.10 ARM64

Consulta realizada em 11/11/2026 antes de implementar o GameBridge. O arquivo `third_party/manifest.json` fixa commits e hashes dos arquivos vendorizados. Nenhum binário do GTA é distribuído.

| Referência | Commit consultado | Uso |
| --- | --- | --- |
| [gtasa-android-plugins-base](https://github.com/kudanilll/gtasa-android-plugins-base/tree/f4e6c8f326a9d9e92e9dbd4a1aebcd306a039580) | `f4e6c8f326a9d9e92e9dbd4a1aebcd306a039580` | Headers Android, AML, ImGui 1.93.1 WIP, RenderWare |
| [aml-psdk](https://github.com/AndroidModLoader/aml-psdk/tree/6dd5c931d74050802b6f3b5b60aab6922edcb0f7) | `6dd5c931d74050802b6f3b5b60aab6922edcb0f7` | Confirma suporte explícito a GTA SA 2.10 64 bits e pool/eventos/layout |
| [GTASA_PedFuncs](https://github.com/AndroidModLoader/GTASA_PedFuncs/tree/0200709f664e99152c30f5fd84bcd8113fa4569c) | `0200709f664e99152c30f5fd84bcd8113fa4569c` | Referência de uso do SDK para tipos/modelos de ped e integração Android |

O código compilado usa a adaptação de SDK do template. Não se misturam tipos C++ de duas cópias diferentes do SDK. PedFuncs é referência de implementação, não é dependência do mod nem fornece uma API de identidade permanente.

## APIs realmente utilizadas

Caminhos relativos a `third_party/base/plugin-sdk/`:

| Recurso | Fonte verificada | Uso e restrições |
| --- | --- | --- |
| CJ | `game_sa/entity/PlayerPed.h`, `FindPlayerPed(int)`, `_Z13FindPlayerPedi` | `-1` = jogador local; validar no pool antes de ler |
| Pedestres | `game_sa/other/Pools.h`, `CPools::GetPed(int)`, `CPools::GetPedRef(CPed*)` | Resolver por referência nativa; validar bit de slot vazio e retorno consistente |
| Pool | `CPools::ms_pPedPool`, `CPool<CPed, CCopPed>` | Slot GOT **0x84B968 de 2.10 ARM64**, fornecido tanto pelo template quanto pelo aml-psdk; o valor 32 bits do header não é compilado |
| Posição | `game_sa/entity/Placeable.h`, `GetPosition()` | Usa `m_matrix` ou `m_placement`, conforme o layout Android do SDK |
| Modelo, tipo, saúde, estado, veículo | `game_sa/entity/Ped.h`, `Entity.h` | Campos Android do SDK; exige ped vivo, tipo ped, modelo carregado/RW, sem remoção pendente |
| Ciclo de vida | `Events.h`: `pedCtorEvent`, `pedDtorEvent`, `entityRemoveEvent`, pools/restart | Construtor/destrutor/remover do mundo invalidam imediatamente; callbacks do SDK, sem inventar símbolos |
| Atualização | `Events::gameProcessEvent.before` | Toda leitura de CPed/pool/horário ocorre na thread do jogo |
| Controles | `game_sa/engine/Pad.h`, `_ZN4CPad10UpdatePadsEv`, `CPad::GetPad(0)`, `CPad::Clear(false,false)` | Limpar estado de entrada enquanto chat aberto; não alterar persistentemente flags de missão |
| Horário | `game_sa/other/Clock.h` | `ms_nGameClockHours`/`Minutes`, GOT 2.10 ARM64 do SDK |
| Região ampla | `game_sa/engine/Zones.h`, `GetLevelFromPosition` | Opcional, resolvida por símbolo; não afirma conhecer o bairro |
| Render e touch | Backend RenderWare derivado do template | Símbolos validados; hook de touch obrigatório; captura modal publicada via coordenador compartilhado |

`Events.h` e alguns dados RenderWare usam slots GOT/PLT selecionados por `BYBIT`, provenientes do SDK Android 2.10 ARM64. Não são offsets de PC. O mod exige arquitetura AArch64 em compilação, APK informando exatamente `2.10`, AML >= 1.4.1 e biblioteca de tamanho compatível antes de acessar esses dados. Isso não autentica um APK modificado: diferentes executáveis que conservem a mesma versão precisam de auditoria própria. Não se oferece compatibilidade com 2.00, 2.11, Definitive Edition, porte PC ou launchers SA-MP.

A compilação executa as verificações `CHECKSIZE` do SDK (por exemplo CPed 0x988, CPlayerPed 0x998 e CPool 0x20). Elas confirmam consistência dos headers compilados; a correspondência com o executável instalado ainda precisa do teste no aparelho.

## Identidade e vida útil

A chave pública tem formato `s<128 bits aleatórios>.p<serial monotônico>`. O registro interno guarda slot, referência geracional, modelo e snapshot de valores; não guarda CPed. A referência é obtida/resolvida pelo pool antes de qualquer leitura. Mudança de referência/modelo, ausência em uma varredura, destruição, remoção do mundo e reinicialização invalidam a associação. Os eventos de criação/destruição também cobrem reutilização com wrap dos 7 bits da geração do pool. Não se depende apenas do handle geracional.

O chat selecionado é cancelado imediatamente em destruição/remoção e revalidado a cada tick. A varredura é linear no tamanho do pool (limitado defensivamente a 4096). A memória local mantém até 32 conversas e 2..100 mensagens por identidade; o servidor tem limites próprios e TTL. O namespace adicional `conversation_id` muda ao limpar ou recriar uma conversa local, tornando respostas antigas inutilizáveis nessa conversa.

Ao sair do raio, o chat fecha; o histórico continua associado ao mesmo ped enquanto ele permanecer seguramente identificado no pool. Ao remover um NPC do mundo, optamos por perder a identidade mesmo que o jogo volte a adicionar o mesmo objeto; é uma decisão conservadora. Outra sessão do jogo não recupera identidades de peds aleatórios. Persistência do servidor não altera essa restrição.

## Personalidade e fatos observados

Os modelos originais 102..104 (BALLAS1..3), 105..107 (FAM1..3) e 261/262 (WMYCD1/BMOCD) geram personalidades inferidas. O tipo `PED_TYPE_COP` do SDK seleciona policial. Os demais modelos geram um pedestre comum. Os identificadores nominais dos modelos foram também conferidos em [eModelID.h](https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Enums/eModelID.h); somente os IDs de assets foram consultados, sem importar código, estruturas ou endereços PC.

O servidor recebe modelo, tipo numérico, estado numérico, área/interior, horário, região ampla quando disponível e flag de estar em veículo. Não recebe conhecimento inventado de missão, nível de procurado, arma ou bairro. Nem estar em veículo nem usar uma skin de taxista prova que o NPC esteja trabalhando como motorista. Skins e scripts alterados podem mudar o significado narrativo dos modelos. A identificação na UI explicita a inferência do modelo original.

## Teclado Android e limitações do SDK

`game_sa/engine/OS.h` declara `OS_KeyboardRequest(int)` e eventos de teclado. Esses headers não definem o contrato do payload `OSET_KeyboardSoft` para receber texto composto, acentos ou colagem. Abrir a IME sem essa ponte não entrega uma edição confiável. Esta versão usa teclado ImGui próprio, com minúsculas/maiúsculas, números, pontuação e acentos portugueses, além de apagar por ponto UTF-8. Não há ponte JNI/IME nativa validada ou promessa de suporte a teclado físico. Implementar IME exige auditar os eventos no executável/dispositivo e testar composição; nenhuma estrutura de evento foi inventada.

## Validação que falta no aparelho

Não houve acesso a aparelho, APK ou `libGTASA.so` para execução. Ainda precisam ser verificados: resolução dos símbolos no APK instalado, cobertura dos hooks para todos os caminhos de destruição, leitura dos layouts reais, captura de touch em cada escala, restauração dos controles (incluindo gestos já mantidos), renderizador/contexto compartilhado com outros mods, carregamento inicial/reload, memória/FPS e acesso loopback entre GTA e Termux sob SELinux e restrições de bateria. Os testes host não demonstram que essas condições já passaram.

## Adendo 1.3.0 — ações e pickups

Fonte das assinaturas/layouts: `third_party/base/plugin-sdk/game_sa/ai/tasks/TaskTypes.h`, `ai/TaskManager.h`, `enum/AnimGroup.h`, `enum/TaskEnums.h`, `other/Pickups.h`, `engine/ModelInfo.h`, `engine/Streaming.h`, `engine/World.h` da revisão fixada no manifest. Os três repositórios técnicos indicados pelo usuário continuam registrados em `third_party/manifest.json`. Não foi utilizado endereço PC para a implementação nova.

- Orientação: construtor coordenada `_ZN35CTaskComplexTurnToFaceEntityOrCoordC2ERK7CVectorff`, fallback `CTaskSimpleAchieveHeading`. Vetor frontal do ped confirma ângulo; nenhuma escrita direta de heading/posição.
- Parada: `CTaskSimpleStandStill`. Fuga: `CTaskComplexSmartFleePoint`. Seguimento temporário: `CTaskSimpleGoToPoint`. Medo: `CTaskSimpleDuck` e `CanPedDuck`. São wrappers de construtores/operadores reais dos headers Android, não classes ou símbolos inventados.
- Animação: `CTaskSimpleRunTimedAnim(AssocGroupId,AnimationId,float,float,int,bool)`, grupo `ANIM_GROUP_DEFAULT`, IDs simbólicos `ANIM_STD_CHAT`, `ANIM_STD_HANDSUP`, `ANIM_STD_PARTIAL_FUCKU`, `ANIM_STD_ENDCHAT2`. O campo `CTaskSimpleAnim::m_pAnim` confirma associação, mas não prova conteúdo visual correto; validar cada clip no aparelho.
- Slots: tarefas temporárias de resposta a evento e tarefas secundárias de animação/abaixar. Os slots físicos, primários de missão e resposta não temporária são prioritários e impedem controle conversacional. Tipo PED_MISSION, estado de combate/queda/veículo e dano recente também impedem. O mod não restaura uma cópia bruta de tarefas antigas.
- Vida de tarefas: hooks de `_ZN5CTaskD2Ev` e `_ZN5CTaskdlEPv`, declarados no SDK. Sem ambos as ações ficam inelegíveis. Só são removidas tarefas cujo endereço ainda pertence à identidade geracional e corresponde ao slot atual; callbacks de destruição/delete invalidam essa posse. A invalidação do ped limpa o registro antes de tentar remover tarefas de uma entidade reciclada.
- Dinheiro: `CPickups::GenerateNewOne`, `RemovePickUp`, `GetActualPickupIndex`, `GetUniquePickupIndex`. Tipo `PICKUP_MONEY`, valor via parâmetro ammo e verificação `m_nAmmo` do header Android; o saldo final coletado continua exigindo teste real. O array `aPickUps` vem do PLT ARM64 `0x84F818` **já declarado no SDK Android**, sem novo endereço ou capacidade de array copiados do PC. Índice vem da função nativa e geração é conferida pelo handle único.
- Asset consultado por `CModelInfo::GetModelInfo("money", &model)`; não foi codificado um ID numérico PC. Streaming assíncrono por RequestModel; ground via FindGroundZFor3DCoord. Falhas recusam entrega e removem o lote parcial.

**Alcance da auditoria:** headers, assinaturas, layouts compiláveis ARM64 e verificações de disponibilidade em runtime. Não há `libGTASA.so` nem aparelho neste ambiente; exportação e semântica destas APIs no executável do usuário não foram confirmadas. Os logs de capacidades e o roteiro `RELEASE_1_3_0.md` são obrigatórios antes de considerar as ações validadas. Um símbolo encontrado tampouco prova a semântica completa do jogo. Nenhum mock é usado no backend Android; somente nos testes host.

## Adendo 1.4.0 — persistência e vida das tarefas

Novamente consultados os headers Android vendorizados `game_sa/ai/tasks/TaskTypes.h`, `ai/TaskTimer.h`, `ai/TaskManager.h`, `anim/AnimBlendAssociation.h` e enums de animação. Não foram adicionados offsets numéricos nem IDs de PC.

- `CTaskSimpleStandStill(int iDuration, bool bForever, bool bTillStopped, float fIdleBlendDelta)` declara explicitamente bForever no header Android. A tarefa própria agora usa esse recurso, sem renovar espera de 1000 ms nem parar a cada frame.
- O construtor temporizado de animação e o construtor nomeado declaram suporte ao último frame; `CTaskSimpleAnim` contém `m_bHoldLastFrame`, `m_bIsFinished`, `m_pAnim`. Mãos levantadas usam a opção de segurar o último frame; clips já pertencentes ao mod mantêm a identidade da tarefa.
- `CTaskSimpleRunTimedAnim` declara `m_iDuration` e `m_timer`; `CTaskTimer` declara `m_nStartTime`, `m_nInterval`, `m_bStarted`, `m_bStopped` e construtor inline `(start,interval)`. Apenas o timer **da tarefa de mãos levantadas própria, ainda no slot e observada** é renovado a cada ~55 s para segmento de 60 s. Não se escreve o tempo do clip nem posições/orientação/velocidade do ped.
- `CAnimBlendAssociation::m_fCurrentTime` e `m_fBlendAmount`, com tamanho ARM64 conferido pelo SDK, são consultados apenas depois de validar a vida da tarefa e sua presença no slot. Progresso e blend >0.05 confirmam efeito, em vez de somente associação não nula. A semântica visual ainda exige dispositivo.
- Primárias e secundárias são removidas separadamente por ação e prova de propriedade. O marcador de remoção intencional diferencia pedidos do mod de destruições externas nos logs. No hook de destruição são usados somente metadados copiados; não se lê uma associação já destruída. Término após observação gera notificação sem reter ponteiro; não identifica por si só a causa completa do término.
- Gestos alternam somente `ANIM_STD_CHAT` e `ANIM_STD_ENDCHAT2` já existentes, além dos IDs de rendição/irritação/recusa previamente auditados. Nenhum IFP externo ou razão de loop de um clip desconhecido foi introduzido. A persistência usa hold/timer do SDK e recuperação limitada quando a tarefa desaparece.
- O resolver físico separa identidade/vida da distância inicial. O controlador valida distância inicial para adquirir um NPC; atores já controlados usam distância/histerese por estado. Encerrar uma UI enfileira o detach para o tick; reset e callbacks de vida mantêm a limpeza obrigatória.

Alcance: fonte Android, assinaturas, layouts compilados e guardas runtime. Não existe verificação do novo comportamento visual no executável do usuário neste ambiente. Os bloqueios de prioridade foram preservados; não foi classificado um slot primário desconhecido como caminhada inofensiva sem evidência. Alguns NPCs podem permanecer inelegíveis; use Action gate e o roteiro da versão para investigar.

## Adendo 2.0.0 — percepção e perfil de save

Headers novamente inspecionados: `engine/GenericGameStorage.h`, `engine/World.h`, `other/Weather.h`, `other/Clock.h`, `other/Pools.h`, `other/PlayerData.h`, `other/Wanted.h`, `other/Explosion.h`, `entity/Ped.h`, `entity/Weapon.h`, `Events.h` e `base/ColModel.h`. São os headers Android fixados no manifest dos repositórios de referência; não foram adicionados offsets PC.

- Contexto de CJ: posição/velocidade já usadas pela base, `m_nSelectedWepSlot` com limite [0,13), `m_aWeapons[].m_eWeaponType`, `bIsAimingGun`, `bInVehicle`, `m_pPlayerData->m_pWanted->m_nWantedLevel` com ponteiros verificados. Wanted fica local; não é dado de percepção pessoal automático ao modelo.
- Ambiente: CClock e CWeather por wrappers PLT Android existentes. Dados são snapshots, não confirmação de visão da mudança pelo NPC.
- Veículos: pool/mapa, `CPools::GetVehicle/GetVehicleRef`, renderer/área/posição válidos e callbacks de construtor. Listener de vehicleCtorEvent recebe só CVehicle*, pois o mapeamento de argumentos do evento expõe o índice 0, embora o construtor real tenha outro argumento. Esse detalhe foi corrigido durante a compilação.
- LOS: `GetIsLineOfSightClear` e `ProcessLineOfSight` declarados no SDK. A consulta detalhada pode aceitar colisão com o próprio alvo como visibilidade, sem mudar estado global de ignoreEntity. O FOV é aproximado pelo corpo, não rastreamento real de olhos.
- Arma: weaponFireEvent observa invocação da rotina e não é anunciado como som/projétil confirmado. Explosão: callback seguido de `CExplosion::TestForExplosionInArea` com tipo/posição e região pequena; não se supõe capacidade de array PC. Dano de veículo não prova colisão/acidente. Morte usa saúde zero de entidade ainda validada no pool/scan, sem ler entidade já destruída.
- Save: hooks dos símbolos declarados `_ZN19CGenericGameStorage18OpenFileForReadingEPcj`, `_ZN19CGenericGameStorage11GenericLoadERb`, `_ZN19CGenericGameStorage11GenericSaveEi`. São usados argumentos e resultado bool, com slot [0,99] apenas como nome lógico — sem acessar um array por esse limite. Load exige sucesso e ausência de wrongVersion; falta de capacidade deixa perfil automático isolado e admite perfil manual explícito.
- Identidade entre sessões: nenhum símbolo de identificador individual estável foi inventado. A estratégia continua a identidade geracional com serial de vida; nenhuma memória é restaurada só por modelo/posição.

Alcance: inspeção de fonte, assinaturas, layouts compilados, guardas e testes portáveis. Sem executável/aparelho nesta sessão, presença efetiva dos símbolos, listener em runtime, qualidade da visibilidade, detecção de slot e semântica visual permanecem pendentes. A lista de capacidades no log permite conferir os hooks no aparelho. Nenhum teste host foi apresentado como validação dessas funções no GTA.

## Adendo 2.0.1 — configuração e inicialização

Nenhum offset, símbolo de tarefa, ID de animação ou estrutura do GTA foi alterado. O caminho primário continua `GetAndroidDataPath()`, documentado em `third_party/base/mod/iaml.h` como `/sdcard/Android/data/.../files/*`; o fallback restrito usa `GetConfigPath()` do mesmo header, disponível na ABI AML alvo. O prefixo real retornado pelo loader pode ser Android_unprotected, sem necessidade de reconstruí-lo manualmente.

A falha opcional de NativeServices não invalida mais o editor registrado pelo mesmo DEX. `PrepareNativeProvider` contém exceções e retorna estado; GameBridge/ImGui são instalados independentemente desse resultado. Permanecem os bloqueios reais de versão APK/ABI, libGTASA e disponibilidade do renderizador. Não houve teste destes caminhos no dispositivo nesta sessão.

## Adendo 2.1.0 — Piper e carregamento AML

Nenhum offset, ID de animação, estrutura CPed ou assinatura de tarefa nova foi adicionada. O controlador existente ganhou VoicePlayback para usar progresso real de áudio respeitando as mesmas prioridades. A worker TTS não acessa SDK.

Foi auditado o loader oficial https://github.com/AndroidModLoader/AndroidModLoader/tree/cea56d084e59a5e957f78ec012edb5e397abdcb6 : main.cpp::LoadMods copia cada .so de mods para g_szDataDir/GetDataPath, chama dlopen e remove a cópia. Portanto o path de dladdr pode apontar para um arquivo temporário removido; não se pressupõe carregamento automático de subpastas. GetDataPath/GetAndroidDataPath/GetInternalModsPath existem na ABI AML >=1.4, conforme header vendorizado (o último desde 1.2.3). A implementação prepara cópias próprias e verificadas em GetDataPath()/npcai-tts-lib.

Piper é API C oficial de revisão fixada, com adaptador próprio explicitamente nomeado npcai_piper_*. Os símbolos desse adaptador não são símbolos inventados da engine. APIs de áudio são Android AudioTrack e JNI. ONNX Android vem do AAR 1.22.0, minSdk24; Piper e plugin são compilados NDK r27d/API26. Nenhum binário Linux do teste host faz parte da distribuição Android.

## Adendo 2.2.0 — classes de voz e evidência de taxista

Nenhum offset novo foi introduzido. ePedType vem de third_party/base/plugin-sdk/game_sa/enum/PedEnums.h. Modelos Ballas/Families 102..107 preservam o agrupamento já usado no projeto. IDs de taxi/cabbie são resolvidos por nome por CModelInfo::GetModelInfo1, declarado no SDK Android em engine/ModelInfo.h com símbolo _ZN10CModelInfo12GetModelInfoEPKcPi; se indisponível não se supõe um ID. O pool CVehicle é percorrido uma vez por snapshot existente, validado por CPools::GetVehicle/GetVehicleRef/mapa, e o campo m_pDriver do header Android Vehicle.h é apenas comparado com um CPed validado. Nenhum CVehicle*/CPed* é mantido pela worker ou entre snapshots. A observação booleana pertence à identidade geracional da Registry.

261/262 são modelos de crupiês, não fundamento seguro para profissão de taxista; a inferência incorreta foi removida da identificação/prompt nativos e do servidor legado. O campo de contexto observed_taxi_driver mantém essa evidência no legado; não modifica tarefas nem dá comandos ao veículo. Os testes host exercitam o classificador com valores simulados, e não comprovam essa observação no executável real.
