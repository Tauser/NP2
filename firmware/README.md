# NP2 P4 firmware

Read `../AGENTS.md`, `../docs/RESTART-HARDWARE-BRINGUP.md` and
`../docs/PLANO-FIRMWARE-PREMIUM.md` before changing this target.

From an ESP-IDF 5.5.4 environment:

```powershell
idf.py set-target esp32p4
idf.py build
```

The image initializes P4-local PSRAM, EK79007 MIPI-DSI/LVGL, GT911, the
backlight and the Phase 2 persistence probes. At the start of Phase 3 it also
runs an asynchronous Hosted/SDIO + Wi-Fi scan and keeps the connection worker
alive. Wi-Fi uses RAM-only storage. A future restricted provisioning channel
can submit a station request through its one-entry private mailbox; that
worker owns association, forget and retry. It does not persist credentials,
perform DNS/NTP/HTTPS, or access LVGL.

The current RAM-only diagnostic deliberately does not reconnect after a P4
reboot. Product credential retention and automatic cold-boot recovery remain
blocked until the secure persistence path and the Phase 3 continuity protocol
in `../docs/G3-CONTINUITY-VALIDATION.md` pass on hardware.

`CONFIG_SPIRAM_XIP_FROM_PSRAM=y` remains a qualified flash/render variant,
not a production baseline. Hardware results must be captured in
`../docs/BRINGUP-EVIDENCE.md`.
