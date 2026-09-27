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

## 2026-09-07 — Fase 2, reteste de manutenção NVS (reprovado)

```text
Observed operation result: NVS maintenance #2 returned ESP_OK after 64 writes in 1,141 ms; free NVS entries remained 988>988.
Observed visual result: the panel again showed several flashes during the operation. No reset or subsequent artifact was reported.
Gate interpretation: repeated flashing persists after the two-touch transition and the 500 ms backlight-settling interval. The cause is therefore not approved as a UI gesture artifact; NVS compaction/erase on this MSPI/display configuration is blocked.
Containment: remove the maintenance/compaction action from the diagnostic UI and coordinator. Retain only the physically approved small, queued, rate-limited NVS commit during active render. Treat cache as RAM-only until a separately measured platform alternative passes.
```

## 2026-09-09 — Fase 3, falhas DNS e TLS de manutenção

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. C6 was not
flashed. The station was associated to the disposable open lab AP before each
request.

DNS timeout: the physical maintenance command CHECK_DNS_TIMEOUT was accepted
and the worker reported dns=ESP_ERR_TIMEOUT, ntp=ESP_ERR_INVALID_STATE,
https=ESP_ERR_INVALID_STATE in exactly 5000 ms. This proves the worker's
caller-visible DNS deadline; a resolver task may finish later in isolation.

TLS reject: CHECK_TLS_REJECT was accepted. DNS and NTP returned ESP_OK. The
official ESP x509 CRT bundle found no matching trusted root for the fixed
expired.badssl.com target; mbedTLS returned -0x3000 and HTTP returned
ESP_ERR_HTTP_CONNECT. The completed maintenance result was
dns=ESP_OK, ntp=ESP_OK, https=ESP_ERR_HTTP_CONNECT in 4583 ms. This is the
expected secure rejection: neither CA nor hostname validation was disabled.

Gate interpretation: DNS timeout and invalid-TLS rejection pass as negative
tests. They do not prove the DHCP-silent case, a slow backend, UI/touch
continuity measurements, or C6 recovery.
```

## 2026-09-09 — Fase 3, candidato P4 sem reinício automático do host

```text
Decision: CONFIG_ESP_HOSTED_HOST_TRANSPORT_RESTART_ON_FAILURE is now disabled
both in the versioned defaults and in the effective P4 sdkconfig. The previous
effective value was y; the Hosted option documents a host restart after a
runtime transport fault, which violates the G3 local-continuity contract.

Build: ESP-IDF 5.5.4 P4 build passed. The candidate containing only this
configuration transition had app size 0x16C340 and 0x693CC0 bytes (82%) free
in the smallest 8 MiB OTA partition. It was flashed through COM8; esptool
identified ESP32-P4 revision v1.3 and hash-verified bootloader, partition
table, OTA data and application. C6 was not flashed.

Follow-up candidate: Hosted lifecycle events TRANSPORT_UP, TRANSPORT_DOWN and
TRANSPORT_FAILURE are now recorded by the P4 connectivity worker. On a down
event it marks Wi-Fi offline and suppresses further Wi-Fi RPC retries on the
failed SDIO link; it does not drive GPIO54, restart the P4 or attempt an
unvalidated deinit/reinit sequence. Build passed with app size 0x16C570 and
0x693A90 bytes (82%) free. This image was also flashed and every P4 block was
hash-verified; C6 remained untouched.

Gate interpretation: build and P4 flash verification pass. A normal boot,
association and induced C6 reset/absence campaign on this exact candidate are
still pending, so no recovery or continuity approval is claimed.
```

## 2026-09-09 — Fase 3, associação na candidata sem auto-restart P4

```text
After the 0x16C570 P4 candidate was flashed and hash-verified, the operator
used the physical 60-second arm control and submitted OPEN NP2-TEST-OPEN over
COM8. The bridge returned OK, the worker logged the private-mailbox association
request, and the panel reported IP acquired before 20 seconds. No C6 flash was
performed.

Gate interpretation: normal open-AP association and DHCP pass for the current
no-auto-host-restart candidate. This observation does not induce SDIO failure,
does not measure touch/memory through the transition, and does not prove C6
recovery.
```

## 2026-09-09 — Fase 3, candidato de recovery Hosted limitado

```text
Implementation: after a physical RECOVER_C6 request, the P4 worker runs the
official lifecycle esp_wifi_stop/deinit -> destroy the P4 STA netif ->
esp_hosted_deinit -> create WIFI_STA_DEF -> esp_hosted_init/connect_to_slave
-> Wi-Fi RAM-only initialization. It retains only the existing private station
request in RAM for reassociation. It never drives reset54 directly. The
ESP-Hosted reset strategy retains ownership of the C6 reset pulse.

Limits: recovery is reachable only through the already physical 60-second USB
maintenance arm. The worker keeps at most three recovery timestamps in a ten
minute window; a fourth request starts a five-minute cooldown. Planned
deinitialization is not counted as an SDIO transport failure.

Build and flash: ESP-IDF 5.5.4 passed with app size 0x16D170 and 0x692E90
bytes (82%) free in the smallest OTA partition. The P4 image SHA-256 is
8153AB25B6A29DAF0FF1D62D97F8FC24F9B49ABB7937E65C4073C2DDCE37876F.
Esptool identified P4 revision v1.3 and hash-verified all P4 blocks. C6 was
not flashed.

Gate interpretation: this is implementation/build/P4-flash evidence only. A
physical RECOVER_C6 lifecycle with active render, touch and reassociation is
pending; no C6-recovery pass is claimed.
```

## 2026-09-09 — Fase 3, primeiro recovery C6 pelo ciclo Hosted

```text
Board and port: same ESP32-P4 v1.3 unit on COM8, recovery candidate
8153AB25B6A29DAF0FF1D62D97F8FC24F9B49ABB7937E65C4073C2DDCE37876F. C6 was not
flashed.

The operator associated the open laboratory AP, physically armed maintenance,
then submitted RECOVER_C6. The bridge returned OK. Serial showed the expected
station disconnect, Hosted auto-deinitialization, SDIO buffer release and a
TRANSPORT_DOWN observation. It then reinitialized SDIO four-bit at 40 MHz on
the validated pins and the Hosted component, not application code, reset the
C6 through GPIO54. The operator confirmed that IP was acquired again.

Gate interpretation: one full P4-local/Hosted/C6/Wi-Fi recovery succeeded. The
remaining required cycles, cooldown behavior, render/touch/memory observations
and C6-absent fault remain pending. No P4 reset, screen or touch conclusion is
inferred from the serial capture alone.
```

## 2026-09-09 — Fase 3, segundo recovery C6 pelo ciclo Hosted

```text
The second physically armed RECOVER_C6 request was accepted. The serial path
again showed station disconnect, Hosted Wi-Fi deinitialization, TRANSPORT_DOWN,
SDIO four-bit reinitialization and an ESP-Hosted-owned reset through GPIO54.
The operator confirmed that IP was acquired successfully in less than 10 s.

Gate interpretation: cycle 2 of the required 3/10-minute recovery limit passes.
Cycle 3, the fourth-request cooldown, and explicit render/touch observation
remain pending.
```

## 2026-09-09 — Fase 3, terceiro recovery C6 pelo ciclo Hosted

```text
The third physically armed RECOVER_C6 request was accepted at 1,127,765 ms.
The expected lifecycle repeated: Wi-Fi deinitialization, observed SDIO down,
SDIO four-bit bus reinitialization and a Hosted-owned GPIO54 C6 reset. The
operator confirmed that IP was acquired in less than 6 s.

Gate interpretation: the three allowed recovery cycles in the ten-minute
window pass. The fourth-request cooldown and explicit render/touch observation
remain pending.
```

## 2026-09-09 — Fase 3, campanha repetida de recovery C6

```text
The operator reported more than 20 recovery attempts over the bench campaign;
the observed outcome remained C6 recovery and IP reacquisition. The individual
serial timestamps available in this evidence are separated by more than the
ten-minute policy window, so they do not prove the fourth-request cooldown.

Gate interpretation: repeated recovery behavior is operator-observed evidence.
The three-per-ten-minute limiter is implemented and its strict cooldown branch
remains physically unverified; it is not represented as a passing result.
```

## 2026-09-09 — Fase 4, cache LittleFS com duas gerações e CRC

```text
Implementation: the storage writer now serializes a versioned cache record in
alternating cache.0/cache.1 files. Each record contains magic, schema version,
header size, monotonic generation, payload length, payload CRC32 and header
CRC32. It writes cache.tmp, fsyncs, atomically renames only the next slot, then
validates the destination. It never removes the prior generation first.

Mount behavior: cache.0 and cache.1 are independently validated. The newest
valid generation is selected; both missing is an empty cache, and any corrupt
record is rejected without automatic storage format. The UI reports the chosen
generation and CRC result after boot.

Build and flash: ESP-IDF 5.5.4 built the P4 image at 0x16D380 bytes with
0x692C80 bytes (82%) free in the smallest OTA partition. Esptool hash-verified
all P4 blocks; C6 was not flashed.

Physical result: with active render, the operator ran the existing 64 x 4 KiB
LittleFS workload. It completed with cache generation 64 and CRC=ESP_OK. The
operator then reset P4 and confirmed the UI reported recovery of generation 64
with schema 1 and CRC=ESP_OK, with the panel remaining normal.

Gate interpretation: two-generation write, CRC validation and reboot recovery
pass once on this board. Corrupt-newest fallback, interrupted write, full
filesystem and power-cut cases remain required for G4.
```

## 2026-09-09 — Fase 4, fallback após corrupção do cache mais novo

```text
Build and flash: the P4 candidate added a physically armed
CACHE_CORRUPT_NEWEST command. It refuses to run without a valid older cache,
flips one byte only in the newest payload, fsyncs it, and verifies that the
other generation becomes selected. The app image was 0x16D6A0 bytes with 82%
free in the smallest OTA partition; esptool hash-verified P4 blocks only.

Physical result: the command was accepted and the flash coordinator reported
"cache newest-generation corruption 1 completed in 170ms". After reboot, the
operator confirmed that the cache was recovered. The expected selected record
is generation 63 because generation 64 was the newest valid record before the
controlled one-byte corruption; the exact UI generation text was not
transcribed.

Gate interpretation: corrupt-newest fallback passes once, with the generation
value operator-confirmed only as cache recovery rather than a copied UI value.
No autoformat was requested or observed. Interrupted-write, filesystem-full
and physical power-cut tests remain open.
```

## 2026-09-09 — Fase 4, corte de energia durante gravação de cache

```text
Physical procedure: with render load active, the operator started the 64 x 4
KiB cache workload and disconnected P4 USB power about one second later. After
three seconds the USB supply was restored and P4 completed normal boot. C6 was
not flashed.

Observed result: the panel reported "Cache offline recuperado geracao 63 schema
1 crc esp-ok". This is the previously retained valid generation; the interrupted
newer write did not replace it. No storage format was requested or observed.

Gate interpretation: one physical power-cut during cache write passes the
two-generation fallback behavior. Filesystem-full and configuration/NVS
partial-write cases remain required for G4.
```

## 2026-09-09 — Fase 4, primeira geração do journal de configuração NVS

```text
Build and flash: the P4 image added a journal in the existing NVS partition.
It alternates cfg0/cfg1 blobs in namespace np2_config. Each blob has the same
versioned header, payload CRC32 and header CRC32 used by the cache format;
the new blob is committed and selected again before success is reported.
The image size was 0x16DD00 with 82% free in the smallest OTA partition.
ESP-IDF 5.5.4 build passed and esptool hash-verified P4 blocks only; C6 was
not flashed.

Initial physical state after boot: cache generation 63 selected with
CRC=ESP_OK; configuration had no stored record, reported as generation 0 and
ESP_ERR_NOT_FOUND. This is the specified empty-journal state.

Physical write: CONFIG_WRITE was sent through the physically armed USB bridge
as its exact 13-byte ASCII frame (CONFIG_WRITE plus LF). The bridge accepted
it, returned ESP_OK, and the coordinator completed configuration generation 1
in 38 ms. The active UI then reported cache g63 CRC=ESP_OK and config g1
CRC=ESP_OK. No display artifact was reported.

Gate status: write-and-read verification of the first NVS generation passes.
The operator then issued one physical P4 reset and confirmed cache g63
CRC=ESP_OK with config g1 CRC=ESP_OK after boot. First-generation persistence
therefore passes.
```

## 2026-09-09 — Fase 4, duas gerações e fallback do journal NVS

```text
Second generation: a subsequent CONFIG_WRITE selected configuration generation
2 with CRC=ESP_OK while the cache remained generation 63 with CRC=ESP_OK. A
temporary log label showed request sequence 1 after reboot; this was corrected
to report the actual selected configuration generation. The UI state is the
source of the observed generation: g2.

Persistence: a later P4 application flash, which did not write the NVS
partition, restarted the board and the operator confirmed cache g63 CRC=ESP_OK
and config g2 CRC=ESP_OK. The two NVS slots therefore survive P4 resets and
unrelated application updates.

Controlled fallback: the candidate added CONFIG_CORRUPT_NEWEST. It is accepted
only with two valid diagnostic configuration generations. It changes only the
payload of the newest np2_config blob while leaving its stored CRC unchanged,
commits it, and succeeds only if the already-valid prior generation becomes
selected. The operator confirmed the fallback and reboot result. This validates
selection of the older NVS generation after newest-record corruption; cache,
network, C6 and product credentials were not part of the operation.

Gate status: two-generation write, reboot persistence and corrupt-newest
fallback of the diagnostic NVS journal pass. An interrupted NVS commit and
filesystem quota/full behavior are still open; G4 itself remains blocked until
G3 is formally closed and its complete data/UX scope is implemented.
```

## 2026-09-09 — Fase 3, servidor HTTPS lento com deadline global

```text
Build and flash: the P4 candidate makes the 20-second diagnostic-request
budget effective. DNS and NTP consume that same budget; the HTTPS client gets
only the remaining time, capped at ten seconds. CHECK_HTTPS_TIMEOUT uses a
fixed 15-second HTTPS endpoint and accepts no URL from the USB host. C6 and
NVS were not flashed or modified.

Physical result: after RAM-only association to NP2-TEST-OPEN, the physically
armed command was accepted with ESP_OK. DNS=ESP_OK and NTP=ESP_OK. The IDF
HTTP client logged "Connection timed out before data was ready" and the
published HTTPS result was ESP_ERR_HTTP_EAGAIN after 15,878 ms total. In this
ESP-IDF path EAGAIN is the client return for this read-timeout condition; it
is a rejected slow response, not a successful HTTPS result. The duration is
below the 20,000 ms request deadline.

Gate status: bounded slow-backend behavior passes once. The supplied capture
does not independently record visual/touch continuity during the wait.
```

## 2026-09-09 — Fase 3, campanha automática de cooldown do C6

```text
Implementation: RECOVER_C6_COOLDOWN is a physically armed, one-shot campaign.
It requires an active RAM-only station with IP, executes at most three complete
Hosted recoveries, waits up to 20 s for IP after each, then submits a fourth
recovery solely to require the five-minute-cooldown rejection. A rejected
fourth request no longer marks an already-recovered link down. The P4 never
drives GPIO54; ESP-Hosted retains that operation.

Physical result: after RAM-only association to NP2-TEST-OPEN, the campaign was
accepted. The C6 was reset by Hosted at 182560, 188860 and 195160 ms. IP
returned at 188500, 194800 and 201100 ms respectively: approximately 5.94 s
for each recovery. All three rounds re-negotiated ESP-Hosted 3.0.6, RPC V2,
SDIO 4-bit streaming and SW_AGGR. The final log was "Hosted cooldown campaign
passed: 3 cycles, fourth rejected". No P4 reset appears in the capture.

Gate status: the three-cycles-per-ten-minutes limiter and immediate fourth
rejection pass on hardware. The full five-minute expiry was not waited out in
this run. Display and touch continuity were not independently described in
the supplied serial capture.
```

## 2026-09-09 — Fase 3, injeção local de DHCP silencioso

```text
Implementation: CHECK_DHCP_TIMEOUT is a physically armed, local fault
injection. With an already-online RAM-only station, it stops the P4 DHCP
client, reassociates once, waits for the existing 20-second DHCP deadline,
then restarts DHCP and lets the normal 2/4/8/16/30-second bounded retry path
recover. It is explicitly not represented as an AP physically withholding
DHCP replies.

Physical result: the command was accepted at 420946 ms. Association occurred
without a lease, and the injection began waiting at 421206 ms. The associated
station was seen at 423926 ms; the DHCP deadline fired at 443956 ms, about
20.03 s later. DHCP was restored, the worker scheduled a 2262 ms retry, and
the IP returned at 450616 ms. The final record was "DHCP silence injection
passed: deadline then automatic recovery". No P4 reset appears in the log.

Gate status: the local DHCP deadline, restoration and bounded reassociation
pass on hardware. A real AP that accepts association while withholding DHCP,
with the same UI-continuity observations, remains a distinct G3 test.
```

## 2026-09-10 — Fase 3, rejeição streaming de corpo HTTPS acima do limite

```text
Implementation: CHECK_HTTPS_OVERSIZE accepts no URL from the USB host. It uses
a fixed 2 KiB HTTPS response, counts received chunks without retaining the
body, and rejects the operation once more than 512 bytes have been received.
The diagnostic executor remains single-flight and shares the 20-second total
request budget with DNS and NTP.

Physical result: after RAM-only association, the armed command was accepted.
DNS=ESP_OK, NTP=ESP_OK and the CA bundle validated the server certificate.
The final result was HTTPS=ESP_ERR_INVALID_SIZE in 2301 ms. The oversized body
was therefore rejected after TLS validation, below the deadline and without a
successful external request result.

Gate status: bounded oversized-body rejection passes once. UI/touch continuity
was not independently described in the serial capture.
```

## 2026-09-10 — Fase 3, campanha completa de cooldown C6: falha encontrada

```text
Physical result: RECOVER_C6_COOLDOWN_FULL was run on the associated laboratory
open AP. The first three Hosted-owned C6 reset/recovery cycles each reacquired
an IP address. The campaign then waited for the five-minute cooldown to expire.
At approximately 301 s after that wait began, the requested fifth recovery was
still rejected and the worker reported "Hosted cooldown campaign ended:
ESP_ERR_TIMEOUT".

Finding: this was a firmware limiter defect, not evidence that the C6 could not
recover. The three timestamps from the just-completed window were still retained
after cooldown expiry, so the next request immediately re-entered cooldown.
The supplied capture contains no P4 boot/reset record. It does not independently
establish display or touch continuity during the campaign.

Remediation pending physical confirmation: on expiry, clear the three retained
timestamps and start a fresh ten-minute recovery window before allowing the
fifth recovery. The P4 application owns only this policy; ESP-Hosted continues
to own the C6 GPIO54 reset sequence.

Gate status: full-cooldown expiry is failed on the tested image and must be
repeated after the corrective P4 image is built and flashed.
```

## 2026-09-10 — Fase 3, correção candidata da janela de cooldown C6

```text
Implementation: when the five-minute cooldown has elapsed, the P4 policy now
clears the retained recovery timestamps and begins a fresh ten-minute window
before it considers the next recovery. This fixes the demonstrated policy
defect without changing the ESP-Hosted lifecycle or application ownership of
GPIO54.

Build and flash: ESP-IDF 5.5.4 P4 build passed. np2_p4.bin is 0x16EF30 bytes,
with 0x6910D0 bytes (82%) free in the smallest OTA partition; SHA-256 is
555E3D410E26D4AC6762BB4BE838C4320F67A21D28CC2C11435F8EFD31AA53F9.
Esptool identified ESP32-P4 revision v1.3 and hash-verified bootloader,
application, partition table and OTA-data writes through COM8. C6 was not
flashed.

Gate status: software correction, build and P4 flash pass. The full physical
cooldown-expiry campaign remains required to validate the behavior.
```

## 2026-09-10 — Fase 3, campanha completa de cooldown C6: correção aprovada

```text
Physical result: on the corrective P4 image, the operator associated the open
laboratory AP and ran RECOVER_C6_COOLDOWN_FULL. The final observed sequence was
station configuration, reassociation, IP acquisition at 427968 ms, followed by
"Hosted full cooldown campaign passed: fifth recovery allowed after cooldown"
at 428078 ms. Thus the fifth Hosted recovery was admitted after the five-minute
cooldown and the station again received an IP address.

Gate interpretation: the hardware now validates the three-recoveries window,
immediate fourth rejection, cooldown expiry, and a permitted fifth Hosted
recovery. The supplied capture has no subsequent P4 boot/reset line. It does
not by itself supply independent display, touch or memory measurements, so
those continuity sub-gates remain open.
```

## 2026-09-11 — Fase 3, candidata de provisionamento WPA2 RAM-only gravada

```text
Implementation: the diagnostic screen now has a CONFIGURAR WI-FI WPA2 control
that opens a touch modal. The provisioning service, rather than any LVGL text
widget, owns the SSID and password input buffers in internal RAM. The screen
shows the SSID and password bullet count only. On submit it transfers one
private copy to the existing connectivity mailbox and wipes the input buffers.
There is no NVS, LittleFS, USB or log password path. Authentication rejection
queues an erase of the driver-side RAM configuration and requires a new user
entry rather than a retry loop.

Build and flash: ESP-IDF 5.5.4 P4 build passed. np2_p4.bin is 0x1701A0 bytes,
with 0x68FE60 bytes (82%) free in the smallest OTA partition; SHA-256 is
DFBE61F31A63C9688D2C492DE4CCE9C1201FC2264EACEF9A54A4BFB799BB5DAD.
Esptool identified ESP32-P4 revision v1.3 and hash-verified bootloader,
application, partition table and OTA-data writes through COM8. C6 was not
flashed; NVS and storage were not targets of this flash command.

Gate status: implementation/build/P4-flash pass. Touch-modal appearance,
WPA2 successful association, wrong-password behavior and proof of RAM-only
retention remain physical tests.
```

## 2026-09-11 — Fase 3, primeira associação WPA2 pelo touch

```text
Physical result: after the P4 candidate booted, the operator opened the
CONFIGURAR WI-FI WPA2 modal, entered a WPA2 network through the touch keyboard
and submitted the connection. The main UI reported "IP adquirido". No SSID or
password was supplied to this evidence, USB bridge, source tree or log capture.

Gate interpretation: the touch modal and RAM-only private mailbox support one
successful WPA2 association on hardware. This does not prove wrong-password
handling, persistence through reboot or encrypted credential storage.
```

## 2026-09-11 — Fase 3, senha WPA2 inválida: correção e aprovação

```text
Initial physical result: the first WPA2 wrong-password attempt entered the
ordinary recovery state, which is not an acceptable credential-rejection
outcome. The initial classifier covered AUTH_FAIL and handshake timeout events
but omitted the AUTH_EXPIRE/ASSOC_NOT_AUTHED values that ESP-Hosted may forward
for WPA2 authentication failure.

Correction: those reason codes, plus the existing WPA2 handshake and 802.1X
authentication failures, now stop retries, queue a driver-side RAM
configuration erase outside the event callback, and publish
ESP_ERR_WIFI_PASSWORD. No credential value is logged.

Build and flash: ESP-IDF 5.5.4 build passed with np2_p4.bin at 0x170220 bytes
and 0x68FDE0 bytes (82%) free in the smallest OTA partition. The corrective P4
image was flashed through COM8 with hash verification of each written block;
C6, NVS and storage were not flashed.

Corrective physical result: the operator repeated the invalid-password flow
and confirmed that it now produced the expected terminal result rather than
remaining in recovery. The valid-password reassociation on this exact image
remains the final WPA2 RAM-only confirmation.
```

## 2026-09-11 — Fase 3, WPA2 válido após rejeição de senha inválida

```text
Physical result: following the accepted terminal invalid-password outcome, the
operator entered the valid WPA2 password through the same touch-only modal.
The main UI reported "IP adquirido". The password was not supplied in this
evidence or through USB.

Gate interpretation: on the current image, the WPA2 flow supports both a
non-looping rejected credential and a later valid credential in RAM. A reboot
check is still required to prove that the current implementation did not retain
the credential in NVS or storage.
```

## 2026-09-11 — Fase 3, retenção WPA2 RAM-only confirmada por reboot

```text
Physical procedure: after successful WPA2 association through the touch modal,
the operator rebooted the P4.

Observed result: after boot, the panel reported "sem credencial" and did not
recover an IP automatically. This is the required result for the current
RAM-only implementation; neither NVS nor storage retained the WPA2 password.

Gate interpretation: WPA2 touch entry, invalid-password termination, valid
reassociation and explicit non-persistence through reboot pass on hardware.
Encrypted credential persistence remains deferred to its separate security,
eFuse and OTA/recovery gate.
```

## 2026-09-11 — Fase 3, C6 ausente/travado bloqueado por fixture

```text
Physical constraint: the operator confirmed that this unit has no jumper,
switch or other safe physical method to isolate the ESP32-C6. The application
must not take over GPIO54; ESP-Hosted retains ownership of the C6 reset path.

Decision: do not emulate C6 absence by driving GPIO54 or by an unvalidated
SDIO manipulation. The 30-second Hosted startup supervisor remains implemented.
The board schematic identifies the C6 `CHIP_PU` path, but it exposes no
operator-safe isolation point; on 2026-09-11 the operator decided not to alter
the hardware. The literal C6-absent/travado physical gate is therefore outside
the scope of this unit. This does not invalidate the already approved Hosted
recovery/cooldown tests.
```

## 2026-09-07 — Fase 2, variante XiP em PSRAM compilada (sem evidência física)

```text
P4 configuration change: CONFIG_SPIRAM_XIP_FROM_PSRAM=y;
CONFIG_SPI_FLASH_AUTO_SUSPEND=n retained. No C6, BSP, LVGL, adapter, PSRAM
frequency/mode, rotation or framebuffer-count setting changed.

The first build after the defaults change retained XiP disabled in the
pre-existing generated sdkconfig. It is not XiP evidence and must not be used
for binary size or platform approval. The generated configuration was then
explicitly switched to XiP.

Software validation: ESP-IDF 5.5.4 completed `idf.py fullclean && idf.py
build` for target `esp32p4`. The effective configuration contains
`CONFIG_SPIRAM_XIP_FROM_PSRAM=y`, `CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y`,
`CONFIG_SPIRAM_RODATA=y` and `CONFIG_SPIRAM_FLASH_LOAD_TO_PSRAM=y`; it retains
HEX PSRAM at 200 MHz, three DPI framebuffers and flash auto-suspend disabled.
The application image is 0xE7F00 bytes; the smallest app partition is
0x800000 bytes, leaving 0x718100 bytes free.

Gate status: the XiP P4 build sub-gate passes. No XiP flash, boot, render, NVS
erase/GC, LittleFS, Hosted/SDIO or OTA physical result is claimed by this entry.
```

## 2026-09-07 — Fase 2, variante XiP gravada e iniciada no P4

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. The C6 image
was not flashed.

Command: idf.py -p COM8 flash. esptool identified ESP32-P4 revision v1.3 and
32 MiB flash; it wrote bootloader, P4 app, partition table and initial OTA
data, and hash-verified every written block before hard reset.

Serial boot evidence: the bootloader loaded ota_0; 32 MiB HEX PSRAM passed its
memory test at 200 MHz; `mmu_psram` reported `.rodata xip on psram` and
`.text xip on psram`. The EK79007/MIPI-DSI display and LVGL adapter initialized,
then the board reached `P4 local bring-up ready`. ESP-Hosted reset C6 through
GPIO54 and negotiated SDIO 4-bit streaming, RPC v2 and SW_AGGR; host and
coprocessor versions both reported 3.0.6.

Gate status: XiP flash and cold-boot sub-gates pass. This serial capture is not
evidence for artifact-free scanout, NVS program/erase/GC, LittleFS, endurance,
or OTA apply/revert; those physical tests remain required.
```

## 2026-09-07 — Fase 2, lote NVS sob render com XiP

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. C6 was not
flashed. P4 source state: 0657cff-dirty; application SHA-256:
E3935D3B7B58F71BF130E11EA1E79206305A39A5D5B9C4B9C7FFF1E95B7BC428.
Effective sdkconfig SHA-256:
FFF95DD1A5137A19835A230F868454ABE1D7DDF37B0652B4C5E13138CBC080B7.

Instrumentation: the normal active-render probe remains a single NVS u32
commit. A deliberate long press on the same control queues 64 commits of a
512-byte blob in namespace `np2_diag`; it does not turn off the backlight or
write another partition. Command: idf.py build; idf.py -p COM8 flash; idf.py
-p COM8 monitor.

Observed serial result after the flashed XiP boot: small commit #1 completed
in 2 ms. Batch #2 completed in 618 ms with ESP_OK, 64 writes, and NVS free
entries reported as 988 before and 988 after. The unchanged free-entry count
is consistent with NVS reclaiming obsolete versions during the workload, but
does not by itself prove which physical sectors were erased.

The monitor recorded a serial disconnect and a fresh boot before this second
campaign; no cause is assigned to it. After the batch, the monitor remained
connected for 10 s without a reset, panic or WDT log. The operator observed no
screen artifact, flicker, blanking or interruption of normal panel operation
during the active-render batch. No video was made, so this is direct visual
observation rather than frame-by-frame scanout evidence.

Follow-up physical result: the operator completed two further active-render
64x512-byte NVS batches, each followed by a successful restart. Together with
the serial-instrumented first batch, this completes three batch-and-reboot
cycles on the XiP image. No video or additional serial capture was made for
the two follow-up cycles; their result is direct bench observation.

Gate status: the XiP NVS program/compaction workload passes its three-cycle
subgate on this board: all batches and restarts completed, and no screen
artifact was observed during the campaign. This does not approve the complete
G2 gate; LittleFS, OTA and soak remain untested.
```

## 2026-09-08 — Fase 2, LittleFS sob render

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. C6 was not
flashed. The XiP image adds no automatic format: a long press explicitly
formatted only the `storage` partition, then mounted it. Serial result:
LittleFS explicit format #1 completed in 45 ms.

With the moving render workload active, a short press ran LittleFS batch #2:
64 cycles of 4 KiB write -> fsync -> rename -> read/verify. It completed with
ESP_OK in 3,814 ms and verified 262,144 bytes (256 KiB). This is a successful
integrity result for the first filesystem workload.

The subsequent reboot and 30-minute active-render result are recorded below.
Gate status: LittleFS write/fsync/rename/read and explicit format/erase pass
once in this qualification workload.
```

## 2026-09-08 — Fase 2, reboot pós-LittleFS e fechamento do soak

```text
The operator rebooted the P4 after the LittleFS batch. The serial monitor saw
the P4 return through normal boot, pass the 32 MiB PSRAM memory test, initialize
EK79007/LVGL and restore the C6 SDIO link with RPC v2 and SW_AGGR.

Operator-reported diagnostic values after the render run: SRAM delta 0 KiB,
minimum 291 KiB; PSRAM delta 0 KiB, minimum 2,829 KiB; LVGL stack free
28,169 bytes. The UI reports the stack figure in bytes, not KiB.

Audit note (2026-09-12): the 28,169-byte stack value cannot be reconciled with
the 12,288-byte LVGL stack configured in the cited source. It is retained as
historical operator output but is not accepted as stack-margin evidence until
a new capture identifies the exact image, task handle and configured size.

Gate status: reboot recovery and the reported memory floor pass. The exact
duration and the visual observation have now been confirmed by the operator:
the active-render run lasted 30 minutes and had no screen artifact, flicker,
blanking, stall or reset. Together with the three NVS batch-and-reboot cycles,
this closes G2 for the P4 qualification scope.

Traceability: after the physical workload, a clean ESP-IDF 5.5.4 P4 build was
flashed and hash-verified by esptool. It booted normally with P4 revision 1.3,
32 MiB PSRAM at 200 MHz, `.text`/`.rodata` XiP in PSRAM, EK79007/LVGL and the
C6 Hosted SDIO link. Source baseline was `0657cff409f7ff207a26bc02b92097ffe70f1449`;
the final `np2_p4.bin` SHA-256 was
`E25D2803732FC7A0CD2E0A9CA0E0388E00FBC83AC5BCDA6F3EE596DB3301A550`, and the
effective `sdkconfig` SHA-256 was
`FFF95DD1A5137A19835A230F868454ABE1D7DDF37B0652B4C5E13138CBC080B7`.
```

## 2026-09-08 — Fase 3, Hosted/SDIO e scan Wi-Fi sem credenciais

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. The C6 was
not flashed. P4 source state: 0657cff-dirty; final application SHA-256:
4FBA9727E6F48019E0711CFE7F86013F88DAC9DAC2B2DE00DB12A9E467C63251.
Effective sdkconfig SHA-256:
FFF95DD1A5137A19835A230F868454ABE1D7DDF37B0652B4C5E13138CBC080B7.

Commands: clean P4 build; idf.py -p COM8 flash; idf.py -p COM8 monitor. The
final app measured 0x113EA0 bytes, leaving 0x6EC160 bytes (87%) in the smallest
8 MiB app partition. Esptool hash-verified every P4 block. No C6 flash or
configuration write was requested.

Observed serial result: the P4 booted with 32 MiB PSRAM at 200 MHz and XiP
text/rodata in PSRAM, then initialized EK79007/LVGL and the existing C6 link.
Hosted reported C6 chip 0x0d (ESP32-C6), host/coprocessor version 3.0.6,
RPC V2, SDIO 4-bit streaming and SW_AGGR with 15,872-byte buffers in both
directions. The Phase 3 worker logged "Hosted SDIO link is up", started Wi-Fi
with RAM-only storage and completed its credential-free scan with 38 APs.
It neither supplied a station configuration nor called connect, DNS, NTP or
HTTPS; no SSID or credential was printed.

Gate status: the first Hosted/SDIO/Wi-Fi scan sub-gate passes on this unit.
G3 remains open for display-responsiveness observation, a physical association
and reconnect campaign, provisioning UX, DHCP/DNS failure recovery, time
quality and a single serialized HTTPS request.
```

## 2026-09-08 — Fase 3, boot da FSM de associação RAM-only

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. C6 was not
flashed. P4 source state: 0657cff-dirty; application SHA-256:
57DD11CCDC6A7D4A7AF32B7E2B47E14AC6789F4C3BAD79FC781200C61CBCBA70.
Effective sdkconfig remains
FFF95DD1A5137A19835A230F868454ABE1D7DDF37B0652B4C5E13138CBC080B7.

Commands: idf.py build; idf.py -p COM8 flash; idf.py -p COM8 monitor. Build
size was 0x114AD0, leaving 0x6EB530 bytes (86%) in the smallest app partition;
esptool hash-verified the P4 blocks.

Observed serial result: the persistent connection worker repeated normal boot,
C6 chip 0x0d, Hosted host/coprocessor 3.0.6, RPC V2, SDIO 4-bit streaming and
SW_AGGR. Its credential-free startup scan completed with 46 APs. No association
request was submitted, so no station credentials, DHCP lease, DNS, NTP or HTTPS
operation occurred. This confirms the FSM's idle startup path only.

Gate status: boot/scan remains passing after adding the private mailbox and
backoff state machine. Association, forget, AP loss/recovery and visual UI
responsiveness require a restricted provisioning entry point and a separate
bench campaign.
```

## 2026-09-08 — Fase 3, mailbox seguro e deadlines Wi-Fi

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. C6 was not
flashed. P4 source state: 0657cff-dirty; application SHA-256:
8E151125EA4802F6883F6A655DCC093FFFD302E180D622A387E20E31752F4B24.
Effective sdkconfig SHA-256:
FFF95DD1A5137A19835A230F868454ABE1D7DDF37B0652B4C5E13138CBC080B7.

Change under test: the private one-entry mailbox now clears the consumed copy
under its own short lock, avoiding a race with a concurrent sender. The worker
also imposes 15 s association and 20 s DHCP deadlines. Authentication failure
stops automatic retries and needs an explicit new request; other disconnects
continue through the existing bounded backoff. No provisioning request was
submitted in this boot.

Build and flash: after regenerating the P4 configuration with
ESP_IDF_VERSION=5.5, a single serialized Ninja build completed. The final
image was 0x114E50 bytes and left 0x6EB1B0 bytes (86%) in the smallest 8 MiB
app partition. Esptool hash-verified each P4 block. Only P4 offsets
0x2000/0x10000/0x1B000/0x20000 were written; the C6 image was untouched.

Observed serial result: normal P4 v1.3 boot; 32 MiB PSRAM at 200 MHz; XiP
text/rodata in PSRAM; EK79007, GT911 and LVGL initialized; C6 chip 0x0d;
Hosted host/coprocessor 3.0.6; RPC V2; SDIO 4-bit streaming; SW_AGGR with
15,872-byte buffers each way. The RAM-only scan completed with 45 APs.

Gate status: this is a boot/scan and build result only. It does not validate
association, the two new deadlines, credential rejection, AP/DHCP/C6 recovery,
UI responsiveness during those faults, DNS/NTP/HTTPS, or automatic recovery
after a P4 reboot. Those remain open under G3-CONTINUITY-VALIDATION.md.
```

## 2026-09-08 — Fase 3, telemetria visual da FSM

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. C6 was not
flashed. P4 source state: 0657cff-dirty; application SHA-256:
8B83ACC0A8CA9CF91966EA80B9B5A6E10C276C0CC64C9D80B7BBFC72E6FC4B93.
Effective sdkconfig SHA-256:
FFF95DD1A5137A19835A230F868454ABE1D7DDF37B0652B4C5E13138CBC080B7.

Change under test: the LVGL diagnostic view reads the lock-protected network
snapshot once per second and renders only operational state, AP count, retry
count, IP readiness and error name. It has no SSID, password, provisioning
field or persistence path. This keeps LVGL as the only owner of UI objects and
makes the RAM-only worker observable under render stress.

Build and flash: single serialized P4 build passed. The image was 0x1151D0
bytes, leaving 0x6EAE30 bytes (86%) in the smallest 8 MiB app partition.
Esptool hash-verified all P4 blocks; C6 was untouched.

Observed serial result: normal P4 v1.3 boot, PSRAM 32 MiB at 200 MHz, XiP
text/rodata in PSRAM, EK79007/GT911/LVGL initialized, Hosted 3.0.6, RPC V2,
SDIO 4-bit streaming and SW_AGGR. The RAM-only Wi-Fi scan completed with 48
APs. The serial capture cannot prove that the new line remained visually
correct on the panel; that observation is pending from the operator.

Gate status: observability build/boot/scan passes. It does not add a
credential path or validate association, DHCP, AP/C6 recovery, DNS, NTP or
HTTPS; G3 remains open.
```

## 2026-09-08 — Fase 3, confirmação visual da telemetria

```text
The operator confirmed that the network status line appeared on the panel and
that the animated render and touch continued to function normally. No screen
artifact, flicker, blanking or UI interruption was reported for this
observability boot.

Gate status: visual observability is confirmed for the RAM-only scan path. No
station association, DHCP, AP loss, C6 reset, DNS, NTP or HTTPS fault was
induced in this observation.
```

## 2026-09-08 — Fase 3, bridge USB de rede aberta

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. C6 was not
flashed. P4 source state: 0657cff-dirty; application SHA-256:
29EF40B3D882BB1C194CE3B29EA94CE928EBDEC9EAF815B89696517B86011B25.
Effective sdkconfig SHA-256:
FFF95DD1A5137A19835A230F868454ABE1D7DDF37B0652B4C5E13138CBC080B7.

Change under test: a disarmed USB Serial/JTAG maintenance bridge accepts one
open-network request only after a physical 60-second arm action in the LVGL
diagnostic view. It supports OPEN <ssid> and FORGET, clears input, does not
accept passwords and does not persist or log the request.

Build and flash: a single serialized P4 build passed. The image was 0x118350
bytes and left 0x6E7CB0 bytes (86%) in the smallest 8 MiB app partition.
Esptool hash-verified all P4 blocks. C6 was untouched.

Observed serial result: normal P4 boot; 32 MiB PSRAM at 200 MHz; XiP
text/rodata in PSRAM; EK79007, GT911 and LVGL initialized; C6 chip 0x0d;
Hosted 3.0.6; RPC V2; SDIO 4-bit streaming; SW_AGGR; and RAM-only scan with
53 APs. No "maintenance service unavailable" error appeared. No OPEN/FORGET
command, association, DHCP or fault injection was performed in this boot.

Gate status: the bridge build/boot sub-gate passes. The open AP association,
AP-off/AP-on and DHCP deadline campaign remains pending.
```

## 2026-09-08 — Fase 3, correção de desconexão interna antes de associação

```text
Board and port: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8. C6 was not
flashed. P4 application SHA-256:
20A8FF89AEA7743120D704D668B30BDE1BE4415A4B36F00487A5DAED03AF1EE5.

Change under test: the connectivity worker now consumes only its own
WIFI_REASON_STA_LEAVING (36) event published asynchronously by
esp_wifi_disconnect() while replacing a RAM-only station configuration. Other
disconnect reasons remain visible and keep their existing retry policy.

Build and flash: the image was 0x118520 bytes and left 0x6E7AE0 bytes (86%)
in the smallest 8 MiB app partition. Esptool hash-verified bootloader,
partition table, OTA data and application blocks; then requested a P4 hard
reset. C6 was untouched.

Observed serial result: a post-flash 18-second passive serial capture yielded
no text, so it is not used as boot evidence. No Wi-Fi association, DHCP or
display observation has been claimed for this image.

Gate status: P4 flash verification passes. The corrected association path
still requires physical Wi-Fi and display validation.
```

## 2026-09-08 — Fase 3, boot observado após correção de desconexão interna

```text
Observed operator serial boot after P4 flash: reset reason
CHIP_USB_UART_RESET; ESP-IDF v5.5.4-dirty; ESP32-P4 revision v1.3; 32 MiB
SPI flash in DIO at 80 MHz; application loaded from ota_0 at 0x20000; and
the expected 32 MiB HEX PSRAM at 200 MHz.

The boot also reports ".rodata xip on psram" and ".text xip on psram". This
confirms the configured XiP mapping was active for this boot. The supplied
excerpt ends at multicore application start and therefore does not establish
EK79007/LVGL initialization, C6/ESP-Hosted health, association, DHCP or
display continuity.
```

## 2026-09-08 — Fase 3, associação Wi-Fi aberta e DHCP

```text
Board and image: same ESP32-P4 v1.3 unit and P4 application SHA-256
20A8FF89AEA7743120D704D668B30BDE1BE4415A4B36F00487A5DAED03AF1EE5;
C6 was not flashed.

Procedure: after the physical 60-second arm control, the one-shot local
maintenance bridge returned OK for a temporary open 2.4 GHz Wi-Fi AP with
DHCP. The request was delivered without recording the SSID: esp-hosted logged
STA set_config and the P4 worker logged that association was requested from
its private mailbox.

Observed panel status: "IP adquirido"; APs=35; attempts=0; IP=yes; last
result=ESP_OK. This demonstrates P4-to-C6 ESP-Hosted/SDIO control, Wi-Fi STA
association and DHCP completion for the temporary open-network path.

Gate status: initial association/DHCP sub-gate passes. This does not validate
the protected-network onboarding path, DNS/NTP/HTTPS, AP loss/recovery, C6
reset/recovery, or the strict continuous-render campaign.
```

## 2026-09-08 — Fase 3, limpeza RAM-only após associação

```text
Procedure: the physical arm control was used again and the local maintenance
bridge returned OK for FORGET.

Observed result: esp-hosted set the STA configuration to an empty SSID; the
P4 logged "RAM-only station configuration cleared"; the panel showed
"Wi-Fi pronto", APs=35, attempts=0, IP=no and last result=ESP_OK. The C6
then published StaDisconnected with reason=8 and RSSI=-28. No retry was
scheduled because the active RAM-only credentials had already been cleared.

Follow-up: the pre-flash filter recognized only reason=36 for a local
disconnect, so it emitted an unnecessary warning for this expected reason=8
callback. The follow-up source change consumes the single callback following
the application's own disconnect request, whatever its driver reason; it is
not yet physical evidence until rebuilt and flashed.

Gate status: association normal execution 1/3, including explicit RAM-only
forget, passes functionally. Continuous display/touch observation, repeated
runs and fault recovery remain open.
```

## 2026-09-08 — Fase 3, correção do callback local de FORGET gravada

```text
Follow-up P4 application SHA-256:
DC702B0F729A6A3BB30550E3D38BC8C6CF9C68E07B1A27ED2550D596631197FA.
The compiled image remained 0x118520 bytes with 0x6E7AE0 bytes (86%) free in
the smallest app partition. Esptool hash-verified every written P4 block and
requested a hard reset. C6 was untouched.

The change consumes the single asynchronous disconnect callback caused by the
application's own configuration replacement or FORGET request. It awaits a
new physical association/FORGET cycle before claiming this log classification
is validated.
```

## 2026-09-08 — Fase 3, ciclo completo na imagem de callback corrigido

```text
Procedure: on the P4 image SHA-256
DC702B0F729A6A3BB30550E3D38BC8C6CF9C68E07B1A27ED2550D596631197FA, the
temporary open AP was joined through the physically armed bridge and then
explicitly forgotten through the same one-shot path.

Association observation: panel reported "IP adquirido", APs=36, attempts=0,
IP=yes and last result=ESP_OK. Forget observation: bridge returned OK;
esp-hosted set an empty STA SSID; the worker logged that the RAM-only
configuration was cleared; C6 published StaDisconnected reason=8, RSSI=-30;
and the worker logged "ignored expected station disconnect while replacing
RAM-only config". Panel then reported "Wi-Fi pronto", APs=36, attempts=0,
IP=no and last result=ESP_OK.

Gate status: first complete association/FORGET cycle on the corrected image
passes. The earlier pre-correction cycle is discovery evidence and is not
counted toward the required three consecutive corrected-image runs. Two
complete runs, display/touch continuity observation and recovery faults remain
open.
```

## 2026-09-08 — Fase 3, terceiro ciclo corrigido em estado quente

```text
Starting condition: the P4 remained powered after the second corrected cycle,
with no credential retained in RAM.

Association observation: bridge returned OK; esp-hosted applied the temporary
open-AP STA configuration; panel reported "IP adquirido", APs=44,
attempts=0, IP=yes and last result=ESP_OK. Forget observation: bridge returned
OK; the worker cleared RAM-only configuration; C6 published StaDisconnected
reason=8, RSSI=-30; the worker classified it as an expected local disconnect.
Panel then reported "Wi-Fi pronto", APs=44, attempts=0, IP=no and last
result=ESP_OK.

Gate status: three consecutive corrected-image association/FORGET cycles pass
(one before and one after cold boot, followed by this warm-state cycle). This
closes only the normal open-AP association sub-gate. Display/touch continuity
was not measured in this evidence, and AP/DHCP/C6 fault recovery remains open.
```

## 2026-09-08 — Fase 3, primeira tentativa de AP ausente

```text
Operator observation: after disabling the AP and waiting well beyond 60
seconds, the panel still displayed "IP adquirido". The observation has no
independent record that the 2.4 GHz AP radio was absent, so it is inconclusive
as an AP-outage injection. It cannot approve recovery and it cannot yet
distinguish a live AP from missing remote beacon-timeout configuration.

Follow-up under test: P4 source now explicitly sets the C6 STA inactive time
to 6 seconds through ESP-Hosted/esp_wifi_remote and reads it back after Wi-Fi
start. The modified P4 build passes but has not been flashed or physically
validated at the time of this entry. C6 is not to be reflashed for this test.
```

## 2026-09-08 — Fase 3, imagem de liveness explícito gravada

```text
P4 application SHA-256:
6536F7F86DFACA2D17F736239093DE74A282EE82CA3518206F23629602977DAE.
The image was 0x118690 bytes with 0x6E7970 bytes (86%) free in the smallest
app partition. Esptool hash-verified all written P4 blocks and requested a
hard reset. C6 was not flashed.

This image sets the remote STA inactive time to 6 seconds and reads it back
through ESP-Hosted before declaring Wi-Fi ready. Its physical boot readback
and AP-outage behavior remain pending.
```

## 2026-09-08 — Fase 3, liveness do C6 confirmado no boot

```text
Observed serial boot on the liveness-image P4: Hosted SDIO link came up;
the P4 initialized the RAM-only station; the C6 returned a confirmed STA
inactive time of 6 seconds through ESP-Hosted; and the credential-free scan
completed with 38 APs.

Gate status: remote inactive-time configuration and readback pass on physical
hardware. AP-outage detection, bounded retry, UI continuity and recovery are
still pending physical validation.
```

## 2026-09-08 — Fase 3, bridge diagnóstica e baseline de carga gravados

```text
P4 application SHA-256:
A9B8C334C48B6BD167C3D866B547330ABE78DCA9D404AE0793B72C04ED917C5B.
The image was 0x11B400 bytes with 0x6E4C00 bytes (86%) free in the smallest
app partition. Esptool hash-verified every written P4 block and requested a
hard reset; C6 was not flashed.

Changes pending physical validation: an armed bridge rejection now returns an
ESP-IDF error name without SSID material, and the render-load button publishes
the baseline once on activation while preserving the no-periodic-redraw policy
for the active load. An AP-off attempt in progress was interrupted by this P4
update and is invalidated rather than counted as a liveness result.
```

## 2026-09-08 — Fase 3, AP-off com timeout explícito (exploratório)

```text
Starting state: temporary open AP associated, panel reporting IP acquired, and
render load active. Baseline shown during load: SRAM minimum 251 KiB and PSRAM
minimum 28137 KiB. The complete initial largest-block line was not captured
before AP removal.

Injection observation: after the AP was disabled, the panel left "IP
adquirido" and displayed association/recovery. This confirms that the
configured C6 inactivity liveness path did not retain a stale online state.
After AP restoration, the panel reported IP acquired, APs=32, attempts=0,
IP=yes and last result=ESP_OK. End-of-observation memory: SRAM free=261 KiB,
largest=172 KiB; PSRAM free=28137 KiB, largest=27648 KiB; soak minima remained
251 KiB SRAM and 28137 KiB PSRAM.

Gate status: functional AP-loss detection and later recovery were observed.
This is exploratory only: elapsed recovery time, full initial memory snapshot,
reset counters, and explicit display/touch continuity observation were not
recorded, so it cannot approve the AP-absent case.
```

Operator follow-up: AP restoration to "IP adquirido" took 25 seconds. No
screen artifact, touch failure or P4 reboot was reported during the 10-minute
AP absence. The recovery time satisfies the 90-second limit; the round remains
exploratory only because its initial full memory snapshot and reset evidence
were incomplete.

Operator-reported AP-off/AP-on repetition: same result as the preceding
round—recovery to IP acquired in 25 seconds; no screen artifact, touch failure
or reboot; soak SRAM minimum 251 KiB; soak PSRAM minimum 28137 KiB; SRAM free
261 KiB with 172 KiB largest block; PSRAM free 28137 KiB with 27648 KiB largest
block. No video was recorded by operator decision. This is the second observed
functional AP-absent repetition; the absence of a video is retained as an
evidence limitation rather than represented as a capture.

Operator closure statement: more than ten cumulative AP-off/AP-on rounds had
the same outcome as above. The operator explicitly declined further
repetition and video capture. This is accepted as user-supplied campaign
history, not substituted raw per-round evidence; its missing timestamps,
initial snapshots and serial artifacts remain the limitation of this
sub-gate.

## 2026-09-08 — Fase 3, segundo ciclo corrigido após boot frio

```text
Starting condition: the P4 was cold-booted with no RAM-only credential. One
initial bridge request returned ERR without changing Wi-Fi state; a repeated,
physically armed request was accepted and is the only request counted below.

Association observation: bridge returned OK; esp-hosted set the temporary
open-AP configuration; panel reported "IP adquirido", APs=44, attempts=0,
IP=yes and last result=ESP_OK. Forget observation: bridge returned OK; C6
published StaDisconnected reason=8, RSSI=-26; the corrected worker logged
"ignored expected station disconnect while replacing RAM-only config"; panel
returned to "Wi-Fi pronto", APs=44, attempts=0, IP=no and last result=ESP_OK.

Gate status: second complete corrected-image association/FORGET cycle passes.
The preceding ERR did not reach station configuration and remains an
unclassified maintenance-bridge rejection; it is not treated as an association
or recovery result. One warm-state cycle and continuity/fault observations
remain open.
```

## 2026-09-08 — Fase 3, candidato DNS/NTP/HTTPS não gravado

```text
P4 candidate application SHA-256:
0C80B0DDE4D5C087719BE1A20A3FBAF351891FDA6F083B27BF1741416E8AFB82.

Change under test: the physically armed USB maintenance bridge now accepts
CHECK only while the RAM-only STA has an IP. A single 8 KiB background worker
performs bounded DNS, one-shot NTP and one HTTPS HEAD in sequence. The HTTPS
path uses the ESP-IDF CA bundle and does not disable certificate or hostname
validation. It accepts no hostname, URL, CA, body, credential or token from
the console. Two DNS failures can request one worker-owned reassociation per
minute. GPIO54 remains owned by ESP-Hosted; this candidate does not take the
pin or flash/reset the C6 directly.

Build: ESP-IDF 5.5.4 `idf.py build` and `idf.py size` passed. The P4 image was
0x16BA70 bytes; the smallest 8 MiB OTA partition retained 0x694590 bytes
(82%). The size report placed 1,003,872 bytes of text and 367,732 bytes of
rodata in external RAM, with 202,106 bytes of DIRAM used. The map parser
emitted its known address-placement warning for `.rodata.__func__.0`; the
link and binary generation succeeded.

Flash attempt: `idf.py -p COM8 flash` was stopped before writing because COM8
was held by the VS Code monitor (`PermissionError: access denied`). Therefore
this entry is build evidence only: no P4 block was written, no reset was sent,
and C6 was untouched. Boot, CHECK, DNS/NTP/HTTPS, memory and UI continuity
remain pending physical validation on this candidate.
```

## 2026-09-08 — Fase 3, candidato DNS/NTP/HTTPS gravado no P4

```text
The same candidate P4 application SHA-256
0C80B0DDE4D5C087719BE1A20A3FBAF351891FDA6F083B27BF1741416E8AFB82 was
subsequently flashed after COM8 was released from the VS Code monitor.

Command: ESP-IDF 5.5.4 `idf.py -p COM8 flash`.
Result: esptool identified ESP32-P4 revision v1.3, wrote bootloader,
application, partition table and OTA data, hash-verified every written block,
and issued a P4 hard reset. The application write was 1,489,520 bytes at
0x20000. C6 was not flashed, reset, or otherwise modified by this operation.

Physical boot, CHECK results, render/touch continuity, memory snapshots and
fault cases remain pending and must be captured separately.
```

## 2026-09-09 — Fase 3, DNS/NTP/HTTPS com AP aberto

```text
Board and image: same ESP32-P4 v1.3 unit on USB Serial/JTAG COM8, candidate
application SHA-256
0C80B0DDE4D5C087719BE1A20A3FBAF351891FDA6F083B27BF1741416E8AFB82. C6 was
not flashed or reset.

Procedure: the temporary open AP was associated through the physically armed
RAM-only USB bridge. The panel then showed IP acquired. With the bridge armed
again, a literal ASCII CHECK plus LF was sent directly through COM8; the bridge
returned OK.

Observed result on the panel: DNS=ESP_OK, NTP=ESP_OK, HTTPS=ESP_OK in 5112 ms.
This proves the serial external-check executor resolved its fixed host,
obtained usable NTP time and completed its one verified HTTPS HEAD request in
less than the 20-second total deadline. It does not establish a general
provider, credential retention, negative DNS behavior, TLS rejection, C6
recovery, visual continuity or memory bounds for this round because those
observations were not supplied.
```

## 2026-09-09 — Fase 3, gatilhos DNS/TLS controlados gravados

```text
P4 candidate application SHA-256:
18FB194B688438A973CB895BCA61DB4930EB8A6C33652DB2150BF4DC3FDD8D84.

Change under test: the armed physical bridge has fixed CHECK_NXDNS,
CHECK_DNS_TIMEOUT and CHECK_TLS_REJECT modes. NXDOMAIN resolves only a
reserved `.invalid` name. DNS timeout saves all host DNS state, replaces it
temporarily with the reserved TEST-NET-1 address 192.0.2.1, then restores it
before reporting. TLS rejection uses a fixed expired-certificate endpoint and
retains CA and hostname verification. None accepts a host, URL, CA or secret.

Build: ESP-IDF 5.5.4 build and size passed. Image was 0x16C1F0 bytes, leaving
0x693E10 bytes (82%) in the smallest OTA partition; 1,005,280 bytes text and
368,252 bytes rodata were mapped in external RAM. The known map-parser
warning for `.rodata.__func__.0` appeared without link failure.

Flash: `idf.py -p COM8 flash` detected ESP32-P4 revision v1.3, wrote and
hash-verified bootloader, application (1,491,440 bytes at 0x20000), partition
table and OTA data, then hard-reset P4. C6 was not flashed or reset. Physical
fault observations are pending.
```

## 2026-09-09 — Fase 3, DNS NXDOMAIN controlado

```text
Board and image: same ESP32-P4 v1.3 and P4 application SHA-256
18FB194B688438A973CB895BCA61DB4930EB8A6C33652DB2150BF4DC3FDD8D84. C6 was
not flashed or reset.

Procedure: after the open AP associated and the panel reported IP acquired,
the physical USB window was armed and CHECK_NXDNS was sent through COM8.
The bridge logged `maintenance command=CHECK_NXDNS result=ESP_OK` and returned
OK.

Observed worker result: mode=1; DNS=ESP_ERR_NOT_FOUND;
NTP=ESP_ERR_INVALID_STATE; HTTPS=ESP_ERR_INVALID_STATE; duration=19 ms. The
negative DNS result therefore did not incorrectly start NTP or TLS, did not
wait for the five-second timeout and did not alter the C6. Visual continuity,
touch and memory after this round were not independently supplied.
```

## 2026-09-09 — Fase 3, timeout DNS e reassociação limitada — primeira rodada

```text
Board and image: same ESP32-P4 v1.3 and P4 application SHA-256
18FB194B688438A973CB895BCA61DB4930EB8A6C33652DB2150BF4DC3FDD8D84. C6 was
not flashed or reset.

Procedure: after the preceding NXDOMAIN failure and a restored open-AP IP,
CHECK_DNS_TIMEOUT was accepted through the physically armed bridge. The host
DNS configuration was restored by the test before it reported the result.

Observed serial result: mode=2; DNS=ESP_ERR_TIMEOUT;
NTP=ESP_ERR_INVALID_STATE; HTTPS=ESP_ERR_INVALID_STATE; duration=7000 ms.
The second consecutive DNS failure caused exactly one bounded reassociation:
the worker requested it at 542215 ms, scheduled a 2398 ms retry, and obtained
the IP address at 548795 ms. No P4 or C6 reset is present in the supplied log.

Gate status: recovery behavior is positive, but DNS deadline is not approved.
The blocking resolver returned at 7 s, exceeding the 5 s contract. The next
firmware increment must isolate the resolver and return control at 5 s before
this case can count as a pass.
```

## 2026-09-09 — Fase 3, correção do deadline DNS gravada

```text
P4 application SHA-256:
90E608F7CBCB33A4544957993D4EDDAFE9EC7EE89A9D83CFAB6CC6131051B228.

Change under test: getaddrinfo now executes in a 4 KiB disposable resolver
task. The check worker waits no more than 5 s. If a resolver response arrives
late, it remains isolated and prevents only a second concurrent DNS query;
the UI and the external-check executor are released at the deadline. The
temporary DNS server restoration still occurs before the result is published.

Build and flash: ESP-IDF 5.5.4 build passed; image size 0x16C380 with
0x693C80 bytes (82%) free in the smallest OTA partition. `idf.py -p COM8
flash` identified P4 revision v1.3, hash-verified all bootloader,
application, partition-table and OTA-data writes, and requested a P4 hard
reset. C6 was not flashed or reset. Physical timeout revalidation is pending.
```

## 2026-09-09 — Fase 3, timeout DNS com deadline aprovado

```text
Board and image: same ESP32-P4 v1.3 and P4 application SHA-256
90E608F7CBCB33A4544957993D4EDDAFE9EC7EE89A9D83CFAB6CC6131051B228. C6 was
not flashed or reset.

Procedure: after OPEN was accepted on the physically armed bridge, the
bridge accepted CHECK_DNS_TIMEOUT. The command held COM8 open for 12 seconds
to collect the asynchronous worker result.

Observed result: mode=2; DNS=ESP_ERR_TIMEOUT;
NTP=ESP_ERR_INVALID_STATE; HTTPS=ESP_ERR_INVALID_STATE; duration=5000 ms.
The DNS check returned precisely at the five-second contract boundary, and it
did not start NTP or TLS. No P4/C6 reset is present in the supplied log.

Gate status: DNS-timeout deadline and separation of downstream work pass for
this round. UI/touch and memory observations remain pending for the broader
continuity campaign.
```

## 2026-09-09 — Fase 3, rejeição TLS com validação ativa

```text
Board and image: same ESP32-P4 v1.3 and P4 application SHA-256
90E608F7CBCB33A4544957993D4EDDAFE9EC7EE89A9D83CFAB6CC6131051B228. C6 was
not flashed or reset.

Procedure: the physical bridge accepted CHECK_TLS_REJECT on an associated open
AP.

Observed result: mode=3; DNS=ESP_OK; NTP=ESP_OK;
HTTPS=ESP_ERR_HTTP_CONNECT; duration=4583 ms. ESP-IDF logged that no matching
trusted root was found, certificate verification failed, the mbedTLS handshake
failed with -0x3000, and the HTTP client could not open the connection. The
firmware did not disable CA or hostname validation to make this pass, and it
did not accept the HTTPS request. No P4/C6 reset appears in the supplied log.

Gate status: TLS rejection with the CA bundle enabled passes. Render/touch and
memory were not independently recorded for this round.
```

## 2026-09-11 — Fase 3, soak de conectividade e fechamento no escopo da unidade

```text
Operator report: the panel remained associated to the laboratory Wi-Fi for
more than 24 hours. No failure was observed. The touchscreen remained
functional throughout the soak, with no reported display artifact, reset,
panic, or loss of local operation.

Scope: this observation complements the recorded association, timeout, TLS,
Hosted recovery/cooldown, and WPA2 RAM-only campaigns. It is operator-observed
evidence rather than an exported continuous serial or heap time series.

Known exclusion: this unit has no operator-safe C6 isolation point and the
operator decided not to alter hardware. The literal C6 physically
absent/travado scenario is outside this unit's G3 scope; application firmware
does not take GPIO54 from ESP-Hosted to emulate it.

Gate decision: G3 is closed for promotion to G4 in this scoped hardware
configuration. This is not a production qualification: G6 retains the 72/168 h
instrumented soak, thermal/power campaign and broader multi-unit evidence.
```

## 2026-09-11 — Fase 4, corrupção mais nova e reboot com fallback confirmado

```text
Board: ESP32-P4 revision v1.3 over USB Serial/JTAG COM8. The current P4 image
was built with ESP-IDF 5.5.4; np2_p4.bin was 0x1710C0 bytes, with 0x68EF40
bytes (82%) free in the smallest OTA partition. `idf.py -p COM8 flash` wrote
and hash-verified bootloader, P4 application, partition table and otadata only.
It did not write NVS, storage or C6 firmware.

Procedure: with render active, the operator ran the LittleFS diagnostic batch.
The coordinator reported `LittleFS batch 2 completed in 5287ms`. The physically
armed USB maintenance bridge then accepted `CACHE_CORRUPT_NEWEST` and returned
OK. The asynchronous worker reported `cache newest-generation corruption 3
completed in 99ms`. The P4 was rebooted.

Observed result: the post-reboot dashboard reported `Geracao 126 integra |
ativo` in CACHE LOCAL. The supplied boot shows normal P4 boot, 32 MiB PSRAM,
display/touch start, and C6 Hosted 3.0.6/RPC v2/SDIO SW_AGGR initialization;
there is no supplied panic, WDT or storage format. The coordinator only reports
the corruption request as complete after selecting a valid prior generation,
so generation 126 is the recovered fallback.

Gate interpretation: the controlled corrupt-newest and reboot recovery path
passes again on this unit after the portable cache-record contract refactor.
This does not close G4: power interruption at every write boundary, full/quota
filesystem behavior, configuration partial writes, cache age/schema migration,
and real offline product data remain pending.
```

## 2026-09-12 — Fase 4, recuperação de tela preta e boot observado

```text
Board: ESP32-P4 revision v1.3, USB Serial/JTAG COM8. The candidate P4 image
was `np2_p4` version 1d3ca02-dirty, ELF SHA prefix 07025ae3d. No C6, NVS or
storage write was made during this diagnosis.

Incident: after maintenance-console attempts, the panel was reported black.
The first monitor capture failed because a residual IDF monitor process held
COM8 (`PermissionError: access denied`). That process tree was terminated.

Observed boot after a physical RESET with an IDF monitor attached using
`--no-reset`: 32 MiB PSRAM test passed; MIPI DSI and EK79007 initialized;
GT911 was found at 0x5d; LVGL task started; initial refresh completed;
backlight was set to 60%; and the application logged `Phase 4 offline
dashboard active`. Hosted then negotiated C6 3.0.6, RPC v2 and SDIO SW_AGGR,
and completed a credential-free scan of 33 APs. No panic, WDT, mount error or
display-initialization failure appears in the captured boot. The operator
confirmed that the panel rendered again after this reset.

Interpretation: this is recovery evidence for the incident, not a G4 cache
pass. The full-filesystem and power-cut scenarios remain unexecuted.
```
# 2026-09-12 — G4 filesystem cheio: tentativa inválida/falha de implementação

- Placa P4 v1.3; imagem P4 de bancada `0x172d60`; C6, NVS e gerações de cache
  não foram gravados pelo flash da aplicação.
- O botão `G4: FS CHEIO` foi aceito na fila. Com a carga sintética contínua
  ativa, o pedido permaneceu pendente; ao pausá-la, o worker executou e a UI
  reportou `G4 FS cheio concluido: ESP_FAIL em 114619ms`.
- Resultado: **não conta como passe G4**. A investigação encontrou que
  `cache.tmp` era limpo somente após a tentativa de proposta, podendo manter
  blocos alocados e invalidar a condição de filesystem cheio. O ensaio foi
  interrompido por flash P4 de recuperação; o próximo boot limpa somente
  `full-probe.tmp` e uma repetição corrigida continua obrigatória.
- A repetição posterior concluiu `ESP_FAIL` em 113574 ms, e uma terceira
  repetição concluiu `ESP_FAIL` em 115055 ms. A causa adicional foi
  identificada: o preenchimento em 16 KiB aceitava `ENOSPC` diretamente,
  sem executar a cauda menor. A próxima imagem deve retentar, após esse
  `ENOSPC`, no tamanho exato da proposta de cache (cabeçalho + payload);
  essas tentativas não contam como passe.
- A imagem seguinte alcançou `ESP_ERR_INVALID_STATE` em 113513 ms: a proposta
  temporária foi aceita e chegou ao `rename`, portanto a geração foi alterada.
  Isso também não conta como passe. O ensaio passa a usar um segundo arquivo
  descartável para consumir o espaço residual e exerce somente o caminho
  `cache.tmp + write + fsync`, sem promover nem sobrescrever `cache.0/1`.
- A versão sem `rename` também alcançou `ESP_ERR_INVALID_STATE` em 227923 ms.
  Isso confirmou que LittleFS mantém reserva alocável após os dois arquivos de
  preenchimento receberem `ENOSPC`; as gerações ativas foram preservadas. A
  próxima versão consome essa reserva por propostas descartáveis limitadas,
  cada uma com o formato e `fsync` do cache, até observar a rejeição.

## 2026-09-12 — Remediação A1–A6, build limpo e flash do candidato P4

```text
Placa: ESP32-P4 revisão v1.3, USB Serial/JTAG COM8. C6 não foi atualizado ou
reiniciado separadamente.

Build: ESP-IDF 5.5.4-dirty, target esp32p4, diretório novo
build/audit-fix-final-20260912. A aplicação mede 0x173160 bytes; o menor slot
OTA tem 0x68cea0 bytes (82%) livres. SHA-256 de np2_p4.bin:
8D5164425F3F70B40A3EFFB0B04D3421DD3DB80F5B13D778A16F9C9803CA5EA6.
SHA-256 do sdkconfig:
078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A.

Validação de software: quatro testes host (cache record, codec/semântica,
parser JSON estrutural e formatação de sinal) passaram com C11,
-Wall -Wextra -Werror. O build limpo terminou sem erro.

Flash: esptool.py 4.12.0 gravou bootloader em 0x2000, tabela de partições em
0x10000, otadata em 0x1b000 e a aplicação em 0x20000. O hash de cada bloco foi
verificado e houve hard reset por RTS. NVS, storage, imagem C6 e eFuses não
foram escritos.

Interpretação: esta entrada prova build e gravação da imagem corrigida. Não é
um passe funcional dos ensaios HTTPS lento/excedido, filesystem cheio ou
cortes de energia; esses gates continuam pendentes.
```

## 2026-09-12 — G4, novo relato de filesystem cheio e correção candidata

- O operador relatou `concluido: ESP_ERR_INVALID_STATE em 234461ms`.
  Não forneceu captura serial completa ou hash dessa imagem nesta interação;
  não se atribui o resultado a um artefato exato. A tentativa continua reprovada.
- A investigação host reproduziu a aceitação de todas as propostas com o
  núcleo LittleFS fixado: após ENOSPC, o arquivo não sincronizado podia ser
  fechado sem persistir o preenchimento. A causa de software está no ADR-020.
- Foi corrigido o preenchimento com checkpoints, reabertura após ENOSPC,
  preservação da causa de erro e limpeza verificada em cada saída. A UI
  diferencia aprovação de falha. O teste host usa as rotinas reais extraídas
  do coordenador e LittleFS sobre NOR simulada de 9 MiB.
- A correção passou os cenários host de cache técnico e snapshot real,
  repetição/remontagem, I/O, limpeza e prazo. Não houve flash, acesso serial,
  corte de energia nem operação C6 nesta atividade. Falta repetir na placa
  com a imagem candidata identificada, log e observação de UI/reboot; G4 aberto.
- Build limpo candidato: ESP-IDF 5.5.4, target `esp32p4`, base
  `1d3ca02-dirty` com alterações locais preservadas;
  `idf.py -B build/full-probe-fix-20260912 -D SDKCONFIG=build/full-probe-fix-20260912/sdkconfig -D IDF_TARGET=esp32p4 build`.
  App `0x173290`, 82% livre no slot OTA. Dois warnings de geração de
  `esp_rom gdbinit`; compilação/link concluíram com código zero.
  SHA256 P4: `BC812CF7E19A6B9464FF5333D243EC3A430A1D175A824AC7C937ADE96B0BDFC6`.
  SHA256 sdkconfig: `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`.
  Artefatos e log em `firmware/build/full-probe-fix-20260912*`, ignorados pelo
  Git. Esta imagem não foi gravada. Teste reproduzível:
  `./tools/run_cache_full_probe_host_test.ps1`.

## 2026-09-12 — G4, primeira aprovação relatada do ensaio cheio corrigido

- Resultado informado pelo operador após as instruções de flash/reteste:
  `aprovado esp_ok em 154535ms` — 154,535 s (2 min 34,535 s).
- A UI reportou aprovação da operação; no código candidato esse resultado
  exige rejeição por ENOSPC, limpeza confirmada, recuperação do espaço e
  preservação do registro selecionado. É o primeiro resultado positivo
  relatado após a correção de checkpoints do ADR-020.
- A imagem indicada no procedimento foi `build/full-probe-fix-20260912`,
  SHA256 `BC812CF7E19A6B9464FF5333D243EC3A430A1D175A824AC7C937ADE96B0BDFC6`.
  Não houve nesta resposta captura do flash, identificação do binário em
  execução, log serial ou números de geração; a associação ao artefato é
  contexto do procedimento, não uma verificação independente.
- Após RESET, o operador confirmou no painel: `geracao 137 integra | cache v1
  dados v0`. Isso confirma a seleção de uma geração de cache válida após a
  limpeza/reboot, sem dados de domínio inventados (`dados v0`).
- Pendentes: segunda execução, observação explícita de continuidade visual/touch
  e logs do ensaio. Este passe operacional parcial não fecha G4 nem os testes
  de corte de energia/dados offline.

## 2026-09-12 — G4, imagem de aviso visível para corte físico

- O primeiro acionamento de `G4: CORTE PRE-RENAME` expirou em
  `ESP_ERR_TIMEOUT` após 10.114 ms sem que o operador visse um aviso na tela.
  Isso não é um teste físico de recuperação e não é contado como falha do
  journal: o marco `POWER_CUT_NOW` existia somente no serial.
- A interface agora lê a janela de corte publicada pelo `FlashCoordinator` e
  mostra `G4: DESLIGUE AGORA! antes rename (10s)` ou
  `G4: DESLIGUE AGORA! apos rename (10s)`. Se ninguém desligar a alimentação,
  ela mostra `G4 corte: NAO VALIDADO (sem corte em 10s)`.
- Build limpo e flash em 2026-09-12: ESP-IDF 5.5.4, target `esp32p4`,
  `build/power-cut-ui-fix-20260912`; app `0x173480`, com `0x68CB80` livres
  no slot OTA de 8 MiB. SHA256 P4:
  `13F61D7023E7F38DC1046524D637F377F00DA7F35EED9E9B48D6A0A13AD592FF`;
  SHA256 do sdkconfig:
  `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`.
- Esptool gravou bootloader, tabela de partições, OTA-data e app em COM8 e
  verificou o hash de cada bloco. A placa identificada foi ESP32-P4 v1.3.
  O C6 não foi gravado nem acessado. Este registro prova a imagem de teste;
  os dois cortes continuam pendentes de execução física.

## 2026-09-12 — G4, TimeService: build e flash P4

- Commit P4: `2445f05` (`Tools: habilitar refresh manual de dados offline`).
  A imagem contém o `TimeService`, que mantém SNTP disponível após a
  inicialização e publica a confiança de hora consumida pelos cards offline.
- Build: ESP-IDF 5.5.4, target `esp32p4`, diretório
  `build/time-service-20260912`. Aplicação `0x174e70`, com `0x68b190` bytes
  (82%) livres no menor slot OTA. SHA256 P4:
  `5B62C7F3C4D26CC0E03512D543AADC6F5AFE5C911621FE7CC0AD518863D8E459`.
  SHA256 do sdkconfig:
  `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`.
- Flash: `idf.py -B build/time-service-20260912 -p COM8 flash` detectou
  ESP32-P4 revisão v1.3. Esptool 4.12.0 gravou e verificou por hash
  bootloader (`0x2000`), aplicação (`0x20000`), tabela de partições
  (`0x10000`) e OTA-data (`0x1b000`), seguido de hard reset via RTS.
  NVS, `storage`, C6 e eFuses não foram gravados.
- Este registro comprova a gravação do candidato. Ainda falta o ensaio
  funcional: após rede e NTP `ESP_OK`, disparar refresh Brasília e confirmar
  `hora confiavel` no rodapé e `ha 0 min` nos dois cards.

## 2026-09-12 — G4, fechamento físico reportado pelo operador

- O operador confirmou que executou os ensaios restantes na unidade P4 v1.3 e
  que todos funcionaram corretamente: corrupção/reboot, filesystem cheio com
  limpeza, corte antes do `rename`, corte após o `rename`, refresh Brasília e
  reboot sem rede.
- O painel permaneceu funcional; após NTP e refresh os dois cards mostraram
  dados locais com hora confiável e idade inicial correta. Após reboot sem
  rede, os dados locais permaneceram disponíveis em estado seguro.
- Esta é uma confirmação operacional do operador, sem novo log serial bruto
  anexado nesta interação. Ela fecha G4 com escopo limitado a esta unidade;
  qualificação multiunidade, fault injection e campanhas longas continuam em
  G6.

## 2026-09-12 — G5, políticas portáveis e build P4 inicial

- Base Git: `e66001982f2c9f328f7d26bd45b14133a293aa37`, mais alterações
  locais deste incremento (`update_policy.c/.h`, entrada CMake, teste host e
  documentação ADR-022/G5). Evidência exclusivamente de software, sem flash
  ou nova observação física da unidade.
- `tools/run_update_policy_host_test.ps1` passou com GCC MSYS2 15.2.0,
  C11, `-O2 -Wall -Wextra -Werror -pedantic`. Casos: metadados
  incompatíveis/limites, app pendente/ocupada, recusa de troca C6/bootloader/
  partições, saúde por 15 s, falta de progresso, observação perdida,
  timestamps inválidos, deadline 60 s e fallback ausente.
- Build limpo em diretório novo com ESP-IDF 5.5.4, target explícito P4:
  `idf.py -B build/g5-policy-20260912 -D SDKCONFIG=build/g5-policy-20260912/sdkconfig -D IDF_TARGET=esp32p4 build`.
  As 2096 etapas passaram, incluindo compilação de `update_policy.c`; nenhum
  warning de compilação foi encontrado no log. `idf.py -B
  build/g5-policy-20260912 size` também passou.
- App `0x175100` B, slot OTA `0x800000`, livre `0x68af00` B (82%).
  Bootloader `0x5a50` B, livre `0x85b0` B. DIRAM estática no relatório:
  223.394 B; isto não mede heap/pilhas em runtime. A política não possui
  consumidor no app, portanto pode ser removida do link por garbage collection.
- SHA-256 P4:
  `DBCF41EA7E15B782B2A42831B93007CAA80A770838F903B5817B253D98CE2DDF`.
  SHA-256 sdkconfig:
  `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`.
  SHA-256 tabela gerada:
  `9AB122C32036A53AA7F0DCB9422DCF89F007C15C5DEE5467101FD3779F4406A6`.
- Configuração efetiva preserva P4 revisão mínima 100, flash 32 MiB,
  XiP experimental em PSRAM, três FB, TLS interno, reset C6 ativo baixo e
  rollback habilitado. Auto-suspend, ESP_HOST_WIFI, reinício P4 por falha
  Hosted, Secure Boot, Flash Encryption e anti-rollback permanecem desligados.
- Logs locais ignorados: `firmware/build/g5-policy-20260912-build.log` e
  `firmware/build/g5-policy-20260912-size.log`. C6 não foi compilado ou
  regravado; seu binário instalado/hash permanece pendência de inventário.
  A inspeção do exemplo C6 e os próximos passos estão em `G5-VALIDATION.md`.
- G5 continua aberto: não há assinatura, streaming, journal, confirmação
  real de slot ou ensaio de rollback/cortes neste incremento. Nenhum resultado
  acima comprova OTA recuperável nem promove XiP a baseline de produção.

## 2026-09-12 — G5, flash P4 e captura de boot

- Por instrução explícita do responsável, build e flash passam a compor a
  entrega de alterações de firmware, conforme `AGENTS.md`. O incremento G5
  acima foi gravado na mesma unidade pela COM8, identificada pelo esptool
  como ESP32-P4 revisão v1.3, aproximadamente às 23:50–23:52 (UTC−03).
- Comando: `idf.py -B build/g5-policy-20260912 -D IDF_TARGET=esp32p4 -p COM8 flash`.
  O comando verificou o build e gravou bootloader em `0x2000`, app em
  `0x20000`, tabela em `0x10000` e otadata em `0x1b000`, com verificação
  de hash de cada bloco e hard reset via RTS. Não gravou NVS, storage, C6
  ou eFuses. Isso é flash de desenvolvimento, não ciclo aplicar/reverter OTA.
- SHA-256 da app, confirmado antes/depois:
  `DBCF41EA7E15B782B2A42831B93007CAA80A770838F903B5817B253D98CE2DDF`.
  Base Git, configuração e tabela são as registradas no build anterior.
- Monitor oficial `esp_idf_monitor` 1.10.0: COM8, 115200, target `esp32p4`,
  ELF `build/g5-policy-20260912/np2_p4.elf`, captura limitada a 30 s, com
  reset inicial USB/UART. O processo criado para a captura foi encerrado ao
  final e a porta liberada.
- Boot observado: flash 32 MiB, PSRAM 32 MiB, app `e660019-dirty`, EK79007,
  GT911, LVGL e dashboard RGB565/180°/TRIPLE_PARTIAL/3 FB iniciados;
  backlight a 60% em 1724 ms e app pronta em 1764 ms. Hosted confirmou
  C6 3.0.6 pareado, RPC v2 e SW_AGGR; scan sem credenciais concluiu em
  6084 ms. Nenhum panic/WDT observado na janela capturada.
- Avisos preservados: adapter sem `on_frame_buf_complete`, reconhecimento
  de gestos LVGL desabilitado e reset C6 por GPIO54 na inicialização.
  O monitor também tentou decodificar os PCs salvos do reset com prefixo
  default Xtensa indisponível; é erro de ferramenta de decodificação, sem
  interrupção da captura serial. Futuras capturas devem passar explicitamente
  o prefixo RISC-V da toolchain. Não houve inspeção visual/touch nesta captura.
- Logs locais ignorados: `firmware/build/g5-policy-20260912-flash.log` e
  `firmware/build/g5-policy-20260912-boot.log`. O C6 não foi regravado e seu
  hash instalado permanece pendente. G5 continua aberto, com a política
  portável ainda sem consumidor em runtime.

## 2026-09-13 — G5, reprodução C6 e auditoria do patch SDIO

- Base de software: ESP-IDF 5.5.4 e Hosted 3.0.6, hash de componente
  `1b1c2aa8f82e0826950ec92ff16fd8f327abd2de6c8a3899301ad8cfb4747879`.
  A recipe versionada em `coprocessor/` resolveu seu próprio lock C6 e não
  duplicou o componente Hosted.
- Auditoria de patch: a referência da tag oficial IDF 5.5.4 teve SHA-256
  `32041DCBBD0E1F4DB26AF901C68A3E0804B5D01142B291314C5016CA0EA0A6AA`.
  O SDK local tinha SHA-256 bruto
  `1B04D1B8958BAE7B3AA9C5927CF4BD23C899A11036CE907F993F0553AAE7BF53`,
  correspondente ao arquivo original mais a única alteração oficial da guarda
  SDIO. `eh.py patch-idf --idf-path C:\esp\v5.5.4\esp-idf` foi chamado duas
  vezes e não modificou o arquivo; o patch e o relatório estão em
  `coprocessor/patches/`.
- Dois builds limpos C6 em diretórios distintos, `c6-repro-det-a-20260913` e
  `c6-repro-det-b-20260913`, target `esp32c6`, passaram sem warnings de
  compilação identificados. App `eh_cp_ota_coprocessor_ota.bin`: 1.108.176 B,
  SHA-256 `CFEDFC093A1BC28E70042E659D3C5274AA27FEFD125C429B014E5B9AC76CB136`.
  Bootloader e tabela também tiveram hashes iguais nas duas execuções. A app
  cabe nos slots C6 `ota_0/ota_1` de 1.835.008 B e no staging P4 de 2 MiB.
- Configuração/tabela efetivas: Hosted/RPC v2/Wi-Fi/SDIO SW_AGGR ativos;
  Secure Boot, Flash Encryption, anti-rollback, rollback C6, Bluetooth e
  auto-suspend inativos. A verificação e 11 testes negativos de recipe passaram.
- Não houve flash C6 ou alteração no rádio. A COM8 chega ao P4; `idf.py flash`
  deste projeto não é rota autorizada. Slave OTA continua bloqueado até
  inventário do bootloader/layout instalado, recovery autônomo e ensaio físico
  de recuperação. Esta evidência não fecha G5 nem declara a imagem recuperável.

## 2026-09-13 — G5, manifesto canônico e flash P4

- `update_manifest.c/.h` adiciona parser de envelope canônico P4 de 104 B e
  teste host. Ele aceita somente bytes exatos, campos reservados nulos, ID e
  SHA-256 não nulos e perfil P4; recusa C6, bootloader, tabela ou flags
  desconhecidas. Ainda não verifica RSA-PSS, hash de stream, key ID, download,
  journal ou seleção de slot.
- `run_update_manifest_host_test.ps1` e
  `run_update_policy_host_test.ps1` passaram com GCC C11, `-Wall -Wextra
  -Werror -pedantic`. O build limpo ESP-IDF 5.5.4 no diretório
  `build/g5-manifest-20260913`, target `esp32p4`, produziu app `0x175100` B;
  o menor slot OTA de `0x800000` B manteve `0x68af00` B livres. Bootloader:
  `0x5a50` B, com `0x85b0` B livres. O `idf.py size` reportou DIRAM estática
  de 223.394 B e apenas o aviso conhecido do parser de mapa.
- SHA-256 P4: `C5D9783155FE3203134FD55CBF3FF967D9F9ECDACC48F65281E9C014AAEC099A`;
  sdkconfig: `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`;
  tabela: `9AB122C32036A53AA7F0DCB9422DCF89F007C15C5DEE5467101FD3779F4406A6`.
  A configuração efetiva preservou P4, flash 32 MiB, rollback habilitado,
  Hosted, alvo C6 e reset SDIO ativo baixo; `ESP_HOST_WIFI` e auto-suspend
  continuaram desligados.
- Flash em COM8 concluiu com hash verificado para bootloader (`0x2000`), app
  (`0x20000`), tabela (`0x10000`) e OTA data (`0x1b000`); esptool identificou
  ESP32-P4 v1.3. NVS, `storage`, staging `c6_ota`, C6 e eFuses não foram
  gravados. A captura de boot de 30 s confirmou 32 MiB de flash/PSRAM,
  display EK79007, GT911, dashboard RGB565/180°/TRIPLE_PARTIAL/3 FB, backlight
  e app local prontos; o C6 negociou Hosted 3.0.6, RPC v2 e SW_AGGR, e o scan
  sem credenciais encontrou 48 APs. Não houve panic ou WDT na janela.
- A inspeção da API Hosted confirma que o descritor público do C6 devolve só
  a versão 3.0.6; ele não prova SHA, bootloader ou tabela instalada. Nenhuma
  alteração privada foi aplicada ao componente. O C6 permanece sem flash e a
  OTA conjunta permanece bloqueada pelos gates físicos.

## 2026-09-13 — G5, RSA-PSS destacada e flash P4

- `update_signature.c/.h` verifica, sem I/O, assinatura destacada sobre os
  bytes canônicos já validados: RSA-3072, PSS, SHA-256, MGF1 SHA-256 e salt de
  32 B. O envelope contém key ID e assinatura de 384 B; a chave pública DER é
  fornecida por um adapter futuro. Não há chave privada, keyring de produção,
  download, hash de stream, journal ou ativação neste corte.
- `run_update_signature_vector_test.ps1` usou vetor de chave efêmera fora do
  repositório. OpenSSL aceitou a assinatura original e recusou o manifesto
  alterado em um byte. A chave privada de teste foi removida após o ensaio.
  Os testes host existentes de manifesto e política também passaram.
- Build limpo ESP-IDF 5.5.4, target `esp32p4`, diretório
  `build/g5-signature-20260913`: 2.098 etapas, app `0x175100` B, menor slot
  OTA livre `0x68af00` B (82%), bootloader livre `0x85b0` B. SHA-256 P4:
  `41AED97DB17C7C825C209A2B8B0819C6B124330F8AA8536350DE31BC287B2862`;
  sdkconfig e tabela permaneceram respectivamente
  `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A` e
  `9AB122C32036A53AA7F0DCB9422DCF89F007C15C5DEE5467101FD3779F4406A6`.
- Flash COM8 gravou e verificou hash de bootloader, app, tabela e OTA data;
  esptool identificou P4 v1.3. NVS, `storage`, `c6_ota`, C6 e eFuses ficaram
  intocados. Na captura de 30 s, o display/touch e o dashboard ficaram prontos
  em 1.764 ms; C6 negociou Hosted 3.0.6, RPC v2 e SW_AGGR em 3.184 ms, e o
  scan sem credenciais encontrou 32 APs em 6.084 ms. Nenhum panic/WDT ocorreu.

## 2026-09-13 — G5, hash streaming da imagem e flash P4

- `update_image_hash.c/.h` introduz a sessão SHA-256 incremental de imagem:
  estado de digest apenas, blocos de no máximo 4 KiB, contagem exata do tamanho
  assinado e comparação final do digest. Não possui rede, escrita de slot,
  journal ou ativação.
- `run_update_image_hash_vector_test.ps1` passou para o payload fixo dividido
  em blocos de 7 B, com SHA-256
  `2f283ef73d59a91a41236708b6764fe0058e6c18c8cb4bff48165650ba0e28e7`.
  Os vetores de assinatura, manifesto e política também passaram. O teste de
  hash é independente; a sessão de firmware foi compilada para P4 e ainda não
  recebe um stream de produção.
- Build ESP-IDF 5.5.4, target `esp32p4`, diretório
  `build/g5-image-hash-20260913`: app `0x175100` B. SHA-256 P4:
  `8E6545B56737440CF00EA694F8F9C9A56FE2931228809A86559ABE27E9632CCE`;
  sdkconfig: `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`;
  tabela: `9AB122C32036A53AA7F0DCB9422DCF89F007C15C5DEE5467101FD3779F4406A6`.
- O comando de flash P4 pela COM8 terminou antes da captura. O boot posterior
  identificou ESP32-P4 v1.3 e a app compilada às 08:06:50; em 1.764 ms o
  display EK79007, GT911 e dashboard RGB565/180°/TRIPLE_PARTIAL/3 FB ficaram
  prontos. Em 3.184 ms o C6 negociou Hosted 3.0.6, RPC v2 e SW_AGGR; o scan
  sem credenciais encontrou 50 APs. Não houve panic ou WDT na captura de 30 s.
  C6, NVS, `storage`, `c6_ota` e eFuses não foram gravados.

## 2026-09-13 — G5, journal OTA e anti-replay persistíveis

- `update_journal.c/.h` adiciona registro versionado com CRC, identidade do
  manifesto e estados P4. O `FlashCoordinator` é o único escritor: alterna
  `np2_update/ota0` e `ota1`, escolhe a maior geração válida e publica seu
  resultado no status após o boot. Apenas o estado `ACCEPTED` vira registro
  anti-replay.
- Os testes host de journal e anti-replay passaram. Build/flash P4 foram
  repetidos; a última app gravada tem SHA-256
  `6AE309B352FA24D34CE328C1944C8F1B0B4EF6D0635213F20411C8B31CC7596A`.
  Esptool verificou bootloader, app, tabela e OTA data. Não houve acionamento
  do journal neste flash e portanto NVS, `storage`, C6 e eFuses ficaram sem
  escrita de produto.
- Não há release assinada, chave pública de produção ou origem HTTPS OTA no
  repositório. Esta evidência não comprova staging, seleção de slot, rollback
  nem recuperação OTA; C6 continua bloqueado pelos gates físicos.

## 2026-09-13 — G5, writer P4 e journal sequencial

- O writer P4 só abre o slot OTA inativo, escreve bytes que participam do
  hash SHA-256 streaming e termina por `esp_ota_end`. A seleção do slot exige
  writer concluído e journal CRC-válido em `P4_PENDING` com geração persistida;
  download parcial, hash divergente e journal inválido não podem selecioná-lo.
  `FlashCoordinator` alterna `np2_update/ota0` e `ota1`, rejeita corrupção,
  ambiguidade de geração e transições fora da sequência.
- Todos os vetores host G5 passaram, incluindo o percurso
  `P4_STAGED → P4_PENDING → ACCEPTED → IDLE` e uma nova release após `IDLE`.
  O teste de assinatura registra uma rejeição OpenSSL para manifesto alterado,
  que é o caso negativo esperado.
- Build ESP-IDF 5.5.4 para `esp32p4`, diretório
  `build/g5-keyring-20260913`: app `0x175770` B, menor slot livre
  `0x68a890` B (82%); bootloader `0x5a50` B, livre `0x85b0` B. SHA-256 P4:
  `E4A66C99E0C2E7A45999A17D0D8F93B32ADAC92B4389402E4D3B5BDC36E3FE22`.
  Flash COM8 verificou cada bloco e identificou ESP32-P4 v1.3. O boot posterior
  confirmou 32 MiB flash/PSRAM, EK79007, GT911, dashboard
  RGB565/180°/TRIPLE_PARTIAL/3 FB e Hosted 3.0.6/RPC v2/SW_AGGR em 3.184 ms;
  não houve panic/WDT na janela observada. C6, NVS de produto, `storage`,
  `c6_ota` e eFuses não receberam escrita de produto.
- Não havia release assinada na unidade. Nenhum slot OTA foi selecionado pelo
  writer, e esta evidência não declara download, rollback ou recovery OTA
  concluídos. C6 permanece bloqueado pelos gates físicos.

## 2026-09-13 — G5, ativação P4 sob o FlashCoordinator

- `esp_ota_set_boot_partition` saiu do writer. Somente o `FlashCoordinator`
  enfileira a seleção e, na sua task serializada, confere que a partição é
  `ota_0`/`ota_1` e que o journal recebido é idêntico à cópia NVS mais nova,
  selada e em `P4_PENDING`. Não há ativação quando a cópia é ausente,
  corrompida, divergente ou fora de sequência.
- O build/flash ESP-IDF 5.5.4 em COM8 passou com app `0x1771c0` B, livre
  `0x688e40` B no menor slot (82%) e SHA-256
  `02753B3F92D47BDEABA8C984EA2B2286793815110566DE68D351D5B66F56F1EA`.
  Esptool verificou bootloader, app, tabela e `otadata`. O boot posterior
  confirmou P4 v1.3, 32 MiB flash/PSRAM, display/touch, dashboard e Hosted
  C6 3.0.6 com RPC v2 e SW_AGGR; não houve panic/WDT observado. Não houve
  transação OTA, escrita NVS de produto, gravação C6 ou ação sobre eFuses.

## 2026-09-13 — G5, supervisor de confirmação P4

- O supervisor inicia somente para `PENDING_VERIFY`; requer frame renderizado,
  heartbeat do `app_loop` e coordenador de flash prontos por 15 s contínuos.
  A confirmação e o rollback passam pela fila do `FlashCoordinator`; rollback
  exige outro slot em estado `VALID`, e ausência de fallback entra em recovery
  sem loop. O dashboard atualiza em 250 ms para fornecer o heartbeat de UI.
- Teste host da política passou. Build/flash P4 ESP-IDF 5.5.4: app `0x178030` B,
  livre `0x687fd0` B no menor slot OTA (82%), SHA-256
  `D7145967C7B1407F9818E951E3D101F19EC98F0EFDC6A7DBBE86CF8DF9F39D59`.
  Esptool verificou todos os blocos gravados. O boot posterior confirmou P4
  v1.3, 32 MiB flash/PSRAM, display/touch, dashboard e C6 Hosted 3.0.6/RPC
  v2/SW_AGGR; não houve panic/WDT. A imagem atual não estava pendente e não
  exercitou confirmação, rollback nem alteração OTA; C6 e eFuses permaneceram
  intocados.

## 2026-09-13 — G5, preflight HTTPS OTA P4 de desenvolvimento

- O pedido de preflight usa a task HTTPS já existente, mantendo no máximo uma
  conexão TLS em voo. A política rejeita URL não-HTTPS, host não autorizado,
  porta, query, fragmento, credenciais ou endpoints repetidos. Com DNS e NTP
  válidos, baixa somente manifesto de 104 B e assinatura destacada de 388 B,
  ambos com tamanho exato e orçamento total de 20 s. A URL da imagem é somente
  validada; download, assinatura, admission, journal e escrita OTA continuam
  desligados até uma configuração de desenvolvimento os fornecer de modo unido.
- Todos os testes host G5 passaram; no teste de assinatura, a mensagem OpenSSL
  para o manifesto adulterado é a rejeição negativa esperada. Build/flash
  ESP-IDF 5.5.4 em COM8: app `0x1780b0` B, menor slot livre `0x687f50` B (82%),
  SHA-256 `A0AE6D2CB566183F5234EF7EF6FD5D675A52D4C6D5E94246E98EDA72521A3120`.
  Esptool verificou bootloader, app, tabela e `otadata`.
- O boot posterior identificou P4 v1.3, 32 MiB flash/PSRAM, EK79007, GT911 e
  dashboard RGB565/180°/TRIPLE_PARTIAL/3 FB. O C6 negociou Hosted 3.0.6, RPC v2
  e SW_AGGR; o scan sem credenciais encontrou 45 APs. Não houve panic/WDT na
  janela observada. NVS de produto, `storage`, `c6_ota`, C6 e eFuses não foram
  escritos.

## 2026-09-13 — UI, caracteres especiais em português

- A fonte padrão anterior possuía ASCII, grau, marcador e ícones, mas não a
  faixa Latin-1. Foi incluída uma fonte estática Montserrat 14 com ASCII,
  Latin-1 e a pontuação usada pela interface, aplicada às telas de painel e
  diagnóstico. Ela mantém fallback para a fonte padrão para símbolos LVGL e
  não introduz carregamento ou rasterização dinâmica no caminho de render.
- Build/flash ESP-IDF 5.5.4 em COM8: app `0x17b980` B, menor slot livre
  `0x684680` B (81%), SHA-256
  `E02B3591363B6C7A6FE157483A60B6237A5AF68B20701A635DA3FC80B0F23B60`.
  Esptool verificou bootloader, app, tabela e `otadata`.
- O boot posterior confirmou P4 v1.3, 32 MiB flash/PSRAM, display EK79007,
  touch GT911 e dashboard. O C6 negociou Hosted 3.0.6, RPC v2 e SW_AGGR. Não
  houve panic ou WDT na janela observada; C6 e eFuses não foram escritos.

## 2026-09-13 — G5, executor OTA P4 de desenvolvimento

- O executor HTTPS único agora une manifesto, assinatura destacada, keyring
  público, admission, SHA-256 streaming, journal e escrita do slot inativo.
  `update_p4_writer` não chama `esp_ota_*`; begin/write/end/abort/seleção
  pertencem à task serializada do `FlashCoordinator`. A imagem só é selecionada
  após hash, tamanho, `Content-Length` e journal `P4_PENDING` conferirem.
- Os vetores host de manifesto, assinatura, hash, keyring, replay, journal,
  recovery, política e endpoints HTTPS passaram. O build ESP-IDF 5.5.4 para
  P4, `build/g5-keyring-20260913`, gerou app `0x17e740` B e SHA-256
  `DE3612A2C215F45E1B82DD29D688B6D1549C2086455779B957733FE16B1EAF3D`.
- O flash obrigatório em COM8 foi tentado duas vezes depois do build e falhou
  antes de gravar por `PermissionError(13): Acesso negado`; a porta está
  ocupada. Portanto não houve boot nem validação física desta revisão. Não
  foram gravados NVS de produto, storage, C6, `c6_ota` ou eFuses.

## 2026-09-13 — Integração EEZ Boot/Home

- Placa/BOM observada: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3,
  flash 32 MiB, PSRAM 32 MiB, EK79007 e GT911. Commit base
  `e66001982f2c9f328f7d26bd45b14133a293aa37`, com árvore de trabalho suja
  contendo as mudanças em validação. Projeto EEZ SHA-256
  `0D2AA6B15EF9FDF08EE24AC73D425F0F9A117EAF63142CA6AB6CAECE2E0837C6`;
  `sdkconfig` efetivo SHA-256
  `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`.
- O Build do EEZ gerou 23 arquivos em `firmware/main/ui_generated`. O build P4
  foi executado com ESP-IDF 5.5.4 por `idf.py build`; app `0x271720` B, menor
  slot livre `0x58e8e0` B (69%), SHA-256
  `8A5FC2FD5C2B6B90099D783D1C68D386E074B2A6F25BE54BCC54188B53D8D606`.
  Configuração efetiva confirmou target `esp32p4`, flash 32 MiB, RGB565 e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`.
- A primeira gravação válida revelou um abort reproduzível depois de iniciar a
  UI: `setenv()` tentava adquirir o lock de Newlib dentro da seção crítica de
  `time_service_start`. O backtrace apontou `lock_acquire_generic` e
  `time_service.c:47`. A configuração de fuso foi movida para fora do lock,
  seguida de novo build e flash; a falha deixou de ocorrer.
- Flash final em COM8 por `idf.py -p COM8 flash`: bootloader, app, tabela e
  `otadata` tiveram hash verificado e o P4 foi reiniciado por RTS. O boot
  corrigido confirmou PSRAM a 200 MHz, teste de memória OK, display, touch e
  `EEZ Boot/Home UI active: RGB565, rotation=180, triple-partial, 3 FBs`.
  `app_main` retornou normalmente. Depois, o C6 negociou Hosted 3.0.6, RPC v2 e
  SDIO SW_AGGR; o scan final sem credenciais encontrou 42 APs. Não houve panic/WDT
  durante a janela serial observada, que atravessou o timeout de 10 s da Home.
- O C6 não foi gravado. O log fornece versão/capacidades do firmware instalado,
  mas não seu hash de imagem; esse hash permanece indisponível nesta unidade.
  NVS de credenciais, `storage`, `c6_ota` e eFuses não foram escritos por esta
  integração. A continuidade sem glitch, o conteúdo visual exato e o gesto da
  gaveta ainda exigem observação do operador na tela física; o log serial não
  encerra esse gate visual.
- O EEZ Studio em execução salvou uma cópia anterior da página Boot e removeu
  seus identificadores de geração. A Home continua totalmente ligada por IDs
  nomeados; no Boot, somente status e detalhe usam a posição fixa dos filhos
  3 e 4, preservada no projeto atual. A compilação e o flash finais foram feitos
  contra essa mesma árvore gerada, portanto uma nova alteração estrutural em
  Boot exige nomear os widgets no Studio antes de executar novo Build.

## 2026-09-14 — Sincronização do Build EEZ

- O EEZ Studio gerou novamente os 23 arquivos declarados em
  `firmware/main/ui_generated`; nenhum arquivo do manifesto ficou ausente.
  Os 27 objetos referenciados por `eez_ui` existem em `screens.h`, inclusive os
  dois labels posicionais da página Boot e os bindings nomeados da Home.
- Build e flash P4 com ESP-IDF 5.5.4 em COM8: app `0x271720` B, menor slot
  livre `0x58e8e0` B (69%), SHA-256 pós-flash
  `8A5FC2FD5C2B6B90099D783D1C68D386E074B2A6F25BE54BCC54188B53D8D606`.
  Projeto EEZ SHA-256
  `0D2AA6B15EF9FDF08EE24AC73D425F0F9A117EAF63142CA6AB6CAECE2E0837C6`;
  `sdkconfig` SHA-256
  `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`.
  Esptool verificou bootloader, aplicação, tabela de partições e `otadata`.
- O boot confirmou P4 v1.3, PSRAM 32 MiB a 200 MHz, display EK79007, touch
  GT911, `EEZ Boot/Home UI active`, C6 Hosted 3.0.6, RPC v2 e SDIO SW_AGGR.
  O scan sem credenciais encontrou 48 APs. Não houve panic ou WDT na janela
  serial observada. C6, NVS de credenciais, `storage`, `c6_ota` e eFuses não
  foram escritos.

## 2026-09-14 — Organização da UI e limpeza de fontes

- Removidas as fontes estáticas Montserrat 20, 24, 32 e 48 que não tinham
  consumidor no firmware. A Montserrat 14 foi preservada porque ainda é usada
  apenas pelo dashboard e diagnóstico legados. As fontes da Home continuam
  pertencendo à saída do EEZ e não foram duplicadas.
- A saída do Studio em `firmware/main/ui_generated` permaneceu intacta, plana
  e regenerável. O bridge manual foi movido para `firmware/main/ui/bridge` e o
  dashboard/diagnóstico legado, inclusive sua fonte, para
  `firmware/main/ui/legacy`. O snapshot visual inativo foi para `ui/archive`.
  `ui/README.md` registra a separação e o que pode ou não ser alterado
  manualmente.
- Build e flash P4 com ESP-IDF 5.5.4 em COM8: app `0x271720` B, menor slot
  livre `0x58e8e0` B (69%), SHA-256 pós-flash
  `7AA5117AF866FE15EA150F1033B7C91E8203A2AF6AF15621FB55EB185F2341C6`.
  `sdkconfig` SHA-256
  `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`.
  Esptool verificou bootloader, aplicação, tabela de partições e `otadata`.
- O boot posterior confirmou P4 v1.3, PSRAM 32 MiB a 200 MHz com teste OK,
  EK79007, GT911 e `EEZ Boot/Home UI active: RGB565, rotation=180,
  triple-partial, 3 FBs`. O C6 negociou Hosted 3.0.6, RPC v2 e SDIO SW_AGGR;
  o scan sem credenciais encontrou 38 APs. Não houve panic ou WDT na janela
  serial observada. C6, NVS de credenciais, `storage`, `c6_ota` e eFuses não
  foram escritos.

## 2026-09-14 — Home EEZ com componentes reutilizáveis e fundo RGB565

- A regeneração do EEZ passou a materializar `NP_Header` e `NP_SideDrawer`
  como instâncias. A ponte de runtime foi alinhada aos identificadores gerados
  dessas instâncias, e `ui_image_bg.c` foi incluído explicitamente no componente
  P4. O fundo 1639×960, originalmente ARGB8888, foi exportado como RGB565;
  isso preserva a composição visual estática e reduz o recurso de 6.293.760 B
  para 3.146.880 B. O projeto EEZ registra o bitmap em 16 bpp.
- Compilação P4 com ESP-IDF 5.5.4 em árvore de build recriada para `esp32p4`.
  A tentativa de `fullclean` não removeu `managed_components` porque um
  subdiretório do LVGL estava aberto, mas `firmware/build` estava vazio antes
  da compilação e as dependências fixadas foram reutilizadas. O binário final
  mede `0x571e30` B; o menor slot de 8 MiB manteve `0x28e1d0` B livres (32%).
  SHA-256 da app: `2CD89873E706578DF6DF31A83C14DF5F7319B820D6945B68F2F0528CF69131D4`.
- Flash P4 pela USB-JTAG COM8: esptool identificou ESP32-P4 v1.3 e verificou
  por hash bootloader, app, tabela e `otadata`, seguido de reset por RTS. O
  C6 não foi gravado. Os hashes do bootloader e da tabela foram,
  respectivamente, `56210F8B9C9C585D3F60CA7DDC3357234244413E344A031B43C235F28FA00092`
  e `9AB122C32036A53AA7F0DCB9422DCF89F007C15C5DEE5467101FD3779F4406A6`.
- O boot posterior confirmou flash 32 MiB, PSRAM 32 MiB a 200 MHz com teste
  aprovado, EK79007, GT911, `EEZ Boot/Home UI active: RGB565, rotation=180,
  triple-partial, 3 FBs` e retorno normal de `app_main`. O C6 já instalado
  negociou Hosted 3.0.6, RPC v2 e SDIO SW_AGGR; o scan RAM-only encontrou 21
  APs. Não houve panic ou WDT na janela serial observada. NVS de credenciais,
  `storage`, `c6_ota` e eFuses não foram escritos. A ausência de glitch e a
  interação da gaveta seguem dependendo de observação física da tela.

## 2026-09-14 — Correção do fundo para a resolução nativa do painel

- Após a observação de que o fundo não aparecia na tela, o recurso foi
  reexportado de 1639×960 para 1024×600, exatamente a resolução do EK79007.
  Continua RGB565/16 bpp, passa a ocupar 1.228.800 B e também foi substituído
  no bitmap embutido de `ui/NP2.eez-project`; uma nova geração pelo EEZ mantém
  o mesmo tamanho de origem. Isso evita depender de escala ou crop do fundo
  pelo LVGL.
- Build ESP-IDF 5.5.4 para `esp32p4` passou: app `0x39d9b0` B, menor slot livre
  `0x462650` B (55%), SHA-256
  `B0E4CE7390779FA6F333D7FE2F88A9516C96ADA8A5C6CE371FB7E15CE3514CB2`.
  Flash em COM8 verificou por hash bootloader, app, tabela e `otadata`, e
  reiniciou o P4 por RTS. O C6 não foi gravado.
- O boot confirmou P4 v1.3, flash 32 MiB, PSRAM 32 MiB a 200 MHz, EK79007,
  GT911 e `EEZ Boot/Home UI active: RGB565, rotation=180, triple-partial,
  3 FBs`; `app_main` retornou normalmente. O C6 instalado negociou Hosted
  3.0.6, RPC v2 e SW_AGGR, e o scan RAM-only encontrou 28 APs. Não houve
  panic ou WDT na janela observada. A confirmação visual do novo fundo e a
  inspeção de glitch continuam sendo observações físicas de tela.

## 2026-09-14 — Correção de inicialização EEZ e fundo RGB565

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash 32 MiB e PSRAM 32 MiB; P4 gravado pela USB em COM8. O C6 não foi alterado.
- Correção: a árvore EEZ é criada sob o mutex LVGL antes de iniciar a task do adapter. A ordem anterior podia bloquear app_main no primeiro esp_lv_adapter_lock(-1), deixando no painel o frame de boot anterior.
- Bitmap de fundo: BG configurado para RGB565/16 bpp; 1024x600 consome 1.228.800 bytes na imagem de firmware, em vez de 2.457.600 bytes em ARGB8888.
- Evidência de boot: EEZ Boot/Home UI active: RGB565, rotation=180, triple-partial, 3 FBs; P4 local bring-up ready; C6 negociou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR; scan concluiu com 34 APs.
- Artefato P4: np2_p4.bin, 3791280 bytes, SHA-256 B27A4F64F6F28CB52091DDD696D471A6AC54CBE7691D52E39583B0D4815FAA3B.
- Limite: esta observação confirma boot, display, UI e enlace de rede; não substitui o gate de render/soak.

## 2026-09-14 — Remoção da UI EEZ e retorno ao LVGL direto

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 revisão v1.3, porta COM8,
  MAC P4 `e8:f6:0a:e0:8f:42`; estado Git base `0418744` com alterações locais.
- Removidos o projeto EEZ, saída `ui_generated`, imagem RGB565, ponte de
  runtime, teclado auxiliar e scripts de regeneração. As fontes foram
  preservadas em `firmware/main/ui/fonts`; a tela ativa passou a ser a UI
  LVGL direta, hoje organizada em `firmware/main/ui/screens/product_ui.c`.
- Build: ESP-IDF 5.5.4, target `esp32p4`, BSP 3.0.1, LVGL 9.5.0 e
  `esp_lvgl_adapter` 0.6.4. `np2_p4.bin` possui `0x185110` bytes; SHA-256
  `CC103C8D6578D2F4891F3080F11955D0E601D6BEED48220085F3D8183DEC0024`.
  `sdkconfig` efetivo SHA-256
  `078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`.
- Flash P4 concluído por `idf.py -p COM8 flash`, com verificação de hash e
  hard reset. A imagem C6 não foi alterada; o artefato de referência
  `eh_cp_ota_coprocessor_ota.bin` tem SHA-256
  `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`.
- Boot capturado: PSRAM 32 MiB, EK79007 inicializado, GT911 identificado,
  `Direct LVGL UI active: RGB565, rotation=180, triple-partial, 3 FBs`, C6
  ESP-Hosted 3.0.6 compatível, RPC v2, SDIO SW_AGGR e scan de 37 APs.
- A captura confirma inicialização sem crash. Ausência de glitch durante uso
  prolongado continua dependendo de observação física em bancada.

## 2026-09-14 — Boot e Home LVGL baseadas no design v5

- Referência visual: `design/v5/screens/np_boot.c`, `np_home.c` e seus mockups.
  Tokens, estilos e componentes foram portados para `firmware/main/ui/core`;
  nenhum arquivo de `design/v5` entra diretamente no binário.
- Boot e Home são construídas uma única vez. Atualizações usam handles fixos e
  escrita guardada; a troca de tela apenas alterna `LV_OBJ_FLAG_HIDDEN`.
- A Boot tem duração mínima de 1,2 s e limite absoluto de 6 s. A Home abre com
  estado indisponível quando rede, NTP ou providers não concluem, evitando uma
  tela de inicialização permanente.
- Home recebe hora corrente, conexão, clima e BTC somente pela projeção do
  `AppState`. Campos sem contrato real permanecem explicitamente indisponíveis.
- Build ESP-IDF 5.5.4 para `esp32p4` aprovado. `np2_p4.bin` possui `0x270f20`
  bytes, 69% livres na menor partição de aplicação, SHA-256
  `82A77698C3C03CE599E7EEF4C4263B25ABF809F2FEA3E675FCAA5B5A00824381`.
- Flash pela COM8 concluído com verificação de hash. O boot confirmou EK79007,
  GT911, 32 MiB de PSRAM, `TRIPLE_PARTIAL` com três framebuffers, C6
  ESP-Hosted 3.0.6/RPC v2/SDIO SW_AGGR, scan de 19 APs e a mensagem
  `product_ui: Boot transition complete; Home visible` aos 7,9 s.

## 2026-09-14 — UI compartilhada design v5 / firmware

- O P4 passou a compilar diretamente `design/v5/core/np_styles.c`,
  `np_components.c`, `screens/np_boot.c` e `np_home.c`. O controlador em
  `firmware/main/ui/screens/product_ui.c` só aplica a projeção de `AppState`.
- A atualização de widgets é condicionada à revisão de `AppState`; nenhum
  label ou card é reescrito no timer quando a projeção não mudou. Boot e Home
  permanecem criadas uma única vez e alternam apenas `LV_OBJ_FLAG_HIDDEN`.
- Header recebeu drawer persistente, aberto pelo ícone de menu sem animação
  contínua. O ícone de configurações mantém o acesso à tela técnica.
- Build limpo: ESP-IDF 5.5.4, target `esp32p4`, LVGL 9.5.0, BSP 3.0.1 e
  `esp_lvgl_adapter` 0.6.4. `np2_p4.bin` mede `0x271260` bytes, com 69% livres
  na menor partição; SHA-256
  `92942722EBA175968C1D60D86DD941AA2D23B9D56F418E18FF76510219DC855B`.
- Flash P4 via `idf.py -p COM8 flash` terminou com hash verificado. O boot
  capturado confirmou EK79007, GT911, PSRAM 32 MiB, três framebuffers,
  `TRIPLE_PARTIAL`, C6 ESP-Hosted 3.0.6/RPC v2/SDIO SW_AGGR, scan de 31 APs e
  `product_ui: Boot transition complete; Home visible` aos 7,9 s.

## 2026-09-22 — Perfil P4 de desenvolvimento com retenção Wi-Fi

- Placa-alvo: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG COM8.
- Build limpa: `firmware/build/wifi-retention-dev-20260922`, IDF 5.5.4, target `esp32p4`, com `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1` explícito na linha de compilação.
- App `0x27c120` B, com `0x583ee0` B livres no menor slot OTA. SHA-256 P4: `884109F12D7E45977778C80E9728EF5D97EA02CECE7EDD34B7D7689A98F8DC94`.
- `idf.py app-flash` escreveu e verificou somente `np2_p4.bin` em `0x20000` (`0x20000` a `0x29cfff`); bootloader, tabela de partições, `otadata`, NVS, storage, C6 e eFuses não foram escritos.
- O esptool executou hard reset após a verificação. Uma abertura posterior da janela USB de manutenção não teve resposta imediata, portanto este registro não afirma boot observado, associação WPA2, retenção, reboot ou segurança de produção.
- Próximo ensaio: provisionar WPA2 pelo painel uma vez, manter IP por pelo menos 30 s, reiniciar P4 e confirmar reassociação; não registrar SSID ou senha.

## 2026-09-22 — Correção da primeira gravação no CredentialVault de desenvolvimento

- A análise do ensaio anterior encontrou a causa da ausência de retenção: quando
  os dois slots `cred0` e `cred1` ainda não existiam, o seletor devolvia
  `ESP_ERR_NVS_NOT_FOUND`; o escritor aceita `ESP_ERR_NOT_FOUND` para criar a
  geração 1. A normalização do estado vazio permite a primeira chamada chegar
  a `nvs_set_blob()` e `nvs_commit()`. Não houve leitura nem exportação da NVS.
- Build limpa: `firmware/build/wifi-retention-dev-fix-20260922`, ESP-IDF
  5.5.4, target `esp32p4`, com
  `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1`. A app mede `0x27c120` B,
  deixando `0x583ee0` B (69%) no menor slot OTA. SHA-256 P4:
  `CCE8265E4683D1C4CA807667FF9512422F6BEA584C9225EF425D9DD499FCA7D9`.
- `idf.py app-flash` escreveu somente a app em `0x20000` e apagou o intervalo
  `0x20000` a `0x29cfff`; esptool confirmou a escrita e fez hard reset.
  Bootloader, tabela de partições, `otadata`, NVS, storage, C6 e eFuses não
  foram escritos.
- O boot capturado confirmou P4 v1.3, flash 32 MiB, PSRAM 32 MiB a 200 MHz
  com teste aprovado, EK79007, GT911, RGB565/rotação 180°/`TRIPLE_PARTIAL`/
  três framebuffers e retorno de `app_main`. O C6 instalado negociou
  ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR; o scan sem credenciais encontrou
  17 APs. Não houve panic ou WDT na janela observada.
- A retenção ainda aguarda o ensaio dinâmico: provisionar WPA2 uma vez nesta
  build, manter IP por 30 s e reiniciar para observar a reassociação. SSID e
  senha continuam fora deste registro.

## 2026-09-22 — Retenção Wi-Fi de desenvolvimento: ensaio de reboot e correção de reassociação

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG COM8.
  O P4 recebeu somente a app; a NVS que contém o cofre de credenciais não foi
  apagada ou exportada durante os ensaios.
- O ensaio dinâmico confirmou que, após o provisionamento WPA2 e mais de 30 s
  com IP estável, o cofre privado foi salvo. Em reboots posteriores a aplicação
  restaurou o registro diretamente para a mailbox privada, sem exibir ou
  registrar SSID, senha ou endereço de rede nesta evidência.
- A primeira implementação reiniciava a tela de configuração enquanto o evento
  LVGL ainda era processado e também iniciava uma desconexão assíncrona mesmo
  sem configuração de estação anterior. A tela agora é removida de forma
  assíncrona; a primeira associação não emite desconexão e uma troca de rede
  só agenda a reassociação depois de receber o evento de desconexão anterior.
- Build limpa final: `firmware/build/wifi-retention-dev-reconnect-20260922`,
  ESP-IDF 5.5.4, target `esp32p4`, com
  `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1`. A app mede `0x27c220` B,
  com `0x583de0` B (69%) livres na menor partição. SHA-256 P4:
  `D94B4420C7FABAD30A26CE2DE3B75968EA9B60E569EF7E4EC2FDFB26D8D9318F`.
- `idf.py app-flash` gravou e verificou a app em `0x20000` (intervalo
  apagado `0x20000` a `0x29cfff`) e realizou hard reset. Bootloader, tabela de
  partições, `otadata`, NVS, storage, C6 e eFuses permaneceram intactos.
- Boot de validação: P4 v1.3, flash 32 MiB, PSRAM 32 MiB/200 MHz, EK79007,
  GT911, RGB565/rotação 180°/`TRIPLE_PARTIAL`/três framebuffers e C6
  ESP-Hosted 3.0.6/RPC v2/SDIO SW_AGGR. A restauração do cofre foi seguida por
  associação e IP em aproximadamente 10 s, sem panic, WDT ou a desconexão
  artificial da versão anterior.
- Este perfil é exclusivamente de desenvolvimento: para produção, a mesma
  retenção exige NVS encryption e flash encryption ativas.

## 2026-09-22 — Sincronização automática de relógio, clima e Bitcoin

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG COM8.
  Build limpa em `firmware/build/product-sync-20260922`, ESP-IDF 5.5.4,
  target `esp32p4`, com `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1`.
- A app `np2_p4.bin` mede `0x27c2e0` B e deixa `0x583d20` B (69%) livres na
  menor partição de aplicação. SHA-256 P4:
  `9081E9E5D72E8A2FAC0D738732499AE66D35FB5A881B405A28875D5872E0C955`.
  `idf.py app-flash` escreveu e verificou somente o intervalo de app
  `0x20000` a `0x29cfff`; NVS, tabela de partições, storage, C6 e eFuses não
  foram escritos.
- No boot, o P4 confirmou PSRAM 32 MiB/200 MHz, EK79007, GT911,
  RGB565/rotação 180°/`TRIPLE_PARTIAL`/três framebuffers; o C6 negociou
  ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR. A credencial persistida reassociou
  e recebeu IP sem registrar os dados da rede neste documento.
- A primeira rodada automática iniciou após o IP e concluiu DNS, NTP e as
  consultas HTTPS fixas de clima de Brasília e BTC/USD com certificados
  validados. Resultado: DNS=`ESP_OK`, NTP=`ESP_OK`, HTTPS=`ESP_OK` em 8.163 ms;
  o FlashCoordinator verificou e publicou a nova geração de cache em 28 ms.
  Não houve panic, WDT ou segundo handshake TLS concorrente na captura.
- Política ativa: primeira sincronização após cada associação com IP; depois,
  a cada 30 minutos. Falha de DNS, NTP ou provider conserva o último snapshot
  como stale e tenta novamente após dois minutos; sem IP não há I/O externo.

## 2026-09-22 — Reversão da tentativa de editor local

- A tentativa de editor local, seus overrides e todas as referências no build
  foram removidos. As telas Boot e Home voltaram a usar diretamente a composição
  LVGL existente, com os mesmos tokens, posições e textos estáticos anteriores.
- Build limpa: `firmware/build/editor-removed-20260922`, ESP-IDF 5.5.4,
  target `esp32p4`, com `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1`. A app
  `np2_p4.bin` mede `0x27c2e0` B, com `0x583d20` B (69%) livres na menor
  partição OTA. SHA-256 P4:
  `78A8508D82A843D1BD21F9A5412F816FEC9D3571457A022D851D9C30D5EE0964`.
- `idf.py app-flash` escreveu e verificou somente a app em `0x20000`; NVS,
  tabela de partições, `otadata`, storage, C6 e eFuses permaneceram intactos.
  O boot confirmou P4 v1.3, PSRAM 32 MiB/200 MHz, EK79007, GT911, RGB565,
  rotação 180°, `TRIPLE_PARTIAL`, três framebuffers e a transição de Boot para
  Home, sem panic ou WDT. O C6 negociou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR.

## 2026-09-22 — Reorganização das fontes da UI no firmware

- As fontes C executáveis foram movidas de `design/v5` para
  `firmware/main/ui`: `core/` contém tokens, estilos e componentes; `screens/`
  contém Boot, Home e o controlador `product_ui`; `fonts/` permanece no mesmo
  diretório de UI. O CMake do componente passou a compilar somente esses
  caminhos. `design/v5` agora contém apenas mockups, imagens e seu gerador.
- Build limpa: `firmware/build/ui-source-relocation-20260922`, ESP-IDF 5.5.4,
  target `esp32p4`, com `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1`. A app
  mede `0x27c2e0` B, com `0x583d20` B (69%) livres na menor partição. SHA-256
  P4: `67E17106057F4EB3480289306957E0C596B0249068049D136FA3CE6C4E0ADA26`.
- `idf.py app-flash` escreveu e verificou somente a app em `0x20000`; NVS,
  tabela de partições, `otadata`, storage, C6 e eFuses permaneceram intactos.
  O boot confirmou P4 v1.3, PSRAM 32 MiB/200 MHz, EK79007, GT911, RGB565,
  rotação 180°, `TRIPLE_PARTIAL`, três framebuffers e a transição de Boot para
  Home, sem panic ou WDT. O C6 negociou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR.
- Após receber IP, a sincronização automática concluiu DNS, NTP e HTTPS e
  publicou uma nova geração do cache offline. A captura não registrou
  credenciais, nomes de rede ou endereço IP.

## 2026-09-23 — Retenção Wi-Fi de desenvolvimento e reconexão após AUTH_EXPIRE

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG COM8.
  O cache CMake ativo havia sido reconfigurado com
  `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=OFF`; por isso a imagem fazia
  somente o scan sem credenciais no boot. As tarefas padrão de build e build
  limpo do VS Code agora passam explicitamente
  `-DNP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=ON`.
- A aplicação foi recompilada com o opt-in ativo. Binário P4:
  `0x39f070` B; SHA-256
  `95D82D3E5FD02154FBC50E28DDF526465467EB04A49D44020EFC5F1B0830CF76`.
  `idf.py -DNP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=ON -p COM8 app-flash`
  escreveu somente a aplicação em `0x20000`; NVS, tabela de partições,
  storage, C6 e eFuses não foram escritos.
- A política de desconexão foi corrigida: `AUTH_EXPIRE` e timeouts de
  handshake entram no backoff de reconexão; uma rejeição definitiva preserva
  o cofre. Somente a ação explícita `FORGET` remove a credencial durável.
- Em boot observado, o cofre restaurou a credencial para a mailbox privada;
  duas tentativas ocorreram com backoff de 2,454 s e 4,311 s, e o P4 recebeu
  IP aos 26,940 s. Nenhuma credencial, nome de rede ou endereço IP foi
  incluído nesta evidência.

## 2026-09-23 — Orquestração serial de clima, dólar e Bitcoin

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG
  COM8. Build incremental em `firmware/build/data-orchestration-20260923`,
  ESP-IDF 5.5.4, target `esp32p4`. A app `np2_p4.bin` mede `0x39fdc0` B e
  deixa `0x460240` B (55%) livres na menor partição de aplicação. SHA-256 P4:
  `DF1B383A11C1C2DC96C3FFEE561572452C24A435EB6FCCED20E92F540A1B5255`.
- `idf.py -B build/data-orchestration-20260923 -p COM8 flash` identificou o
  alvo como ESP32-P4 v1.3, escreveu bootloader, aplicação, tabela de partição
  e `otadata`, e terminou sem erro. O C6 não foi gravado; não houve alteração
  de eFuses ou inclusão de credenciais.
- O monitor posterior confirmou boot da imagem na `ota_0`, flash de 32 MiB,
  PSRAM de 32 MiB/200 MHz, EK79007, GT911, LVGL, RGB565/rotação 180°,
  `TRIPLE_PARTIAL` e três framebuffers. O C6 existente negociou ESP-Hosted
  3.0.6, RPC v2 e SDIO SW_AGGR. Não ocorreu panic, WDT ou reboot durante a
  captura até a Home ficar visível.
- A validação local cobriu codec cache v1/v2, parser BCB e a agenda serial:
  BTC/USD a cada 5 min, clima de Brasília a cada 2 h e USD/BRL PTAX a cada
  24 h; o agendador libera só um domínio por vez e alterna os vencidos. Este
  boot estava sem rede configurada, portanto o ciclo HTTPS completo e a
  confirmação visual dos três valores continuam pendentes de uma sessão Wi-Fi
  de bancada.

## 2026-09-23 — Retenção Wi-Fi padrão para o fluxo do assistente

- A opção CMake `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION` passou a ter
  padrão `ON` para o P4 desbloqueado de desenvolvimento. O perfil de produção
  continua obrigado a passá-la explicitamente como `OFF` e a usar a proteção
  de NVS/flash definida no procedimento de produção.
- A imagem P4 de `build/data-orchestration-20260923` foi reconfigurada com a
  opção efetiva `ON`, gravada pela COM8 e verificada por hash para bootloader,
  aplicação, tabela de partição e `otadata`. NVS, C6 e eFuses não foram
  escritos.
- O boot confirmou que o cofre restaurou a credencial de laboratório para a
  mailbox privada; nenhuma credencial ou identificador de rede é registrado
  nesta evidência. A associação seguinte não se completou e entrou no backoff
  normal de reconexão, portanto a continuidade da rede e o ciclo HTTPS ainda
  requerem nova sessão de bancada.

## 2026-09-23 — Home com card meteorológico e fundo neutro

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG
  COM8. Build limpa inicial e rebuild final em
  `firmware/build/home-weather-card-20260923`, ESP-IDF 5.5.4, target
  `esp32p4`, com `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1` e
  `NP2_WEATHER_BACKGROUNDS_EMBEDDED=0`. A app final mede `0x2740a0` B e deixa
  `0x58bf60` B (69%) livres na menor partição de aplicação. SHA-256 P4:
  `ED23BC9FF588265F3680C6CF03C6B8C233D20B0DB5A4BC10816059747E06E370`.
- `idf.py -B build/home-weather-card-20260923 -p COM8 flash` escreveu e
  verificou bootloader, aplicação, tabela de partições e `otadata`. NVS,
  storage, C6 e eFuses não foram escritos. A opção de assets ficou desligada
  porque os 20 RGB565 finais ainda não foram fornecidos; a Home usa o fallback
  de card escuro neutro.
- O primeiro boot detectou que ativar `clip_corner` em todos os cards prendia a
  task LVGL no renderer e acionava o watchdog. A correção limita esse clip ao
  card meteorológico enquanto uma imagem real estiver visível. Após rebuild e
  reflash, o monitor confirmou boot completo, Home visível, PSRAM 32 MiB/200
  MHz, EK79007, GT911, RGB565/rotação 180°, `TRIPLE_PARTIAL`, três
  framebuffers, ESP-Hosted 3.0.6/RPC v2/SDIO SW_AGGR e os três refreshes HTTPS
  seriados concluídos, sem novo WDT, panic ou reboot na captura.

## 2026-09-23 — Assets meteorológicos da Home no microSD

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG
  COM8. Build em `firmware/build/home-weather-card-20260923`, ESP-IDF 5.5.4,
  target `esp32p4`, com retenção Wi-Fi de desenvolvimento ativa. A aplicação
  mede `0x27c650` B e deixa `0x5839b0` B (69%) livres na menor partição OTA.
  SHA-256 P4:
  `547FA80FC71821FC05BF5F02FA91A2F99848C261FABEABC35E778D74461B0947`.
- Os testes host do mapeador `weather_code`/dia-noite e do catálogo de nomes
  passaram. O instalador `tools/copy_weather_assets_to_sd.ps1` também foi
  executado em diretório temporário: 20 arquivos e 9.240.000 B, todos com
  462.000 B.
- `idf.py -B build/home-weather-card-20260923 -p COM8 app-flash` escreveu e
  verificou somente a aplicação em `0x20000`; NVS, tabela de partições,
  storage, C6 e eFuses não foram escritos.
- No boot observado, o microSD montou em `/sdcard` antes do Hosted. A validação
  deliberadamente não formatou o cartão e reportou ausência de
  `/sdcard/np2/weather/np_bg_day_clear.bin`; a Home preservou o card escuro.
  Em seguida, o C6 negociou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR, e a Home
  abriu sem panic, WDT ou reboot. A cópia dos 20 assets para o cartão e a prova
  visual após NTP/clima continuam pendentes; o ensaio simultâneo SDMMC + Wi-Fi
  deve ser repetido após esse provisionamento.

## 2026-09-23 — Correção FATFS LFN e renderização dos assets meteorológicos

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG
  COM8. Build final em `firmware/build/home-weather-card-20260923`, ESP-IDF
  5.5.4, target `esp32p4`, com `CONFIG_FATFS_LFN_HEAP=y` e
  `CONFIG_FATFS_MAX_LFN=64`. A app mede `0x27d840` B e deixa `0x5827c0` B
  (69%) livres na menor partição OTA. SHA-256 P4:
  `0C4B5A29185264A33539A110646852D2F64B35BE18E1E45595B61290DB631B27`.
- A causa da primeira falha de catálogo foi configuracional: sem LFN, o
  FATFS expunha somente aliases 8.3, e não os nomes `np_bg_day_*` e
  `np_bg_night_*`. Depois do perfil LFN, o boot montou `/sdcard` e validou os
  20 arquivos em `/sdcard/np2/weather/`, cada um com 462.000 B.
- Na captura com rede disponível, NTP e as três consultas HTTPS seriadas
  concluíram; o worker de SD carregou e publicou
  `np_bg_night_partly_cloudy.bin`. A task LVGL apenas trocou a source da
  imagem já existente. O recorte de cantos foi movido do container para a
  própria imagem e o overlay, removendo a layer temporária que antes acionava
  o watchdog. A solicitação do worker também passou a coalescer o intervalo de
  carregamento, evitando uma segunda leitura para a mesma condição.
- A última imagem foi recompilada, gravada e verificada com
  `idf.py -B build/home-weather-card-20260923 -p COM8 app-flash`; somente o
  intervalo de aplicação `0x20000` foi escrito. NVS, tabela de partições,
  storage, C6 e eFuses não foram alterados. O boot posterior confirmou o
  catálogo e a Home sem novo WDT no intervalo previamente afetado; nessa
  tentativa a associação Wi-Fi não concluiu antes do fim da captura.
- Os testes host de mapeamento `weather_code`/dia-noite e do catálogo dos 20
  nomes passaram novamente. Esta é uma captura de integração funcional; não
  substitui o gate de estresse prolongado de SDMMC e Wi-Fi.
- A correção provisória que escolhia dia sem NTP foi substituída pelo snapshot
  offline v3 descrito na evidência seguinte; ela não é o comportamento atual.

## 2026-09-23 — Home: restauração offline do último fundo meteorológico

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG
  COM8. Build final em `firmware/build/home-weather-card-20260923`, ESP-IDF
  5.5.4, target `esp32p4`. A aplicação mede `0x27d8c0` B e deixa `0x582740` B
  (69%) livres na menor partição OTA. SHA-256 P4:
  `44F8CE33160C4A8DEFDF2569D7E6CAAA4137EE05E602F94C46427F6B4828EC9B`.
- Os testes host do codec offline v3/migração v1-v2, do mapeador
  `weather_code`/dia-noite e do catálogo dos 20 assets passaram. O decoder
  conserva snapshots v1/v2 legíveis, mas sem uma seleção visual eles mantêm o
  card neutro até o primeiro refresh válido promover o cache a v3.
- `idf.py -B build/home-weather-card-20260923 -p COM8 app-flash` escreveu e
  verificou somente a aplicação em `0x20000`; NVS, tabela de partições, C6 e
  eFuses não foram gravados pelo flash. Durante o refresh normal posterior, o
  `FlashCoordinator` publicou a geração de cache 194 com o novo campo visual.
- No primeiro boot com hora confiável, o worker selecionou e leu
  `np_bg_night_partly_cloudy.bin`. Após um reset sem novo flash, o segundo boot
  montou o cartão e carregou o mesmo asset aos 3,763 s, antes da tentativa de
  associação Wi-Fi e antes de NTP. A associação dessa segunda captura não se
  completou, o que confirma que a imagem veio do cache local, não da rede. Não
  houve panic, WDT ou reboot na captura.

## 2026-09-23 — Home: aplicação LVGL do fundo meteorológico no card

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, USB Serial/JTAG
  COM8. Build ESP-IDF 5.5.4 para `esp32p4` em
  `firmware/build/home-weather-card-20260923`; a app mede `0x27da50` B e deixa
  `0x5825b0` B (69%) livres na menor partição OTA. SHA-256 P4:
  `9274A38CF01A85B547EE7094A252CCD91FD7DCE37B9681953F73B0D8FB0C7EFA`.
- Diagnóstico: o cartão e o worker já liam corretamente o bitmap RGB565, mas
  a primeira exposição da Home dependia da invalidação implícita após a troca
  de source. A correção explicita a opacidade, mantém a imagem como primeiro
  filho do card, invalida o card somente quando a source muda e reduz o overlay
  escuro de 50% para 30%. Assim, a foto é perceptível sem comprometer a leitura
  do texto; atualizações do relógio não redesenham a região de 500 x 462 px.
- `idf.py -B build/home-weather-card-20260923 build app-flash -p COM8`
  recompilou e gravou somente `0x20000`. Nenhuma partição NVS/storage, C6 ou
  eFuse foi gravada pelo flash. No boot, o catálogo validou os 20 arquivos; o
  worker carregou `np_bg_night_partly_cloudy.bin` aos 3,744 s e a task LVGL
  confirmou a aplicação de `500x462 RGB565` aos 4,244 s. A Home ficou visível
  aos 7,994 s, sem panic ou WDT na captura.

## 2026-09-24 — Recuperação do boot após contrato offline v4 proposto

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash e PSRAM de
  32 MiB, USB Serial/JTAG COM8. Código em `04187448b9891ee173dcf98d80e969534628f642-dirty`.
  C6 não foi gravado; o boot reportou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR.
  O hash da imagem atualmente no C6 não foi extraído nesta sessão.
- Falha relatada antes da correção: após `Direct LVGL UI active` aos 1,873 s,
  `Instruction access fault` com `MEPC=0`, `RA=0` e referências a
  `sdmmc_host_init`/`sdmmc_host_get_clk_dividers` na pilha. O log original foi
  fornecido pelo responsável; o ELF correspondente não estava disponível para
  uma decodificação completa. O responsável confirmou que o panic começou
  somente após alterar `offline_data_model.h`. Os endereços SDMMC na pilha
  não identificam por si só a causa do salto para endereço zero.
- O cabeçalho ativo anunciava schema offline 4, mas o codec/cache continuava
  com payload v3 de 34 B e o snapshot em RAM havia crescido. O contrato v4
  foi preservado em
  `design/offline_data_model_v4_proposal.h`; o firmware voltou a anunciar v3
  com os campos efetivamente suportados pelo codec. A ordem original dos
  serviços em `app_main.c` e o worker de SD permaneceram inalterados na imagem
  final. O schema v4 exige codec, migração e orçamento de memória/pilha antes
  de voltar ao firmware ativo.
- Build limpo: ESP-IDF 5.5.4, target `esp32p4`,
  `idf.py -B build/boot-recovery-20260924 -D SDKCONFIG=build/boot-recovery-20260924/sdkconfig -D IDF_TARGET=esp32p4 build`.
  Após restaurar a ordem original, o rebuild final gerou app `0x1d6670` B;
  menor partição `0x800000` B, `0x629990` B livres (77%). SHA-256 final P4:
  `55D03F4579B5D8B956000EC16E2A9FE2BE6BA297F6C2E1A5BEE780BCCA270A5F`.
  SHA-256 do `sdkconfig` efetivo:
  `DDC89CA7B33FD0A25AD4647A1F83BBF15881D6359507A563CE7488E77078BEFA`.
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` permaneceu desligado e
  `CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3`.
- `idf.py -B build/boot-recovery-20260924 -p COM8 app-flash` gravou e
  verificou por hash somente `0x20000` a `0x1f6fff`. Bootloader, tabela,
  `otadata`, NVS, storage, C6 e eFuses não foram escritos. O teste host do
  codec v3 e `git diff --check` passaram.
- A primeira imagem de teste também serializou a montagem do SD; passou em
  dois boots, mas não permitia atribuir a recuperação ao cabeçalho. Essa
  mudança de ordem foi então removida e a imagem final foi recompilada e
  gravada. Com a sequência original, dois boots consecutivos montaram o SD,
  carregaram o visual offline, negociaram C6 3.0.6/RPC v2/SW_AGGR e exibiram
  a Home aos 7,874 s, sem panic no intervalo observado. No primeiro ciclo,
  Wi-Fi, NTP/TLS e três consultas HTTPS também concluíram; o cache avançou
  para a geração 248. O segundo ciclo foi observado até 17,9 s.
- Este A/B confirma que a mudança do cabeçalho é o gatilho observado e que a
  ordem original de SDMMC pode permanecer. Ainda não separa qual alteração do
  cabeçalho causou o acesso inválido nem valida o contrato v4 proposto.

## 2026-09-24 — Ícones Material Symbols na Home

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash NOR e PSRAM de
  32 MiB, USB Serial/JTAG COM8. Base do código:
  `04187448b9891ee173dcf98d80e969534628f642-dirty`. O C6 não foi gravado;
  o boot reportou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR. O hash da imagem
  executada no C6 não foi extraído nesta sessão.
- A Home usa agora a fonte Material Symbols Rounded para os quatro indicadores
  meteorológicos, localização, Bitcoin, dólar, mercado e itens do painel
  lateral. O botão de menu de quatro quadrados permaneceu intacto. Foram
  adicionados ao subset de 18/24 px `place`, `device_thermostat` e
  `attach_money`, e ao de 48 px `currency_bitcoin`, `attach_money` e
  `finance_mode`. Fontes regeneradas com `lv_font_conv@1.5.3` a partir do
  Material Symbols Rounded local.
- Build limpo: ESP-IDF 5.5.4, target efetivo `esp32p4`, LVGL 9.5.0,
  `idf.py -B build/home-icons-20260924 build`. App `0x26e630` B; menor
  partição de app `0x800000` B, `0x5919d0` B livres (70%). SHA-256 P4:
  `1271F91065A7888D7CFF80E5B0A1C30572A4AB8C3BBB17FE584490DFFFEF8755`.
  SHA-256 do `firmware/sdkconfig` efetivo:
  `DDC89CA7B33FD0A25AD4647A1F83BBF15881D6359507A563CE7488E77078BEFA`.
  `CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3` e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` desligado. `git diff --check` passou.
- `idf.py -B build/home-icons-20260924 -p COM8 app-flash` gravou apenas o app
  em `0x20000`; esptool confirmou `Hash of data verified`. A primeira
  tentativa encontrou a COM8 ocupada pelo monitor; após o responsável fechar
  o monitor, a gravação completou. Bootloader, partições de dados, C6 e eFuses
  não foram escritos.
- `idf.py -B build/home-icons-20260924 -p COM8 monitor`: o P4 iniciou o
  display RGB565 em rotação 180°, `triple-partial`, 3 framebuffers aos 1,991 s;
  microSD montado aos 2,261 s; C6 3.0.6/RPC v2/SW_AGGR negociado aos 3,381 s;
  `Home V2 visible` aos 7,971 s. Wi-Fi, NTP e HTTPS concluíram sem erro até
  14,201 s, sem panic no intervalo observado. Os glifos individuais não foram
  inspecionados por foto nesta sessão.

## 2026-09-24 — Header compacto e relógio ampliado

- Placa: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash NOR e PSRAM de
  32 MiB, USB Serial/JTAG COM8. Base do código:
  `04187448b9891ee173dcf98d80e969534628f642-dirty`. C6 não gravado nesta
  sessão; o boot reportou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR. O hash da
  imagem executada no C6 não foi extraído.
- A data foi removida apenas do header; a data do card de clima segue ativa.
  O relógio usa a fonte de 48 px e ocupa 153 px da faixa disponível de 156 px
  à direita do divisor. Header reduzido de 72 para 64 px; os cards principais
  sobem de `y=88` para `y=76` e os strips inferiores de `y=480` para `y=468`.
  O botão de menu de quatro quadrados não foi alterado.
- Build limpo: ESP-IDF 5.5.4, target explícito `esp32p4`, LVGL 9.5.0,
  `idf.py -B build/header-compact-20260924 -D IDF_TARGET=esp32p4 build`, sem
  warnings de compilação. App `0x2841c0` B; menor partição de app `0x800000` B,
  `0x57be40` B livres (69%). SHA-256 P4:
  `7D7BEF5DC4D28DCC0B188902DB42E05AC1503B89022C5AC63863426BF8EA7E8E`.
  SHA-256 do `firmware/sdkconfig` efetivo:
  `DDC89CA7B33FD0A25AD4647A1F83BBF15881D6359507A563CE7488E77078BEFA`.
  `CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3`, flash de 32 MiB e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` desligado.
- `idf.py -B build/header-compact-20260924 -p COM8 app-flash` gravou apenas
  a partição de app em `0x20000` e confirmou `Hash of data verified`; dados,
  bootloader, C6 e eFuses não foram escritos.
- `idf.py -B build/header-compact-20260924 -p COM8 monitor`: display RGB565
  rotação 180°, `triple-partial`, 3 framebuffers aos 2,020 s; microSD montado
  aos 2,290 s; C6 3.0.6/RPC v2/SW_AGGR negociado aos 3,400 s; `Home V2 visible`
  aos 8,000 s. Consultas HTTPS concluíram até 16,520 s sem panic no intervalo
  observado. A geometria visual do relógio não foi conferida por foto.

### 2026-09-24 — Ícones animados Meteocons e alinhamento do card de clima

- Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3, P4 com flash/PSRAM de
  32 MiB, C6 pareado por SDIO. Base Git
  `04187448b9891ee173dcf98d80e969534628f642-dirty`.
  O C6 não foi gravado nesta atividade; o boot negociou firmware 3.0.6,
  RPC v2 e SDIO SW_AGGR. Hash da imagem C6 executada não extraído.
- `tools/build_weather_icons.py` gerou 20 pacotes NPWI a partir dos 20 SVGs
  fornecidos em `design/clima/`. Validação local: todos os nomes casam com o
  catálogo de condições, 20 cabeçalhos/tamanhos/CRC32 íntegros,
  27.427.296 B totais, quadros distintos e prévia visual conferida.
  `tools/copy_weather_icons_to_sd.ps1` copiou e validou os 20 pacotes em um
  diretório de teste local; ainda não foram copiados ao microSD da placa.
- Build limpo: ESP-IDF 5.5.4, `esp32p4`, BSP 3.0.1, LVGL 9.5.0,
  `esp_lvgl_adapter` 0.6.4; comando
  `idf.py -B build/weather-icons -D IDF_TARGET=esp32p4 build`. Aplicação
  `0x284b40` B, partição mínima `0x800000` B, 69% livre. SHA-256 P4
  `54F76744E63BC1BFDB49CC7C6DECEFEF071BC6B23AED5AEC3F7E12664E358A36`.
  SHA-256 do `firmware/sdkconfig` efetivo
  `DDC89CA7B33FD0A25AD4647A1F83BBF15881D6359507A563CE7488E77078BEFA`;
  `CONFIG_LV_USE_ANIMIMG=y`, três framebuffers e auto-suspend desligado.
- `idf.py -B build/weather-icons -p COM8 app-flash` gravou apenas a aplicação
  P4 em `0x20000` e confirmou `Hash of data verified`.
  `idf.py -B build/weather-icons -p COM8 monitor`: RGB565, rotação 180°,
  triple-partial, três FBs aos 2,052 s; microSD e 20 backgrounds validados
  aos 2,332 s; C6 3.0.6/RPC v2/SW_AGGR aos 3,402 s; `Home V2 visible`
  aos 8,032 s; HTTPS de clima/mercado concluído até 14,952 s, sem panic
  no período observado. A animação e o posicionamento em tela precisam de
  verificação física após provisionar os pacotes no microSD.
- Ajuste subsequente da Home: descrição e data do clima em `x=132`, alinhadas
  com a temperatura; descrição em `y=174` e data em `y=208`, abaixo da linha
  da fonte hero (111 px a partir de `y=58`). O novo build no mesmo diretório
  recompilou `np_home.c`, gerou aplicação de `0x284b40` B e SHA-256 P4
  `308F8E05F2142D7EA4141FE2F44B5CAFC1183013FCDA5BAA47881FF62D95F228`.
  `idf.py -B build/weather-icons -p COM8 app-flash` confirmou `Hash of data
  verified`. O monitor confirmou `Home V2 visible` aos 7,972 s e HTTPS
  de clima/mercado até 13,992 s, sem panic no intervalo observado.

### 2026-09-24 — Card de clima sólido e ícones independentes dos backgrounds

- Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3, P4 com 32 MiB de flash e
  PSRAM; C6 pareado por SDIO. Base Git
  `04187448b9891ee173dcf98d80e969534628f642-dirty`.
- O card da Home usa a superfície sólida `#111820`. A UI deixou de criar a
  imagem RGB565 e o overlay. O worker do microSD carrega apenas o pacote
  animado NPWI; os backgrounds existentes no cartão não foram removidos.
- Build limpo: ESP-IDF 5.5.4, `esp32p4`, em
  `firmware/build/weather-solid-20260924`, comando
  `idf.py -B build/weather-solid-20260924 -D IDF_TARGET=esp32p4 build`.
  Aplicação `0x2840e0` B; partição `0x800000` B, 69% livre. SHA-256 P4
  `7F906927F6FA3451DDFF13FE1123698DF08D9C6C5FE97BD80B6D6D23606D6032`.
  SHA-256 do `firmware/sdkconfig` efetivo
  `DDC89CA7B33FD0A25AD4647A1F83BBF15881D6359507A563CE7488E77078BEFA`;
  `CONFIG_LV_USE_ANIMIMG=y`, `CONFIG_SPI_FLASH_AUTO_SUSPEND` desligado e
  formatação automática do SD desligada.
- `idf.py -B build/weather-solid-20260924 -p COM8 app-flash` gravou somente a
  aplicação P4 em `0x20000` e confirmou `Hash of data verified`.
  `idf.py -B build/weather-solid-20260924 -p COM8 monitor` mostrou RGB565,
  rotação 180°, triple-partial e três FBs aos 2,041 s; microSD montado e
  ícones prontos aos 2,101 s; C6 com ESP-Hosted 3.0.6, RPC v2 e SW_AGGR aos
  3,401 s; pacote de 24 quadros carregado aos 4,791 s; Home visível aos
  8,021 s; HTTPS de clima/mercado concluído até 18,721 s e outro pacote de
  24 quadros aos 18,961 s. Sem panic nesse intervalo. Imagem C6 não foi
  alterada; hash do C6 em execução não extraído. O responsável confirmou na
  placa que o card ficou com fundo sólido escuro e o ícone segue animando.

### 2026-09-24 — Contraste dos quatro cards de métricas do clima

- Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3, P4 com flash/PSRAM de
  32 MiB e C6 pareado por SDIO; base Git
  `04187448b9891ee173dcf98d80e969534628f642-dirty`.
- Fundo do card de clima mantido em `#111820`. Os quatro cards de vento,
  umidade, sensação e UV passaram de `#151D26` para `#23313F`, com borda de
  1 px `#34495C`, sem sombra.
- Build limpo ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/weather-tiles-20260924 -D IDF_TARGET=esp32p4 build`.
  Aplicação `0x284160` B em partição de `0x800000` B, 69% livre;
  SHA-256 P4 `3E280E368ABDDD1A0C94B9AF8AACBF927AF1B0E51CA3ED259BD919AEFAE05FF5`.
  Configuração efetiva do P4 como na seção anterior; C6 não alterado e hash
  de sua imagem em execução não extraído.
- `idf.py -B build/weather-tiles-20260924 -p COM8 app-flash` gravou só a
  aplicação P4 e confirmou `Hash of data verified`. No monitor, display
  RGB565, rotação 180°, triple-partial e três FBs aos 2,050 s; microSD
  montado e ícones prontos aos 2,110 s; C6 3.0.6/RPC v2/SW_AGGR aos 3,400 s;
  ícone animado de 24 quadros aos 4,800 s; Home visível aos 8,030 s;
  HTTPS dos três domínios concluído até 14,310 s, sem panic no intervalo.
  O responsável confirmou visualmente que os quatro cards ficaram destacados
  e legíveis na placa.

### 2026-09-24 — Ícone animado de clima ampliado na Home

- Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3, P4 32 MiB flash/PSRAM,
  C6 pareado por SDIO; base Git
  `04187448b9891ee173dcf98d80e969534628f642-dirty`.
- O widget animado passou a ocupar `160 x 160` px, em `x=24`, `y=62` dentro
  do card de clima. Temperatura, condição e data começam em `x=200`; as
  métricas continuam em `y=250`. O LVGL amplia os quadros NPWI de `96 x 96`
  px na renderização. O conteúdo do microSD não foi alterado.
- Build limpo ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/weather-icon-large-20260924 -D IDF_TARGET=esp32p4 build`.
  Aplicação `0x2841e0` B; partição de `0x800000` B, 69% livre. SHA-256 P4
  `6973E1E37660756883CAECB7765F52BA446910AAA5A0600FC493316FCAE35FFB`.
  Configuração efetiva do P4 como nas seções anteriores; C6 não alterado e
  hash de sua imagem em execução não extraído.
- `idf.py -B build/weather-icon-large-20260924 -p COM8 app-flash` gravou só a
  aplicação P4 em `0x20000` e confirmou `Hash of data verified`. No monitor,
  display RGB565, rotação 180°, triple-partial e três FBs aos 1,981 s;
  microSD montado e ícones prontos aos 2,041 s; C6 3.0.6/RPC v2/SW_AGGR
  aos 3,401 s; pacote de 24 quadros aos 4,701 s; Home visível aos 7,971 s;
  HTTPS dos três domínios concluído até 18,281 s e novo pacote animado aos
  19,031 s, sem panic no intervalo. O responsável confirmou que o ícone
  ampliado anima, ocupa a altura desejada e não encosta nos textos ou cards.

### 2026-09-24 — Pacotes Meteocons nativos de 160 px preparados

- Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3, P4 32 MiB flash/PSRAM,
  C6 pareado por SDIO; base Git
  `04187448b9891ee173dcf98d80e969534628f642-dirty`.
- `tools/build_weather_icons.py` gerou 20 pacotes NPWI de `160 x 160` px em
  `design/clima/generated-160/`, a partir dos SVGs locais. Total
  76.186.080 bytes; todos os cabeçalhos, tamanhos e CRC32 íntegros, com
  quadros distintos. O gerador preservou os pacotes antigos de 96 px.
- O carregador P4 aceita pacotes de 96 e 160 px, lê o tamanho no cabeçalho e
  reserva dois buffers PSRAM para o limite de 48 quadros nativos. A Home
  desenha 160 px; com pacotes nativos passa a 1:1. O instalador do SD valida
  os 20 arquivos, copia para staging, compara SHA-256 e preserva a pasta
  antiga como backup antes de ativar a nova.
- Build limpo: ESP-IDF 5.5.4, `esp32p4`, comando
  `idf.py -B build/weather-icon-native-20260924 -D IDF_TARGET=esp32p4 build`.
  Aplicação `0x2841e0` B; partição `0x800000` B, 69% livre. SHA-256 P4
  `22760721C42AA8E3CAB4B7600BAE89DF7C674A8D92B41AA84D1F9A79B4160DB5`.
  SHA-256 do `firmware/sdkconfig` efetivo
  `DDC89CA7B33FD0A25AD4647A1F83BBF15881D6359507A563CE7488E77078BEFA`.
- `idf.py -B build/weather-icon-native-20260924 -p COM8 app-flash` gravou só
  a aplicação P4 em `0x20000` e confirmou `Hash of data verified`. No monitor,
  display RGB565, rotação 180°, triple-partial e três FBs aos 1,990 s;
  microSD montado e ícones prontos aos 2,050 s; C6 3.0.6/RPC v2/SW_AGGR
  aos 3,400 s; pacote antigo de 24 quadros `96x96` carregado aos 4,700 s e
  novamente aos 17,290 s; Home visível aos 7,980 s; HTTPS dos três domínios
  concluído até 16,210 s, sem panic no intervalo. C6 não alterado; hash de
  sua imagem em execução não extraído.
- O responsável optou por copiar os novos pacotes ao microSD depois. A
  renderização nativa de 160 px na placa ainda não foi verificada; a Home
  continua usando os arquivos antigos ampliados até essa cópia.

### 2026-09-24 — Confirmação dos pacotes de 160 px no microSD

- Após a substituição dos pacotes pelo responsável, o monitor registrou
  `loaded animated weather icon: 24 frames, 160x160`; a Home apareceu sem
  panic e o responsável confirmou melhora visual. O monitor foi encerrado.

### 2026-09-24 — Detalhes de clima, BTC e PTAX na Home

- Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3, P4 com 32 MiB flash e
  PSRAM; C6 3.0.6 por SDIO. Base Git
  `04187448b9891ee173dcf98d80e969534628f642-dirty`.
- Contrato/codec offline v4 de 169 B, leitura compatível com cache v1-v3;
  Open-Meteo fornece vento, sensação e UV; CoinGecko `/coins/markets` fornece
  máxima, mínima e volume USD de 24 h; BCB SGS 1 fornece as duas últimas PTAX
  para a variação em pontos-base. Ibovespa ficou indisponível por escolha do
  responsável. Os testes host do codec e dos parsers passaram.
- GETs de validação dos endpoints públicos retornaram HTTP 200 com respostas
  de 556 B (Open-Meteo), 757 B (CoinGecko) e 79 B (BCB), abaixo dos limites
  de 768, 3072 e 192 B. Nenhum corpo HTTP entra no estado ou nos logs.
- Build limpo: ESP-IDF 5.5.4, target `esp32p4`, comando
  `idf.py -B build/home-details-20260924 -D IDF_TARGET=esp32p4 build`.
  Após ajuste do rótulo para “Dólar PTAX”, rebuild terminou com app
  `0x2856c0` B em partição `0x800000` B, 68% livres. SHA-256 final P4
  `1109194E72CE07CF64FC753813AF80610994487512A43292C9D36C5373173C29`;
  SHA-256 do `firmware/sdkconfig` efetivo
  `DDC89CA7B33FD0A25AD4647A1F83BBF15881D6359507A563CE7488E77078BEFA`.
- `idf.py -B build/home-details-20260924 -p COM8 app-flash` gravou somente
  a aplicação P4 em `0x20000` e confirmou `Hash of data verified`. No boot
  final, display RGB565/180°/triple-partial/3 FBs, microSD montado, C6
  3.0.6/RPC v2/SW_AGGR, pacote animado `160x160` aos 7,711 s e Home
  visível aos 7,971 s. BTC, clima e PTAX concluíram HTTPS+parse com
  `ESP_OK` até 17,121 s; geração 275 do cache terminou. Sem panic no período
  observado. C6 não foi alterado e o hash de sua imagem em execução não foi
  extraído; a aparência dos campos novos depende de confirmação visual.

### 2026-09-27 — Navegação com ciclo de vida sob demanda

- Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3, P4 com 32 MiB de flash e
  PSRAM; C6 permaneceu na imagem 3.0.6/RPC v2/SW_AGGR por SDIO. Base Git
  `174d66c-dirty`.
- O controlador agora mantém uma única cena de produto em memória: Boot é
  destruída ao criar a Home; Configurações só é criada pelo toque no novo item
  do drawer; e, ao voltar pela Home da própria tela, Configurações é destruída
  antes de recriar a Home. A projeção de AppState é atualizada apenas quando a
  Home está ativa. As trocas usam `lv_async_call`, para nunca apagar a árvore
  que está despachando o toque.
- Build limpo ESP-IDF 5.5.4, target `esp32p4`:
  `idf.py -B build/lazy-navigation-20260926 -DIDF_TARGET=esp32p4 build`.
  Aplicação `0x2867c0` B em partição de `0x800000` B, com `0x579840` B (68%)
  livres. SHA-256 P4
  `85EACC667E015310EB458A5D02A938994F7184F17762CD6108D03D728CF83534`.
  SHA-256 do `firmware/sdkconfig` efetivo
  `DDC89CA7B33FD0A25AD4647A1F83BBF15881D6359507A563CE7488E77078BEFA`.
- A primeira gravação da build validou o ciclo de vida e o monitor confirmou
  P4 v1.3, PSRAM 32 MiB, EK79007, GT911, RGB565, rotação 180°,
  triple-partial com três framebuffers, C6 3.0.6/RPC v2/SW_AGGR e Home visível
  aos 7,982 s. Wi-Fi recebeu IP e as três atualizações HTTPS concluíram com
  `ESP_OK`; não houve panic ou WDT na captura.
- Após acrescentar a recuperação de falha de agendamento LVGL, o rebuild
  incremental produziu o SHA-256 final acima e
  `idf.py -B build/lazy-navigation-20260926 -p COM8 app-flash` gravou somente
  a aplicação P4 em `0x20000`, com `Hash of data verified`. A abertura de
  Configurações e a volta para Home continuam pendentes de observação direta
  pelo touch.

### 2026-09-27 — Revelação gradual dos cards de Configurações

- A árvore de Configurações continua sendo criada somente após o toque no
  item do drawer. A raiz é exibida primeiro e os cards de conectividade, tela
  e som, e sistema são revelados em três ciclos LVGL de 48 ms. O timer é
  cancelado ao voltar para Home; se a alocação dele falhar, os três cards são
  exibidos de uma vez para que a tela permaneça utilizável.
- Build ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/lazy-navigation-20260926 build`. Aplicação `0x286940` B
  em partição de `0x800000` B, com `0x5796c0` B (68%) livres. SHA-256 P4
  `0073F885D20646E0B6F9042C7C7703E6C8B13C354CC4F16A422C8B9CBD11D098`.
- `idf.py -B build/lazy-navigation-20260926 -p COM8 app-flash` gravou apenas
  a aplicação P4 em `0x20000`. O boot posterior confirmou P4 v1.3, PSRAM de
  32 MiB, EK79007, GT911, RGB565/180°/triple-partial/3 FBs, C6
  3.0.6/RPC v2/SW_AGGR, Home aos 8,031 s, Wi-Fi com IP e as três atualizações
  HTTPS com `ESP_OK`; não houve panic ou WDT na captura. A abertura pelo
  touch e a sequência visual dos cards ainda requerem observação direta.

### 2026-09-27 — Remoção da navegação duplicada em Configurações

- A barra lateral própria de Configurações foi removida. A tela usa somente o
  drawer do cabeçalho, igual à Home: o item Configurações fica destacado nela
  e o item Home retorna à Home. Os três cards foram redistribuídos na largura
  livre entre `x=24` e `x=1000`.
- Build ESP-IDF 5.5.4 para `esp32p4` passou com app `0x286940` B e
  `0x5796c0` B (68%) livres no slot de `0x800000` B. SHA-256 P4:
  `280185A57C15C220D8BEBC96EDC0DC649DD141551A483762C8A2ABC9330A97A4`.
- A imagem foi gravada somente no P4, em `0x20000`, via COM8. O boot final
  confirmou P4 v1.3, PSRAM 32 MiB, display EK79007, GT911,
  RGB565/180°/triple-partial/3 FBs, C6 3.0.6/RPC v2/SW_AGGR, Home aos
  7,981 s e Wi-Fi com IP. Não houve panic ou WDT na captura; o monitor foi
  encerrado e a COM8 liberada. A aparência da navegação no touch ainda requer
  confirmação visual na placa.

### 2026-09-27 — Layout de Configurações alinhado à referência visual

- A referência recebida redefine a intenção da barra lateral: ela é a
  navegação persistente da tela, não uma duplicação acidental. Configurações
  volta a exibir os cinco ícones na rail, com Configurações ativa e Home como
  retorno. Os painéis reproduzem a composição de perfil/rede, tela/som e
  sistema da referência. A cena ainda só é construída após o toque no menu.
- Build ESP-IDF 5.5.4 para `esp32p4`: app `0x290690` B em slot de
  `0x800000` B, com `0x56f970` B (68%) livres. A gravação da aplicação P4 em
  `0x20000` pela COM8 concluiu com `Hash of data verified`. SHA-256 P4:
  `F5F166FD45971011A401055B00AD7EF21B2359E9463767BECFB9C7C1EE67B82D`.
- O monitor posterior confirmou P4 v1.3, PSRAM 32 MiB, EK79007, GT911,
  RGB565/180°/triple-partial/3 FBs, C6 3.0.6/RPC v2/SW_AGGR, Home aos
  7,998 s, Wi-Fi com IP e a primeira atualização HTTPS com `ESP_OK`, sem
  panic ou WDT durante a captura. O monitor foi encerrado ao fim da captura.

### 2026-09-27 — Correção do watchdog na abertura de Configurações

- A captura de campo registrou `task_wdt` com `IDLE0` bloqueada e a task
  `lvgl` em `open_settings_async`: a abertura criava os três painéis inteiros
  no mesmo ciclo LVGL. A tela agora cria apenas a raiz, o header e o drawer no
  ciclo de navegação; cada painel é construído e revelado em um ciclo LVGL
  separado, espaçado em 80 ms. O fallback sem timer usa chamadas assíncronas
  separadas e não volta a construir todos os painéis de uma vez.
- Build ESP-IDF 5.5.4 para `esp32p4` passou: app `0x2870b0` B em slot de
  `0x800000` B, com `0x578f50` B (68%) livres. A gravação P4 não foi repetida
  nesta etapa porque a COM8 estava ocupada por um monitor ESP-IDF externo
  ativo (`C:\Espressif\tools\python\v5.5.4\venv`); nenhuma sessão do
  responsável foi encerrada. O teste físico de abrir Configurações permanece
  pendente dessa gravação.

### 2026-09-27 — Retorno ao layout anterior de Configurações

- O layout ampliado foi revertido por preferência visual. Configurações voltou
  à composição anterior, preservando a criação escalonada dos cards em ciclos
  LVGL separados para não bloquear `IDLE0`.
- Build ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/lazy-navigation-20260926 build`. Aplicação `0x2870b0` B
  em partição de `0x800000` B, com `0x578f50` B (68%) livres. SHA-256 P4:
  `B60BD6ED90D5D8DC58DDBF53F6D14CD40443042C3F395CF3442D804D592E6AFD`.
- `idf.py -B build/lazy-navigation-20260926 -p COM8 app-flash` gravou apenas
  a aplicação P4 em `0x20000`, com `Hash of data verified`. O boot posterior
  confirmou P4 v1.3, PSRAM de 32 MiB, EK79007, GT911,
  RGB565/180°/triple-partial/3 FBs, C6 3.0.6/RPC v2/SW_AGGR, Home visível aos
  8,023 s e Wi-Fi com IP. A captura de 15 s não registrou panic nem watchdog.

### 2026-09-27 — Simplificação de Configurações

- O seletor de tema foi removido. O painel Sistema deixou de exibir dados
  técnicos de display, touch, firmware, atividade e temperatura; permanece
  somente com as ações de atualização e reinício. A criação escalonada dos
  cards LVGL continua inalterada.
- Build ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/lazy-navigation-20260926 build`. Aplicação `0x286c50` B
  em partição de `0x800000` B, com `0x5793b0` B (68%) livres. SHA-256 P4:
  `1B629BA323F6C6D421AB961D2656AD26E8AD23401F79A4777BCD880A65749106`.
- `idf.py -B build/lazy-navigation-20260926 -p COM8 app-flash` gravou apenas
  a aplicação P4 em `0x20000`, com `Hash of data verified`. O boot posterior
  confirmou P4 v1.3, PSRAM de 32 MiB, EK79007, GT911,
  RGB565/180°/triple-partial/3 FBs, C6 3.0.6/RPC v2/SW_AGGR, Home visível aos
  8,033 s, Wi-Fi com IP e HTTPS com `ESP_OK`; não houve panic nem watchdog.
