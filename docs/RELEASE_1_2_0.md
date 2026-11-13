# NPC AI 1.2.0 — Gemini e balões de conversa

## Como funciona

Aproxime CJ de um NPC vivo dentro do raio configurado (3 m por padrão). O NPC válido mais próximo recebe um balão **Conversar...**. Toque nele para abrir o editor Android: histórico acima e campo de mensagem com **Enviar** junto do teclado do sistema. Não é necessário voltar ao ImGui para enviar.

Ao enviar, o editor fecha e você volta ao mundo. O balão mostra **NPC está pensando...**; depois mostra a fala do NPC por 15 segundos, num estilo semelhante aos balões do Roblox. Após esse tempo volta a **Conversar...**. Toque para continuar. A resposta completa fica no histórico; o balão mostra uma prévia de até 280 bytes UTF-8 quando o texto é longo. O botão flutuante e o teclado touch continuam como alternativas.

O balão é associado à identidade de sessão do NPC. A posição vem do snapshot validado do pool e é projetada por `CSprite::CalcScreenCoors` do SDK Android. Nenhum CPed é consultado pelo callback do balão. A âncora usa a posição do pedestre + 2,15 m: é uma aproximação de altura acima da cabeça, não acompanhamento de um osso da animação. NPCs sentados ou em veículos podem precisar de ajuste. O balão é ocultado fora da projeção/frustum, mas não há teste de oclusão por paredes. Apenas o candidato mais próximo tem balão, evitando sobreposição de muitos marcadores.

Durante um gesto, uma mudança de identidade cancela o clique. Destruição, streaming e troca de NPC mantêm a invalidação de conversas e de respostas atrasadas. O campo nativo conserva limite de 1200 bytes UTF-8 e recebe composição, acentos e colagem pelo Android.

## Instalar atualização

1. Feche completamente o GTA e pare o servidor anterior com Ctrl+C.
2. Substitua o plugin antigo pelo novo `libs/arm64-v8a/libnpcai.so`, mantendo uma única cópia na pasta mods do AML.
3. No aparelho do crashlog a pasta efetiva é `/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/mods/`. Use a pasta real do seu AML.
4. **Atualize também `server.py` no Termux** com o arquivo `server/server.py` deste ZIP; não basta trocar o `.so` para ter Gemini.
5. Preserve `npcai.ini` e seu token local, se configurado. Não coloque chaves de IA no plugin ou nesse INI.
6. Comece no modo mock para testar os novos balões sem custos. Depois configure o provedor desejado.

Requisitos preservados: GTA SA Android 2.10 ARM64, AML >=1.4, Android >=8/API26. O Java/DEX está embutido no `.so`; não instala APK adicional. Como houve relato de conflito com outros mods, teste primeiro na combinação em que a versão 1.1.0 funcionou e adicione os demais um por vez.

## Configurar Gemini no Termux

O servidor aceita `NPC_AI_MODE=mock`, `openai` ou **`gemini`**. Continua usando somente bibliotecas padrão do Python, sem pacotes pip adicionais.

Na pasta privada onde você atualizou o servidor:

```sh
cd ~/npcai-server
export NPC_AI_MODE=gemini
export GEMINI_MODEL=gemini-2.5-flash-lite
read -r -s -p 'Chave Gemini: ' GEMINI_API_KEY
export GEMINI_API_KEY
printf '\n'
python server.py
```

Use uma chave da Gemini Developer API/Google AI Studio com acesso ao modelo configurado. Modelo e quota dependem da conta; troque `GEMINI_MODEL` se necessário. As chamadas podem consumir quota/créditos conforme o plano. Não envie sua chave no chat ou a coloque nos arquivos públicos. `.env.example` não contém credenciais e não é carregado automaticamente; use as instruções do arquivo caso prefira um `.env` privado.

A integração usa REST `v1beta/models/{modelo}:generateContent`, cabeçalho `x-goog-api-key`, `systemInstruction`, histórico em `contents` com papéis `user`/`model` e extração de `candidates[].content.parts`. Pensamentos não são exibidos como fala. A personalidade e o contexto observado são os mesmos usados no provedor OpenAI. Timeout, limite de resposta, memória independente, autenticação local e tratamento de falhas continuam aplicados. O cliente GTA continua falando apenas com `127.0.0.1:8765`.

Referência consultada: https://ai.google.dev/api/generate-content

Erros HTTP 401/403/429/404/502 devem ser diagnosticados pela chave, permissões, quota, nome do modelo e conectividade; o erro aparece na conversa. O servidor não imprime a chave nem a inclui na URL. Se a API bloquear a resposta ou não retornar texto, informa erro em vez de inventar uma fala.

## Compilar

```sh
./tools/build_keyboard.sh
./tools/test.sh
ANDROID_NDK_HOME=/caminho/android-ndk-r27d ./build.sh
python3 tools/package.py
```

O script Java usa ECJ 3.37.0, stubs Android e D8 8.3.37, remove classes antigas antes de recompilar e gera o DEX embutido. O workflow GitHub Actions também recompila a ponte Java. As dependências do SDK Android continuam verificadas pelo manifesto de hashes.

## Validação e pendências

A versão 1.1.0 foi informada como funcional pelo usuário no aparelho. Isso não valida automaticamente as mudanças desta versão.

A versão 1.2.0 foi compilada em Java/DEX e C++17 com NDK r27d para ARM64/API26. O ELF foi conferido, incluindo exports AML, bibliotecas Android padrão e alinhamento de 16 KiB. Os testes de host verificam contrato REST Gemini com provedor substituído, timeout, erros, histórico, UTF-8 e limites; também verificam o clique no balão e o envio direto em quatro resoluções com a ponte JNI substituída.

**Não houve chamada real ao Gemini nem teste desta versão dentro do GTA.** A projeção no executável do jogo, a aparência/posição do balão sobre cada modelo, o comportamento do teclado com resize da janela e o conflito entre outros mods precisam de teste no aparelho. Logs desta entrega ficam em `docs/validation/1.2.0/`.

Resultado final desta entrega: **59 verificações aprovadas** (8 cenários nativos, 9 de UI/touch/fluxo e 42 testes Python/HTTP/compatibilidade/Gemini). Build ARM64/API26 aprovado. Plugin de 2005040 bytes, SHA-256 `f0b199fdec2c4eb4e8a2f79761c757eb9aece6813c3b5178f71285acac735ba8`.
