# NPC AI 1.1.0 — entrada Android nativa e compatibilidade AML 1.4

O teste informado pelo usuário foi: sem NPC AI não travou; com apenas NPC AI e os outros mods removidos também não travou, mas não apareceu botão. Isso sugere interação entre mods e/ou problema de inicialização; não identifica sozinho o componente responsável. A menção ao loader ILUSION na pilha anterior não prova que ele causou a falha.

## Alterações

- O requisito de AML passa para 1.4. A chamada `GetAppVersionName`, exclusiva de 1.4.1, foi removida. A versão do APK é consultada pelo PackageManager Android. A leitura da engine continua restrita a APK 2.10 ARM64.
- Um adaptador em `native/compat/plugin-sdk/sdk_base.h` protege a resolução antecipada de símbolos. Os arquivos vendorizados originais continuam intactos.
- Logs por etapa de inicialização na tag `NPC_AI`, versão 1.1.0. Se a ponte Java ou a versão não puderem ser verificadas, a inicialização termina e registra o motivo.
- Editor Android real (`AlertDialog` + `EditText`) no Activity do GTA, executado via `runOnUiThread`. Suporta teclado instalado no sistema, composição, acentos, emoji e colagem. Não usa o payload de teclado desconhecido da engine.
- Java compilado por ECJ 3.37.0 e D8 8.3.37. DEX embutido no `.so`, carregado por InMemoryDexClassLoader; nenhum APK ou arquivo DEX externo precisa ser instalado. Fontes em `android/`, reconstrução por `tools/build_keyboard.sh`.
- Texto retorna via JNI em UTF-8 padrão, com limite de 1200 bytes e sincronização por ticket. Resultados de edição são descartados se o NPC mudar, a conversa for limpa ou o chat fechar. O teclado touch continua disponível.
- Esta versão requer Android 8/API 26 ou superior por causa do carregador DEX em memória. O aparelho informado tem API 33.

## Instalar e testar

1. Feche o GTA e guarde uma cópia do plugin antigo fora da pasta mods.
2. Comece com os outros mods desativados, conforme o teste que não travou. Não apague saves ou modelos comerciais.
3. Copie o novo `libnpcai.so` para a pasta usada pelo AML. No log enviado, a pasta é `/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/mods/`. Remova cópias duplicadas do NPC AI nessa pasta; não instale o probe junto.
4. Inicie o servidor mock como antes. Abra GTA e carregue o save. O botão flutuante deve existir mesmo se nenhum NPC for selecionável (texto `NPC AI · buscar`). Se ele não aparecer, obtenha o log de inicialização antes de testar outros mods.
5. Aproxime-se de um pedestre vivo a até 3 m; toque no botão de conversa e depois em **Teclado Android**. Digite no editor, toque **Usar texto**, depois **Enviar** no chat.
6. Teste acentos/colagem/cancelamento. Se o NPC desaparecer ou sair do alcance durante a edição, o texto pode ser descartado para evitar enviar para outra entidade.
7. Só após esse teste, reintroduza os outros mods um por vez para identificar a interação que causa o crash.

Captura para diagnosticar o botão ausente:

```sh
adb logcat -c
adb logcat -v threadtime > npcai-inicializacao.txt
```

Com a captura em execução, abra o GTA e carregue um save; depois encerre a captura com Ctrl+C. Procure `NPC AI 1.1.0`, `etapa 1`, `etapa 2`, `etapa 3`, erros JNI e `ImGui/RW`. Não filtre exclusivamente a tag NPC_AI durante a coleta: mensagens do loader sobre dependências também são necessárias.

## Limites da validação

A ponte Java/DEX e o `.so` são compilados, mas não foram executados em Android neste ambiente. O teste de layout usa substituto para a ponte JNI: não valida a IME real. A compatibilidade com AML 1.4, a disponibilidade do Activity durante a inicialização e a interação com os mods do usuário ainda precisam ser confirmadas no dispositivo. Não há correção comprovada da causa do crash anterior.

## Resultados desta compilação

- NDK r27d, ARM64, API26: build aprovado. ELF e exports AML verificados, alinhamento 16 KiB.
- Java ECJ e DEX D8: compilados com sucesso; DEX de 4916 bytes embutido.
- Testes host: **46 aprovados** (8 nativos, 5 UI/touch, 33 Python/HTTP/compatibilidade).
- Plugin: 1986896 bytes, SHA-256 `974bb446bec5bb34a8324414bfcba73d4832029170a4e58e3d55cb3783a453c1`.
- Logs em `docs/validation/1.1.0/`. Não houve teste da janela IME em aparelho.
