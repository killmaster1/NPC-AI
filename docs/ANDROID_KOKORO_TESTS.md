# Roteiro no aparelho — 2.3.0

**Ainda não executado em Android/GTA nesta entrega.** Compilar para ARM64 e sintetizar no Linux não confirmam carregamento pelo AML, qualidade ou FPS no celular. A confirmação anterior do Piper não valida Kokoro.

1. Instale plugin+bibliotecas+pacote FP32, preserve chave e memórias. Confira versão 2.3.0 no diagnóstico. Comece com 2 threads, volume 0.85 e modo Mock explícito.
2. Sem modelo instalado, confirme que Conversar, balão, IME e envio textual funcionam; Testar voz deve mostrar erro separado. Repita após interromper a cópia de tokens/vozes e reinstale o pacote íntegro.
3. Sem rede, teste as três vozes pelo menu. A frase deve sair completa, com acentos e sem ruídos/NaN. Observe tempo da primeira inicialização e de falas seguintes. Testar voz não deve incrementar requisições Gemini.
4. Converse em Mock e depois Gemini. Compare balão com fala, sem JSON, ações ou erros falados. Escute Dora/Alex/Santa; avaliar naturalidade exige audição humana.
5. Troque de NPC durante carregamento, inferência e reprodução. Volte ao anterior. Não deve sair voz atrasada ou duas falas ao mesmo tempo. Identidade reciclada não deve herdar perfil por endereço.
6. Feche o chat, pressione Voltar, pause o Android, troque de aplicativo e retorne. Não retomar fala antiga. Feche o jogo durante uma fala e reabra; texto/IME/controles devem continuar funcionais.
7. Com variação ativada, converse várias vezes com o mesmo NPC e outros próximos; a voz de cada identidade permanece estável. Com variação=0, use cada speaker padrão via INI. Não espere identidade permanente para pedestres aleatórios.
8. Rode a câmera: NPC à esquerda deve favorecer canal esquerdo, à direita o direito. Afaste CJ até 15 m, verifique atenuação e interrupção. Não é áudio posicional 3D/HRTF; fora da projeção o pan fica central.
9. Confirme gestos durante playback real, inclusive entre segmentos, sem cancelar rendição, fuga, missão ou combate. O RMS é aproximado, não sincronização labial fonética.
10. Faça 20 conversas sucessivas, registre FPS e RAM antes/depois, primeira latência e latência aquecida. Compare threads=1/2. Meça separadamente o GTA sem TTS. Não atribua os números Linux ao telefone.
11. Ative descarregamento após 60 s; observe sessão ir a zero e reabrir na próxima fala. Verifique que não há crescimento contínuo de RAM (o alocador pode reter páginas após free).
12. Teste Android 8/API26 e um Android recente ARM64 com AML >=1.4, orientação/resoluções diferentes. Confirme que namespace do linker permite o preload privado e que não existe colisão com outros mods.

Anote: modelo do aparelho, Android/API, RAM, AML, libGTASA 2.10 ARM64, threads, todos os erros do diagnóstico, latências, RSS/PSS e FPS. Não compartilhe chave, tokens, histórico privado ou memórias pessoais.
