# Migração Settings — tela Sistema

Data: 2026-09-28. Branch: `codex/settings-migration`. Decisão: ADR-043.

## Arquivos completos e reutilização

Os arquivos solicitados `firmware/main/ui/screens/settings/np_settings_system.c/.h`
já existiam com o modal real. Foram ampliados, preservando a API/modal legado.
`product_ui.c` integra a cena, navegação, sincronização e lifecycle.
O registro CMake já existente continua suficiente; `np_settings.c` e seus
controles não foram alterados nesta fase.

O helper visual de labels conserva a geometria antiga no modal e permite linhas
alinhadas na cena. Os handles de firmware, temperatura e reinício continuam
no mesmo contrato `np_settings_system_t`, com sincronização compartilhada.
O callback `system_restart_confirmed` permanece igual: confirmação existente,
request ao coordenador de flash, recusa coerente durante salvamento/manutenção
e refresh da projeção. A confirmação muda somente o pai visual conforme a
origem, modal antigo ou cena. Não foram criados backend, serviço, schema,
operação de flash direta ou request de rede na UI.

## Conteúdo

Header compartilhado, painel de informações e painel Ações em 1024x600.

- Display: 1024x600, RGB565, baseline existente.
- Touch: capacitivo, baseline existente; não há falsa indicação de saúde "OK".
- Firmware: versão real da projeção; Carregando enquanto ausente.
- Temperatura: temperatura interna real do chip, ou indisponível.
- Atualizar sistema: desabilitado, com a indicação existente de manutenção.
- Reiniciar dispositivo: confirmação e backend real preservados.

Uptime não existe na projeção atual e foi omitido. IP e RSSI conectado também
não existem nela; não foram adicionados valores sintéticos. Informações de
rede continuam no fluxo Wi-Fi existente. Ícones novos desta cena reutilizam
glyphs das fontes existentes; nenhum ícone foi desenhado manualmente.

## Navegação e lifecycle

Preferências → Sistema → Voltar → Preferências.
Tela e som permanece na cena migrada; Wi-Fi, Fuso e Notificações conservam
o fallback. A antiga Settings continua com seu modal Sistema funcional.

Sistema tem construção lazy idempotente: raiz não nula impede nova construção.
Na primeira visita são registrados os callbacks de header, Voltar e Reiniciar.
Nas seguintes entradas são reutilizados os handles e atualizada a projeção
antes de mostrar a raiz. Na saída, a raiz é ocultada e o drawer fechado; o
trabalho adiado de confirmação é cancelado e sua árvore descartada.
Existe no máximo uma cena Sistema retida; nenhum modal futuro é antecipado.
O custo é a retenção de heap LVGL após a primeira visita, ainda não medida.

## Verificação e limites

Revisão do fluxo confirma guard de construção e de instalação dos bindings.
O callback de comando de reinício foi preservado. Foram acrescentados logs
de construção com duração e número de objetos. Uma nova entrada compara
a contagem com a primeira construção e registra divergência. A contagem
é feita só na navegação; nenhuma varredura de objetos ocorre a cada frame.

Nenhuma mudança no WDT de 5 s, display, flash, C6 ou schema offline.
Build e revisão de código não comprovam WDT, heap ou estabilidade na placa.
Flash permanece manual. Para aceite físico:

1. Abrir e voltar 100 vezes, incluindo saídas para Perfil/Home e para fallback.
   Deve aparecer uma única mensagem `System scene built`; não deve aparecer
   `System scene object count changed`, crash ou Task Watchdog.
2. Medir heap e high-water da task LVGL antes/depois da primeira abertura e
   após os ciclos. A primeira visita retém a cena; não deve haver crescimento
   contínuo por entrada. Conferir tempo de construção no log.
3. Abrir confirmação, cancelar, reabrir e sair da cena: nenhum diálogo ou
   trabalho adiado deve sobreviver. Voltar com drawer fechado.
4. Confirmar reinício normal e testar recusa durante salvamento/manutenção.
   Cada confirmação aceita deve produzir no máximo um comando.
5. Verificar firmware/temperatura reais, estado de reinício pendente, botão
   de atualização desabilitado e funcionamento do modal legado no fallback.

## Build real

Build limpa em diretório novo passou:
`idf.py -D IDF_TARGET=esp32p4 -B build/system-clean build`, ESP-IDF 5.5.4.
Nenhum warning de compilação no log. Build habitual `idf.py build` também
passou; descritor final atualizado por `idf.py reconfigure build`.
Imagem de `0x2ad980` bytes, 67% livres no slot de 8 MiB; bootloader
`0x5a50`, 60% livres. Partições e configuração efetiva preservadas:
target esp32p4, RGB565/16 bits, WDT 5 s, auto-suspend desabilitado.

O map registra `s_ui` com `0xec8` (3784 bytes), aumento estático de 136 bytes
em relação à fase anterior. O heap retido da cena ainda não foi medido.
Callback de confirmação de reinício comparado com o commit anterior:
implementação idêntica. `git diff --check` passou.

Build final com descritor atualizado passou. Binário para flash manual:
`firmware/build/np2_p4.bin`, descritor `77e819a-dirty`, SHA-256
`8880A6CF2FD7099C4394495E9B936BADA6843B09FCD6FFF852A3E216BCDD2D78`.
Esse descritor representa o pai mais as alterações compiladas antes do commit.
Nenhum flash foi realizado; WDT, contagem em ciclos e heap têm aceite físico
pendente conforme os passos acima.
