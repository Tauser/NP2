# NP2 — instruções para agentes e colaboradores

## Missão do repositório

NP2 contém o firmware do smart panel premium baseado na Waveshare
ESP32-P4-WIFI6-Touch-LCD-7B. O P4 executa a aplicação e o C6 fornece Wi-Fi por
ESP-Hosted/SDIO. A fonte de verdade de bring-up é
[`docs/RESTART-HARDWARE-BRINGUP.md`](docs/RESTART-HARDWARE-BRINGUP.md); a
direção de arquitetura e os gates estão em
[`docs/PLANO-FIRMWARE-PREMIUM.md`](docs/PLANO-FIRMWARE-PREMIUM.md).

Leia os dois documentos integralmente antes de alterar hardware, build,
partições, display, rede, armazenamento, segurança ou atualização.

## Baseline obrigatório

- ESP-IDF 5.5.4 para P4 e C6, com target explícito (`esp32p4` ou `esp32c6`).
- P4 v1.3, flash NOR de 32 MiB e PSRAM de 32 MiB.
- BSP Waveshare 3.0.1, LVGL 9.5.0 e `esp_lvgl_adapter` 0.6.4.
- ESP-Hosted 3.0.6 nos dois chips, `esp_wifi_remote` 1.6.4, RPC v2 e SDIO
  SW_AGGR. O IDF do C6 requer somente o patch oficial idempotente do
  ESP-Hosted; registre seu diff e hash.
- Display EK79007 RGB565, rotação 180°, três framebuffers e
  `TRIPLE_PARTIAL`. Não troque para um modo DOUBLE sem novo gate físico.
- Reset C6 no GPIO54 ativo em nível baixo; SDIO 4-bit nos pinos validados.

Versões transitivas, hashes, `dependencies.lock`, `sdkconfig.defaults`,
`sdkconfig.c6.defaults` e `partitions.csv` devem ser versionados. Diretórios
`build/`, `managed_components/`, `sdkconfig` gerado, chaves, credenciais,
artefatos de release e logs de dispositivo não entram no Git.

## Arquitetura e concorrência

- O domínio não depende de ESP-IDF, LVGL ou drivers. I/O entra por contratos
  pequenos e resultados tipados.
- `app_loop` é o único escritor de estado. Eventos, filas e buffers têm limite,
  dono e política de saturação explícitos.
- Somente a task LVGL toca objetos LVGL. UI não faz HTTP, persistência ou acesso
  a hardware diretamente.
- Há no máximo um handshake/conexão HTTPS em voo no produto. O executor de
  requests também coordena OTA e qualquer telemetria futura.
- TLS, JSON e corpos HTTP usam SRAM interna e têm tamanho máximo. PSRAM é
  prioritária para display; qualquer exceção deve ser medida e documentada.
- A persistência é serializada pelo coordenador de flash. Nenhuma escrita nasce
  de callback de toque. NVS contém dados pequenos; cache versionado fica no
  filesystem, com CRC, duas gerações e recuperação após perda de energia.

## Restrições de hardware e segurança

- Nunca habilitar `CONFIG_SPI_FLASH_AUTO_SUSPEND` nesta placa: produziu boot
  loop. Escritas/erases podem afetar display e cache; seguir a política de
  `FlashCoordinator` e os gates de render.
- Não habilitar `ESP_HOST_WIFI` junto de `esp_wifi_remote`.
- Não usar `esp_lvgl_port`, render FULL, dois framebuffers, fontes dinâmicas no
  caminho quente, `abort()` para falha de display ou `static std::function`
  global.
- Atualizar C6 somente por Slave OTA via SDIO. A USB ligada ao P4 nunca é rota
  de flash do C6. Tratar atualização conjunta P4/C6 como transação com estados
  intermediários compatíveis.
- Segredos nunca entram em logs, eventos, AppState, view-models, URLs ou dumps
  exportados. Não adicionar chaves de assinatura ou credenciais ao repositório.
- Secure Boot, Flash Encryption e anti-rollback só podem ser ativados após os
  gates de OTA/recovery e autorização humana explícita para eFuses.

## Estrutura prevista

- `firmware/` — aplicação P4 ESP-IDF.
- `coprocessor/` — configuração, patch e recipe reproduzível da imagem C6;
  não duplicar o ESP-Hosted inteiro sem justificativa.
- `docs/` — decisões, procedimentos, evidências e planejamento.
- `tools/` — verificações reproduzíveis sem segredos.
- `.vscode/` — preferências compartilháveis do editor; caminhos locais ficam
  em arquivos ignorados.

## Fluxo de trabalho e validação

1. Antes de alterar dependências ou Kconfig, registre a decisão e atualize os
   defaults/locks correspondentes.
2. Antes de build C6, aplique/valide o patch oficial de SDIO SW_AGGR; não altere
   fontes do IDF manualmente fora de patch rastreado.
3. Faça build limpo para o target afetado. Verifique tamanho, tabela de
   partições, configuração efetiva e warnings relevantes.
4. Alterações em display, flash, C6/SDIO, rede, NTP/TLS, OTA, energia ou
   segurança exigem evidência de bancada com data, placa/BOM, commit, hashes P4
   e C6, configuração efetiva, comandos e logs relevantes.
5. Não declare release, estabilidade gráfica, recuperação ou segurança como
   concluídas a partir de build ou foto isolada. Consulte os gates do plano.

Use commits pequenos e reversíveis. Preserve alterações do usuário. Não faça
reset, limpeza destrutiva ou escrita em eFuses sem pedido explícito.
