# Piloto de navegação Home ↔ Preferências

Estado: build, flash P4 e teste curto de navegação concluídos; ensaio de 100 ciclos pendente.
Este documento não fecha gate de estabilidade gráfica ou WDT.

Build de 2026-09-30: ESP-IDF 5.5.4, `IDF_TARGET=esp32p4`,
`idf.py build`, sem warnings ou erros no build final. A imagem
`np2_p4.bin` mede `0x2b7a20` bytes (66% livres na menor partição de app) e
tem SHA-256 `224792CC29B970BE71C3D42525622D6870D08211D840AC05961FEE721AE7E9CD`.
O build usou um cabeçalho local de credencial ignorado pelo Git; seu valor
não integra esta evidência.

## Escopo

- `esp_lvgl_adapter` 0.6.4 local adiciona apenas hooks de início e fim de cada
  passagem de `lv_timer_handler`, sob o lock e na task LVGL.
- Home e Preferências compartilham shell, header e drawer. A página ativa fica
  em `content_host`; `LEAVE`, `CLEAN`, `WAIT_NEXT_PASS`, `BUILD`, `ENTER` são
  fases observáveis. As demais telas conservam o cache lazy anterior.
- `handler_generation` avança a cada passagem do worker e `page_generation`
  avança somente após `ENTER`. Para cada transição, `BUILD` deve registrar
  `handler_generation` diferente da registrada por `CLEAN_WAIT_NEXT_PASS`.
- Os logs `np_navigation` incluem estado, página anterior/destino, heap
  interno livre, maior bloco interno, heap LVGL, contagem de objetos/timers,
  tempo de fase e tempo desde o pedido.

## Ensaio manual após flash P4

1. Registrar data, placa/BOM, commit, SHA-256 da imagem P4 e da imagem C6,
   configuração efetiva, comando de flash e log de boot. O C6 permanece no
   perfil pareado; este piloto não muda sua imagem.
2. Confirmar primeiro frame, rotação/touch, Wi-Fi e Home. Verificar que o
   backlight só acende após o primeiro frame.
3. Alternar Home → Preferências → Home pelo drawer pelo menos 100 vezes.
   Percorrer também Preferências → Wi-Fi, Fuso, Sistema, Notificações, Perfil
   e voltar à Home, repetindo o percurso. Testar toque duplo rápido.
4. Capturar os logs `np_navigation` e `task_wdt`, além de heap interno e maior
   bloco no início, no meio e no fim. Verificar que cada `REQUEST` tem um
   `LEAVE`, `CLEAN_WAIT_NEXT_PASS`, `BUILD` e `ENTER` na ordem, sem gerações
   duplicadas ou timers/objetos crescendo a cada ida e volta entre as duas
   páginas piloto.
5. Não declarar estabilidade a partir do build ou de um único ciclo. Se houver
   WDT, reset, falha de touch ou crescimento persistente, preservar o log e
   voltar ao commit anterior antes de ampliar o piloto.

O responsável autorizou o agente a gravar o P4 na COM8 nesta tarefa. O C6 não
foi gravado.

## Captura serial de 2026-09-30

Após o relato de flash manual, a COM8 mostrou Home aos 20,769 s e WDT da task
`lvgl` a partir de 33,509 s, repetido a cada cinco segundos, com CPU 0 ocupado.
O endereço `0x48046f4e` simboliza `lv_style_get_prop_inlined`. O boot informou
prefixo SHA-256 do ELF `df4413a0`, igual ao descritor da imagem em
`D:\Projetos\NP2\firmware\build\np2_p4.bin`; a imagem deste piloto no
worktree tem prefixo `7267581a`. Portanto, esta captura é da build do checkout
principal e não valida o Navigation Manager. Os logs brutos permaneceram apenas
em `firmware/build/navigation-com8.log`, diretório ignorado pelo Git. Placa/BOM,
hash da imagem C6 e teste físico da imagem piloto continuam pendentes.

Na captura seguinte, a COM8 ficou aberta por 120 s e registrou um novo boot
após o reinício e a reprodução Perfil → Preferências. O boot voltou a informar
ELF `df4413a0`, Home apareceu aos 20,739 s e o WDT da task `lvgl` começou aos
39,269 s, repetindo a cada cinco segundos no mesmo `MEPC=0x48046f4e`.
Não apareceu nenhum evento `np_navigation`. A reprodução confirma a falha da
imagem do checkout principal, mas ainda não exercita o piloto deste worktree.
O log bruto atualizado permanece no mesmo arquivo ignorado pelo Git.

## Captura do piloto em 2026-09-30

O `app-flash` na COM8 foi concluído com verificação de hash. A captura seguinte
mostrou eventos `np_navigation` durante Home → Perfil → Preferências → Tela e som
→ Perfil → Preferências. Na segunda entrada em Preferências, o heap LVGL caiu
para 316 bytes e o WDT acusou a task `lvgl` a partir de 71,435 s. A pilha parou
em `lv_draw_add_task`. O ciclo `CLEAN_WAIT_NEXT_PASS`/`BUILD` usou gerações
distintas; o problema observado é pressão do pool LVGL de 64 KiB com caches
legados ocultos. O log bruto está em `firmware/build/navigation-com8.log`.

Foi adicionada uma evacuação condicional dos caches simples Perfil e Tela e som
antes de construir Preferências quando o heap livre está abaixo de 24 KiB.
Essa correção requer novo build, flash e repetição do percurso na placa.

O build/flash de `da048bf` passou com verificação de hash. O percurso
Perfil → Preferências → Tela e som → Perfil → Preferências passou sem WDT;
o cache de Tela e som foi liberado sob pressão. A continuação por Sistema e
Wi-Fi ainda esgotou o pool: Wi-Fi foi construído com 1688 bytes livres e o
WDT apareceu aos 59,885 s. A política foi ampliada para liberar caches
legados ocultos antes da construção de qualquer página, com reserva maior
para Wi-Fi e Fuso. Esse ajuste ainda requer build, flash e novo ensaio.

O build e `app-flash` de `7535cf4` na COM8 passaram, com hash de flash
verificado. A imagem P4 tem 2.850.512 bytes e SHA-256
`103c3f206d3e4f6192e93ea69bf02801eb9bcbdc5f6880f289aa2b4c6d514792`.
A captura posterior mostrou Home aos 20,706 s, sem WDT, mas não houve toques
durante a janela; as transições corrigidas ainda precisam ser repetidas.

Após o usuário confirmar que a navegação passou, restou uma piscada ao tocar
Preferências a partir de Perfil. A tela Perfil era ocultada no `LEAVE`, antes
do `BUILD` de Preferências na passagem seguinte. O ajuste mantém Perfil visível
até `ENTER` e prioriza a remoção de outros caches se faltar memória.

O build/flash de `52e56e3` na COM8 passou com hash de flash verificado. A imagem
P4 tem 2.850.544 bytes e SHA-256
`d966d0cdda5a6f8c3370eca75543591a72dcff7e232a7a8d09d714bb48466de9`.
Na captura, Home apareceu aos 20,677 s e Perfil → Preferências concluiu
`REQUEST`, `LEAVE`, `CLEAN_WAIT_NEXT_PASS`, `BUILD` e `ENTER` com gerações
distintas, sem WDT. O usuário confirmou visualmente que a piscada desapareceu.
Isto valida o ajuste observado, mas não substitui o ensaio prolongado do piloto.

A mesma piscada foi relatada em Perfil → Home. O commit `97c64f2` manteve
Perfil visível até `ENTER` também nesse destino. Build e `app-flash` na COM8
passaram com hash de flash verificado. A imagem P4 tem 2.850.544 bytes e
SHA-256 `3161c091c027c14e3a87ab0fc61359e25aa28f32fe3603c3b9871a42e3f258af`.
Na captura curta, houve 16 eventos `ENTER` e nenhum WDT; o usuário confirmou
visualmente que a piscada em Perfil → Home desapareceu. O ensaio de 100 ciclos
permanece pendente.
