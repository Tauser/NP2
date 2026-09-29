# Migração Settings — tela Notificações

Data: 2026-09-28. Branch: `codex/settings-migration`.

## Arquivos completos

`firmware/main/ui/screens/settings/np_settings_notifications.c/.h` já existiam
com o modal real e foram ampliados para a cena. A construção das três linhas
e switches foi compartilhada; o modal conserva posições e dimensões anteriores.
O CMake já compila esses arquivos. `product_ui.c` integra a navegação e usa
uma única função de instalação dos bindings, com guard para cada árvore ativa.
Os testes em `tools/notification_preferences_host_test.c` e
`tools/run_notification_preferences_host_test.ps1` exercitam o serviço real.

## Fluxo preservado

`notification_service` → snapshot do `app_loop` → `app_state`/projection →
`product_ui` → widgets. UI visual não conhece serviço ou armazenamento.
Os callbacks de switches e Testar som em `product_ui` foram comparados com o
commit anterior e mantêm implementação idêntica. Os eventos continuam usando
os três setters reais; áudio continua por `notification_service_request_alert_sound`.

Serviço, AppState, coordenador de flash e schema não foram modificados.
`notification_service` conserva o worker, janela existente de 500 ms,
poll de 200 ms, coalescing e reconhecimento da sequência gravada. Alterações
durante uma gravação em voo continuam pendentes para o próximo perfil.
FlashCoordinator continua proprietário da escrita: perfil de três booleans,
CRC, duas gerações NVS, commit e releitura; a restauração é aplicada pelo
serviço quando o armazenamento está pronto.

## Opções e navegação

Header compartilhado, painel Dark Graphite e somente:

- Notificações gerais.
- Som das notificações.
- Alertas do sistema.
- Testar som, usando o backend real e o volume geral existente.

Não existem banner on/off, modo noturno ou categorias adicionais nesta cena.
O serviço conserva a recusa do teste quando notificações/som estão desabilitados.
O callback mantém o feedback existente de sucesso/falha.

Preferências → Notificações → Voltar → Preferências. Sistema e Tela e som
mantêm seus destinos migrados; Wi-Fi e Fuso continuam no fallback antigo.
O modal Notificações da Settings antiga permanece funcional.

Salvamento é automático ao alterar, sem botão Salvar nem timer de UI novo.
Um label usa ready, persistence_pending, persisted_generation e last_result
reais para indicar carregamento, salvamento, persistência confirmada ou erro.
Controles ficam desabilitados até a restauração do serviço estar pronta.
Na entrada os valores são sincronizados antes de mostrar a raiz.

Construção lazy pela navegação. Como nas cenas Perfil/Preferências/Tela e som,
esta árvore é liberada ao sair e seus handles e guard de callbacks são zerados.
Sistema continua sendo o único cache de cena aprovado na ADR-043. Não foram
adicionados timers, modais antecipados, operações lentas ou ícones desenhados.

## Validação host

`tools/run_notification_preferences_host_test.ps1` passou com GCC C11,
`-Wall -Wextra -Werror`. O teste inclui o `notification_service.c` real; somente
FreeRTOS, coordenador/armazenamento e áudio usam shims. Foram verificados:

- Recusa de alterações antes da restauração.
- As oito combinações dos switches, submissão de um perfil final após várias
  alterações, reconhecimento da gravação e restauração em reboot simulado.
- Mudança durante gravação em voo: não há submissão concorrente; o perfil
  novo permanece pendente e é enviado depois do reconhecimento do anterior.
- Falha de submissão e de commit: não há falsa promoção de geração persistida;
  reinicialização restaura o último perfil reconhecido.
- Gate do teste de som pelo serviço e enfileiramento para o backend de áudio.

Esse teste não executa NVS real, scheduling/tempo de debounce, áudio físico
ou desligamento da placa. Não substitui a validação abaixo.

## Aceite físico pendente — flash manual

1. Alterar os três switches, aguardar o status de persistência confirmada e
   reiniciar pela tela Sistema. Reabrir Notificações e conferir os três valores.
   Repetir com as oito combinações e comparar também com o modal legado.
2. Alternar rapidamente, sair da tela enquanto salva e reabrir após o término.
   O último perfil deve prevalecer e continuar correto após reboot.
3. Testar som com gerais/som ativados, desativados e volume zero/intermediário;
   conferir recusa e feedback existentes e ausência de áudio no callback LVGL.
4. Navegar repetidamente para Home/Perfil/Preferências/Sistema e retornar.
   Medir heap/high-water, verificar callbacks únicos e ausência de Task WDT.
5. Conferir feedback de erro de armazenamento sem indicação falsa de salvamento.

WDT, pipeline de display, C6, partições e schema offline foram preservados.

## Build real

Build limpa `idf.py -D IDF_TARGET=esp32p4 -B build/notifications-clean build`
passou com ESP-IDF 5.5.4, sem warnings de compilação. Imagem de `0x2adf60`
bytes, 67% livres no slot de 8 MiB. O map registra `s_ui` com `0xf58`
(3928 bytes), aumento estático de 144 bytes frente à fase anterior.
Heap LVGL e high-water continuam dependentes da medição física.

Build final habitual `idf.py -D IDF_TARGET=esp32p4 reconfigure build` passou.
Binário para flash manual: `firmware/build/np2_p4.bin`, descritor
`35da39d-dirty`, SHA-256
`11523A26FEF0F5AFC2F74229D946A76F070458463C6D5137CF8F2003574EFDFF`.
O descritor identifica o pai mais as alterações compiladas antes do commit.
Nenhum flash foi executado; o reboot em NVS real continua pendente.
