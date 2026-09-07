# Evidência de configuração inicial

**Data:** 2026-09-07
**Escopo:** configuração e build do P4 no host; nenhum binário foi gravado na
placa e nenhuma funcionalidade de hardware foi declarada validada.

## Ambiente

| Item | Resultado |
|---|---|
| ESP-IDF | `v5.5.4-dirty` em `C:\\esp\\v5.5.4\\esp-idf` |
| Toolchain | RISC-V GCC 14.2.0, instalado pelo ambiente ESP-IDF |
| Target | `esp32p4` |
| Component Manager | 24 dependências resolvidas, com lock versionado |
| IDE compartilhada | Recomendações e tarefas VS Code em `.vscode/` |

O sufixo `-dirty` foi auditado. A única modificação local no ESP-IDF é o patch
oficial exigido por ESP-Hosted 3.0.6 para SDIO SW_AGGR:

```diff
- SDIO_SLAVE_CHECK(len > 0 && len <= 4092, "length out of range: (0, 4092]", ESP_ERR_INVALID_ARG);
+ SDIO_SLAVE_CHECK(len > 0, "len <= 0", ESP_ERR_INVALID_ARG);
```

Ele está em `components/esp_driver_sdio/src/sdio_slave.c`. Antes de construir
a imagem C6, repetir a aplicação idempotente com `eh.py patch-idf` e comparar
o diff. Não acrescentar outras alterações ao SDK.

## Resolução de dependências

O [lock do Component Manager](../firmware/dependencies.lock) fixou os itens
diretos solicitados:

| Dependência | Versão resolvida |
|---|---:|
| BSP Waveshare | 3.0.1 |
| LVGL | 9.5.0 |
| esp_lvgl_adapter | 0.6.4 |
| ESP-Hosted | 3.0.6 |
| esp_wifi_remote | 1.6.4 |
| LittleFS | 1.20.4 |

O lock também registra transitivas e hashes. `managed_components/` é cópia
local ignorada; o lock é a fonte versionada da resolução.

## Tabela de partições aprovada pelo gerador

| Partição | Offset | Tamanho |
|---|---:|---:|
| `nvs` | `0x11000` | 32 KiB |
| `nvs_keys` | `0x19000` | 4 KiB |
| `phy_init` | `0x1A000` | 4 KiB |
| `otadata` | `0x1B000` | 8 KiB |
| `ota_0` | `0x20000` | 8 MiB |
| `ota_1` | `0x820000` | 8 MiB |
| `storage` | `0x1020000` | 9 MiB |
| `coredump` | `0x1920000` | 1 MiB |
| `c6_ota` | `0x1A20000` | 2 MiB |
| Reserva não alocada | `0x1C20000`–`0x2000000` | 3,875 MiB |

O primeiro CSV tinha campos de offset vazios sem a coluna correspondente; o
gerador reportou corretamente “Size field can't be empty”. O CSV final foi
aceito pelo gerador do ESP-IDF. Essa correção é de sintaxe, não altera a
arquitetura aprovada de particionamento.

## Resultado do build

`idf.py partition-table && idf.py build` terminou com sucesso para o scaffold
P4. Tamanhos gerados:

| Artefato | Tamanho | Margem |
|---|---:|---:|
| Bootloader | `0x5A50` (23.120 B) | 60% livre antes da tabela em `0x10000` |
| `np2_p4.bin` | `0x6CC80` (445.568 B) | 95% livre no slot OTA de 8 MiB |
| Tabela de partições | 3.072 B | — |

O tamanho do app é apenas referência da ferramenta: dependências foram ligadas,
mas LCD, touch, PSRAM, Hosted, Wi-Fi, NVS, filesystem e OTA ainda não foram
inicializados pelo scaffold. Este resultado fecha somente a parte de build de
G0; os gates físicos e funcionais do plano permanecem abertos.

## Configuração efetiva verificada

O `sdkconfig` gerado confirmou `esp32p4`, flash de 32 MiB, offset de tabela
`0x10000`, PSRAM habilitada, três buffers DPI, rollback de bootloader,
`ESP_HOSTED`, Wi-Fi remoto e reset C6 ativo baixo. Também confirmou que
`ESP_HOST_WIFI` e iTWT externo estão desabilitados. Os defaults versionados são
o contrato de entrada; o `sdkconfig` local é saída ignorada e deve ser
comparado em toda build limpa.

## Próximo gate

Preparar o primeiro firmware de bring-up incremental: inicializar PSRAM,
painel, adapter, três framebuffers, touch e backlight na ordem prescrita em
`RESTART-HARDWARE-BRINGUP.md`. Antes de gravar, registrar a placa/BOM, portas,
hash do binário, `sdkconfig` efetivo e imagem C6 presente. O build atual não é
autorização para flash nem evidência de comportamento na placa.
