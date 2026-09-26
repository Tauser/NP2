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
alive. Wi-Fi uses `WIFI_STORAGE_RAM` in the driver. Provisioning submits a
station request through its one-entry private mailbox; that worker owns
association, forget and retry and never exposes a password to LVGL or logs.

The `CredentialVault` retains a confirmed WPA2 credential only after 30
seconds of continuous IP, through the sole flash owner. A standard production
build requires NVS Encryption and active Flash Encryption. The normal
development build of this repository retains laboratory credentials locally
across reboots (`NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=ON`); do not use
production credentials there. A production pipeline must pass
`-DNP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=OFF` and use the protected
profile. The production security profile and the Phase 3 continuity protocol
in `../docs/G3-CONTINUITY-VALIDATION.md` still require hardware validation.

`CONFIG_SPIRAM_XIP_FROM_PSRAM=y` remains a qualified flash/render variant,
not a production baseline. Hardware results must be captured in
`../docs/BRINGUP-EVIDENCE.md`.
