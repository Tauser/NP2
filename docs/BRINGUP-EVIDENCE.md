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

## 2026-09-07 — Phase 1 scope closure

```text
Decision source: explicit product-owner instruction to finalize phase 1.
Closure basis: reproducible P4 build, successful serial baseline and physical approval of the visible screen/orientation and repeated boot.
Known exclusions transferred to phase 2: touch coordinate/gesture test, render/tearing stress, memory trend, flash contention, thermal and long-duration behavior.
Interpretation: this closes the minimal local P4 boot/display baseline. It does not assert that the exclusions were tested or that the product is production-ready.
```

## 2026-09-07 — Fase 2, incremento 1: diagnóstico de touch (boot confirmado)

```text
Board and port: same P4 v1.3 unit, USB Serial/JTAG COM8.
P4 source state and binary SHA-256: working tree based on 55db602; 7A83969AD8AA6F929C88527C44EFCCF63819972168EF22B6EA41318DEEAE3BA9.
C6: not flashed; existing C6 again reported ESP-Hosted 3.0.6, RPC V2 and SDIO SW_AGGR.
Effective sdkconfig SHA-256: 9E3F69952675A24BC6C8A6847075292B1D4A9F6168B143D7CF35224B13318225.
Commands: idf.py build; idf.py -p COM8 flash; idf.py -p COM8 monitor.
Serial result: build passed; flash blocks verified by esptool; EK79007, GT911 0x5d and LVGL task initialized; backlight set to 60%; no panic during boot capture.
Diagnostic function: five on-screen targets and LVGL input-device events report x/y coordinates and sample count without storage, network or flash I/O.
Open physical result: touch points and rendered target geometry still require direct observation on the panel; do not mark touch orientation or G2 complete from this serial result.
```
## 2026-09-07 — Fase 2, correção de orientação do touch pendente de reteste

- **Placa:** Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 revision v1.3, flash 32 MiB, PSRAM 32 MiB.
- **Firmware observado:** `8339a83` (`Fase 2: adicionar diagnostico de touch`).
- **Procedimento:** foram tocados os cinco alvos do diagnóstico com a tela em rotação de 180°.
- **Resultado observado:** o centro coincidiu; os quatro cantos foram invertidos nos dois eixos: SE acionou ID, IE acionou SD, SD acionou IE e ID acionou SE.
- **Diagnóstico:** o `esp_lvgl_adapter` já mapeia o ponteiro para o display em rotação de 180°. Os espelhos `mirror_x` e `mirror_y` adicionais no GT911 aplicavam uma segunda rotação.
- **Correção proposta:** manter `swap_xy=0`, `mirror_x=0` e `mirror_y=0` no GT911; a próxima gravação e reteste em bancada são necessários antes de aprovar o gate de touch.

## 2026-09-07 — Fase 2, orientação de touch corrigida e validada

```text
Board and port: same P4 v1.3 unit, USB Serial/JTAG COM8.
P4 source and binary: commit 33bba0b; np2_p4.bin SHA-256 7A468DB75C410CCB2F3AE1814DAA05A61D2F32774E39DA81F8F775BF411CF91D.
C6: not flashed; the existing co-processor image was preserved.
Effective sdkconfig SHA-256: 9E3F69952675A24BC6C8A6847075292B1D4A9F6168B143D7CF35224B13318225.
Commands: idf.py build; idf.py -p COM8 flash.
Serial/flash result: P4 image with the corrected GT911 transform was built successfully; 917,696 bytes, 89% free in the 8 MiB OTA slot; all written blocks were hash-verified by esptool.
Physical result: user retested the five targets. Center and every visual corner activated its matching label (SE->SE, SD->SD, IE->IE, ID->ID).
Gate interpretation: coordinate orientation for the five targets passes. Repeatability (20 touches per point), drag path, ghost-touch, latency and input controls remain open in G2.
```

## 2026-09-07 — Fase 2, instrumentação de render e memória (boot confirmado)

```text
Board and port: same P4 v1.3 unit, USB Serial/JTAG COM8.
P4 source: commit cd89030. C6 was not flashed and its existing image was preserved.
Commands: idf.py build; idf.py -p COM8 flash; idf.py -p COM8 monitor.
Build/flash result: np2_p4.bin is 919,904 bytes; 89% remains free in the smallest 8 MiB OTA partition; esptool hash-verified every written block.
Serial result: P4 v1.3 booted the image, found 32 MiB PSRAM, initialized EK79007, GT911 at 0x5d and the LVGL task. The unchanged C6 transport initialized as SDIO 4-bit with reset GPIO54. No panic occurred during the boot capture.
Diagnostic scope: the screen exposes internal/PSRAM free and largest blocks, LVGL task stack high-water mark, render duration, flush-callback duration and maximum flushes per refresh. The optional small moving render load is paused on boot and performs no flash, NVS, filesystem or network operation.
Known measurement limit: flush_cb measures only LVGL's flush callback. It is not a DSI scan-out-complete measurement because the current adapter reports on_frame_buf_complete unavailable.
Open physical result: metric visibility, moving-load fluency, tearing/glitch observation and time-based memory trend are pending.
```

## 2026-09-07 — Fase 2, correção da métrica de flush por ciclo

```text
Physical observation on cd89030: render measured 1 ms and flush_cb 0–1 ms. The displayed lifetime maximum was 12 flushes/cycle.
Interpretation: the 12 corresponds to the first full 1024x600 render split by the fixed 50-line partial draw buffer (600 / 50 = 12). It cannot represent the small moving-load cycle and must not be compared to the <=4 flushes/update load criterion.
Correction: commit 72ad23e records both the most recent refresh count and a peak that is reset for each activated render-load campaign; the measurement begins after load activation. It does not alter display buffers, adapter mode, flash policy or the C6.
Commands: idf.py build; idf.py -p COM8 flash. Result: 920,000-byte P4 image, 89% free in the 8 MiB OTA slot, with every written block hash-verified by esptool.
Open physical result: retest ciclo and pico carga while the moving bar is active; observe visual integrity and memory trend.
```

## 2026-09-07 — Fase 2, carga móvel com métrica isolada

```text
Source: user observation on the P4 running the image from commit 72ad23e.
Observed under active moving load: ciclo=0–1, pico carga=3, flush_cb=0 ms and render=1 ms.
Gate interpretation: the measured peak is within the G2 limit of at most four flushes per update. Render and flush-callback durations are within the provisional limit. A zero cycle is expected during a refresh interval with no invalidated region.
Limit: the user did not report a visual conclusion about tearing, white frames, stalls or resets in this observation; that criterion remains open.
```

## 2026-09-07 — Fase 2, regressão de flush ao tocar o diagnóstico

```text
Source: subsequent user observation while the moving load was active on the P4 image from commit 72ad23e.
Observed result: touching the diagnostic screen raised pico carga to 9 and the value persisted. This fails the <=4 flushes/update criterion for the diagnostic interaction path.
Root cause: the touch callback reapplied background and border styles to all five targets for every pressed/pressing event, invalidating several disjoint rectangles.
Corrective source: the next P4 image limits style updates to a target entering or leaving highlight, keeps coordinate-only updates during pressing, and adds a 20-valid-press counter to every target. Build passed; physical retest is pending.
```

## 2026-09-07 — Fase 2, repetibilidade de touch e segunda regressão de flush

```text
Source: user observation on the P4 image from commit bfe389f.
Physical result: no stalls occurred and every target completed its 20 valid presses. This supports repeatable target acquisition in the diagnostic layout.
Observed metric under active moving load: pico carga=6. This remains above the <=4 flushes/update criterion, so the render/touch combined path is not approved.
Follow-up correction: retain the current target highlight after release, do not invalidate it on release, and update coordinate text only on the initial pressed event. Build passed; a new physical retest is pending.
```

## 2026-09-07 — Fase 2, terceira medição de flush no caminho touch+carga

```text
Source: user observation on the P4 image from commit 02b5ea0.
Observed result: pico carga began at 4 while touching targets and then reached 5. This still fails the <=4 criterion.
Diagnosis: outside the target itself, the gesture was still invalidating coordinate and status labels in distant screen regions. Their work can overlap the moving-load refresh.
Corrective source: do not update global coordinate/status labels while render load is active; update only the touched target and its local counter. Coordinates remain available with load paused. Build passed; physical retest is pending.
```

## 2026-09-07 — Fase 2, touch sob carga com invalidação isolada

```text
Source: user observation on the P4 image from commit 1607527.
Procedure: active moving render load followed by touches on diagnostic targets.
Observed result: pico carga=4.
Gate interpretation: the touch-plus-load path meets the G2 limit of at most four flushes per update. Prior evidence also recorded no stalls across the 20-press-per-target campaign; a visual tearing/white-frame conclusion and the 30-minute memory/stack soak remain open.
```

## 2026-09-07 — Fase 2, resultado do soak de render de 30 minutos

```text
Source: user observation on the active diagnostic screen after 30 minutes with moving render load.
SRAM free=320 KiB; largest internal block=248 KiB.
PSRAM free=29,158 KiB; largest PSRAM block=28,672 KiB.
LVGL task high-water mark=6,944 B.
Visual and recovery result: no tearing, white frame, other artifact or reboot was observed.
Interpretation: the render soak is visually stable for 30 minutes. A comparable pre-soak memory sample was not captured, so memory-leak trend remains an open G2 item and requires a repeat with baseline and post-soak values.
```

## 2026-09-07 — Fase 2, instrumentação automática para o segundo soak

```text
P4 source and flashed image: commit 011d8aa; np2_p4.bin SHA-256
8170F3873DDED4EAF4B5B91714BD1E63D97E88C7D2349B36920EC61A24982A24.
Effective sdkconfig SHA-256: 9E3F69952675A24BC6C8A6847075292B1D4A9F6168B143D7CF35224B13318225.
Commands: idf.py build; idf.py reconfigure; idf.py -p COM8 flash; idf.py -p COM8 monitor.
Flash/boot result: esptool hash-verified the P4 image. Serial boot identified app version 011d8aa, P4 revision v1.3, 32 MiB flash, 32 MiB PSRAM at 200 MHz, EK79007, GT911 0x5d, LVGL, and the existing C6 Hosted SDIO link. C6 was not flashed.
Instrumentation: activating the moving render load captures initial internal-SRAM and PSRAM free sizes, samples their low-water marks every second without redrawing telemetry during the load, and shows baseline deltas/minima only after the user pauses the load. The LVGL-task stack high-water mark is shown with the result.
Measurement intent: the next 30-minute run will establish a comparable memory trend while preserving the <=4 flushes/update measurement. The prior visual soak remains valid; no claim about memory stability is added until its automatic result is recorded.
```

## 2026-09-07 — Fase 2, correção de retenção do resultado da campanha

```text
Observation on the first automatic-soak run: the displayed SRAM and PSRAM deltas were +0 KiB and their minima equaled the baseline; LVGL stack high-water mark was 6,944 B. However, pausing reset pico carga before the result was rendered. The displayed render/flush/cycle values after pause therefore included the pause UI's own invalidation and cannot qualify the active render path.
Correction: commit e03ca70 keeps independent maxima and last cycle for render, flush callback and flushes per refresh while the load is active. On pause it snapshots those values before issuing pause-related LVGL invalidations and renders a `campanha:` result line.
Validation: ESP-IDF build passed; np2_p4.bin is 921,072 bytes with 89% free in the smallest OTA slot. The P4 image was flashed over COM8 and each written block was hash-verified by esptool. C6 was not flashed.
Open physical check: activate the load for a short interval and pause it; verify that the retained `campanha:` line has non-reset metrics. A new 30-minute result is required only after this behavior is confirmed.
```

## 2026-09-07 — Fase 2, retenção de métricas da campanha validada

```text
Source: user observation on the P4 image from commit e03ca70 after activating and pausing the moving render load.
Retained campaign result: render max=3 ms; flush_cb max=1 ms; last ciclo=0; pico carga=2.
Interpretation: the `campanha:` result was retained across the pause, proving the correction. `ciclo=0` is the last refresh before pause and is not a load peak; pico carga=2 is below the <=4 flushes/update G2 limit. The measured render and flush callback maxima are within the provisional diagnostic budget.
Gate status: render/touch measurement path passes. The remaining Fase 2 gate is controlled flash persistence during active rendering, followed by its physical fault and recovery checks.
```

## 2026-09-07 — Fase 2, gravação NVS pequena durante carga validada

```text
Board and port: same P4 v1.3 unit on USB Serial/JTAG COM8. C6 was not flashed.
P4 source: commit 796b89c, FlashCoordinator diagnostic image.
Procedure: wait for NVS diagnostic readiness; activate moving render load; after two seconds request the single NVS probe; observe for ten seconds; pause the load.
Observed retained campaign result: render max=4 ms; flush_cb max=1 ms; last ciclo=0; pico carga=2.
Observed persistence result: NVS diagnostic #1 completed with ESP_OK in 1 ms.
Visual/recovery result: user observed no tearing, white frame, artifact, stall, or reboot.
Interpretation: the coordinator's queued, small and rate-limited NVS commit passes the first interactive flash gate on this unit. It does not approve NVS compaction/erase, LittleFS write/fsync/rename/GC, C6 staging, or OTA; those operations remain maintenance-mode tests.
```

## 2026-09-07 — Fase 2, primeira tentativa de manutenção NVS (transição visual reprovada)

```text
P4 source: commit 4f06385. C6 was not flashed.
Observed operation result: NVS maintenance #2 returned ESP_OK after 64 blob commits in 623 ms; free NVS entries were 988 before and after the batch.
Observed visual result: while the user held the original long-press trigger, the panel blinked several times, then became partially dark, then returned to normal. No further artifact or reboot occurred.
Gate interpretation: the NVS batch completed, but its visual transition fails the maintenance criterion. A blanked-backlight interval is permitted only when explicitly announced and controlled; repeated blinking is not approved.
Corrective source: replace the long press with an arm-then-confirm two-touch flow; render the maintenance notice for 750 ms, turn off the backlight, wait 500 ms for PWM settling, then begin the NVS batch. Physical retest remains pending.
```
