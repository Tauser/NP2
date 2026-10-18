# Preferências > Sistema — recursos e ícones Material

## Entrega e limites

2026-10-10, base Git `a92a6ebc777038ddfac34462d8aad6953be89b15`, árvore com
alterações locais desta entrega (`a92a6eb-dirty` no firmware). Placa de bancada:
Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 revisão 103 (v1.3), flash NOR 32 MiB,
PSRAM detectada de 32 MiB, P4 em COM8. Não houve inspeção física completa da BOM.
C6 não foi gravado; o hash da imagem instalada não foi obtido nesta sessão.
RPC v2 e SDIO SW_AGGR foram observados no boot; isso não substitui um hash C6.

A tela mostra CPU total/por core, histórico limitado a 30 amostras, heap interno
livre/maior bloco, PSRAM, temperatura, tempo ligado e ocupação de LittleFS/SD.
Os percentuais de memória representam heap do alocador, não toda a SRAM física.
Coleta de CPU/memória a cada 2 s; LittleFS a cada 30 s; SD a cada 60 s. Cada I/O
continua no worker dono; app_loop é o escritor da projeção e LVGL só renderiza.
Ausência/falha de leitura é apresentada como indisponível, sem números fictícios.

A linha auxiliar foi removida para ampliar os cards de CPU e memória. Margens
internas de títulos: 20 px; separação entre cards: 16 px. Rodapé com firmware
em linha e botões centralizados de 48 px de altura, mantendo alvo de toque.
Atualizar continua desabilitado como antes; reiniciar conserva confirmação e
serialização pelo coordenador de flash.

Arcos/barras animam mudanças durante 160 ms. Não há loop decorativo; animações
são canceladas em cena oculta/manutenção. O render host e o boot não qualificam
o gate físico de flush, toque, estabilidade gráfica, remoção de SD ou operação
prolongada com animações e rede/armazenamento concorrentes. A capacidade do SD
e as animações visíveis ainda precisam de conferência visual na placa.

## Ícones

Material Symbols Rounded em 24 px, 4 bpp, sem compressão ou fontes dinâmicas.
CPU usa `memory`; RAM/PSRAM, `memory_alt`; armazenamento, `storage`; SD,
`sd_card`; reiniciar, `power_settings_new`. `refresh` já existe na fonte atual.
Cinco novos glyphs entram por fallback do subset Material 24, sem modificar os
bitmaps existentes e sem acrescentar tamanhos que não são usados nessa tela.

Fonte oficial Apache-2.0:
[Google Material Rounded](https://github.com/google/material-design-icons/blob/49d4db35df873165d6bd6ba09b063c7dafbac2f4/variablefont/MaterialSymbolsRounded%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf).
SHA-256 `32e4011709e055596eb8d2dd0f7cb547b47c1d40ac9ecb8fad54f7f7eac202e7`.
Receita: `tools/generate_system_material_icons.py`, fonttools 4.60.1,
lv_font_conv 1.5.3; eixos fixados em FILL=0, GRAD=0, opsz=24 e wght=400.
Somente o C gerado entra no firmware; TTF e ferramentas temporárias ficam no build.

## Validação reproduzível

`tools/run_settings_system_ui_host_test.ps1` compila os fontes reais da tela,
fontes do produto e LVGL 9.5 com `-Wall -Wextra -Werror`. Passaram: cálculo CPU
com rollover de 32 bits, percentuais, glyphs sem placeholders e fallback correto,
histórico limitado, dados ausentes, manutenção, cancelamento de animações,
reinício pendente e 100 ciclos de navegação sem animações remanescentes.
Renderizações 1024×600, normal e sem leituras, foram inspecionadas. Seus valores
são fixtures de teste e não medidas da placa. Artefatos locais:
`firmware/build/system-ui-host-20261010/`.

Build limpo inicial: IDF 5.5.4, target explícito esp32p4, diretório novo
`build/system-resources-20261010`, SDKCONFIG separado. Incrementais posteriores
incluíram a nova fonte e os espaçamentos finais. Sem warnings de compilador.
Imagem final: `0x34f350` = 3.470.160 bytes; slot de 8 MiB, 59% livre.
Tabela de partições igual à baseline. Configuração efetiva: 32 MiB flash,
três framebuffers RGB565, reset C6 GPIO54 ativo baixo, estatísticas FreeRTOS
com ESP Timer e contadores U32; sem flash auto-suspend ou AES DMA. Display
mantém rotação 180° e TRIPLE_PARTIAL. Sem alteração de C6, locks, eFuses ou OTA.

Após `export.ps1` do IDF 5.5.4, a partir de `firmware/`:

```powershell
python C:/esp/v5.5.4/esp-idf/tools/idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/system-resources-20261010/sdkconfig -B build/system-resources-20261010 build
python C:/esp/v5.5.4/esp-idf/tools/idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/system-resources-20261010/sdkconfig -B build/system-resources-20261010 -p COM8 -b 460800 app-flash
```

Logs de build/flash e captura sanitizada de boot ficam no diretório ignorado
`firmware/build/`. A captura serial usa 115200 baud, abre COM8 sem enviar
comandos, e retém somente diagnóstico de boot/recursos; não salva logs brutos
de rede, credenciais ou identificadores privados.

## Hashes SHA-256 da versão final

| Artefato | SHA-256 |
| --- | --- |
| P4 bin | `167250E95557FF924FB5AED5A5089A0A7454504ACD29A90238A43F19F3CA551F` |
| P4 ELF | `C571E147D78773CBC98E3DCE56DFB0CDA492F99B74CB9A30B5CEC5AB0D4A6C32` |
| sdkconfig efetivo | `6E250C3E0E66AC3C86B4CDF4EF85DC05DA1AF899BB4F8180432F1BC864E692F4` |
| partition-table.bin | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| dependencies.lock | `6F605BE935C36425D451A65EC2A700AEF5878D9DAB7EA1000C2721199C552707` |

Flash final em COM8/460800: 3.470.160 bytes em `0x20000`, hash verificado pelo
esptool, reset concluído. Captura pós-reset de 100 s encerrada, COM8 liberada.
ELF no boot `c571e147d...`, correspondente ao artefato acima; ota_0/state=2.
Bring-up local pronto em 3,728 s, PSRAM 33.554.432 bytes, SD montado em 3,708 s,
RPC v2 e SW_AGGR (15.872 bytes nas duas direções). Amostra em 91 s: CPU 3%,
cores 5%/0%, heap interno livre 54.187 bytes, maior bloco 27.648 bytes, PSRAM
livre 15.547.716 bytes. Sem panic/watchdog nas linhas da captura sanitizada.
Não foi capturado um ensaio físico de interação nesta versão final.

Logs locais finais: `system-resources-20261010-spacing-build.log`,
`system-resources-20261010/flash-final.log` e
`system-resources-20261010/boot-final-sanitized.log` dentro de `firmware/build/`.

## Rastreabilidade e conferência — 18/10/2026

As alterações desta entrega foram versionadas no commit `3ed6276`. O descritor
`a92a6eb-dirty` identifica o build histórico gravado em 10/10 e foi preservado
neste registro; não houve recompilação ou nova gravação nesta conferência.

O comando `./tools/run_settings_system_ui_host_test.ps1` foi repetido com
sucesso em 18/10, incluindo 100 ciclos de navegação. SHA-256 de app, ELF,
SDKCONFIG efetivo, tabela binária e lock locais conferem com a tabela acima.
Isso confirma a integridade dos artefatos arquivados e o resultado host; não
constitui novo ensaio na placa.

A amostra de 54.187 B de heap interno livre está abaixo do piso de 80 KiB do
plano. O maior bloco de 27.648 B também deve entrar na análise de admissão
HTTPS e fragmentação. A entrega não aprova a margem de memória da carga atual:
é necessário recuperar margem e medir os picos de TLS, navegação e I/O, além
das pilhas. Interação física, animações, SD e soak continuam pendentes.
