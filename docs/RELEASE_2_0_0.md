# NPC AI 2.0.0 — instalação e validação no aparelho

Alvo: **GTA San Andreas Android 2.10 ARM64, AML >=1.4, Android >=8**. O plugin agora contém C++, o editor Java anterior e serviços Java de HTTPS/credenciais no DEX embutido. Não exige Termux, Python em execução ou aplicativo auxiliar.

**Estado da validação:** código compilado e testes host/JVM executados; não houve execução desta versão em aparelho nem chamada autenticada real ao Gemini. A base anterior foi confirmada pelo usuário, mas isso não valida o novo HTTPS, Keystore, memória por slot e percepção no executável. Consulte [VALIDATION_2_0_0.md](VALIDATION_2_0_0.md).

## Instalar

1. Feche o GTA por completo. Guarde o plugin anterior fora da pasta `mods`. Se o servidor antigo estiver ligado, pode encerrá-lo; ele não é usado pelo transporte nativo.
2. Extraia `NPC_AI_2.0.0_ARM64.zip`. Copie `npcai-android/libs/arm64-v8a/libnpcai.so` para a pasta efetiva de mods AML, mantendo uma única cópia. No aparelho anteriormente relatado: `/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/mods/`.
3. Atualize o INI já usado pelo mod, preservando opções antigas de balão/controle/dinheiro. O log `Config path=.../npcai.ini` identifica o caminho exato. Use `config/npcai.ini` como referência; evite opções duplicadas.
4. Comece com Mock: `npc_ai_transport=native` e `gemini_enabled=0`. Reinicie GTA e converse. Nenhuma chave ou processo externo é necessário.
5. Para Gemini, escolha uma modalidade de credencial abaixo e use um modelo realmente disponível à sua conta. O padrão é `gemini-2.5-flash-lite`, usado no projeto anterior; nomes como `gemini-3.5-flash-lite` são aceitos sintaticamente, mas sua existência/disponibilidade não é presumida. Use o nome curto, sem `models/`.

## Credencial em texto puro — escolha explícita

```ini
npc_ai_transport=native
gemini_enabled=1
gemini_model=gemini-2.5-flash-lite
gemini_timeout_ms=25000
gemini_max_retries=2
gemini_key_storage=ini
gemini_key_action=none
gemini_api_key=SUA_CHAVE_PRIVADA
```

Essa opção facilita desenvolvimento, mas **qualquer aplicativo/mod capaz de ler o INI pode obter a chave**. Não compartilhe o arquivo preenchido, não o publique nem o inclua em ZIP/APK. O exemplo distribuído não contém credenciais. Não há registro da chave em logs, exceções ou falas.

Para remover o uso da chave em texto puro, apague o valor `gemini_api_key`, escolha Mock ou use `gemini_key_action=remove`. A opção remove bloqueia o uso de credencial nessa inicialização, inclusive se uma chave antiga ainda existir; apague também o texto do INI, pois o plugin não reescreve silenciosamente seu arquivo.

## Android Keystore — recomendado

Para importar/substituir:

```ini
gemini_enabled=1
gemini_key_storage=keystore
gemini_key_action=replace
gemini_api_key=SUA_CHAVE_PRIVADA
```

Abra GTA e confira o estado `credential_stored` no log. Depois feche o jogo, **apague o valor da chave do INI** e mude `gemini_key_action=none`. Na próxima abertura a chave é recuperada da credencial criptografada privada, usando Android Keystore.

A implementação usa AES-GCM, chave AES de 256 bits criada no Android Keystore e arquivo privado autenticado. Hardware seguro não é garantido: depende do aparelho/provedor. A cifra e o formato foram exercitados no JVM; **a criação/recuperação real pelo Android Keystore permanece pendente de aparelho**. Se a importação falhar, o log mostra `credential_storage` e o mod não troca silenciosamente para armazenamento inseguro.

Para apagar a credencial protegida:

```ini
gemini_key_storage=keystore
gemini_key_action=remove
gemini_api_key=
```

Reinicie e confira `credential_removed`. Volte a `none` antes de importar outra chave. A remoção tenta excluir alias do Keystore e arquivos de credencial. Não use cópia desse arquivo criptografado como backup portátil: ele depende da chave/UID do aplicativo. Outros mods no mesmo processo podem acessar memória e APIs sob o mesmo UID; Keystore protege principalmente a chave em repouso, não torna um processo com mods não confiáveis um enclave.

## Permissão INTERNET e TLS

O plugin verifica `android.permission.INTERNET` no **APK host**. Estado `permission` indica impedimento ou ausência da permissão. Não é possível adicionar uma permissão ausente no manifesto apenas com um `.so`; `pm grant` também não inventa uma permissão não declarada.

Não foi fornecido o APK nesta sessão, portanto sua permissão real não foi confirmada. Se faltar, use um APK/host AML legitimamente preparado com essa permissão, ou mantenha Mock nativo. Não é oferecido bypass da proteção Android; um servidor local também não concede permissão de rede ao APK.

HTTPS usa `HttpsURLConnection`, verificadores TLS padrão, URL Google fixa, chave em header e redirects desativados. Não há trust-all, desativação de hostname ou chave na URL. 429/500/502/503/504 e falhas transitórias admitem até dois retries adicionais, com backoff/jitter e Retry-After quando cabe no orçamento. Credencial/modelo inválido e erro TLS não são repetidos indiscriminadamente. O prazo inclui o orçamento do cliente; cancelar retorna à worker nativa sem bloquear o jogo. As threads Java remanescentes são limitadas e têm deadlines/desconexão de melhor esforço.

## Memória e saves

```ini
npc_memory_enabled=1
npc_memory_autosave_seconds=10
npc_memory_persistent=1
npc_memory_max_mb=10
npc_memory_global=1
npc_memory_individual=1
npc_memory_profile=auto
npc_memory_clear=0
npc_memory_send_context=0
npc_context_tokens=2048
```

Arquivos são criados automaticamente no diretório privado `no_backup/npcai` do aplicativo. O INI continua no caminho AML existente. A memória não fica junto do `.so` nem no servidor Python. Não depende do callback de saída: uma worker verifica alterações a cada 10 s reais, tenta salvar ao fechar conversa, em pausa Android, em eventos adequados do GTA e em load/save. Sem alterações, não regrava o mesmo estado.

No modo `auto`, os hooks dos símbolos Android `OpenFileForReading`, `GenericLoad` e `GenericSave` usam o slot informado e o resultado real da operação para escolher o perfil. Só um load bem-sucedido confirma o slot de leitura. Save As cria uma cópia narrativa no perfil do slot salvo. Recibos/respostas em trânsito carregam um epoch e não devem contaminar o perfil seguinte.

**Se os hooks não estiverem disponíveis:** a sessão sem slot confirmado é isolada, em vez de reutilizar um arquivo genérico de outra partida. Nesse caso, configure explicitamente `npc_memory_profile=minha_partida_1`; use o mesmo nome para continuar essa partida e outro nome para outro save, reiniciando GTA após editar. O perfil manual não identifica sozinho a troca de save: a separação passa a ser responsabilidade dessa seleção. Confira `save_open/save_load/save_write` no log. Sobrescrever um slot com uma campanha diferente exige limpar/escolher outro perfil.

Há JSON com versão de esquema, checksum de corrupção, escrita temporária/rename/fsync, backup e limite agregado de tamanho dos arquivos de memória. Arquivos incompletos podem recuperar o backup; esquema mais novo não é sobrescrito automaticamente por esta versão. Limites: 128 registros individuais, 24 notas por indivíduo, 96 notas globais, limpeza de registros antigos e controle do espaço. O checksum não é assinatura contra adulteração intencional por outro mod.

Para apagar a memória do perfil, use `npc_memory_clear=1`, reinicie/carregue o perfil e depois retorne a 0. Deixar 1 solicita limpeza nas próximas cargas. Para remover todos os dados privados, use a limpeza de dados do APK — isso também remove outros dados do aplicativo, portanto prefira a limpeza por perfil quando possível.

### Limite obrigatório de identidade

Nenhum identificador individual estável de NPC foi confirmado nesta integração da engine. Pedestres aleatórios usam identidade efêmera por sessão/vida, nunca apenas modelo. Suas notas podem permanecer arquivadas, mas **não são recuperadas como se um novo pedestre do mesmo modelo fosse o mesmo indivíduo**. Não há recriação automática de personagens ou tarefas antigas.

A API de memória suporta bindings estáveis verificados e sua restauração é testada no host, mas **o GameBridge desta entrega não fornece esse binding para personagens do GTA**. A continuidade real entre sessões é global/narrativa por save, rotulada como tal; não se promete a lembrança pessoal do mesmo pedestre no dia seguinte. Criar NPCs persistentes próprios ou reconhecer personagens nomeados com prova estável fica pendente.

Por padrão, `npc_memory_send_context=0` mantém notas persistentes fora da requisição Gemini. Histórico curto da conversa atual, personalidade e contexto atual necessário continuam sendo enviados ao conversar. Use 1 se quiser autorizar resumos persistentes limitados no contexto; relatos globais são rotulados como **não sendo lembrança pessoal deste NPC**. Não há telemetria nem servidor próprio.

## Percepção, desafios e perplexidade

```ini
world_perception=1
world_perception_interval_ms=500
world_event_memory=1
world_expectations=1
world_max_active_expectations=3
npc_perplexity_enabled=1
npc_perplexity_decay=1
npc_perplexity_gain=1.0
```

Leituras da engine ficam na thread do jogo; detecção, classificação, emoções, resumos e gravação ficam na worker. O pool não é analisado integralmente a cada frame. Atores controlados continuam recebendo validação de vida/prioridade por tick.

O observador acompanha hora, período, chuva/clima, interior, CJ (posição, movimento, arma, mira, veículo, wanted lido localmente), pedestres/veículos próximos, estados de combate, saúde zero e callbacks disponíveis de arma/explosão/dano. Não envia um snapshot inteiro ao Gemini. A região disponível é ampla e não deve virar bairro inventado.

Limites: até quatro observadores recentes/selecionado, 128 peds/64 veículos de amostra, 12 consultas LOS por amostra e filas limitadas. FOV usa orientação corporal aproximada. Entidades novas têm rótulo de presença observada/streaming/indício de construtor; nenhum desses dados prova autoria de CJ. Visibilidade não verificada recebe confiança baixa e rótulo próprio. Interior impede inferência ambiental de céu/chuva. Wanted não é revelado automaticamente ao personagem como conhecimento pessoal.

O callback de arma observa a invocação da rotina, **não confirma projétil nem som**. O evento é rotulado como tal, sem fingir audição. Explosão usa consulta nativa de atividade próxima após o callback; dano em veículo não é tratado automaticamente como acidente. Objetos arbitrários relevantes, colisão exata, visão da cabeça, autoria causal completa e um detector auditivo real de todos os disparos não estão implementados. Eventos antigos e fora do alcance são descartados ou classificados conservadoramente.

Expectativas monitoráveis: `night`, `time_reversed`, `rain_started`, `ped_appeared`, `vehicle_appeared`, `weapon_holstered`, `player_left`. Campos: `event`, `count` (1..10; contagem múltipla só entidades), `timeout_s` (15..180), `radius` (1..40), `min_confidence` (.35..1), `require_visible`. Contagem usa entidades distintas. Eventos fora do prazo, duplicados, baixa confiança ou falta de visibilidade não satisfazem a expectativa. Expectativas são de sessão; não são restauradas para um NPC que não existe mais.

Exemplo em Mock, durante o dia: diga que controla o mundo; ele pode propor anoitecer. Altere o horário com uma ferramenta já sua. O mod detecta a mudança, relaciona temporalmente o desafio e **não afirma autoria**. Perplexidade/medo/curiosidade são atualizados em C++, com habituação, dedup e decaimento. Não se aumenta confiança automaticamente por uma demonstração estranha. A perplexidade é um estado narrativo, não uma medida literal de consciência.

## Modos de reação e custo

```ini
npc_reaction_mode=economy
npc_spontaneous_ai=0
npc_spontaneous_cooldown_s=60
npc_spontaneous_budget_hour=4
```

- `economy`: detecção, emoções, resumos e comentários locais sem API; Gemini só ao conversar. Mesmo habilitar a flag espontânea não fura esse modo.
- `balanced`: pode sugerir comentários de IA para acontecimentos muito relevantes se `npc_spontaneous_ai=1`.
- `immersive`: aceita mais tipos de acontecimentos relevantes, ainda sujeito ao mesmo orçamento/cooldown configurados.

A flag é opt-in para custo extra; o padrão faz zero chamadas espontâneas. Reações textuais são mostradas para o NPC selecionado e respeitam ocupação do chat/editor; outros observadores podem atualizar memória/emoções sem um balão próprio. Fuga local só usa a whitelist e as validações existentes. Não há observação de todos os NPCs do mapa nem garantia de comentário em todo evento.

Mantidas as animações da 1.4: parada/orientação, gestos proporcionais à leitura, rendição/seguimento persistentes, soltura por distância/comando, prioridade de missão/combate e pickups deduplicados. Nenhuma tarefa física é restaurada do arquivo de memória.

## Roteiro de validação real — todos os itens precisam de aparelho

| Teste | Critério |
|---|---|
| Mock sem Termux | Servidor desligado, só .so/INI: conversar e executar ações elegíveis |
| HTTPS Gemini | Chave válida, modelo da conta, resposta com servidor desligado; conferir permissão/latência |
| Credencial protegida | Importar, retirar texto do INI, reiniciar e conversar; substituir; remover e comprovar ausência de uso da anterior |
| TLS/rede | Rede sem Internet, certificado não confiável/captive portal, 429/503 quando ocorrerem: erro separado, retries finitos, sem chave no log |
| Teclado/balões | Enviar, IME SEND, Voltar, reabrir, trocar NPC, resoluções/orientação; balão default 15 s |
| Expectativa de noite | Durante dia propor desafio, alterar hora, verificar compatibilidade temporal e causa não confirmada |
| Chuva | Desafio com tempo seco, iniciar chuva; interior/fora do alcance não deve gerar visão falsa |
| Aparição | Spawn/streaming/entrada no raio, inclusive atrás de prédio: não afirmar criação/autoria sem prova |
| Contagem | Dez entidades distintas visíveis dentro do prazo; duplicação de amostra não aumenta contagem |
| Arma/afastamento | Guardar arma equipada e perder contato visual dentro do prazo; não inventar localização invisível |
| Perplexidade | Várias demonstrações: habituação, variação de medo/curiosidade, sem confiança positiva obrigatória |
| Autosave | Observar métricas após 10 s com mudança; sem mudanças não deve haver gravação idêntica |
| Reiniciar | Fechar/abrir GTA, carregar mesmo slot e autorizar resumo se desejado: recuperar narrativa global sem identificar novo ped como antigo |
| Trocar save | Slots A/B isolados; testar Save As e hook ausente com perfil manual |
| Corrupção/limpeza | Em cópia de teste, arquivo truncado deve usar backup; limpar perfil não deve ressuscitar notas antigas |
| Persistência física | Mãos levantadas >30 s, chat fechado, distância/comando, dinheiro e seguimento; missão/dano/remover têm prioridade |
| FPS e custos | Comparar observação 500 ms, orçamento de tokens e modo economy com métricas; nenhum request a cada frame |

## Logs e remoção

```sh
adb logcat -c
adb logcat -v time > npcai-200-logcat.txt
# Reproduza e filtre no computador:
rg 'NPC_AI|Native services|World capabilities|Intelligence metrics|Animation|Action gate' npcai-200-logcat.txt
```

`Native services state` informa permissão/credencial sem conteúdo sensível. `World capabilities` informa hooks/LOS; métricas periódicas incluem tempo acumulado de percepção, eventos, expectativas, gravações, tamanho atual, latência, requisições, retries, tokens reportados pelo provedor e erros. Tokens são os reportados pela API, não uma auditoria de cobrança; o orçamento local usa estimativa por bytes. Nenhum log registra diálogo, chave ou token de autenticação.

Desinstalar o mod: feche GTA, retire `libnpcai.so`, opcionalmente o INI. Para apagar credencial/memória use as opções antes de remover o plugin ou a limpeza de dados do aplicativo. Desinstalar/limpar dados do APK também remove dados privados e pode afetar saves e configurações do próprio GTA. Não descarregue o `.so` durante a sessão: hooks permanecem até terminar o processo.
