# NP2

Firmware do smart panel premium baseado na Waveshare
ESP32-P4-WIFI6-Touch-LCD-7B. O ESP32-P4 executa a aplicação; o ESP32-C6 opera
como coprocessador Wi-Fi por ESP-Hosted sobre SDIO.

O projeto ainda está na fase de base reproduzível. O firmware mínimo criado
nesta etapa só prova a ferramenta e o target no host: ele não inicializa LCD,
touch, PSRAM, C6, Wi-Fi ou persistência.

## Referências do projeto

- [Bring-up e restrições de hardware](docs/RESTART-HARDWARE-BRINGUP.md)
- [Plano de arquitetura e gates](docs/PLANO-FIRMWARE-PREMIUM.md)
- [Configuração de desenvolvimento](docs/DEVELOPMENT.md)
- [Instruções para colaboradores e agentes](AGENTS.md)

## Estrutura

- `firmware/`: aplicação ESP-IDF do P4.
- `coprocessor/`: configuração e receita da imagem C6; o fonte do C6 vem do
  exemplo oficial fixado de ESP-Hosted, não de uma cópia local não rastreada.
- `docs/`: decisões, procedimentos e evidências de bancada.
- `tools/`: verificações reproduzíveis sem segredos.

O baseline é ESP-IDF 5.5.4, BSP Waveshare 3.0.1, LVGL 9.5.0,
`esp_lvgl_adapter` 0.6.4, ESP-Hosted 3.0.6 e `esp_wifi_remote` 1.6.4. A
configuração efetiva e os artefatos de P4 e C6 precisam ser registrados em
cada evidência de bancada.
