# Migração Settings — fase 2: Tela e som

Data: 2026-09-28. Branch: `codex/settings-migration`.

## Implementação

- Novos arquivos completos: `firmware/main/ui/screens/settings/np_settings_display_sound.c/.h`.
- Construção dos sliders, valores, bubbles e switch noturno extraída da
  implementação real de `np_settings.c`. A Settings antiga continua compilando
  e criando esses mesmos controles pelas funções compartilhadas; seus controles
  não foram removidos. O contrato de handles foi agrupado em
  `np_display_sound_controls_t` dentro de `np_settings_view_t`.
- `product_ui.c` conserva os callbacks de liberação/perda de toque e os métodos
  `device_control_set_brightness`, `device_control_set_volume` e
  `device_control_set_night_mode`. O preview de áudio do volume foi preservado.
  A sincronização pela projeção real também é compartilhada, inclusive estados
  do modo noturno, espera de hora confiável e falha de aplicação de brilho.
- Serviços, AppState, coordenador de flash e persistência não foram alterados.
  A política noturna existente continua 22:00–06:00, limite de 15%, sem aumentar
  brilho abaixo desse limite e sem alterar volume.
- Novo arquivo registrado no CMake principal. `np_screens.h` inclui o contrato
  compartilhado. `np_tokens.h` e a fonte bitmap Material de 24 px recebem
  `dark_mode` U+E51C. Comparação de codepoints: somente essa adição, nenhuma
  remoção. Não há desenho manual de ícones novos.

## Layout e navegação

Header compartilhado, painel Dark Graphite em 1024x600. Título e subtítulo,
brilho e volume à esquerda e modo noturno à direita. Sem Tema e sem novas
configurações. Os sliders mantêm geometria/estilo da implementação anterior.

Preferências → Tela e som abre a cena nova. Voltar → Preferências.
As outras quatro linhas mantêm a Settings antiga como fallback. Engrenagem
mantém Perfil; drawer mantém Home e Preferências. Home não foi redesenhada.

Construção sob demanda: somente a cena solicitada é construída, após liberar
a árvore anterior na task LVGL. Aproximadamente 60 objetos incluindo header
e drawer; contagem estimada pelo código, sem medição de heap em hardware.
Não há criação antecipada de futuros módulos ou de modais antigos nesta cena.

O timer do bubble é removido antes de destruir a cena. Os handles são zerados
e o guard de instalação dos callbacks é reiniciado para a próxima construção;
o mesmo guard impede instalar bindings duplicados na árvore ativa.

## Feedback

Os callbacks locais conservam o bubble e não chamam
`np_feedback_show_osd`. Toasts de falha e resultado da persistência continuam.
O componente/API de OSD global permanece disponível para produtores externos;
esta fase não introduz novo produtor externo de brilho ou volume.

## Validação

Build inicial real `idf.py build` passou com ESP-IDF 5.5.4, target esp32p4,
incluindo a nova tela e a Settings antiga. Validação final e hash abaixo.

Build limpa em diretório novo: `idf.py -B build/phase2-clean build`, passou.
Imagem final `0x2ad190` bytes; slot de 8 MiB com 67% livres. Sem warnings de
compilação no log. Configuração efetiva: `CONFIG_IDF_TARGET="esp32p4"`, RGB565,
LVGL 16 bits, Task WDT de 5 s e auto-suspend de flash desabilitado.
O map registra `s_ui` com `0xe40` (3648 bytes), aumento estático de 120 bytes
em relação aos 3528 bytes da fase anterior. Heap LVGL continua não medido.
Comparação textual da função dos sliders, normalizando somente o nome e
quebras de linha, confirmou implementação idêntica à anterior.
Fonte de 24 px mantém line_height 26 e baseline 2.

`idf.py build` final também passou no diretório habitual. Binário para flash
manual: `firmware/build/np2_p4.bin`, descritor `8e2670b-dirty`, SHA-256
`ABD95B5EC551C30493214BFDB90C83FF295CFAB7EC071281BBDE98CF8042D9ED`.
O descritor identifica o pai mais as alterações compiladas antes do commit.

Flash manual pelo operador. Esta entrega não comprova estabilidade física.
Conferir na placa:

1. Abrir/voltar repetidamente entre Preferências e Tela e som; navegar também
   para Perfil/Home e fallback antigo, sem crash ou callback duplicado.
2. Brilho e volume mostram os valores reais ao entrar. Ajustar nos extremos
   e em posições intermediárias; soltar/perder toque mantém o fluxo atual.
3. Bubble local aparece e desaparece; nenhum OSD global é emitido pelo slider.
   Volume mantém preview; resultado de persistência continua informado.
4. Sair durante o bubble e reabrir: nenhum timer acessa a árvore anterior.
5. Alternar modo noturno e confirmar a projeção e persistência após reboot.
   Testar horário confiável/ausente e período ativo, observando o limite real.
6. Verificar textos, glyphs, alvos de toque e estabilidade durante persistência.
   Medir heap e high-water da task LVGL antes/depois de ciclos de navegação.

WDT, pipeline de display, C6, partições e schema offline foram preservados.
