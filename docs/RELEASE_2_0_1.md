# NPC AI 2.0.1 — INI recuperável e diagnóstico sem ADB

Alvo preservado: GTA San Andreas Android 2.10 ARM64, AML >=1.4, Android >=8. Não exige Termux, Python em execução, aplicativo auxiliar, root ou alteração do APK. O usuário já confirmou INTERNET concedida no APK; isso não significa que a configuração/credencial/JNI tenham sido inicializados corretamente.

Esta versão foi compilada e testada no host/JVM. **Não foi executada em aparelho por este ambiente.** Consulte [VALIDATION_2_0_1.md](VALIDATION_2_0_1.md) para os resultados e limites.

## Causas encontradas no código 2.0.0

1. `LoadSettings` comparava o nome exato antes de `=` sem remover espaços. Assim `gemini_enabled = 1` não alterava o padrão 0, e aparecia `transport=native_mock`. Valores com espaços/comentários no fim podiam falhar. Um BOM antes do primeiro nome também podia impedir seu reconhecimento.
2. Arquivo ausente/inacessível retornava os padrões sem distinguir claramente a situação no log. A última duplicata válida determinava o valor, sem aviso; um INI com `gemini_enabled=0` posterior sobrescrevia 1.
3. Uma opção inválida ou arquivo acima de 8 KiB lançava exceção. O único `try/catch` envolvia toda a inicialização e chamava `Stop()`; portanto GameBridge e ImGui podiam nem ser instalados. Isso constitui um caminho concreto para sumir o botão ao passar a ler um INI existente.
4. O editor Java só era considerado pronto depois de carregar/registrar NativeServices. Falha nesse serviço opcional abortava a ponte inteira antes da integração do jogo.

**O diretório `files` não era um erro:** `GetAndroidDataPath()` é documentado pelo próprio header AML como o diretório externo `.../files`. O caminho informado pelo usuário é compatível. Não recebemos o conteúdo do seu INI nem novo crashlog nesta etapa; não é possível apontar qual linha específica acionou a falha no aparelho. As causas acima foram demonstradas pela inspeção e convertidas em regressões testadas.

## Instalação no aparelho, sem ADB

1. Feche completamente GTA. Guarde uma cópia do plugin e do seu INI atuais fora da pasta `mods`.
2. Extraia `NPC_AI_2.0.1_ARM64.zip`. Substitua a única cópia de `libnpcai.so` na pasta efetiva de mods AML por `npcai-android/libs/arm64-v8a/libnpcai.so`. Não deixe versões antigas com outro nome `.so` na mesma pasta.
3. Mantenha seu INI em:

   `/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/files/npcai.ini`

   Esse é o caminho informado pelo seu AML. Não é necessário mover para outra pasta. O exemplo completo está em `npcai-android/config/npcai.ini`; use-o para comparar, preservando seus ajustes e sua chave privada. O plugin não move nem sobrescreve o INI.
4. Abra GTA e toque no botão fixo **NPC AI · Diagnóstico**, no canto inferior esquerdo. Não é necessário haver NPC próximo. O launcher **Conversar · NPC AI** e os balões continuam separados.
5. Confira caminho, INI encontrado, erros por parâmetro, transporte e estado do serviço. Se houver erros, feche GTA, corrija o arquivo e reinicie; esta versão não recarrega configurações enquanto o jogo está aberto.

## Gemini, sem fallback silencioso

Para seu caso em texto puro, use uma única ocorrência de cada opção:

```ini
npc_ai_transport=native
gemini_enabled=1
gemini_model=gemini-2.5-flash-lite
gemini_key_storage=ini
gemini_key_action=none
gemini_api_key=SUA_CHAVE_PRIVADA
```

Escolha um modelo realmente disponível à sua conta. O parser aceita uma chave ASCII com letras, números, `_` e `-`, até 256 bytes; essa validação apenas verifica formato/limite, **não autentica a chave**. Não há buffer fixo pequeno para a credencial. A rede decide sua validade real.

O diagnóstico deve mostrar `android_https_gemini`, Gemini ativado, credencial presente, HTTPS disponível e INTERNET permitida. Se não aparecer assim, a tela permite distinguir o erro sem ADB. `native_mock` é selecionado quando Gemini está explicitamente desativado ou quando não há INI e se usam os padrões gratuitos — a tela indica INI ausente/padrões. Uma opção Gemini inválida não é convertida silenciosamente em resposta Mock.

Uma falha de IA aparece como erro separado, nunca como fala do NPC. Ela não desliga seleção, balões, editor, ImGui nem a validação/execução local já existente. Sem Gemini disponível não há nova decisão produzida pelo modelo: não é fingida a execução de uma ação solicitada por texto. Postura inicial e controladores locais permanecem independentes.

Para selecionar Mock explicitamente:

```ini
npc_ai_transport=native
gemini_enabled=0
```

O armazenamento recomendado continua sendo Android Keystore; importação/substituição/remoção seguem o [guia 2.0.0](RELEASE_2_0_0.md#android-keystore--recomendado). Uma configuração essencial de IA inválida não importa/remove credenciais como efeito colateral. INI em texto puro continua legível por mods/apps com acesso ao arquivo: não publique seu arquivo preenchido.

## Regras do INI e descoberta

- Caminho primário: `GetAndroidDataPath()/npcai.ini`. Fallback restrito, somente se o primário não existe: `GetConfigPath()/npcai.ini`. Caminhos iguais são deduplicados; barras finais não criam `files/files` nem barra duplicada.
- Um primário existente, mesmo vazio/inválido/inacessível, tem prioridade. Não se escolhe um segundo arquivo para ocultar erro do primeiro. O caminho efetivo aparece na tela e no log. Não há busca recursiva no armazenamento.
- UTF-8 com/sem BOM, LF/CRLF, espaços/tabulações em torno de nomes/valores e nomes ASCII sem distinção de maiúsculas são aceitos. Salve em UTF-8: UTF-16/NUL gera erro explícito.
- Comentários de linha `#` e `;` aceitam indentação. Comentários ao final exigem espaço antes de `#` ou `;`. Cabeçalhos `[seção]` são aceitos, mantendo o namespace plano original.
- Duplicatas geram aviso: última ocorrência válida vence. Valor inválido não apaga uma opção local válida anterior; erros em opções essenciais de IA bloqueiam a requisição até correção. Uma ocorrência válida posterior pode corrigir a mesma chave, mantendo o aviso histórico dessa leitura.
- Valores inválidos são rejeitados individualmente, preservando padrão/valor anterior quando seguro. Erros de relações entre distâncias/tempos restauram valores seguros. Nomes desconhecidos geram aviso, sem imprimir o valor.
- Limites: arquivo 64 KiB, linha 4096 bytes. Linha excessiva é descartada sem alocação ilimitada. Truncamento/leitura parcial ou codificação inválida impedem uso ambíguo da IA, preservando a interface. Um INI ausente/vazio é permitido e usa os padrões mostrados.

## Diagnóstico dentro do jogo

A tela contém versão, caminho/estado do INI, erros/avisos, transporte, Gemini, modelo, presença da credencial, disponibilidade do serviço HTTPS, INTERNET, último HTTP/erro, estado do GameBridge, seleção e projeção dos balões.

**Testar Gemini** faz uma requisição curta (`Responda apenas OK`, até 32 tokens de saída) numa worker, sem histórico, memória, identidade de NPC ou ações físicas. Não bloqueia o loop do jogo e não testa por Mock. Respeita timeout/retries configurados; pode consumir quota da API, inclusive em retries. Erros 401/403/404 apontam para credencial/modelo/permissão do serviço; 429 para cota/limite; TLS/rede/JNI são separados. Um HTTP 200 ainda pode conter resposta inválida/vazia: confira também o resultado do teste.

**Exportar relatório** escreve `npcai-diagnostic.txt` ao lado do INI selecionado, numa worker. O caminho e o resultado aparecem na tela. Não exporta chave, token local, mensagens, memória, corpo HTTP ou texto de exceção. O arquivo existente com esse nome é substituído somente ao tocar em Exportar; o INI nunca é reescrito. Se o diretório não permitir gravação, a tela mostra a falha. O relatório usa códigos de serviço conhecidos e não copia mensagens arbitrárias de erro do provedor.

Os balões ainda dependem de NPC elegível, vivo, próximo, identidade válida e projeção na tela. Uma falha real de símbolos/hooks do GameBridge ou RenderWare é diferente de erro Gemini; não é possível prometer balão sem a integração da engine. A tela distingue GameBridge indisponível de rede indisponível quando o renderizador está operacional.

## Roteiro de validação no GTA — pendente

| Cenário | Resultado esperado |
|---|---|
| INI ausente/vazio | Interface e launcher; diagnóstico informa ausência/padrões, Mock gratuito |
| INI com espaços/BOM/CRLF | Gemini ativado reconhecido e caminho correto |
| `radius=nan`, depois Gemini válido | Erro de radius, padrão 3 m, balões/teclado continuam |
| Chave ausente/inválida | Gemini permanece selecionado, erro separado, sem resposta Mock |
| Modelo inexistente | Testar Gemini mostra HTTP/erro, jogo continua |
| Permissão já concedida | INTERNET permitida; não atribuir erro de INI/JNI à permissão |
| Sem NPC próximo | Diagnóstico acessível pelo botão fixo |
| NPC elegível próximo | Balão Conversar, toque, IME, Enviar/Voltar; controles restaurados |
| JNI/HTTPS falha | Editor/touch e GameBridge independentes, erro de serviço visível |
| Teste e exportação | Interface responsiva, resultado sem fala no histórico, arquivo sem segredos |
| Regressões | Memória/autosave, perplexidade, percepção/expectativas, rendição >10 s, dinheiro deduplicado, seguir/parar e combate/missão |

A permissão Android já informada como concedida não exige modificação no APK. Não houve chamada autenticada real ao Gemini nesta sessão de desenvolvimento. Para diagnóstico opcional por ADB, mantenha os procedimentos anteriores de logcat; **eles não são necessários para consultar a nova tela e exportar o relatório**.

## Desinstalar

Feche GTA e retire `libnpcai.so`. INI e relatório podem ser removidos separadamente. Credencial protegida e memória privada permanecem até usar suas opções de remoção/limpeza documentadas na 2.0.0; não é necessário apagar dados do jogo para substituir o plugin. Não houve mudança de esquema da memória nesta correção.
