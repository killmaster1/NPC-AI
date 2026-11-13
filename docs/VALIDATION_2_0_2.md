# Validação NPC AI 2.0.2

## Resultados reais

- NDK r27d 27.3.13750724, C++17/API26, arm64-v8a: compilação concluída, exit 0.
- ELF64 AArch64 DYN, exports AML e LOAD de 16 KiB conferidos. NEEDED: libc.so, libdl.so, liblog.so, libm.so.
- DEX Java atualizado por ECJ 3.37.0/D8 8.3.37 e embutido na biblioteca. Há avisos do SDK e um aviso Java de depreciação de URL(String), sem erros finais.
- 368 arquivos vendorizados verificados pelo manifesto.
- **193 testes/cenários de host aprovados:** configuração/diagnóstico 23; inteligência 22; núcleo 13; animações 20; ações 18; UI 12; unittest Python/HTTP/JVM 85. Os dois testes JVM/TLS já estão incluídos nos 85.

O novo cenário cobre credenciais fictícias com ponto, com/sem aspas simples/duplas, passagem ao bootstrap e ausência da chave no relatório. Mantém a rejeição de espaços internos, controles, barras e aspas desencontradas. O teste JVM usa o validador Java de produção para verificar ponto permitido e controles/aspas/limites rejeitados.

O INI anexado pelo usuário foi lido pelo parser C++ corrigido sem gravar uma cópia de sua credencial ou fazer chamada à API. Resultado: INI encontrado e válido, Gemini ativado, credencial sintaticamente presente, nenhum bloqueio de configuração. Esse teste específico foi uma verificação adicional, não está contado nos 193 cenários da suíte.

Binário: 3093304 bytes; SHA-256 `c722d3522e2dbddb489360bbc66a49d11784c0553a54d7d169a589f79265d197`.
DEX: 33164 bytes; SHA-256 `40e79b793b1f8ea53fbd07c159f602474ce7d2b50ef9d1ffe485f5feab0063ab`.

## Limites

Não houve execução desta versão em aparelho nem chamada autenticada ao Gemini. A aceitação do formato não comprova validade da credencial, disponibilidade do modelo ou autenticação real. O relatório recebido confirma apenas o estado da 2.0.1 no dispositivo; a 2.0.2 precisa ser instalada e testada por seu novo menu.

A causa específica de rejeição agora está demonstrada: a chave anexada contém ponto, não permitido pelos validadores anteriores. O HTTP 0 é compatível com a rejeição anterior à rede. Não foi necessário alterar a permissão INTERNET, o APK nem o caminho do INI.

Não houve alteração de GameBridge, tarefas/animações, memória, percepção ou protocolo de ações nesta correção. As regressões aprovadas usam engine simulada e não substituem teste físico no GTA. O workflow foi atualizado, mas não foi executado remotamente no GitHub nesta sessão.

## Arquivos e evidências

Mudanças funcionais em settings.cpp (aspas e ponto) e NativeServices.java (ponto); testes em test_configuration.cpp e NativeServicesJvmTest.java; DEX regenerado. Versão atualizada em main.cpp/diagnostics.h, empacotamento/workflow, INI de exemplo e documentação. A lista exata está em [source-changes.txt](validation/2.0.2/source-changes.txt).

Logs completos, ELF, toolchain e hashes estão em [validation/2.0.2/](validation/2.0.2/). O INI privado enviado pelo usuário não faz parte da distribuição. Instalação e teste sem ADB: [RELEASE_2_0_2.md](RELEASE_2_0_2.md).
