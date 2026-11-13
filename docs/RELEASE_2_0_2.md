# NPC AI 2.0.2 — credenciais com ponto e aspas opcionais

## Causa confirmada no anexo

O diagnóstico 2.0.1 confirma: INI encontrado, Gemini ativado/selecionado, HTTPS JNI disponível, INTERNET permitida e GameBridge inicializado. A rejeição acontece na linha da chave, antes de qualquer requisição: último HTTP 0.

A credencial anexada contém um ponto (`.`). As validações C++ e Java da 2.0.1 permitiam apenas letras, números, `_` e `-`. Assim, retirar as aspas não resolvia: o ponto continuava sendo rejeitado. O conteúdo da credencial não foi copiado para este documento, exemplo, log ou pacote.

A 2.0.2 aceita ponto nos dois validadores. O parser C++ também remove um par externo correspondente de aspas simples ou duplas. Chaves sem aspas continuam aceitas. O limite é 256 bytes depois dessa remoção; espaços internos, controles, CR/LF, barras e aspas desencontradas continuam rejeitados. Aceitar o formato não comprova que a credencial é válida na API.

O INI anexado foi lido pelo parser C++ corrigido e passou: arquivo válido, Gemini ativado, credencial presente sintaticamente e nenhum bloqueio de configuração. A chave não foi usada para chamada autenticada ao Gemini.

## Instalar sem ADB

1. Feche GTA completamente.
2. Substitua a única cópia de `mods/libnpcai.so` pela biblioteca deste ZIP, em `npcai-android/libs/arm64-v8a/libnpcai.so`.
3. **Mantenha seu INI atual.** Não substitua sua chave pelo exemplo vazio. O parser novo aceita o arquivo anexado com as aspas correspondentes que ele contém; retirar essas aspas também é permitido.
4. Abra GTA e **NPC AI · Diagnóstico**. Confirme a versão **2.0.2**, configuração válida, transporte `android_https_gemini` e credencial presente.
5. Toque em **Testar Gemini**. A partir daqui pode haver um código HTTP real. Se houver outro erro, exporte o novo relatório; ele não contém a chave.

Exemplos apenas de sintaxe, com credencial fictícia:

```ini
gemini_api_key=chave.ficticia_123
```

Também são aceitos `gemini_api_key="chave.ficticia_123"` e `gemini_api_key='chave.ficticia_123'`. Não há expansão de variáveis nem escapes dentro das aspas.

Seu modelo configurado é preservado. Esta correção não confirma a disponibilidade do modelo à sua conta. Se a API retornar erro de modelo/autorização, escolha o nome disponível na sua conta e consulte o novo HTTP/resultado; esse é um estágio posterior ao bloqueio de parser demonstrado aqui.

Mantidas as correções da [2.0.1](RELEASE_2_0_1.md): interface independente do provedor, sem fallback silencioso para Mock, diagnóstico sem ADB, exportação por worker e descoberta restrita do INI. Memória, percepção, ações, rendição, balões e IME não foram reescritos.

## Validação

Consulte [VALIDATION_2_0_2.md](VALIDATION_2_0_2.md). Há compilação ARM64, testes de host/JVM e validação sintática do INI anexado. **Ainda é necessário verificar no aparelho o carregamento da 2.0.2, a presença da credencial no Java e uma resposta autenticada do Gemini.** Não houve teste desta versão no GTA pelo ambiente de desenvolvimento.

Para retornar à versão anterior, feche GTA e restaure a biblioteca guardada. Os arquivos e o esquema de memória não mudaram nesta correção. Para instalação geral/Keystore, use o guia 2.0.1 e as instruções de privacidade da 2.0.0; arquivos de chave em texto puro continuam privados e não devem ser compartilhados.
