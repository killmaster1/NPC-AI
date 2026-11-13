# Validação NPC AI 2.0.1

## Resultados efetivamente obtidos

| Verificação | Resultado |
|---|---|
| C++ | Compilação concluída, Android NDK r27d 27.3.13750724, C++17, API26, arm64-v8a |
| ELF | ELF64 AArch64 DYN, exports AML conferidos, LOAD alinhado a 16 KiB |
| NEEDED | libc.so, libdl.so, liblog.so, libm.so; bibliotecas Android padrão aceitas |
| Dependências | 368 arquivos vendorizados verificados pelo manifesto |
| Java/DEX | ECJ 3.37.0 e D8 8.3.37; editor e serviços combinados no DEX embutido |
| Configuração/diagnóstico novos | 22 cenários de host aprovados |
| Inteligência nativa existente | 22 cenários aprovados |
| Núcleo nativo | 13 cenários aprovados |
| Animações persistentes | 20 cenários aprovados com engine simulada |
| Ações/chat | 18 cenários aprovados com engine simulada |
| UI/touch/IME | 12 cenários aprovados no host, incluindo duas regressões novas |
| Python/HTTP/JVM | 85 testes unittest aprovados, incluindo JVM/HTTPS/TLS |

Total: **192 testes/cenários no host** (107 C++ + 85 unittest). Os testes JVM estão incluídos nos 85; não são contados novamente. A suíte final completa terminou com código 0. O workflow foi atualizado, mas não executado remotamente no GitHub nesta sessão.

Binário: 3092776 bytes; SHA-256 `357c30c1709d032e826c1abadbca7d3b30f9433bd74966e304c3bdabacd77821`.
DEX: 33080 bytes; SHA-256 `584cc24b7174224ce66231778e3c870e8b7064f9300592f9e138854d0df2a6e6`.

Há avisos de headers/qualificadores do SDK no build C++; não houve erro final. A compilação Java emitiu um aviso de depreciação de URL(String) no JDK utilizado, sem erro.

## O que foi demonstrado

- INI ausente/vazio, Gemini/Mock explícitos, espaços, BOM, CRLF, comentários de linha e inline, duplicatas, valores inválidos, limite de linha/arquivo, codificação não suportada e chave de até 256 bytes.
- Recuperação independente por parâmetro, limitação de memória do parser, fallback restrito entre caminhos do loader e prioridade do INI primário. O próprio exemplo distribuído foi lido e validado.
- Gemini selecionado pelo estado realmente carregado; provedor indisponível/chave ausente geram erro, sem resposta Mock silenciosa. A falha do estágio opcional Java/JNI é contida pelo helper usado na inicialização.
- Teste Gemini assíncrono com payload curto (menos de 512 bytes, até 32 tokens de saída), sem contexto de conversa; sucesso, erro, exclusão de teste concorrente e encerramento cancelável.
- Relatório exportado por worker sem chave, token, valor inválido, corpo HTTP ou exceção arbitrária. Último HTTP e códigos de serviço conhecidos são preservados.
- ImGui com GameBridge simulado e Gemini em erro: balão clicável, IME/envio, erro separado da fala, diagnóstico acessível mesmo sem NPC. Não se trata de execução do GameBridge real.
- O teste JVM exercita status do serviço e configure(null) sem propagar exceção nem preservar bytes da credencial de entrada; continuam os testes de TLS padrão, rejeição real de certificado localhost não confiável, limites e AES-GCM. Não é um teste do Android Keystore.
- As regressões de memória, percepção, expectativas, perplexidade, HTTP, worker, animações, rendição persistente e ações continuaram aprovadas. Testes antigos que exigiam exceção global para INI inválido foram atualizados para exigir rejeição do parâmetro e diagnóstico recuperável.

## Limites da comprovação

**Nenhum destes testes foi executado no GTA real por este ambiente.** Não houve aparelho Android, APK host nem credencial real disponíveis. Não houve chamada autenticada ao Gemini, importação efetiva por Android Keystore ou teste de HTTPS/JNI dentro da Activity do GTA.

O usuário informou INTERNET concedida e funcionamento prévio dos balões/Mock na 2.0.0. Isso orienta a correção, mas não valida a 2.0.1. O conteúdo do INI problemático não foi fornecido; a causa específica da ocorrência no aparelho não pode ser atribuída a uma linha sem evidência. Os defeitos demonstráveis são detalhados em [RELEASE_2_0_1.md](RELEASE_2_0_1.md).

Precisam de aparelho: carregamento AML/DEX/JNI, caminho retornado pelo loader real, botão e projeção em NPC elegível, IME/Voltar/controles, teste Gemini com modelo/chave da conta, relatório no diretório externo e regressões físicas/memória/FPS. Nenhum offset, tarefa ou ID de animação foi alterado nesta correção.

Os limites de identidade da 2.0.0 permanecem: pedestres aleatórios não recuperam memória individual por simples coincidência de modelo. Memória narrativa por save e o esquema privado não foram reescritos nesta atualização.

## Arquivos modificados

| Grupo | Arquivos |
|---|---|
| Configuração | settings.h/cpp, config/npcai.ini |
| Bootstrap independente | native/main.cpp, provider_startup.h, native_keyboard.cpp |
| Diagnóstico novo | diagnostics.h/cpp, ui.h/cpp |
| HTTPS/JNI | android_services.h/cpp, NativeServices.java, gemini_client.h/cpp |
| DEX/build/distribuição | native/generated/editor_dex.h, Android.mk, tools/test.sh, tools/package.py, workflow |
| Testes | test_configuration.cpp novo; test_native.cpp, test_actions.cpp, test_animation.cpp, test_intelligence.cpp, test_ui.cpp, NativeServicesJvmTest.java |
| Documentação | README.md, RELEASE_2_0_1.md, VALIDATION_2_0_1.md, SDK_AUDIT.md e evidências |

A lista exata comparada ao ZIP da 2.0.0 está em [source-changes.txt](validation/2.0.1/source-changes.txt). Os módulos físicos do GameBridge/ações/animações, memória e mundo foram preservados; mudou o encadeamento de inicialização e a configuração que lhes é fornecida.

## Evidências e reprodução

```sh
NPC_AI_JAVA_TOOLS=/caminho/java-tools ./tools/build_keyboard.sh
./tools/test.sh
ANDROID_NDK_HOME=/caminho/android-ndk-r27d ./build.sh
python3 tools/verify_elf.py libs/arm64-v8a/libnpcai.so
python3 tools/package.py
```

Logs completos, ELF, toolchain, hashes de fontes e binário estão em [validation/2.0.1/](validation/2.0.1/). São ferramentas de desenvolvimento, não exigências de instalação no Android.
