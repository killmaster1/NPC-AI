# Privacidade, credenciais e armazenamento — NPC AI 2.0.0

## O que fica no aparelho

Por padrão, notas persistentes, emoções, resumos, perfis de save e arquivos de recuperação permanecem no diretório privado **no_backup/npcai** do APK. Não há servidor próprio, telemetria, envio automático do banco completo ou resumo produzido por Gemini. O diretório no_backup evita o backup automático normal do Android; limpeza/desinstalação do aplicativo pode apagar tudo.

`npc_memory_send_context=0` é o padrão. Ao conversar com Gemini, são enviados a mensagem atual, um histórico curto da conversa, traços narrativos, estado emocional e contexto atual relevante. Ao habilitar `npc_memory_send_context=1`, resumos persistentes limitados também podem entrar no pedido. Isso não envia o arquivo inteiro. Mock não faz nenhuma chamada de API. Comentários espontâneos de API são opt-in, com orçamento, e desativados em economy.

As notas individuais e globais são separadas. Uma nota individual de um pedestre aleatório não é atribuída a outro do mesmo modelo. Registros globais são rotulados como contexto narrativo do jogador/save, não como lembrança pessoal do interlocutor.

## Credencial Gemini

- `gemini_key_storage=ini`: armazenamento em texto puro, somente por escolha explícita. Não protege contra apps, ferramentas de backup ou mods capazes de ler o arquivo. Nunca compartilhe o INI preenchido.
- `gemini_key_storage=keystore`: recomendado. A implementação usa AES-GCM e chave Android Keystore. A credencial cifrada fica no diretório privado; alias e formato não contêm a chave. A importação usa o valor temporário do INI, que o usuário deve apagar após confirmação. O plugin não modifica silenciosamente seu arquivo de configuração.
- `gemini_key_action=replace`: importa/substitui. `remove`: tenta excluir alias/arquivos e bloqueia o uso da credencial nessa inicialização. `none`: usa a modalidade configurada. Uma falha de operação não autoriza fallback inseguro.
- JVM valida o formato criptográfico, roundtrip e detecção de adulteração; isso **não valida o provedor Android Keystore ou hardware do aparelho**. Esse teste permanece no roteiro real.

A proteção é em repouso e sob o isolamento do UID Android. Outro mod dentro do mesmo processo, um dispositivo comprometido ou código executado sob o mesmo UID não ficam magicamente impedidos de acessar memória/APIs da aplicação. Strings de chave necessárias à conexão podem existir transitoriamente em memória; não se promete apagamento perfeito de objetos Java imutáveis.

Não há chave embutida no APK/.so distribuído nem nos exemplos. Não há chave em URL, logcat, texto de exceção exibido ou prompt. Erros são convertidos em categorias fixas; corpos de erro do provedor não são impressos nos logs.

## Rede

A URL nativa é restrita ao endpoint HTTPS oficial Google. Modelos são nomes curtos validados, não URLs arbitrárias. TLS/hostname usam verificações normais da plataforma, sem trust-all. Redirects não são seguidos com a credencial. Timeouts, limites de resposta, cancelamento e retries são limitados.

A permissão INTERNET pertence ao APK host. O código verifica a permissão e pode falhar claramente; o plugin não concede permissões inexistentes no manifesto. Mock continua disponível. O transporte legado de loopback é opcional e usa apenas 127.0.0.1; Python/OpenAI continuam disponíveis somente nesse caminho de compatibilidade.

## Integridade, retenção e remoção

O checksum dos JSON detecta corrupção acidental; não é assinatura de autenticidade contra um mod que possa editar os arquivos. Há esquema versionado, backup, gravação atômica, controle de espaço, limite de notas e descarte de registros antigos. Os arquivos de memória não são criptografados: o isolamento é o diretório privado do aplicativo. A credencial é tratada separadamente.

A gravação periódica é de melhor esforço. Um encerramento abrupto pode perder mudanças desde a última gravação (intervalo padrão 10 s); nenhuma API garante callback antes de kill do processo. Pausas Android, fechamento de conversa e eventos de save solicitam flush adicional fora da UI/engine.

Use `npc_memory_clear=1` para limpar o perfil e depois volte a 0. Use a opção remove para a credencial protegida e apague qualquer texto restante no INI. Para todos os dados, a limpeza de dados do APK também apaga conteúdo do próprio GTA; faça isso somente se desejar esse efeito amplo. Não há exportação para nuvem habilitada.

Os logs contêm apenas estados/capacidades/métricas, sem diálogo, token ou chave. Ao enviar diagnóstico, revise também logs de outros mods/serviços: o NPC AI não controla o que eles registram.
