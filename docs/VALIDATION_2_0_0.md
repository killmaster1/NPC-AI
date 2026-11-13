# Validação NPC AI 2.0.0

## Resultados reais desta entrega

| Verificação | Resultado |
|---|---|
| Compilação C++ | NDK r27d / 27.3.13750724, C++17, API26, arm64-v8a, concluída |
| ELF | ELF64 AArch64 DYN, exports AML presentes, LOAD alinhado a 16 KiB |
| Dependências compartilhadas | libc.so, libdl.so, liblog.so, libm.so; bibliotecas Android padrão |
| Dependências vendorizadas | 368 arquivos conferidos pelo manifesto |
| Java/DEX | ECJ 3.37.0 + D8 8.3.37, min API26; editor e NativeServices embutidos |
| Inteligência nativa nova | 22 cenários aprovados |
| Núcleo nativo existente | 13 cenários aprovados |
| Animações persistentes | 20 cenários aprovados com engine simulada |
| Ações e chat | 18 cenários aprovados com engine simulada |
| ImGui/touch | 10 cenários aprovados no host |
| Python/HTTP/JVM | 85 testes unittest aprovados, incluindo os dois novos testes JVM/TLS |

Total: **168 cenários/testes de host**, sendo 83 cenários C++ e 85 testes unittest. Nenhum foi executado dentro do GTA por este ambiente. Os testes JVM são conduzidos por unittest e já estão incluídos nos 85.

Binário: 3046016 bytes. SHA-256: `4f440b11cc8166d015becc409bb6f6746e7127181df5a20cdbb5eca6e5aa09e3`.
DEX combinado: 32496 bytes. SHA-256: `889eba9d29ab5982ea2bbca9694116956cb7078518a791d5d1adcd3799edd30b`.

O build final registra avisos provenientes do SDK e qualificadores, sem erros. A compilação Java registra um aviso de depreciação de URL(String) no JDK utilizado, sem erro. O workflow foi atualizado para 2.0.0, mas não foi executado remotamente no GitHub nesta sessão.

## Alcance dos testes

O cliente nativo foi exercitado com transporte HTTPS injetável: Mock, contrato Gemini, UTF-8, JSON inválido/texto antigo, ações desconhecidas, validação, retries 429/503, prazo total, TLS sem retry e erros separados da fala.

A classe Java de produção foi compilada e testada em JVM com conexão controlada: URL fixa, TLS padrão, ausência de redirects, limites, cancelamento e mensagens de erro sem segredo. Um teste separado usa conexão TLS real contra servidor localhost com certificado não confiável e confirma sua rejeição pelo JVM. O codec AES-GCM usado pela credencial passou round-trip e rejeitou adulteração. **Isso não testa o provedor Android Keystore nem a pilha HTTPS real do APK.**

As partes portáveis cobrem mudanças de hora, meia-noite normal, saltos ambíguos, presença versus streaming, visão não verificada, contagem distinta, expectativas expiradas/duplicadas, habituação, decaimento, limites emocionais, isolamento, resumos e memória atômica. Há testes de backup/corrupção, esquema futuro, limpeza, perfis de save e Save As, autosave sem escrita redundante e política determinística de 10 segundos. A integração worker usa intervalo menor para não prolongar a suíte.

A restauração individual é testada com uma identidade estável sintética explicitamente verificada. **O GameBridge não fornece identidade estável de personagem nesta entrega:** NPCs aleatórios não recebem memórias individuais de outro pedestre; somente a narrativa global por save pode continuar entre sessões.

Os testes anteriores de animação, touch, balões, seleção e HTTP foram preservados. Eles verificam agendamento e lógica, não a aparência ou duração efetiva das tarefas no executável Android.

## Validação ainda pendente no aparelho

- Carregamento do DEX/JNI no GTA, permissão INTERNET do APK host, HTTPS autenticado com um modelo disponível à conta e erros reais de rede.
- Criação, recuperação, substituição e exclusão de credencial pelo Android Keystore.
- Hooks de save/load, callbacks do mundo, LOS, coordenadas e métricas de FPS no executável 2.10 ARM64.
- IME, controles, balões, duração real de rendição/gestos, interrupções físicas, dinheiro e demais tarefas nativas após a integração.
- Autosave no armazenamento privado, pausa/encerramento forçado, recuperação e separação de saves em condições reais.

Não houve acesso a aparelho Android, APK host, chave de API ou chamada autenticada ao Gemini. A entrega implementa o caminho nativo sem Termux; seu funcionamento ponta a ponta no aparelho ainda precisa do roteiro em [RELEASE_2_0_0.md](RELEASE_2_0_0.md).

## Arquivos principais criados ou modificados

| Grupo | Arquivos |
|---|---|
| HTTPS/JNI/credencial | android/br/com/npcai/NativeServices.java; native/src/npcai/android_services.*; gemini_client.* |
| Percepção e expectativas | world_types.*; world_core.*; world_bridge.* |
| Memória e contexto | memory_store.*; intelligence_runtime.*; autosave_gate.h |
| Integração existente | main.cpp, game_bridge.*, chat.*, actions.*, settings.*, registry.*, native_keyboard.* e controlador de ações, conforme hashes das fontes |
| DEX e build | native/generated/editor_dex.h; Android.mk; tools/build_keyboard.sh; tools/test.sh; tools/package.py; workflow |
| Configuração | config/npcai.ini, parâmetros novos e opções antigas preservadas |
| Testes | tests/test_intelligence.cpp; tests/test_android_services.py; tests/java/ |
| Documentação | README.md; RELEASE_2_0_0.md; ARCHITECTURE_2_0_0.md; PRIVACY_2_0_0.md; SDK_AUDIT.md |

Os scripts Python antigos permanecem para compatibilidade e testes; não são necessários para usar o transporte nativo no Android. O editor Android anterior foi preservado; o DEX combinado inclui os novos serviços.

## Evidências

Logs completos e dados do binário estão em [validation/2.0.0/](validation/2.0.0/): tests-host.txt, build-android.txt, build-java.txt, elf.txt, toolchain.txt, binary.txt e source-sha256.txt. O manifesto permite verificar que as fontes no ZIP correspondem às auditadas para esta entrega.
