# Hardware bring-up evidence

Append one record per physical attempt. Keep unsuccessful attempts: they are
needed to make future recovery deterministic.

## Template

```text
Date/time and operator:
Board revision / display / C6 BOM:
P4 commit and binary SHA-256:
C6 commit and binary SHA-256 (or "not flashed"):
ESP-IDF version and required Hosted SDIO patch verification:
Effective sdkconfig SHA-256:
Serial port and exact command:
Observed serial log (complete):
Observed panel/touch/network behavior:
Power source and measured conditions:
Pass/fail and next action:
```

## Status

The host-only baseline is documented in
[BASELINE-VALIDATION.md](BASELINE-VALIDATION.md). Physical attempts follow.

## 2026-09-07 — P4 local baseline attempt 1 (failed before LVGL registration)

```text
Date/time and operator: 2026-09-07, Codex assisted bring-up
Board revision / display / C6 BOM: ESP32-P4 v1.3; EK79007 MIPI-DSI; ESP32-C6 over SDIO
P4 commit and binary SHA-256: 0d236c4-dirty; F1F489BEF1A9190F0E3845C52929D0354F0990B7964BDBAFFA67CC8D5580CCDE
C6 commit and binary SHA-256: not flashed; pre-existing image reported ESP-Hosted 3.0.6
ESP-IDF version and required Hosted SDIO patch verification: v5.5.4-dirty; the required SW_AGGR patch was audited before this attempt
Effective sdkconfig SHA-256: 2C0F2C4F7C304950059488B32543748421916954019D4BC58149CAE4F58C976D
Serial port and exact command: COM8; idf.py -p COM8 flash, then idf.py -p COM8 monitor
Observed serial log: PSRAM 32 MiB at 200 MHz and memory test passed; P4 revision v1.3; 32 MiB flash; C6 SDIO link negotiated RPC V2, SW_AGGR and Hosted 3.0.6/3.0.6. DSI panel initialized.
Observed panel/touch/network behavior: no LVGL frame or touch validation; code stopped after the EK79007 rejected esp_lcd_panel_disp_on_off() with ESP_ERR_NOT_SUPPORTED.
Power source and measured conditions: USB Serial/JTAG; no thermal or current measurement in this attempt.
Pass/fail and next action: failed as a display baseline. The unsupported disp_on_off call was removed; rebuild, reflash and repeat the monitor gate. C6 was not flashed.
```

## 2026-09-07 — P4 local baseline attempt 2 (stack-protection crash)

```text
Date/time and operator: 2026-09-07, Codex assisted bring-up
Board revision / display / C6 BOM: ESP32-P4 v1.3; EK79007 MIPI-DSI; ESP32-C6 over SDIO
P4 commit and binary SHA-256: 0d236c4-dirty; 35D5395194587F8082C6DBD6844DB6DC0425408C510F0E516010505A18B641E7
C6 commit and binary SHA-256: not flashed; pre-existing image reported ESP-Hosted 3.0.6
ESP-IDF version and required Hosted SDIO patch verification: v5.5.4-dirty; required SW_AGGR patch unchanged
Effective sdkconfig SHA-256: 2C0F2C4F7C304950059488B32543748421916954019D4BC58149CAE4F58C976D
Serial port and exact command: COM8; idf.py -p COM8 flash, then idf.py -p COM8 monitor
Observed serial log: panel, triple-partial bridge and GT911 0x5d initialized; touch registered in polling mode; LVGL task started. The C6 link again negotiated RPC V2, SW_AGGR and Hosted 3.0.6/3.0.6.
Observed panel/touch/network behavior: no stable initial frame. The first synchronous refresh from app_main caused a stack-protection panic in copy_unrendered_area_from_front_to_back() in esp_lvgl_adapter 0.6.4; the device rebooted repeatedly.
Power source and measured conditions: USB Serial/JTAG; no thermal or current measurement in this attempt.
Pass/fail and next action: failed. Increase the explicitly versioned ESP main bootstrap stack from 3584 to 16384 bytes, rebuild, reflash and retest. The C6 was not flashed.
```

## 2026-09-07 — P4 local baseline attempt 3 (serial gate passed)

```text
Date/time and operator: 2026-09-07, Codex assisted bring-up
Board revision / display / C6 BOM: ESP32-P4 v1.3; EK79007 MIPI-DSI; ESP32-C6 over SDIO
P4 commit and binary SHA-256: working tree based on 0d236c4; ADCE2958079CE56D6365B431367EFCC54FEB8CE6D5F4FD1DCE8B475674D48F79
C6 commit and binary SHA-256: not flashed; pre-existing image reported ESP-Hosted 3.0.6
ESP-IDF version and required Hosted SDIO patch verification: v5.5.4-dirty; required SW_AGGR patch unchanged
Effective sdkconfig SHA-256: 9E3F69952675A24BC6C8A6847075292B1D4A9F6168B143D7CF35224B13318225
Serial port and exact command: COM8; idf.py -p COM8 flash, then idf.py -p COM8 monitor
Observed serial log: P4 v1.3, 32 MiB flash and 32 MiB PSRAM at 200 MHz passed startup memory test. EK79007 initialized; GT911 found at 0x5d and registered in polling mode; LVGL task started; backlight set to 60%; app reported RGB565, rotation 180, triple-partial and three framebuffers. No panic or reboot occurred during the capture.
Observed panel/touch/network behavior: serial gate passed. The monitor cannot observe physical pixels or a finger press, so visual quality, touch orientation and sustained stability remain open physical checks. C6 SDIO initialized with CLK18/CMD19/D0..D3=14..17/reset54, RPC V2, SW_AGGR (15872-byte buffers) and matching Hosted 3.0.6 versions.
Power source and measured conditions: USB Serial/JTAG; no thermal or current measurement in this attempt.
Pass/fail and next action: serial portion passed. Perform the documented visual/touch, tearing, flash-write, long-duration and thermal gates before declaring phase 1 complete. The C6 was not flashed.
```

## 2026-09-07 — Physical observation for attempt 3

```text
Source: user observation on the flashed P4 baseline.
Image/backlight/orientation: passed. The expected centered baseline text is visible and the panel is usable in its intended orientation.
Repeated boot: passed. The board repeatedly reaches the baseline screen without the previously observed panic or reset loop.
Not yet testable with this firmware: touch coordinates and orientation feedback, tearing under active updates, memory trend under workload, flash writes during rendering, Wi-Fi service behavior, thermal and long-duration soak.
Gate status: image and boot sub-gates pass; phase 1 remains in progress.
```
