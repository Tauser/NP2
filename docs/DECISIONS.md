# Decisões de arquitetura

Este registro materializa decisões que já passaram de proposta para execução.
Uma decisão de arquitetura não substitui o respectivo gate físico.

## ADR-007 — Flash/render: coordenador serial e teste NVS explícito

**Estado:** a primeira gravação pequena durante carga passou em bancada.
Compactação/erase NVS foi reprovada por piscadas repetidas e está bloqueada.

**Contexto:** o flash GD `0xC84019` desta placa não permite auto-suspend neste
caminho; habilitá-lo causou boot loop. Erase de flash, busca de glyph em flash
e uso indevido de framebuffer já produziram tela branca ou tearing. Um mutex
LVGL ou uma prioridade menor não torna uma operação SPI1 segura por si só.

**Decisão:** manter `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`. Toda escrita normal
futura passa por `FlashCoordinator`. O primeiro corte vertical é uma task de
flash com fila de um item, que inicializa NVS sem apagar automaticamente e
executa somente uma gravação diagnóstica pequena, sem segredo, limitada a uma
vez por minuto. O callback LVGL apenas enfileira a intenção; a task LVGL é a
única que apresenta seu resultado. Falha de inicialização ou saturação recusa
a solicitação e preserva o estado existente.

**Consequências:** a escrita NVS pequena durante carga móvel passou, mas erase,
GC, LittleFS, staging C6 e OTA continuam bloqueados. Eles não voltam ao produto
por tentativa de UI; exigem uma alternativa de plataforma medida, como XiP em
PSRAM numa variante isolada, ou mudança de armazenamento/BOM.

**Alternativas rejeitadas neste estágio:** auto-suspend (falha já observada),
escrita direta por callback de toque (viola ownership e dificulta recuperação),
e adiar qualquer coordenação até a Fase 4 (deixaria o risco crítico sem ensaio).
