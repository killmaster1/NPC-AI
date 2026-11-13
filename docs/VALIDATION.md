# Histórico: validação da versão 1.0

**Os tamanhos e hashes abaixo são da versão 1.0, não da 1.1.0.** Os resultados e limites atuais estão em RELEASE_1_1_0.md e validation/1.1.0/.

# Resultado real da entrega — 11/11/2026

## Compilação

**Aprovada neste ambiente Linux x86_64.** NDK oficial **r27d / 27.3.13750724**, clang do NDK, C++17, Android API 24, ABI `arm64-v8a`. Foi feito build limpo e recompilação da alteração final da UI. As duas operações concluíram com código 0. Não se trata de um ELF de host renomeado.

- Arquivo: `libs/arm64-v8a/libnpcai.so`
- Tamanho: **1.748.160 bytes**
- SHA-256: `5118faa0bab7d806a5866e0139ae9dfc45c7f8b6893e60c221f01a3e1e55777c`
- ELF64, máquina **AArch64**, tipo DYN, Android 24, stripped.
- Exports AML presentes: `OnModLoad`, `OnModUnload`, `__GetModInfo`, `__GetDepsList`.
- Segmentos LOAD alinhados para páginas de **16 KiB**.
- NEEDED: **libc.so, libdl.so, liblog.so, libm.so**. São bibliotecas padrão do Android e são permitidas pelo verificador. A STL está estaticamente vinculada.
- Sem dependência direta de `libGTASA.so` ou `libc++_shared.so`; o SDK resolve símbolos no processo GTA em execução.
- Os headers vendorizados emitem avisos de qualificadores `const` no retorno sem efeito. Não houve erros no build final.

Logs: [build](validation/build-android.txt), [readelf](validation/elf.txt).

## Testes automatizados

**45 verificações aprovadas**: 8 testes nativos, 5 de UI/touch e 32 testes Python/HTTP. Comando: `./tools/test.sh`, código de saída **0**. Logs completos: [tests-host.txt](validation/tests-host.txt).

| Grupo | Resultado | Cobertura |
| --- | --- | --- |
| Núcleo C++17 | 8/8 | UTF-8/JSON; seleção, raio, interior, exclusão do jogador, morte e posição inválida; destruição/reutilização e wrap geracional; reset do mundo; isolamento, limites e limpeza do histórico; respostas atrasadas; limpeza durante geração; shutdown cancelável; limite de respostas |
| UI Dear ImGui real, render sem GPU | 4/4 layouts | 640×360, 1280×720, 1920×1080, 720×1280; janelas e teclado dentro da tela, sem overflow vertical; glifos `ã`/`ç` disponíveis |
| Coordenador touch real do template | 1/1 | Hit test, captura de movimento fora da janela, captura modal, release após fechamento e retorno dos novos gestos ao jogo |
| Python + cliente C++ por sockets reais | 32/32 | Mock, IDs dinâmicos, UTF-8/emoji, autenticação, histórico isolado, contextos, limites, LRU, persistência e reinicialização sob limite de bytes; erro de provedor; concorrência; reset com resposta tardia; servidor offline; timeout e cancelamento; HTTP truncado, tamanho excessivo, UTF-8 inválido, Content-Length duplicado e status de erro; contrato Responses API |

Os testes de seleção usam observações simuladas no registro extraído do GameBridge; não executam CPed nativo. Os testes de UI executam a implementação real do ImGui e a UI do projeto com GameBridge substituído, sem o renderizador do GTA. Os testes de HTTP iniciam um servidor em loopback e executam o cliente C++ real como subprocesso.

A primeira execução de HTTP falhou porque o sandbox negava sockets; a suíte foi executada com a permissão de loopback necessária e passou. Uma verificação adicional de fonte foi corrigida para consultar a fonte dentro de um frame ativo, em vez de acessar estado transitório após `Render`. A compilação inicial também revelou a assinatura de listener do evento de construtor (o SDK entrega somente CPed ao listener); o código foi corrigido e recompilado. Os resultados acima são da versão final corrigida.

Outras verificações concluídas: hashes de **366 arquivos** vendorizados, sintaxe Python/Bash, exports/dependências/alinhamento ELF e inspeção do ZIP final. O workflow GitHub Actions foi criado; **não foi executado remotamente** nesta sessão.

## Sem validação real no dispositivo

**Não houve execução do `.so` no GTA.** Não há aparelho/ADB/GPU Android, APK ou libGTASA comercial neste ambiente. Portanto a entrega implementa a integração real do SDK, mas não pode ser descrita como comprovadamente funcional no executável instalado.

A OpenAI real também não foi chamada: nenhuma credencial estava configurada. O teste da Responses API intercepta a chamada e confere endpoint, payload, modelo, mensagens e extração de texto. O modo mock foi testado de ponta a ponta entre C++ e Python.

## Roteiro obrigatório de teste no GTA

1. Confirmar APK 2.10 ARM64 e AML >=1.4.1; iniciar o servidor mock e observar os logs de inicialização de GameBridge/ImGui.
2. Aproximar CJ de dois peds vivos; confirmar seleção do mais próximo a até 3 m, identificador `p<serial>`, exclusão de CJ e separação dos históricos ao trocar.
3. Caminhar além do raio, trocar interior, matar/remover o ped, recarregar o save e provocar streaming/reutilização de slots; confirmar fechamento imediato e ausência de histórico atribuído ao ped novo.
4. Enviar texto com `Á/ã/ç/ê`, números e pontuação, arrastar histórico e verificar todas as páginas do teclado em diferentes resoluções.
5. Abrir com dedo no joystick/tiro, testar vários dedos, fechar e voltar de segundo plano; confirmar que os toques da UI não movem CJ/não disparam e que os controles voltam ao normal.
6. Desligar o servidor, usar token incorreto, provocar lentidão, trocar NPC/limpar durante resposta e sair do jogo durante geração; conferir erros, cancelamento e ausência de crash.
7. Ativar `openai` com credencial privada e modelo válido; confirmar português, personalidades, contexto observado e memória por NPC. A execução pode consumir créditos da API.
8. Medir FPS, memória e estabilidade com muitos peds, histórico cheio, pausa/resume e outros mods ImGui. Nenhuma medição de FPS no GTA foi feita nesta entrega.

O SDK usa endereços GOT/PLT auditados para 2.10 ARM64 e validações de símbolos. Um APK modificado que mantenha o mesmo número de versão pode não ter o mesmo layout. Cobertura de todos os caminhos de destruição, renderer, controles e acesso ao loopback sob SELinux permanecem pendências concretas do teste no aparelho.
