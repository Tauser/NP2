# Piloto de navegação Home ↔ Preferências

Estado: build P4 validado em software; flash e ensaio de bancada pendentes.
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

O flash é manual pelo responsável, conforme instrução desta tarefa. Nenhuma
gravação na placa foi feita neste worktree.
