# NovaPanel — Guia de recomeço: hardware, bring-up e software

> Consolidação autossuficiente das descobertas de bancada para iniciar um
> firmware novo sem redescobrir os mesmos defeitos. Os estados de evidência
> deste próprio guia distinguem o que foi comprovado em hardware do que foi
> aprovado apenas em software.

## Objetivo e critério de evidência

O conjunto Waveshare ESP32-P4-WIFI6-Touch-LCD-7B com ESP32-C6 por SDIO já
funcionou em placa v1.3 com display rotacionado, atualização dinâmica limpa,
Wi-Fi, NTP e HTTPS. Este guia preserva essa receita, mas não declara release:
glitch residual, recuperação física de rede, tela mais pesada e OTA assinada
ainda exigem validação.

| Rótulo | Significado |
|---|---|
| **Obrigatório** | Evita uma falha já conhecida. |
| **Validado** | Compilado e observado em placa. |
| **Candidato** | Atualização disponível, mas ainda não aprovada em bancada. |
| **Não fazer** | Alternativa que falhou, é incompatível ou traz risco físico. |

## Hardware que o firmware precisa conhecer

| Recurso | Fato | Consequência |
|---|---|---|
| SoC | ESP32-P4 v1.3, flash NOR 32 MB e PSRAM 32 MB | Build `esp32p4`; imagem aceita revisão v1.0. |
| Display | EK79007 MIPI-DSI 2 lanes, 1024×600, RGB565 | DSI lê framebuffer continuamente; banda flash/PSRAM é crítica. |
| Touch | GT911 I2C GPIO8/7, sem IRQ | Polling no adapter LVGL e correção de coordenada. |
| Backlight | GPIO32 | Ligar só após o primeiro frame. |
| Rádio | C6 por SDIO: CLK18, CMD19, D0..D3 14..17, reset54 | P4 não tem Wi-Fi; toda rede depende de ESP-Hosted. |
| RTC | RTC interno P4 e ML1220 recarregável | Nunca usar CR1220; hora inválida é estado explícito. |
| I2C | GT911 e ES8311 compartilham barramento | Usar lock semântico da HAL. |
| SD | SDMMC: CLK43, CMD44, D0..D3 39..42 | Testar em paralelo a Wi-Fi antes de produto. |
| USB | Serial/JTAG e CH343 chegam ao P4 | Flash, monitor e setup local são do P4. |

## Baseline comprovado

| Camada | Versão/configuração | Motivo |
|---|---:|---|
| ESP-IDF | 5.5.4 | SDK usado nas medições e compatível com P4 v1.3. |
| BSP | `waveshare/esp32_p4_wifi6_touch_lcd_7b ==1.0.4` | Painel, touch e pinout conhecidos. |
| LVGL | `lvgl/lvgl ==9.4.0` | Linha validada. |
| Display | `espressif/esp_lvgl_adapter ==0.5.3` | Rotação e anti-tearing no mesmo pipeline. |
| ESP-Hosted | `espressif/esp_hosted ==2.12.11` | P4 host e C6 remoto exercitados. |
| Wi-Fi remoto | `espressif/esp_wifi_remote ==1.6.2` | Wrapper usado com ESP-Hosted. |
| Cache | `joltwallet/littlefs ==1.20.4` | Cache offline versionado e limitado. |

Fixar versões no manifesto e versionar `dependencies.lock`. `build/`,
`sdkconfig` gerado e `managed_components/` não são fonte de verdade.

## Perfil atualizado — validado em software e na placa v1.3

Em 2026-09-06, o perfil abaixo foi resolvido pelo Component Manager, compilado
para `esp32p4` com ESP-IDF 5.5.4 e submetido a `host_check.sh --app --tests`.
O binário mede `0x194940` (80% livre na menor partição de 8 MiB). Isto prova
compatibilidade de manifesto, Kconfig, código e link. Na mesma bancada, o P4
v1.3 iniciou display, touch e PSRAM; o C6 foi migrado de 2.12.9 para 3.0.6 por
Slave OTA via SDIO; o enlace negociou RPC v2 e SW_AGGR; Wi-Fi reassociou,
recebeu IP, NTP sincronizou e HTTPS de clima/mercado concluiu. Isto não torna
o produto uma release: OTA assinada, rollback ensaiado e soak continuam gates
separados.

| Camada | Versão/configuração testada | Mudança necessária |
|---|---:|---|
| BSP | `waveshare/esp32_p4_wifi6_touch_lcd_7b ==3.0.1` | Exige LVGL 9.5.x e passa a depender do adapter, não do `esp_lvgl_port`. |
| LVGL | `lvgl/lvgl ==9.5.0` | Elevado porque o BSP 3.0.1 rejeita LVGL 9.4.x. |
| Display | `espressif/esp_lvgl_adapter ==0.6.4` | Mantém `TRIPLE_PARTIAL`; 0.6.4 corrige a contagem de buffers para rotação 180° nos modos `DOUBLE_FULL`/`DOUBLE_DIRECT`. |
| ESP-Hosted | `espressif/esp_hosted ==3.0.6` | Migração de Kconfig para `CONFIG_ESP_HOSTED=y`. |
| Wi-Fi remoto | `espressif/esp_wifi_remote ==1.6.4` | Compilado pareado com ESP-Hosted 3.0.6 e C6 selecionado. |

O manifesto deve **remover** a dependência direta de `esp_lvgl_port`: ela não é
mais dependência do BSP 3.0.1 e a aplicação continua usando somente
`esp_lvgl_adapter`. A atualização do ESP-Hosted também exige trocar
`CONFIG_ESP_HOSTED_ENABLED=y` por `CONFIG_ESP_HOSTED=y` e
`CONFIG_ESP_HOSTED_ENABLE_ITWT=n` por
`CONFIG_ESP_HOSTED_HOST_FEAT_WIFI_EXT_ITWT=n`. Em uma chamada direta de
`idf.py`, exportar `ESP_IDF_VERSION=5.5`; o `export.bat` normal já faz isso e
permite que o Kconfig do Wi-Fi Remote seja carregado.

### C6 obrigatório: imagem pareada e patch de SDIO

ESP-Hosted 3.x exige que host P4 e co-processador C6 terminem na mesma linha
3.x. A migração segura é **C6 primeiro, P4 depois**. Para o perfil acima, a
imagem de C6 foi compilada com ESP-IDF 5.5.4, `esp_hosted ==3.0.6`, alvo
`esp32c6`, RPC v2, Wi-Fi e SDIO SW_AGGR. Em 2026-09-06 foram transmitidos
1.108.272 B por Slave OTA, a ativação/reinício foram confirmados e o host
renegociou versão 3.0.6, RPC v2 e SW_AGGR com o C6.

O ESP-IDF 5.5.4 original limita o envio SDIO a 4092 bytes, incompatível com
SW_AGGR. Antes de compilar o C6, aplicar o patch oficial do ESP-Hosted, que
troca **somente** a validação abaixo em
`components/esp_driver_sdio/src/sdio_slave.c`:

```c
// antes
SDIO_SLAVE_CHECK(len > 0 && len <= 4092, "length out of range: (0, 4092]", ESP_ERR_INVALID_ARG);
// depois
SDIO_SLAVE_CHECK(len > 0, "len <= 0", ESP_ERR_INVALID_ARG);
```

O utilitário oficial aplica essa mudança de forma idempotente:

```text
python <esp_hosted>/tools/eh.py patch-idf --idf-path <caminho-do-esp-idf-5.5.4>
```

Para a migração por Slave OTA, construir a imagem C6 no projeto
`examples/ota/coprocessor_ota/cp` do ESP-Hosted 3.0.6, executar `idf.py
set-target esp32c6` e `idf.py build`. Confirmar no `sdkconfig` gerado:

```text
CONFIG_ESP_HOSTED=y
CONFIG_ESP_HOSTED_CP=y
CONFIG_ESP_HOSTED_CP_FOR_MCU=y
CONFIG_ESP_HOSTED_CP_RPC_V2=y
CONFIG_ESP_HOSTED_CP_FEAT_WIFI=y
CONFIG_EH_TRANSPORT_CP_SDIO=y
CONFIG_EH_TRANSPORT_CP_SDIO_MODE_SW_AGGR=y
```

O C6 usa flash de 4 MiB; a imagem de aplicação é transmitida ao slot OTA do C6
pelo P4. As USB da placa chegam ao P4, não ao C6. Portanto, nunca enviar uma
imagem C6 diretamente pela porta COM do P4 e não depender de UART externa do
C6: a rota de atualização desta placa é **Slave OTA via SDIO**. Para migrar uma
linha antiga, gravar primeiro no P4, pela USB, o migrador temporário oficial
configurado para `OTA_METHOD_PARTITION`; ele lê a imagem C6 da sua partição
bruta, executa `begin/write/end/activate` pelo SDIO e reinicia o C6. Só após
confirmar esse enlace deve ser gravado o firmware NovaPanel final no P4.
Durante a migração única de C6 2.12.x para ESP-Hosted 3.0.6, desabilitar no
migrador temporário as verificações `OTA_VERSION_CHECK_HOST_SLAVE` e
`OTA_VERSION_CHECK_SLAVEFW_SLAVE`: o exemplo trata 2.12.x como compatível e
não transfere a imagem, embora a migração de linha seja justamente o objetivo.
Isso não é configuração do firmware final.

## Configuração mínima obrigatória

```text
CONFIG_IDF_TARGET="esp32p4"
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=32768
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_DRAW_BUF_ALIGN=64
CONFIG_LV_DRAW_BUF_STRIDE_ALIGN=64
CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3
CONFIG_MBEDTLS_DYNAMIC_BUFFER=y
CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y
CONFIG_ESP_CONSOLE_UART_DEFAULT=n
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
CONFIG_ESP_WIFI_REMOTE_ENABLED=y
CONFIG_ESP_HOSTED=y
CONFIG_ESP_HOST_WIFI_ENABLED=n
CONFIG_ESP_HOSTED_HOST_FEAT_WIFI_EXT_ITWT=n
CONFIG_SLAVE_IDF_TARGET_ESP32C6=y
CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y
CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y
CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_LOW=y
```

`ESP_HOST_WIFI` e `esp_wifi_remote` são mutuamente exclusivos. Habilitar ambos
produziu `net80211: OS adapter function version error`. Também conferir o
`sdkconfig` efetivo: um perfil H2/SPI herdado selecionou GPIO7–12 e impediu o
enlace C6/SDIO correto. Após migrar o C6 para ESP-Hosted 3.0.6, o reset GPIO54
deve ser ativo em nível baixo; ativo em nível alto deixa o cartão SDIO sem
resposta, mesmo com os pinos de dados corretos.

## Receita final do display

1. Criar painel com `bsp_display_new_with_handles()`.
2. Inicializar `esp_lvgl_adapter` e registrar display MIPI.
3. Usar `ESP_LV_ADAPTER_ROTATE_180` com
   `ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL`.
4. Manter três framebuffers por `CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3`.
5. Usar RGB565 e render parcial; o draw buffer de 50 linhas fica em SRAM.
6. Registrar GT911 no mesmo adapter antes da task LVGL.
7. Corrigir touch: `x = width - 1 - x` e `y = height - 1 - y`.
8. Renderizar o primeiro frame antes de ligar o backlight.

Cada framebuffer usa `1024 × 600 × 2 = 1.228.800 B`; três usam cerca de
3,6 MiB de PSRAM. Esta é a troca necessária para impedir que PPA escreva em
framebuffer ainda lido pelo DSI. A receita validada mediu 1 flush/update, p95
de 9,25 ms e espera de flush próxima de 5 µs.

### Não fazer no display

- No baseline 0.5.3, não usar modos `DOUBLE_*` com rotação: resultavam em
  assert LVGL e task watchdog. O adapter 0.6.4 corrige a contagem de buffers
  para `DOUBLE_FULL`/`DOUBLE_DIRECT`, mas esta placa segue em `TRIPLE_PARTIAL`
  até que a bancada aprove qualquer troca de modo.
- Não voltar ao `esp_lvgl_port`: a rotação necessária conflita com o caminho
  anti-tearing desse backend.
- Não tentar corrigir orientação por MADCTL, mirror ou `swap_xy`; o EK79007
  aceita/ignora ou não suporta esses caminhos.
- Não chamar `bsp_display_brightness_init()` após criar o painel: o BSP já o
  chama e a repetição gera o falso alerta LEDC no GPIO32.
- Não usar somente foto como evidência de render; PWM/rolling shutter enganam.

## Flash, memória e concorrência

O DSI fica sem dados em três situações já atribuídas: erase de flash, fetch de
glyph em flash e escrita em framebuffer ainda lido. A/B com erase de 4 KiB a
cada 500 ms produziu tela branca; fonte dinâmica 48 → 16 reduziu artefato; texto
dinâmico rasgava quando o estático não. As regras são: fontes subsetadas, um
label por dígito, invalidação mínima e três framebuffers no adapter.

O flash GD `0xC84019` não suporta suspend/resume neste caminho. Ativar
`CONFIG_SPI_FLASH_AUTO_SUSPEND=y` causou boot loop; não reativar. Num painel
24/7 a mitigação é frequência: NVS deduplicada, cache no máximo uma vez por
domínio a cada 30 min, escrita `tmp + fsync + rename` e nenhuma escrita iniciada
por toque.

| Regra | Valor e motivo |
|---|---|
| HTTPS | Uma conexão global; três handshakes simultâneos esgotaram SRAM. |
| Corpo HTTP | Máximo 48 KiB em SRAM interna; excesso falha, não trunca. |
| TLS e JSON | SRAM interna; PSRAM fica prioritária para display/DSI. |
| Heap interno | Mais de 80 KiB em regime; TLS serializado mediu mínimo 179 KiB. |
| Prioridades | LVGL 4, rede 3, enlace SDIO 3, USB 2. |
| Pilhas iniciais | LVGL 16 KiB, rede 8 KiB, enlace 4 KiB, app loop 8 KiB. |

Não copiar `AppState` inteiro ou view-model grande para a pilha de render.
Esse padrão já causou `Stack protection fault`. Só a `lvgl_task` toca objetos
LVGL; `StateStore`, `EventBus`, `UiDispatcher`, fila de intenção com pelo menos
16 vagas e worker HTTPS único são requisitos de estabilidade.

## Rede, setup e RTC

```text
display → primeiro frame → backlight → enlace P4↔C6 assíncrono
→ Wi-Fi → IP/DHCP → NTP → HTTPS serializada → providers/cache
```

O enlace SDIO pode bloquear cerca de 21 s ao falhar; iniciar em task assíncrona.
O supervisor consome `TRANSPORT_UP`, `TRANSPORT_DOWN` e `TRANSPORT_FAILURE`,
revoga prontidão e aplica backoff 2, 4, 8, 16 e 30 s com jitter. Não reiniciar
display, não recriar `esp_netif` em loop e não fazer tempestade de HTTPS. Após
duas falhas DNS consecutivas, reassociar para renovar DHCP/DNS.

USB Serial/JTAG deve ser o console primário: a porta secundária mostra logs,
mas não entrega `stdin`. Credenciais passam por mailbox privada, nunca por
estado, eventos, view-model ou log. Associar em RAM e gravar NVS somente após
30 s contínuos com IP. Usar hora RTC plausível no boot; sem ela mostrar estado
não sincronizado. NTP vem antes de HTTPS por causa do certificado.

## Partições, rollback e segurança

Reservar desde o primeiro flash: NVS 32 KiB, `nvs_keys` 4 KiB, `phy_init` 4 KiB,
`otadata` 8 KiB, `ota_0` 8 MiB, `ota_1` 8 MiB, `storage` 9 MiB, `coredump` 1 MiB
e `c6_ota` 2 MiB. A imagem C6 ESP-Hosted 3.0.6 mede 1.108.256 B e não cabe em
1 MiB. Reparticionar unidade em campo não é aceitável.

Habilitar rollback do bootloader. Imagem `PENDING_VERIFY` confirma após display,
primeiro frame, serviços e `app_loop` saudáveis por 15 s; sem saúde em 60 s,
marcar inválida e reiniciar. Rede não é pré-requisito de saúde.

OTA ainda não está concluída: faltam cliente com assinatura verificada, três
ciclos reais aplicar/reverter, recuperação ensaiada e autorização humana antes
de eFuses. Secure Boot, encryption e anti-rollback ficam desligados até isso.

## Atualizações para o projeto novo

O perfil atualizado acima passou os gates de software e os gates básicos de
hardware em 2026-09-06. O baseline histórico permanece como
referência de diagnóstico e rollback. Antes de promover o perfil atualizado,
repetir todos os gates abaixo em placa e registrar o firmware exato do C6; uma
mudança de BSP, adapter, LVGL, ESP-Hosted ou IDF invalida as medidas anteriores
de pilha, flush e RAM.

## Roteiro de bring-up e aceite

Registrar em cada passo: data, commit, `sdkconfig` efetivo, versão do C6,
comando, log relevante e resultado. Falha também é evidência.

| Ordem | Teste | Critério |
|---:|---|---|
| 1 | Toolchain/target | IDF correto, target `esp32p4`, build limpo. |
| 2 | Flash/partições | 32 MiB e A/B, storage, coredump, `c6_ota`. |
| 3 | Boot/PSRAM | P4 v1.3 aceita imagem, PSRAM 32 MiB, sem panic/abort. |
| 4 | Display | Primeiro frame ≤2 s, backlight posterior, orientação certa. |
| 5 | DMA/render | 3 FB + draw alinhados a 64, flush/update ≤4, p95 ≤16 ms. |
| 6 | Touch | GT911 alinhado ao vídeo; gesto, campo e teclado confirmados. |
| 7 | Flash | Cache/NVS medidos, dedup/throttle ativos, sem auto-suspend. |
| 8 | C6/SDIO | Enlace assíncrono, perfil C6/SDIO 4-bit e pinos corretos. |
| 9 | Wi-Fi | Setup sem segredo em log, IP/DNS, reboot reassocia. |
| 10 | RTC/NTP/TLS | RTC offline, NTP antes de TLS, heap >80 KiB. |
| 11 | Falhas de rede | AP, DHCP, DNS e C6 down induzidos; UI operável e dado stale. |
| 12 | SD + Wi-Fi | Operação simultânea sem falha ou latência inaceitável. |
| 13 | Soak/térmico | 24 h, ≤70 °C, sem regressão de heap, pilha ou render. |
| 14 | OTA | Três ciclos assinados aplicar/reverter e recuperação ensaiada. |

## Proibições a carregar para o firmware novo

- `static std::function` global e `abort()` em falha de display.
- Múltiplos TLS, UI fazendo request/persistência/hardware ou LVGL fora da task dona.
- Render FULL, `shadow_width` e `transform_*` em widget atualizado.
- `CONFIG_SPI_FLASH_AUTO_SUSPEND` nesta placa.
- `ESP_HOST_WIFI` junto do Wi-Fi remoto.
- Dois framebuffers. Não trocar `TRIPLE_PARTIAL` por modo DOUBLE sem o gate de
  render em bancada, mesmo com a correção do adapter 0.6.4.
- Escrita flash por toque e crescimento de estado/view-model sem orçamento de pilha.
- Declarar defeito gráfico resolvido com base em foto isolada.

## Referências oficiais

No dia de iniciar ou atualizar dependências, confirmar as versões e a
compatibilidade diretamente nas fontes oficiais:

- ESP-IDF 5.5.4: <https://github.com/espressif/esp-idf/releases/tag/v5.5.4>
- Revisão do P4: <https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/chip_revision.html>
- BSP Waveshare: <https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_7b/versions/3.0.1/readme>
- LVGL adapter: <https://components.espressif.com/components/espressif/esp_lvgl_adapter/versions/0.6.4/changelog>
- ESP-Hosted: <https://components.espressif.com/components/espressif/esp_hosted/versions/3.0.6/changelog>
- Wi-Fi Remote: <https://components.espressif.com/components/espressif/esp_wifi_remote/versions/1.6.4/readme>
- Rollback OTA: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html>
- Recursos opcionais de flash: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_flash/spi_flash_optional_feature.html>
