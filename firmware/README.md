# NP2 P4 firmware

Read `../AGENTS.md`, `../docs/RESTART-HARDWARE-BRINGUP.md` and
`../docs/PLANO-FIRMWARE-PREMIUM.md` before changing this target.

From an ESP-IDF 5.5.4 environment:

```powershell
idf.py set-target esp32p4
idf.py build
```

The first executable baseline initializes only P4-local PSRAM, EK79007
MIPI-DSI/LVGL, GT911 and the backlight. It deliberately does not initialize
ESP-Hosted, Wi-Fi, storage writes, NVS or OTA. Hardware results must be
captured in `../docs/BRINGUP-EVIDENCE.md`.
