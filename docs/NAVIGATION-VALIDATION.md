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

## Integração no projeto normal e WDT prolongado

Em 2026-09-30, o checkout limpo `D:\Projetos\NP2` recebeu por fast-forward os
commits do piloto. O build nesse diretório passou para `esp32p4`/ESP-IDF 5.5.4;
o primeiro flash encontrou COM8 ocupada pelo monitor, e a repetição gravou
2.850.640 bytes com hash verificado. O SHA-256 dessa imagem é
`3411d2e49953c6bcb35cc689b31f5406475bf9fea2f96a052c6d68d61ed8a31f`.
O C6 não foi alterado. A captura curta após o flash teve bytes de boot e rede,
mas perdeu a linha do SHA do ELF; não é ensaio de estabilidade.

O usuário informou WDT de `IDLE0` com task `lvgl` após 1.155,227 s de uso do
piloto anterior. Ele apenas abriu o monitor do checkout normal, sem novo flash
antes desse evento; portanto os símbolos impressos pelo monitor usaram o ELF
errado. O PC isolado não identifica a causa. A configuração efetiva tinha
`CONFIG_FREERTOS_HZ=100` e `task_min_delay_ms=1`, que convertia a espera mínima
do worker em zero ticks. A ADR-048 muda o mínimo para um tick e o máximo para
dois; build, flash e ensaio prolongado dessa alteração ainda são necessários.

O commit `352956f` passou em build limpo P4 com
`idf.py -B build/validation-352956f build` no projeto normal. O build padrão
foi atualizado e `idf.py -p COM8 app-flash` gravou 2.850.640 bytes com hash
verificado. SHA-256 da imagem P4:
`e4973d9d0ed00ab5bcc01de63b6696adea0c7a6c3b19ea09dad74b1f551d77b3`.
Configuração efetiva: `CONFIG_FREERTOS_HZ=100`, timeout do worker LVGL entre
um e dois ticks (10–20 ms), sujeito a despertar antecipado por notificação.
Em captura curta, `handler_generation` foi 4666
aos 37,306 s e 5055 aos 40,406 s, cerca de 125 passagens/s, contra milhares
por segundo antes do ajuste. Home → Perfil → Preferências concluiu fases em
ordem, sem WDT na janela de 50 s. O teste de duração superior a 19 minutos e
o gate de 100 ciclos continuam pendentes; placa/BOM e hash C6 devem ser
registrados antes de fechar o gate físico.

## Quadro preservado em todas as rotas

A ADR-049 suspende invalidação e refresh no `LEAVE` e só os reativa após
`ENTER` ou falha de `BUILD`. Assim o painel mantém o último quadro durante
`CLEAN_WAIT_NEXT_PASS`, independentemente do link escolhido. O tratamento
especial de Perfil → Home/Preferências foi removido. A validação exige build
limpo, flash no P4, captura do boot, navegação visual por múltiplas rotas e
repetição prolongada para observar o WDT; esses resultados ainda serão
registrados após o teste na placa.

O usuário compilou e gravou pelo VS Code a versão `b69b03f` no projeto normal
`D:\Projetos\NP2` e relatou navegação por Home, Perfil, Preferências e outras
telas sem piscada branca, travamento ou reinício. A confirmação visual cobre
o comportamento observado; não há captura serial nem contagem de 100 ciclos
desse flash. O WDT após uso prolongado continua em aberto.

Durante a verificação do build, `dependencies.lock` ainda apontava o adaptador
LVGL para o worktree antigo. O commit `c1b4caf` mudou essa resolução para o
componente local do projeto normal. Um `idf.py reconfigure` no build padrão
confirmou o caminho `D:\Projetos\NP2\firmware\components\espressif__esp_lvgl_adapter`.
O build padrão e o build separado em `build/validation-selfcontained-b69b03f`
passaram após a correção; o segundo também listou o adaptador no checkout
normal. A imagem resultante ocupa `0x2b8010` bytes numa partição de
`0x800000` bytes, com 66% livres. Esta alteração do lock não mudou o código
fonte compilado; a gravação relatada pelo usuário permanece a evidência
visual da correção de navegação.

## Migração completa do ciclo de navegação

A ADR-050 remove a rota compilada de construção assíncrona que ainda atendia
as telas fora de Home e Preferências. Perfil, Tela e som, Wi-Fi, Fuso,
Notificações e Sistema agora são construídos diretamente no `BUILD` do
Navigation Manager. A limpeza do `content_host` ocorre em toda transição,
evitando reter uma página piloto escondida após entrar em configurações. O
resultado exige build, flash e validação manual de todas as rotas antes de
encerrar o gate de navegação.

O build limpo `idf.py -B build/validation-full-navigation build` passou para
`esp32p4` com ESP-IDF 5.5.4. A imagem usa `0x2b8010` bytes em uma partição de
`0x800000` bytes, com 66% livres. Ainda falta gravar esta revisão no P4 e
percorrer visualmente Home, Perfil, Preferências, Tela e som, Wi-Fi, Fuso,
Notificações e Sistema, incluindo os retornos por drawer e botão Voltar.

O commit `9cba15a` foi integrado ao checkout normal `D:\Projetos\NP2`,
compilado e gravado no P4 pela COM8 com `idf.py -p COM8 app-flash`. O esptool
verificou o hash de gravação. A imagem P4 tem 2.849.856 bytes e SHA-256
`801b9cec12ccfffbe3348eabcbda5a4f2c0832914bc4d90d2f27104353770e5b`.
Uma captura de 35 s após o flash registrou `Boot transition complete; Home V2
visible`, sem WDT, panic ou assert nessa janela. A inspeção manual de todas as
rotas e o ensaio prolongado continuam pendentes.
