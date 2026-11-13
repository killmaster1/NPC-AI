# NPC AI 2.4.0 — ElevenLabs, emoções e economia

GTA San Andreas Android **2.10 ARM64**, AML >=1.4, Android >=8/API26. Evolução da versão 2.3.0, sem substituir seleção, balões, editor, ações, rendição, percepção, expectativas ou memória. **Conforme a solicitação mais recente, o padrão do exemplo é Eleven v3** (`eleven_v3`). Flash v2.5 (`eleven_flash_v2_5`) também está implementado, com parâmetros próprios e **sem tags**. Kokoro FP32 continua como alternativa offline, escolhida explicitamente.

## Instalação e atualização

1. Feche completamente o GTA e faça backup do plugin e do INI atual.
2. Extraia `NPC_AI_2.4.0_Install_ARM64.zip` na pasta do GTA que contém `mods` e `files`. O plugin fica em `mods/libnpcai.so`. O caminho `Android_unprotected/data/com.rockstargames.gtasa/files/npcai.ini` informado pelo AML é válido; não precisa mover o jogo ou modificar o APK.
3. O pacote fornece `files/npcai.ini.example`, **sem credenciais**. Não sobrescreve o INI em uso. Preserve o bloco Gemini do seu arquivo (incluindo seu modelo e sua opção de armazenamento). Substitua o bloco de voz antigo por **um único** bloco de voz do exemplo, acrescente as opções ElevenLabs/personalidade e remova as entradas antigas repetidas. Não simplesmente cole um segundo bloco no final.
4. Se preferir usar o exemplo inteiro, transfira conscientemente as configurações privadas do Gemini. O exemplo seleciona Gemini explicitamente; sem credencial válida ele informa erro, sem mudar para Mock. Para testar diálogo gratuito local, configure `gemini_enabled=0`. A ElevenLabs permanece online e pode cobrar mesmo com diálogo Mock.
5. Abra **Diagnóstico → Voz e personalidade**. Importe a chave ElevenLabs pelo botão **Importar chave (Keystore)**. O editor usa a Activity atual, campo de senha e teclado Android; Voltar cancela. A chave não vai ao chat, ao Gemini ou aos logs. Depois use **Atualizar status** e **Consultar vozes da conta**. Essa consulta lista até 100 vozes da conta e não sintetiza áudio. A disponibilidade de síntese ainda depende das permissões/plano de cada voz.
6. Use **Testar voz (cache ou créditos ElevenLabs)** somente quando desejar gerar a fala. Abrir o menu não consulta o TTS automaticamente. Não existe chave ou Voice ID fictício pronto no pacote.

O modelo Kokoro não é necessário para ElevenLabs. Para usar offline, selecione Kokoro e mantenha/instale o pacote `NPC_AI_Kokoro_PTBR_FP32.zip` em `files/npcai-tts/models/kokoro-pt-br/`. As bibliotecas Android opcionais ficam em `mods/npcai-tts/lib/` e são copiadas com verificação SHA-256 para a pasta privada AML antes do preload. Os pesos existentes da versão 2.3.0 são compatíveis, sem download adicional obrigatório. A release 2.4.0 fornece o mesmo pacote como asset opcional.

## Fluxo de voz

Gemini retorna fala, emoção, intensidade, delivery e ação em **uma requisição**. A fala natural é publicada no chat. A worker de voz escolhe a identidade/Voice ID, consulta o cache, reserva o orçamento local e envia um único POST para:

`https://api.elevenlabs.io/v1/text-to-speech/{voice_id}?output_format=pcm_24000`

A credencial segue exclusivamente no header `xi-api-key`. O transporte Java usa HttpsURLConnection, validação TLS/hostname padrão, redirects desativados, deadlines e limites de corpo. Retorna PCM mono **16-bit little-endian, 24 kHz**, decodificado em C++ e convertido para os frames estéreo esperados pelo AudioTrack. MP3/WAV não são tratados como PCM: formatos/marcas incompatíveis são rejeitados. A inferência/rede nunca acontece no tick do GTA.

No v3, apenas o parâmetro suportado e conservador `stability` é enviado (0.5 normalmente, 0 para intensidade alta). O texto recebe uma tag da lista controlada, como `[whispers]`, `[giggles]`, `[sarcastically]`, `[tense]`, `[surprised]` ou a tag emocional correspondente. O campo opcional `delivery` aceita somente `normal`, `whisper`, `giggle`, `sarcastic` e `tense`. Tags são sugestões ao provedor, dependem da voz e **não foram validadas com uma conta real nesta entrega**. Nunca são comandos da engine e não aparecem no balão.

No Flash, enviamos texto limpo, `stability` moderada, `similarity_boost=0.75` e `speed` entre 0.7 e 1.2. **Nenhuma tag v3** é enviada. O v3 não recebe speed/similarity/style não verificados; o controle de velocidade fica indisponível no menu nesse modo. No Kokoro, seguem os controles offline existentes.

O cancelamento usa identidade de sessão, token de conversa, época de trabalho e serial de publicação. Mensagem repetida por falha de interface não gera outra síntese. Um trabalho ativo e um pendente substituível são permitidos. Trocar NPC, fechar, pausar ou invalidar a entidade cancela playback e descarta respostas antigas. Os backends só são acessados/substituídos pela worker; cancelamento externo altera flags, evitando corrida de ponteiros ao trocar motores.

Áudio tem atenuação pela distância até CJ e pan derivado da projeção da câmera, com centro quando não há projeção válida. É aproximação estéreo, não HRTF/oclusão acústica. Gestos seguem início, progresso e fim reais do AudioTrack, respeitando missão, combate, fuga e rendição. RMS é amplitude aproximada por bloco, não lip-sync fonético.

## Voz por identidade

`NpcVoiceManager` separa modelo GTA, identidade geracional de sessão e identificador estável verificado (quando existir). A ponte atual **não identifica personagens nomeados de maneira estável**, então não promete reconhecer Sweet/Ryder entre sessões. A interface para isso existe, mas não é preenchida com informações inventadas.

- `elevenlabs_voice_ids=`: lista local, separada por vírgula, de Voice IDs que o usuário confirmou na própria conta; validação sintática não prova acesso ao plano. Alternativamente, consulte a API pelo menu.
- `elevenlabs_model_voices=`: pares `105:VOICE_ID_REAL,280:OUTRO_ID_REAL`. Esses nomes ilustrativos devem ser substituídos por IDs reais, nunca copiados literalmente. Dois pedestres de modelo 105 continuam sendo indivíduos diferentes, mesmo se o usuário lhes atribuir o mesmo timbre.
- `elevenlabs_character_voices=`: pares de identificador realmente verificado e Voice ID; reservado para futuras integrações seguras.
- Sem mapeamento, distribuição determinística pelo ID da sessão e menor uso no conjunto de identidades vivas. Até haver mais NPCs que vozes, evita repetição entre os perfis atribuídos. Com menos vozes, reutilização é inevitável.
- A atribuição fica fixada enquanto a identidade existir, inclusive se trocar de roupa/modelo. A invalidação remove o perfil. Não restaura uma identidade efêmera antiga por modelo ou ponteiro reciclado. Troca manual de motor é uma escolha explícita de outro sistema de voz.

## Caracteres, cache e orçamento

O prompt pede respostas normalmente entre 100 e `elevenlabs_target_chars` (150), menores para perguntas simples, até `elevenlabs_max_chars` (200). O resumo local escolhe apenas frases completas até o teto e o balão usa o mesmo texto. Quando uma frase única ultrapassa todo o teto e não pode ser reduzida sem mutilação, preservamos o texto/contexto e **não pedimos áudio**. Não há chamada Gemini extra para resumir. JSON, erros técnicos e texto vazio não viram solicitações de voz.

A chave de cache é SHA-256 sobre texto enviado (incluindo tags), Voice ID, modelo, idioma, parâmetros e versão da normalização. PCM tem magic/version e SHA-256; arquivos corrompidos são descartados. A escrita usa temporário + fsync + rename, em `getNoBackupFilesDir()/npcai-elevenlabs/speech/audio-cache/` (ou fallback privado AML se o serviço não fornecer pasta), com limite de MB, LRU por acesso e no máximo 512 entradas. O cache RAM é limitado a 4 MiB quando a persistência está desativada. Desativar persistência não apaga arquivos já existentes: use **Limpar cache**. Não há chave nem texto de conversa nos nomes dos arquivos; PCM continua sendo conteúdo privado audível e pode ser apagado.

- `elevenlabs_credit_per_char`: tarifa **estimada**, padrão conservador 1; ajuste ao seu plano/modelo. Tags e pontuação entram na contagem local de caracteres Unicode enviados/tentados. As regras reais podem ser diferentes.
- `elevenlabs_credit_limit_hour=250`, `..._day=1000`: tetos locais, 0 desativa cada teto. A janela é hora/dia UTC. **250 por hora permite poucas falas de 150 caracteres**; aumente conscientemente se desejar mais testes.
- Reserva e gravação do contador acontecem antes da tentativa de rede. Erros ambíguos não são reembolsados no contador local. Isso é conservador, não auditoria de cobrança. Cache hit não consome reserva.
- Aviso a partir de 80% e bloqueio antes de exceder. Contador persistente com schema e checksum resiste a reinício normal; arquivo corrompido bloqueia chamadas, em vez de zerar o saldo silenciosamente. Não é proteção contra adulteração manual do relógio/arquivos. Sem armazenamento privado, limites ativos impedem chamadas pagas.
- Sem retries automáticos de síntese, inclusive 401/402/403/429/503. Uma tentativa pode ter consumido créditos no servidor mesmo se o cliente recebeu timeout. O menu exibe caracteres/tentativas, estimativa hora/dia, cache hits, chamadas evitadas e último HTTP, separados das métricas Gemini.

## Personalidade, emoções e linguagem

Traços de sociabilidade, paciência, coragem, agressividade, curiosidade, humor, desconfiança, empatia, gírias e propensão a palavrões são estáveis por identidade de sessão. São enviados como resumo compacto, junto de relação, fatos e perplexidade relevantes. Não se atribui conhecimento de todo o mapa ao NPC.

A relação começa neutra. Cumprimento e pergunta estranha não são tratados como ameaça. Evidência verbal explícita pode aumentar irritação/medo; pedidos de desculpa/gratidão e tempo permitem recuperação gradual, preservando um piso ligado a agressões/ameaças marcantes. A análise verbal local é heurística, não prova de dano físico. O flag de mira próxima não prova que CJ mirou nesse NPC. Não implementamos uma nova atribuição de autoria de dano da engine sem hook seguro: os estados de missão/combate existentes continuam tendo prioridade.

O prompt desencoraja insultos e hostilidade sem motivo. `npc_aggression_multiplier` modula escalada local, sem editar agressividade real da engine. `npc_profanity_level` é limite de intensidade 0..3, não obrigação de xingar; padrão 1. Correções locais leves suavizam termos fortes e removem uma abertura repetida em várias respostas. Não substituem toda a fala por resposta genérica nem garantem uma compreensão semântica perfeita de toda gíria.

Os estados incluem neutro, feliz, animado, curioso, surpreso, desconfiado, assustado, triste, irritado, furioso, sarcástico, envergonhado e impressionado. Surpresa diante de eventos estranhos não significa medo obrigatório. O modelo sugere emoção e ação; o C++ limita valores, permite somente delivery/ações conhecidos e continua validando o contexto da engine.

`npc_personality_persistent=1` conserva resumos de traços na memória existente. Para NPCs aleatórios, a memória individual permanece limitada à identidade segura da sessão; não é restaurada em outro pedestre por usar a mesma skin. Memória global/narrativa e separação entre saves continuam existentes. Não se recriam tarefas físicas após reinício.

## Configurações e credenciais

Todos os parâmetros do INI de exemplo são lidos pelo parser. Booleanos são 0/1; volume 0..1; velocidade 0.6..1.5 (Flash efetivamente 0.7..1.2); alvo/teto 1..200; timeout ElevenLabs 1.000..60.000 ms; cache 1..500 MiB; intensidade 0..1; agressividade 0..2; palavrões 0..3. Tarifas/limites negativos e valores não finitos são rejeitados. Duplicatas conflitantes TTS/ElevenLabs bloqueiam somente a voz. Números usam ponto decimal, independentemente da locale Android.

Keystore ElevenLabs é separado do Gemini: alias `npcai.elevenlabs.v1`, chave AES-GCM e arquivo próprio em `getNoBackupFilesDir()/npcai-elevenlabs/`. A proteção é a infraestrutura Android; hardware-backed depende do aparelho. O acesso por mods maliciosos no mesmo processo não é isolado pelo mod. Se escolher `elevenlabs_key_storage=ini`, a chave será texto puro por escolha explícita. Para importar do INI uma vez, use `..._key_action=replace`, confirme status, apague o valor e volte para `none`.

O botão de remoção limpa a credencial ativa/protegida e cria um marcador para impedir reimportação silenciosa do INI no reinício; nova importação explícita libera o uso. Se usava texto puro, apague também a linha privada do INI. O botão de importação usa Keystore; mantenha `elevenlabs_key_storage=keystore` no INI para reutilizá-lo na próxima execução.

Dados TTS ficam no aparelho, exceto o texto final/tags enviados diretamente à ElevenLabs. O provedor aplica suas próprias regras de retenção; não ativamos suposto zero-retention restrito a planos Enterprise. O contexto completo do jogo não é enviado à ElevenLabs. Nenhuma chave, contador pessoal, cache, log privado ou memória de jogador acompanha os ZIPs/GitHub.

## Diagnóstico e validação

A UI de chat/GameBridge é inicializada independentemente do provedor. O erro antigo `native_mock` com Gemini habilitado é coberto pelos testes de seleção/configuração; não há fallback silencioso quando Gemini está explicitamente selecionado. O caso confirmado de 2.3.0 era o INI com dois blocos Piper/Kokoro conflitantes. O parser agora explica a duplicação, sem imprimir valores secretos.

Testes host e JVM usam mocks sem credenciais. Compilação r27d/ELF ARM64 não provam que ElevenLabs, tags, Keystore ou playback funcionaram em um Android real. Veja `ANDROID_ELEVENLABS_TESTS.md` e o relatório de validação da release. Diagnóstico mostra erro sem ADB; opcionalmente use `adb logcat | grep -iE 'NPC_AI|AndroidRuntime|AudioTrack'`, sem compartilhar chaves/histórico privado.

## Desinstalação

Feche o GTA e remova `mods/libnpcai.so`; remova apenas as bibliotecas próprias `mods/npcai-tts/lib/` se não quiser Kokoro. Modelos locais e memórias podem ser preservados. Antes de remover o plugin, use o menu para apagar credencial/cache se desejar. Guarde seu INI privado com segurança; não apague bibliotecas do GTA ou de outros mods.
