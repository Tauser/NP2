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

### 2026-09-27 — Espaçamentos de Configurações

- O espaço liberado foi redistribuído sem restaurar os dados removidos: Modo
  noturno foi centralizado na área inferior de Tela e som; Sistema voltou à
  altura total disponível, com ações maiores de atualização e reinício
  separadas por seções.
- Build ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/lazy-navigation-20260926 build`. Aplicação `0x286cf0` B
  em partição de `0x800000` B, com `0x579310` B (68%) livres. SHA-256 P4:
  `7D67BD9EB38BDFD5BEFD85F7D8D23E7142709FA35EDF50287CE5706D489CFEBE`.
- `idf.py -B build/lazy-navigation-20260926 -p COM8 app-flash` gravou apenas
  a aplicação P4 em `0x20000`, com `Hash of data verified`. O boot posterior
  confirmou P4 v1.3, PSRAM de 32 MiB, EK79007, GT911,
  RGB565/180°/triple-partial/3 FBs, C6 3.0.6/RPC v2/SW_AGGR, Home visível aos
  7,983 s, Wi-Fi com IP e HTTPS com `ESP_OK`; não houve panic nem watchdog.

### 2026-09-27 — Revisão de UX de Configurações

- Configurações foi alinhada à grade da Home V2: painel principal em
  `24/560` e coluna auxiliar em `600/400`, com o mesmo espaçamento de 16 px,
  superfícies chapadas compartilhadas e cabeçalhos de seção sem texto
  redundante. Os grupos foram distribuídos por tarefa: tela e som, conexão e
  ações do sistema. A criação gradual dos cards LVGL foi preservada.
- Build ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/lazy-navigation-20260926 build`. Aplicação `0x286a00` B
  em partição de `0x800000` B, com `0x579600` B (68%) livres. SHA-256 P4:
  `AA27462FD99F99C5F1C2BDC967F941C2E52E4BC3E96DABE549286F846F4B60C4`.
- `idf.py -B build/lazy-navigation-20260926 -p COM8 app-flash` gravou apenas
  a aplicação P4 em `0x20000`, com `Hash of data verified`. O boot posterior
  confirmou P4 v1.3, PSRAM de 32 MiB, EK79007, GT911,
  RGB565/180°/triple-partial/3 FBs, C6 3.0.6/RPC v2/SW_AGGR, Home visível aos
  7,993 s e Wi-Fi com IP; não houve panic nem watchdog na captura de 15 s.

### 2026-09-27 — Composição principal de Configurações

- A tela adotou um painel principal horizontal para Tela e som e dois painéis
  inferiores equivalentes para Conectividade e Sistema. A composição segue a
  referência visual recebida, preservando tema removido e Sistema restrito às
  ações de atualizar e reiniciar. Os três painéis continuam criados em ciclos
  LVGL separados quando a tela é aberta.
- Build ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/lazy-navigation-20260926 build`. Aplicação `0x286a00` B
  em partição de `0x800000` B, com `0x579600` B (68%) livres. SHA-256 P4:
  `09F3136BA1D2108D8A700DE61DF1B5C57DB8271E25034C563498DFF59CDEBE9C`.
- `idf.py -B build/lazy-navigation-20260926 -p COM8 app-flash` gravou apenas
  a aplicação P4 em `0x20000`, com `Hash of data verified`. O boot posterior
  confirmou P4 v1.3, PSRAM de 32 MiB, EK79007, GT911,
  RGB565/180°/triple-partial/3 FBs, C6 3.0.6/RPC v2/SW_AGGR, Home visível aos
  8,001 s e Wi-Fi com IP; não houve panic nem watchdog na captura de 15 s.

### 2026-09-27 — Três cards de Configurações

- Settings usa exclusivamente `np_header()` e mantém seu drawer oculto por
  padrão. A tela contém Tela e som (`24,76,976,286`), Conectividade
  (`24,378,480,198`) e Sistema (`520,378,480,198`), criados em ciclos LVGL
  separados. Tema e modo noturno foram organizados no lado direito do card
  superior; Sistema usa grade 2x2 e dois botões. Dados sem contrato de binding
  permanecem em `--`; schema offline, cache, providers e Home não mudaram.
- Build ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/lazy-navigation-20260926 build`. Aplicação `0x286cc0` B
  em partição de `0x800000` B, com `0x579340` B (68%) livres. SHA-256 P4:
  `2BEE1FDAA3C0CDFEBEAB704DBE1695EB495000DCE17588F38EB93334E0D4A29C`.
- `idf.py -B build/lazy-navigation-20260926 -p COM8 app-flash` gravou apenas
  a aplicação P4 em `0x20000`, com `Hash of data verified`. O boot posterior
  confirmou P4 v1.3, PSRAM de 32 MiB, EK79007, GT911,
  RGB565/180°/triple-partial/3 FBs, C6 3.0.6/RPC v2/SW_AGGR, Home visível aos
  8,042 s, Wi-Fi com IP e HTTPS com `ESP_OK`; não houve panic nem watchdog.

### 2026-09-27 — Controles de brilho e volume em Configurações

- Os controles de Brilho da tela e Volume geral passaram de elementos
  estáticos para sliders LVGL. A tela somente devolve os handles; `product_ui`
  atualiza o percentual e encaminha a intenção à `device_control_service`.
  Essa task coalesce alterações de toque, aplica brilho pelo BSP e inicializa
  sob demanda o ES8311 antes de ajustar o volume. Não há I/O na task LVGL nem
  escrita em NVS.
- Build ESP-IDF 5.5.4 para `esp32p4`:
  `idf.py -B build/lazy-navigation-20260926 build`. Aplicação `0x296650` B
  em partição de `0x800000` B, com `0x5699b0` B (68%) livres. SHA-256 P4:
  `C047ABB260D3D31A287FAAD6D19104A2379A49AF8A96CC23FA75F8639DE1B13D`.
- A tentativa de `idf.py -B build/lazy-navigation-20260926 -p COM8 app-flash`
  não gravou a placa: a porta retornou `PermissionError(13)` por estar ocupada.
  Nenhum processo de monitor foi interrompido e não há evidência de boot para
  este binário até que a COM8 seja liberada.

### 2026-09-27 — Redraw da Home e watchdog LVGL

- Placa ESP32-P4 v1.3 com 32 MiB de flash e PSRAM, EK79007, GT911 e C6
  conectado por SDIO. O boot mostrou ESP-Hosted 3.0.6 no P4 e C6, RPC v2 e
  SW_AGGR. O hash da imagem instalada no C6 não foi coletado neste ensaio;
  nenhum flash do C6 foi executado.
- No commit `aad5a71`, a Home passou a atualizar os cards de clima e mercado
  quando o snapshot exibido ou o ícone mudam. O relógio continua atualizado
  pela projeção. Isso evita chamar novamente o desenho da linha do mercado
  em cada atualização de segundo do `app_loop`.
- Build `idf.py build` para `esp32p4` com IDF 5.5.4: aplicação `0x298780` B,
  68% livres na menor partição de aplicação. SHA-256 da imagem P4:
  `AE7E02327E5135B5DDC9384D34060CA2E75E1577A125E4A3669DBA150D172BC6`.
  A configuração efetiva mostrou RGB565, três framebuffers, ESP-Hosted e
  Wi-Fi remoto; o boot confirmou rotação de 180° e `TRIPLE_PARTIAL`.
- `idf.py -p COM8 flash` gravou o P4 e verificou os hashes. Em seguida,
  `idf.py -p COM8 monitor` reiniciou a placa: a Home apareceu em 8,004 s,
  o ícone animado de clima (48 frames, 160×160) carregou em 12,134 s e
  DNS/NTP/HTTPS terminaram com `ESP_OK` até 31,234 s. A observação em repouso
  passou de 60 s sem novo `task_wdt` ou panic. O gesto que produziu o log
  original ainda não foi reproduzido neste ensaio; estabilidade sob toque
  e soak prolongado permanecem abertos.

### 2026-09-27 — Transição Home para Settings

- O commit `4941d26` libera a Home e sua animação antes de criar a árvore
  Settings na task LVGL. A troca continua assíncrona em relação ao callback
  do botão; o header e o drawer compartilhados não foram modificados.
- `idf.py build` com IDF 5.5.4 para `esp32p4` passou. Aplicação `0x298780` B,
  68% livres no slot de 8 MiB. SHA-256 da imagem P4:
  `FC7576577BDA175FDB94248277FE5F29B770E47CD9323C8C0F0332A00A2E4B9F`.
  `idf.py -p COM8 flash` confirmou `Hash of data verified` e reiniciou o P4.
- `idf.py -p COM8 monitor` mostrou Home aos 8,014 s, ícone animado aos
  12,164 s, C6 3.0.6/RPC v2/SW_AGGR e DNS/NTP/HTTPS `ESP_OK` até 17,004 s.
  A observação em repouso passou de 60 s sem `task_wdt` ou panic. Não houve
  toque físico em Settings durante essa captura; o gatilho original permanece
  sem reprodução controlada. C6 e eFuses não foram escritos.

### 2026-09-27 — Persistência de brilho e volume e correção do abort

- Placa ESP32-P4 v1.3, flash/PSRAM de 32 MiB, EK79007, GT911 e C6 por
  ESP-Hosted 3.0.6/RPC v2/SDIO SW_AGGR. O hash da imagem C6 instalada não foi
  coletado; o C6 não foi regravado. Configuração efetiva: IDF 5.5.4,
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` desativado, display RGB565/180°/
  `TRIPLE_PARTIAL`/três framebuffers e Wi-Fi remoto.
- `de8dab1` adicionou um perfil independente de dois slots com CRC em NVS
  (`np2_controls`), escrito pelo `FlashCoordinator` após liberação dos sliders
  e pausa de 500 ms. O serviço restaura os valores no boot; a UI apresenta
  sucesso apenas após confirmação da escrita. A volta para Home libera a árvore
  Settings antes de reconstruir a cena.
- O primeiro ensaio físico reproduziu um `abort()` em `lock_acquire_generic`
  logo após ajustar o brilho. A pilha capturada continha
  `_lock_acquire_recursive` e `_vfprintf_r`. A causa era `ESP_LOGI` dentro
  de `portENTER_CRITICAL` no caminho de sucesso do coordenador.
  `6b0b692` moveu esse log para fora do lock, preservando somente a atualização
  de estado protegida.
- `idf.py clean build` passou para `esp32p4`; após o segundo commit,
  `idf.py build` gerou a imagem P4 `0x2995c0` B na partição de `0x800000` B
  (68% livres). SHA-256 P4:
  `FEF9BA007EA4FF649F1855363D61432090EC3440A44BEC109E2D6AAE86DA7595`.
  `idf.py -p COM8 app-flash` confirmou `Hash of data verified` e reiniciou o P4.
- No ensaio com a imagem final, a captura serial registrou brilho em 64% e
  100%, seguido de `display and volume preferences saved` em 11 ms e 12 ms,
  sem novo abort nessa sequência. O responsável confirmou que os sliders,
  o feedback e a volta para Home funcionaram. Um reboot via
  `idf.py -p COM8 monitor` restaurou `brightness=100 volume=65` e mostrou a
  Home aos 8,045 s. O valor alterado de volume não foi verificado em reboot
  nesta captura; soak prolongado permanece aberto.

### 2026-10-01 — Ajuste da tela Clima

- Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3, confirmada pelo boot;
  flash/PSRAM de 32 MiB, EK79007 e GT911. C6 conectou por SDIO com ESP-Hosted
  3.0.6 nos dois lados, RPC v2 e SW_AGGR. O hash da imagem C6 instalada não
  foi coletado; nenhum flash do C6 foi executado.
- A tela Clima permite quebra de linha nas descrições diárias, zera borda e
  padding herdados dos cards e mantém o card de detalhes sem título. Títulos e
  descrições continuam alinhados como na referência; o conteúdo foi ajustado
  dentro dos cards sem centralizar a composição dos textos.
- Os ícones de previsão, nascer/pôr do sol e detalhes usam o pacote oficial
  `@meteocons/svg-static` 0.1.0, estilo fill, com licença em
  `design/clima/static/LICENSE`. O gerador `tools/build_meteocons_static.py`
  seleciona 24 SVGs e gera as versões 32 × 32 e 48 × 48 como descritores LVGL
  ARGB8888 embutidos no firmware. Isso não adiciona leitura de SVG em execução
  nem alocação de PSRAM. O ícone principal continua animado, carregado da
  microSD.
- Build limpo ESP-IDF 5.5.4, target `esp32p4`, comando
  `idf.py -B build/weather-meteocons-final-20261001 -D IDF_TARGET=esp32p4 build`.
  Imagem `0x309700` B, com 62% livres na menor partição de 8 MiB. SHA-256 P4:
  `B4771687DD427F7BF3371C58CF48B10BE0813185FD552E82BC892CC7031D64B0`.
  Configuração efetiva: RGB565, três framebuffers, rotação 180°,
  `TRIPLE_PARTIAL`, auto-suspend da flash desligado e Wi-Fi remoto.
- `idf.py -B build/weather-meteocons-final-20261001 -p COM8 app-flash` gravou
  somente a aplicação em `0x20000`; `Hash of data verified`. A captura de
  `idf.py -B build/weather-meteocons-final-20261001 -p COM8 monitor` confirmou
  P4 v1.3, display/touch, UI LVGL RGB565/180°/`TRIPLE_PARTIAL`/3 FBs, C6
  3.0.6/RPC v2/SW_AGGR, microSD e ícone animado 24 × 160 × 160. Home visível
  aos 19,004 s. A associação Wi-Fi caiu com motivos 2 e 205; DNS/NTP/HTTPS não
  foram confirmados nesta captura. Nenhum panic ou watchdog apareceu durante
  os ~19 s observados. O operador não navegou até Clima nesta captura; sua
  apresentação na tela física ainda requer inspeção visual.

### 2026-10-01 — Cadência dos ícones animados

- Inspeção do pipeline identificou que `tools/build_weather_icons.py` amostra
  os SVGs a 8 quadros/s (`125 ms`), enquanto Home e Clima definiam duração de
  `250 ms` por quadro. Ambos exibiam 4 quadros/s e levavam o dobro do tempo por
  ciclo. Home havia recebido esse limite junto com uma mudança de renderer SW
  para reduzir carga; Clima herdou o limite sem motivo próprio.
- Home e Clima agora calculam a duração por `frame_count * frame_ms` do pacote
  NPWI, sem alterar os dados dos frames ou o orçamento de PSRAM. Build limpo
  ESP-IDF 5.5.4/`esp32p4`:
  `idf.py -B build/weather-animation-20261001 -D IDF_TARGET=esp32p4 build`.
  Imagem `0x309700` B, 62% livres no menor slot de 8 MiB. SHA-256 P4:
  `2D3C129064C6E1EBAA686B475AD0B9E563FCE26F2DBB1A09982EE36973158822`.
- `idf.py -B build/weather-animation-20261001 -p COM8 app-flash` gravou só a
  aplicação P4 em `0x20000` e confirmou `Hash of data verified`. Boot frio na
  Waveshare ESP32-P4 v1.3 confirmou display RGB565/180°/`TRIPLE_PARTIAL`/3 FBs,
  microSD e pacote animado de 24 quadros, 160 × 160; C6 3.0.6 negociou RPC v2 e
  SW_AGGR. Home visível aos 11,985 s. DNS/NTP/HTTPS concluíram com `ESP_OK`.
  Cerca de 60 s de monitoramento não registraram panic ou watchdog. O quadro
  serial confirma o carregamento e a estabilidade curta, mas a fluidez visual
  foi conferida pelo operador na tela física e confirmada como fluida. Soak
longo continua aberto.

# 2026-10-01 — Mercado v6, altcoins e Fear & Greed

Implementado e gravado no P4 o primeiro fluxo de dados da tela Mercado. O
snapshot offline v6 guarda Bitcoin, Ethereum, Solana, BNB, XRP e Fear & Greed;
cache v1 a v5 continua sendo migrado com os campos novos indisponíveis. Os
ativos cripto vieram em uma consulta CoinGecko, e Fear & Greed em uma consulta
serializada da Alternative.me. A tela mostra a atribuição `Alternative.me` ao
lado do indicador. O usuário pediu para manter todos os blocos da referência;
S&P 500, Nasdaq e Ibovespa seguem visíveis, porém indisponíveis enquanto não
houver uma fonte de dados e licença compatíveis configuradas.

**Placa e baseline:** Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash
NOR 32 MiB, PSRAM 32 MiB; BSP 3.0.1, ESP-IDF 5.5.4, LVGL 9.5.0, target
`esp32p4`, RGB565, rotação 180°, `TRIPLE_PARTIAL`, três framebuffers. C6
reportou 3.0.6, RPC v2 e SDIO SW_AGGR negociado; C6 não foi gravado.

**Referências de imagem e execução:** HEAD base `80c489d5ed2808c9501b2dd474baa7ad5b051dcb`;
árvore de trabalho alterada. P4 `firmware/build/np2_p4.bin`, SHA-256
`0E8B78DF45B6E56D1ACE8FD62CE7177D015FF8D7EA9E39A05007064A6DEE7058`, tamanho
`0x30c2c0` bytes, 62% da partição de app livre. ELF SHA-256
`3BA3185C808EFF27CFD852214174827536F5B010901C4BDB8404CFDF54B126A8`. A
referência do firmware C6 existente em
`coprocessor/build/c6-repro-final-20260913/eh_cp_ota_coprocessor_ota.bin` tem
SHA-256 `6B4E50892CDB750318D2A192F6254447A335673EFFB67E225D3E30D02941FA61`
(1,108,256 bytes); no boot, host e slave reportaram `3.0.6` compatível.

**Comandos e resultados:**

- `gcc -std=c11 -Wall -Wextra -Werror` nos testes host de codec, providers e
  scheduler — PASS.
- `idf.py build` com o ambiente do build configurado para Python 3.14 e IDF
  5.5.4 — PASS; partição e tamanhos acima confirmados.
- `idf.py -p COM8 flash` — PASS, hashes de cada segmento verificados pelo
  esptool; hard reset concluído.
- `idf.py -p COM8 monitor` — boot chegou à UI Home; SDIO 4-bit, RPC v2 e
  SW_AGGR levantaram; Wi-Fi obteve `192.168.1.16`; HTTPS/NTP concluíram.
  Provedores CoinGecko/BTC+altcoins, Open-Meteo, BCB/PTAX e Alternative.me
  reportaram `ESP_OK`; o cache offline publicou geração 692. O primeiro boot
  havia identificado limite de corpo insuficiente para Alternative.me; o
  limite foi aumentado para 512 bytes, refeito o build e a gravação, e o
  endpoint confirmou `ESP_OK` nos boots seguintes, incluindo a imagem com o
  ajuste visual final. Nenhum flash C6 ocorreu.

**Limites desta evidência:** o boot e as atualizações de dados foram observados
na placa; não houve navegação por toque nem inspeção fotográfica da composição
Mercado. Esta evidência não valida estabilidade gráfica nem autoriza release.

### 2026-10-01 — Mercado: tipografia e distribuição dos cards

Ajustado o card BTC: `US$` menor e separado do preço, variação em menor escala,
indicador de status apenas como ponto, e USD/BRL movido para a quarta coluna da
linha inferior do card. O painel Fear & Greed foi ampliado e os três cards de
índices redistribuídos no espaço liberado. Os glifos Material Symbols de seta
para cima/baixo foram adicionados à fonte de 48 px; a variação exibe também o
sinal negativo. Os cards continuam sem bordas.

Os valores de S&P 500 e Nasdaq continuam indisponíveis e o campo de Ibovespa
segue sem serviço de atualização. Nenhuma cotação foi simulada. O feed atual de
Fear & Greed continua sendo Alternative.me; sua atribuição foi mantida junto ao
valor conforme as condições da API. A tela Mercado aguarda confirmação de uma
fonte com direitos de exibição para os três índices.

**Placa e configuração efetiva:** Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4
v1.3, flash NOR 32 MiB, PSRAM 32 MiB; BSP 3.0.1, ESP-IDF 5.5.4, LVGL 9.5.0,
target `esp32p4`, RGB565, rotação 180°, `TRIPLE_PARTIAL`, três framebuffers,
`CONFIG_SPI_FLASH_AUTO_SUSPEND=n`. C6 reportou ESP-Hosted 3.0.6, RPC v2 e
SDIO SW_AGGR negociado; C6 não foi gravado. SHA-256 de `firmware/sdkconfig`:
`32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.

**Build e gravação:** base `80c489d5ed2808c9501b2dd474baa7ad5b051dcb`,
árvore de trabalho alterada. `idf.py build` — PASS; imagem `0x309250` bytes,
62% da partição mínima de app livre. SHA-256 de `firmware/build/np2_p4.bin`:
`9C0E4EAE8B311CC974AAE81BC81BFA0E954CA3D33115F5A2CCC468CD7D717D99`.
ELF SHA-256 `F7B6C9C4230F12A1CD70E87C14ADDAE4EBA2F37F3C0735A5CCF903989DDB3894`.
`idf.py -p COM8 app-flash` gravou a aplicação em `0x20000`; esptool confirmou
`Hash of data verified` e executou hard reset. O firmware C6 de referência
`coprocessor/build/c6-repro-final-20260913/eh_cp_ota_coprocessor_ota.bin`
permanece com SHA-256 `6B4E50892CDB750318D2A192F6254447A335673EFFB67E225D3E30D02941FA61`;
não foi alterado.

**Boot observado:** `idf.py -p COM8 monitor` confirmou inicialização do
display/touch, UI Home visível aos 19,013 s, C6 3.0.6/RPC v2/SW_AGGR e IP
`192.168.1.16`. Os refreshes de Bitcoin/altcoins, clima e USD/BRL terminaram
com `ESP_OK`; o monitor foi encerrado após o agendamento do próximo domínio.
Não houve watchdog nos cerca de 29 s capturados. A navegação e o layout Mercado
após esta gravação ainda precisam de inspeção visual na placa.

### 2026-10-01 — Mercado: tipografia BTC alinhada à Home

O preço principal do BTC voltou a usar `NP_FONT_BRAND` (64 px), como o card de
Bitcoin da Home; `US$` usa `NP_FONT_SM`. A variação passou para `NP_FONT_LG`,
igual ao texto de variação das altcoins. As setas agora usam os glyphs Material
`NP_ICON_RISE`/`NP_ICON_FALL` da fonte de 24 px, seguindo a implementação da
Home. Nenhuma outra composição da tela foi alterada.

Build limpa ESP-IDF 5.5.4, target `esp32p4`, em
`firmware/build/market-btc-typography-20261001`: `0x309250` bytes, 62% livres
na menor partição de 8 MiB. SHA-256 P4:
`4619739D981D7568AE3F9E0A603D3B5F3CA17DA703EEBAF17C7633526F1EBA04`;
ELF `CDD86CBD590D9CCE82E97E810A58B3FEBD312D4721FF3454EC44F641FBB0F481`;
`sdkconfig` `32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.
`idf.py -B build/market-btc-typography-20261001 -p COM8 app-flash` gravou a
aplicação em `0x20000`; esptool confirmou `Hash of data verified` e hard reset.

Captura `idf.py ... monitor`: placa Waveshare P4 v1.3, display RGB565,
rotação 180°, `TRIPLE_PARTIAL`, três FB; C6 3.0.6 pareado, RPC v2 e SDIO
SW_AGGR. Home ficou visível aos 12,004 s; Wi-Fi obteve `192.168.1.16` e os
refreshes sequenciais de mercado, clima, câmbio e Fear & Greed terminaram com
`ESP_OK`. Esta captura não incluiu navegação até Mercado nem inspeção visual da
tipografia após o flash.

### 2026-10-01 — Mercado: gráfico BTC, setas e USD/BRL em linha

O gráfico BTC foi ampliado para `598 × 202` px, com 5 px em cada lateral e
criado antes dos textos para permanecer no fundo do card. A posição usa a mesma
área vertical de gráfico da Home; preço e variação ficam por cima. Setas das
altcoins, USD/BRL e faixa de índices agora usam `NP_ICON_RISE/FALL`, os glyphs
Material Rounded de tendência já empregados na Home. Os widgets receberam
largura para mostrar o glyph completo dentro do espaço disponível. A variação
USD/BRL passou para a mesma linha da cotação, na quarta coluna do card BTC.

Build incremental ESP-IDF 5.5.4/`esp32p4` em
`firmware/build/market-btc-typography-20261001`: imagem `0x309250` bytes,
62% livres na menor partição. SHA-256 P4:
`DCC4D20BB32A0E0CF4179D18E3F8724DF3C3CED59BC54B8EA84B3858DC856FBE`;
ELF `109F9932DC1B141B60805A61CBAA36F453A98E2405ADE40EBE0642C106D5FC9D`.
`idf.py -B build/market-btc-typography-20261001 -p COM8 app-flash` gravou
somente a aplicação P4 em `0x20000`; esptool confirmou `Hash of data verified`
e hard reset.

Boot frio capturado no monitor serial: placa P4 v1.3; display RGB565/180°,
`TRIPLE_PARTIAL`/3 FB; C6 3.0.6, RPC v2 e SW_AGGR; Wi-Fi recebeu
`192.168.1.16`; Home visível aos 12,963 s; refreshes de mercado, clima, câmbio
e Fear & Greed concluídos com `ESP_OK`. Monitoramento curto de aproximadamente
17 s sem panic ou WDT. Não houve navegação até Mercado nem inspeção visual nesta
captura.

### 2026-10-01 — Fear & Greed: medidor semicircular segmentado

O card agora mostra um arco semicircular de cinco faixas coloridas (medo
extremo, medo, neutro, ganância e ganância extrema), com gaps entre faixas,
marcador branco posicionado pelo valor, número e classificação. Para manter o
desenho dentro do padding de 5 px do card de `300 × 120` px, o arco foi
redesenhado como polilinhas elípticas de 8 px, com pontos entre x=11..289 e
y=31..108. As linhas ficam dentro de x=7..293 e y=27..112 após considerar a
espessura. O marcador de 12 px permanece dentro de x=5..295 e y=25..114 em
toda a escala. `Fear & Greed` e a atribuição `Alternative.me` permanecem no
topo. Segmentos removem estilos herdados antes de aplicar suas cores.

Build incremental ESP-IDF 5.5.4/`esp32p4` em
`firmware/build/market-btc-typography-20261001`: imagem `0x30a4f0` bytes,
62% livres na menor partição. SHA-256 P4:
`87D38B84BAADC2DEA5B1C3E27011588FCC4CE0A80C216E2D42E10C55C690350C`;
ELF `5B6885AB7C0B8D2009595AFD4C69F0B40549E15FD8555F21782AF18FE222E9C1`;
`sdkconfig` `32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.
`idf.py -B build/market-btc-typography-20261001 -p COM8 app-flash` gravou
somente a aplicação em `0x20000`; esptool confirmou `Hash of data verified`
e hard reset.

Boot em Waveshare P4 v1.3: RGB565/180°, `TRIPLE_PARTIAL`/3 FB; C6 3.0.6, RPC
v2 e SDIO SW_AGGR; Wi-Fi recebeu `192.168.1.16`; Home visível aos 10,004 s.
Quatro ciclos sequenciais de refresh, incluindo o domínio de Fear & Greed,
terminaram com `ESP_OK`. Sem panic ou WDT nos cerca de 15 s capturados. Nesta
captura o equipamento permaneceu na Home; a tela Mercado não foi inspecionada
visualmente após a gravação.

### 2026-10-01 — Fear & Greed: composição compacta do medidor

O card de 300 × 120 px segue a referência enviada: título no topo, meia-lua
contínua à esquerda, valor dentro da abertura e classificação ao lado. O arco
tem pontas arredondadas e degradê contínuo pelas cinco cores anteriores:
`#FF454D`, `#F28A32`, `#F3D13A`, `#8BCB35` e `#08C995`. O número e a
classificação usam a cor interpolada na posição atual do valor. Um marcador
circular branco com contorno escuro indica a posição correspondente ao valor
atual no arco. A atribuição `Alternative.me` foi removida conforme pedido
anterior. O raio de 67 px e a espessura de 14 px mantêm o arco e o marcador
dentro da área útil do card, respeitando os 5 px das bordas.

Build limpo ESP-IDF 5.5.4/`esp32p4` em
`firmware/build/fear-greed-marker-clean-20261001`: imagem `0x30a380` bytes,
62% livres na menor partição. SHA-256 P4:
`D1F7BE86C783D16AF57300D883F0A00DF630DBBA18AFE085366662D7B9C7751E`;
ELF `963FB7923ED241DE85FAC694315A02CC84CE4D22F9995644ECCE8A538D158A2D`;
`firmware/sdkconfig` `32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.
`idf.py -B build/market-btc-typography-20261001 -p COM8 app-flash` gravou
somente o P4 em `0x20000`; esptool confirmou `Hash of data verified` e hard
reset.

Boot frio capturado na Waveshare ESP32-P4 v1.3: display EK79007 inicializado,
RGB565/180°, `TRIPLE_PARTIAL`/3 FB; C6 3.0.6 negociou RPC v2 e SDIO SW_AGGR;
Wi-Fi recebeu `192.168.1.16`. Home visível aos 13,014 s e quatro refreshes
terminaram com `ESP_OK`. Sem panic ou WDT durante a captura de cerca de 17 s.

Build limpo concluído em diretório separado. O binário desse build foi gravado
em COM8 em 2026-10-01; o esptool verificou o hash dos dados e concluiu o hard
reset. Boot capturado: display e touch inicializados, C6 3.0.6 negociou RPC v2
e SDIO SW_AGGR, Wi-Fi recebeu `192.168.1.16`, e Home ficou visível aos
13,014 s. Não houve panic ou WDT nos 15 s observados. A tela Mercado e o
marcador não foram inspecionados visualmente nesta captura.
O operador ainda precisa navegar até Mercado para inspeção visual do novo
arco; esta captura confirma o boot, mas não sua apresentação física.

### 2026-10-01 — glifo dos indicadores de mercado

A fonte Material Symbols Rounded de 48 px foi regenerada incluindo o glifo
`currency_exchange` (U+EF92), usado pelos cards S&P 500 e Nasdaq, que estava
ausente da fonte e por isso não aparecia. Build limpo ESP-IDF 5.5.4/`esp32p4`
em `firmware/build/market-index-icons-clean-20261001`: imagem `0x30aa60`
bytes, 62% livres na menor partição. SHA-256 P4:
`6385C3D9BCFDED9FF1FA18020BC608A29E96726326306E1DE1C660AC49A24011`.
`idf.py -B build/market-index-icons-clean-20261001 -p COM8 app-flash` gravou
somente a aplicação P4 em `0x20000`; esptool confirmou `Hash of data verified`
e hard reset.

Boot frio capturado na Waveshare P4 v1.3: display EK79007 e touch inicializados,
RGB565/180° com `TRIPLE_PARTIAL`/3 FB; C6 3.0.6 negociou RPC v2 e SDIO SW_AGGR;
Wi-Fi recebeu `192.168.1.16`. Home V2 ficou visível aos 19,225 s; quatro
refreshes de conectividade terminaram com `ESP_OK`. Sem panic ou WDT nos cerca
de 32 s capturados. A tela Mercado não foi inspecionada visualmente nesta
captura. S&P 500, Nasdaq e Ibovespa continuam sem cotações até a escolha de uma
fonte com permissão de exibição; nenhum valor foi simulado.

### 2026-10-01 — tentativa intermediária de ícone no Ibovespa

O badge foi temporariamente trocado para `NP_ICON_RISE`. Após esclarecimento do
responsável, essa opção foi substituída pelo mesmo glifo de mercado dos cards
S&P 500 e Nasdaq, registrado na entrada seguinte. O caractere `NP_ICON_ARROW_UP`
usado antes era um chevron e aparecia como `^`. Build limpo ESP-IDF
5.5.4/`esp32p4` em `firmware/build/ibovespa-rise-icon-clean-20261001`: imagem
`0x30ab10` bytes, 62% livres na menor partição. SHA-256 P4:
`EA00E276FC4746200060926338E48ED8CC2D4D1ECEA3DCE390C651AB014830F9`.
`idf.py -B build/ibovespa-rise-icon-clean-20261001 -p COM8 app-flash` gravou
somente a aplicação P4 em `0x20000`; esptool confirmou `Hash of data verified`
e hard reset.

Boot frio capturado na Waveshare P4 v1.3: display EK79007 e touch inicializados,
RGB565/180° com `TRIPLE_PARTIAL`/3 FB; C6 3.0.6 negociou RPC v2 e SDIO SW_AGGR;
Wi-Fi e quatro refreshes HTTPS concluíram com `ESP_OK`. Sem panic ou WDT nos
cerca de 40 s capturados. A tela Mercado não foi inspecionada visualmente.

### 2026-10-01 — ícone de mercado no card Ibovespa

Conforme esclarecimento do responsável, o badge do Ibovespa usa agora
`NP_ICON_MARKET`, o mesmo glifo U+EF92 (`currency_exchange`) dos cards S&P 500 e
Nasdaq; o badge mantém o fundo verde. Build limpo ESP-IDF 5.5.4/`esp32p4` em
`firmware/build/market-ibovespa-market-icon-clean-20261001`: imagem `0x30ab10`
bytes, 62% livres na menor partição. SHA-256 P4:
`645BCFE16CD7CD54267BB73DF60EBD70C4EE898298FAD083F6B034FBCADEF922`.
`idf.py -B build/market-ibovespa-market-icon-clean-20261001 -p COM8 app-flash`
gravou somente a aplicação P4 em `0x20000`; esptool confirmou
`Hash of data verified` e hard reset.

Boot frio capturado na Waveshare P4 v1.3: display EK79007 e touch inicializados,
RGB565/180° com `TRIPLE_PARTIAL`/3 FB; navegação e refreshes HTTPS apareceram no
log serial. Sem panic ou WDT nos cerca de 40 s capturados. O card não foi
inspecionado visualmente após o flash.
### 2026-10-01 — Brapi para Ibovespa, S&P 500 e Nasdaq

- Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3; revisão C6 não
  alterada. Worktree baseado em `80c489d` com alterações locais.
- Build limpo ESP-IDF 5.5.4, alvo `esp32p4`, diretório
  `firmware/build/market-brapi-clean-20261001`. Comando: ativar
  `C:\esp\v5.5.4\esp-idf\export.bat` e executar
  `idf.py -B build\market-brapi-clean-20261001 build` de `firmware/`.
- Imagem P4: `0x30b780` bytes, 62% livres na menor partição. SHA-256:
  `BB6ACE29C344FE9806C15C4E5064B1E0B0F4E0A4822C300F70C57C51EF8D2FEB`.
  `firmware/sdkconfig` SHA-256:
  `32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.
- Flash: `idf.py -B build\market-brapi-clean-20261001 -p COM8 app-flash`;
  somente aplicação P4 em `0x20000`. Esptool confirmou `Hash of data verified`
  e hard reset. Não foram gravados bootloader, tabela de partições ou C6.
- Boot serial: display EK79007 e touch GT911 iniciados; PSRAM 32 MiB; C6
  ESP-Hosted 3.0.6 nos dois lados, RPC v2 e SDIO SW_AGGR negociados; Wi-Fi
  recebeu IP. Home completou a transição. Quatro refreshes de produto
  terminaram com `ESP_OK`; o refresh de índices foi agendado e informou chave
  brapi indisponível (`ESP_ERR_INVALID_SIZE`), pois a configuração local está
  vazia. Assim, não há confirmação física de cotações nem de suporte real para
  `^GSPC` e `^IXIC`. Sem panic ou watchdog nos cerca de 40 s de log capturado.
- C6: não gravado; artefato preexistente
  `eh_cp_ota_coprocessor_ota.bin`, SHA-256
  `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`.
- Os testes host do parser Brapi (três símbolos e resposta parcial), do cache
  offline v6/v7 e do scheduler passaram com GCC C11 e `-Wall -Wextra -Werror`.

### 2026-10-01 — Brapi com uma chamada por ativo

- Worktree baseado em `80c489d`; board Waveshare ESP32-P4-WIFI6-Touch-LCD-7B,
  P4 v1.3. C6 não alterado.
- Build limpo ESP-IDF 5.5.4, alvo `esp32p4`, em
  `firmware/build/market-brapi-single-clean-20261001`. Imagem P4 `0x30b850`
  bytes, 62% livres na menor partição. SHA-256:
  `C9815C30ABE2AFE418F4F14C2813544CD3D4D4A62C4D113A7178C06263ECC6BF`.
  `firmware/sdkconfig` SHA-256:
  `32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.
- Flash por `idf.py -B build\market-brapi-single-clean-20261001 -p COM8
  app-flash`: somente a aplicação em `0x20000`. Esptool confirmou
  `Hash of data verified` e hard reset; bootloader, partições e C6 não foram
  gravados.
- Monitor COM8 inicialmente retornou `PermissionError(13)` por processos Python
  residuais do ambiente IDF. Após encerrá-los, o monitor abriu normalmente.
  Display EK79007, touch GT911 e P4 inicializaram; C6 negociou ESP-Hosted 3.0.6,
  RPC v2 e SDIO SW_AGGR. Home iniciou.
- A conexão Wi-Fi foi rejeitada com motivo `202` antes de obter IP. As três
  chamadas brapi não foram executadas nesta inicialização; portanto os dados e
  a cobertura dos símbolos no serviço ainda não foram confirmados na placa.
  Não houve panic ou watchdog nos aproximadamente 90 s capturados.
- Host tests do parser por resposta individual, cache v6/v7 e scheduler
  passaram com GCC C11 e `-Wall -Wextra -Werror`. A chave permanece somente na
  configuração local ignorada pelo Git.

### 2026-10-01 — espaçamento dos valores nos cards de índices

- Board Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3; C6 não alterado.
  Worktree baseado em `80c489d5ed2808c9501b2dd474baa7ad5b051dcb` com mudanças
  locais. Os textos dos cards S&P 500, Nasdaq e Ibovespa foram deslocados de
  x=58 para x=68, deixando 12 px entre o badge de 48 px (x=8) e os textos; o
  ícone de variação foi deslocado 10 px e o percentual 14 px, deixando 6 px
  entre o glyph da seta e o texto. Os três índices exibem a unidade `pts`, pois
  suas cotações são pontos de índice, não preços em moeda.
- Build limpo ESP-IDF 5.5.4, alvo `esp32p4`, diretório
  `firmware/build/market-card-spacing-clean-20261001`, usando
  `idf.py -B build\market-card-spacing-clean-20261001 build`. Imagem P4
  `0x30b860` bytes, 62% livres na menor partição. SHA-256:
  `16D193CC485304EE83FB541BBBB38548F554EA0AEA06817F8F22DBCEB4CE3F23`.
  `firmware/sdkconfig` SHA-256:
  `32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.
- Flash por `idf.py -B build\market-card-spacing-clean-20261001 -p COM8
  app-flash`: somente aplicação P4 em `0x20000`; esptool confirmou
  `Hash of data verified` e hard reset. Bootloader, partições e C6 não foram
  gravados.
- Monitor COM8 abriu. P4 v1.3, PSRAM 32 MiB, display EK79007, touch GT911,
  RGB565/180° com `TRIPLE_PARTIAL`/3 FB iniciaram. C6 3.0.6 negociou RPC v2 e
  SDIO SW_AGGR. Home iniciou, Wi-Fi recebeu `192.168.1.16` e os cinco refreshes
  sequenciais de dados terminaram com `dns=ESP_OK ntp=ESP_OK https=ESP_OK`.
  Sem panic ou watchdog nos cerca de 18 s capturados. A apresentação dos cards
  de Mercado não foi inspecionada visualmente nesta captura.

### 2026-10-01 — cadência horária dos índices

- Board Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3; C6 não gravado.
  Worktree baseado em `80c489d5ed2808c9501b2dd474baa7ad5b051dcb` com mudanças
  locais. A consulta normal dos índices passa a ocorrer a cada 60 minutos;
  retry após falha segue em 2 minutos.
- Build limpo ESP-IDF 5.5.4, alvo `esp32p4`, diretório
  `firmware/build/market-indices-hourly-clean-20261001`, comando
  `idf.py -B build\market-indices-hourly-clean-20261001 build`. Imagem P4
  `0x30b860` bytes, 62% livres na menor partição. SHA-256:
  `F023D2E9990D4B0D9A40AFD6315AD99DF2E63C7E4ED84300D5A923277893CB26`.
  `firmware/sdkconfig` SHA-256:
  `32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.
- Flash por `idf.py -B build\market-indices-hourly-clean-20261001 -p COM8
  app-flash`: somente aplicação P4 em `0x20000`; esptool confirmou
  `Hash of data verified` e hard reset. Bootloader, tabela de partições e C6
  não foram gravados.
- Monitor COM8: P4 v1.3, PSRAM 32 MiB, display EK79007 e touch GT911
  inicializados; C6 3.0.6 negociou RPC v2 e SDIO SW_AGGR. Wi-Fi recebeu
  `192.168.1.16`; os cinco domínios de refresh terminaram com
  `dns=ESP_OK ntp=ESP_OK https=ESP_OK`. Sem panic ou watchdog nos cerca de 20 s
  capturados.

### 2026-10-01 — primeira tela IoT

- Board Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash de 32 MiB e
  PSRAM de 32 MiB; commit base `80c489d5ed2808c9501b2dd474baa7ad5b051dcb`,
  com mudanças locais. A tela foi adicionada ao drawer; contém somente
  Dispositivos e Sensores, sem cenas ou dados de demonstração. Os botões de
  adição abrem a orientação do gateway. A rota e a aparência IoT ainda precisam
  de inspeção visual por toque na placa.
- Build limpo ESP-IDF 5.5.4, target `esp32p4`, com `idf.py fullclean` e
  `idf.py build`. Imagem `firmware/build/np2_p4.bin`: `0x30c6b0` bytes, 62%
  livres na menor partição. Depois do build limpo, ajustei os ícones para o
  subset Material de 24 px e rodei `idf.py build` incremental. SHA-256 final da
  imagem gravada:
  `DBB0DECAE78D51B1BC0B0737CE46F0713FA515B58EEF1C03758C2428FF11BAD2`.
  SHA-256 de `firmware/sdkconfig`:
  `32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.
  Configuração efetiva: target `esp32p4`, tabela customizada,
  `CONFIG_SPIRAM_XIP_FROM_PSRAM=y`,
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` desabilitado. Hash da tabela de partições:
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
- Comando de gravação: `idf.py -p COM8 flash monitor`, ESP-IDF 5.5.4. O
  esptool verificou os hashes gravados e executou hard reset; esta gravação
  atualizou bootloader, tabela de partições, dados OTA e aplicação P4 conforme
  os `flash_args` do build. O monitor registrou P4 v1.3, display EK79007,
  GT911, RGB565, rotação 180°, `TRIPLE_PARTIAL` com 3 framebuffers e PSRAM de
  32 MiB. O C6 informou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR negociados;
  recebeu IP `192.168.1.16`. A aplicação chegou à Home sem panic ou watchdog
  durante os cerca de 24 s capturados após o reset.
- C6 não foi gravado nesta operação; artefato registrado em evidência anterior
  com SHA-256 `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`.
  Esta evidência confirma build, gravação e boot básico; não confirma a rota IoT
  por toque nem a integração com gateway ou dispositivos SONOFF.
- Após a correção dos ícones, `idf.py -p COM8 app-flash monitor` regravou somente
  a aplicação; esptool verificou o hash e o P4 reiniciou. O segundo boot
  repetiu a inicialização de display, touch, PSRAM e Hosted/C6; a Home abriu e o
  Wi-Fi recebeu IP. O monitor foi encerrado após a captura para liberar COM8.

### 2026-10-01 — integração SONOFF TX1C/TX2C/TX3C

- O usuário confirmou os modelos TX1C, TX2C e TX3C. A matriz atual do projeto
  comunitário SonoffLAN lista os três como locais: UIID 6/7/8, respectivamente
  1/2/3 canais, firmware 3.8.0 na entrada da matriz. Isso é evidência de
  compatibilidade reportada pelo mantenedor, não validação física dos aparelhos
  do usuário. mDNS `_ewelink._tcp` anuncia metadados; com firmware original,
  `devicekey` ainda é necessária para decifrar estado e cifrar comandos. A chave
  é obtida da conta eWeLink no fluxo SonoffLAN. A tela mostra os modelos e o
  requisito da chave, sem iniciar cadastro ou controle ainda.
- Build incremental ESP-IDF 5.5.4, target `esp32p4`: passou. Imagem
  `0x30c6f0` bytes, 62% livres na partição de 8 MiB. SHA-256:
  `5A5C60CB1688537910849FBDE28768DB42A38502369C4E4750A130F434C451E0`.
  SHA-256 de `sdkconfig`:
  `32622AC03EE0DB5AA03E8EE5F0993D76FB7D3A7621AF537E1852B2578E64AFC0`.
- `idf.py -p COM8 app-flash monitor` gravou somente a aplicação P4; esptool
  confirmou `Hash of data verified`. Boot em P4 v1.3 confirmou display, touch,
  PSRAM, ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR. Wi-Fi obteve IP
  `192.168.1.16` e as verificações DNS/NTP/HTTPS capturadas terminaram com
  `ESP_OK`. C6 não foi gravado. Monitor encerrado para liberar COM8. A rota IoT
  não foi inspecionada por toque e os interruptores não foram testados.

### 2026-10-01 — descoberta mDNS dos SONOFF (flash bloqueado)

- Board Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash NOR 32 MiB,
  PSRAM 32 MiB; revisão local baseada em `80c489d5ed2808c9501b2dd474baa7ad5b051dcb`.
  Alteração fixa `espressif/mdns` 1.5.3 e faz a busca `_ewelink._tcp` em worker,
  exibindo até oito IDs/endereço/tipo sem chave. C6 permanece no hash conhecido
  `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`; não foi
  gravado.
- `idf.py fullclean` foi tentado, mas não concluiu: Windows retornou
  `PermissionError(WinError 32)` para `firmware/build/log/idf_py_stderr_output_134140`,
  arquivo aberto por outro processo. Em seguida, `idf.py build` regenerou o
  projeto e compilou com sucesso no ESP-IDF 5.5.4, target `esp32p4`. Binário P4
  `0x3164c0` bytes, com `0x4e9b40` bytes livres na partição de 8 MiB. SHA-256 do
  binário `7572F45D05E0548B9B3F513F18DFF5DEB72727EB4F2656DF112D61136C9D9ADC`;
  `sdkconfig` `424C82D40949B6FA8A707166AE77FC1A65C7347C1E817177E85976108A82E31F`;
  tabela `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`; target e configuração customizada
  `esp32p4`/partições confirmados durante o build.
- `idf.py -p COM8 flash monitor` não gravou: esptool retornou
  `PermissionError(13, 'Acesso negado')` ao abrir COM8. A inspeção de processos
  identificou o monitor ESP-IDF do VS Code (`esp_idf_monitor`, PID 52160) e o
  comando `idf.py -p COM8 app-flash monitor` (PID 56328) mantendo a porta.
  Nenhum processo foi encerrado. Sem flash, boot, inspeção visual ou teste mDNS
  físico nesta rodada; repetir `app-flash monitor` quando o monitor liberar
  COM8. Build aprovado não comprova descoberta nem compatibilidade dos TX1C/TX2C/TX3C.

### 2026-10-01 — resultado da gravação IoT após liberar COM8

- A COM8 estava ocupada por processos órfãos `idf.py -p COM8 app-flash monitor`
  e `esp_idf_monitor`, iniciados às 14:06. O usuário confirmou que não tinha
  monitor aberto. Os processos antigos foram encerrados; a COM8 voltou a estar
  disponível. Nenhuma janela ou processo do VS Code foi fechado.
- `idf.py -p COM8 app-flash monitor` gravou apenas a aplicação P4 em `0x20000`;
  esptool reportou `Hash of data verified` e hard reset. Imagem ESP-IDF 5.5.4,
  target `esp32p4`, tamanho `0x3164c0`, SHA-256
  `7572F45D05E0548B9B3F513F18DFF5DEB72727EB4F2656DF112D61136C9D9ADC`.
  `sdkconfig` SHA-256 `424C82D40949B6FA8A707166AE77FC1A65C7347C1E817177E85976108A82E31F`;
  tabela SHA-256 `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  Hash conhecido do C6 `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`;
  C6 não foi gravado.
- Boot serial observado por cerca de 48 s: P4 v1.3, flash 32 MiB, PSRAM
  32 MiB, EK79007/GT911 e `TRIPLE_PARTIAL`/3 FB iniciados. C6 3.0.6 negociou
  SDIO streaming, RPC v2 e SW_AGGR. A aplicação chegou à Home; Wi-Fi recebeu
  `192.168.1.16`. Cinco verificações DNS/NTP/HTTPS terminaram com `ESP_OK`.
  Sem panic ou watchdog na captura. A conexão passou por duas retentativas antes
  de receber IP. O monitor iniciado nesta sessão foi encerrado para liberar COM8.
- O boot não testa a consulta `_ewelink._tcp`: a busca precisa ser acionada pela
  interface e ainda requer ensaio físico com TX1C/TX2C/TX3C na mesma rede. A
  rota de cadastro/controle e a autorização OAuth seguem pendentes da aprovação
  da aplicação eWeLink.

- **Relato do operador após a gravação:** ao acionar “Buscar dispositivos na
  rede” na tela IoT, o painel encontrou três dispositivos. Isso confirma a
  descoberta `_ewelink._tcp` na rede local para os aparelhos presentes; ainda
  não confirma a identificação individual dos modelos, obtenção de `devicekey`
  nem acionamento. Não foram copiados IDs ou endereços para este registro.

### 2026-10-01 — correções nas telas de configurações

- Board Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash 32 MiB e PSRAM
  32 MiB; código baseado no commit `80c489d5ed2808c9501b2dd474baa7ad5b051dcb`
  com mudanças locais. Ajustado o registro de callbacks quando Tela e som e
  Notificações são recriadas; a senha Wi-Fi agora é desenhada uma única vez a
  partir do buffer privado, sem asteriscos residuais sobrepostos; a fonte Material
  de 48 px usa a fonte de 24 px como fallback para os glifos que faltam.
- Build ESP-IDF 5.5.4, target `esp32p4`, comando `idf.py build`. Imagem P4
  `0x3164c0` bytes, com `0x4e9b40` bytes livres na menor partição; SHA-256
  `0931D13ED6D91ABF83DE664BEE231A665EE94F19680A1D724671E63FB27B45FD`.
  `sdkconfig` SHA-256
  `424C82D40949B6FA8A707166AE77FC1A65C7347C1E817177E85976108A82E31F`;
  tabela de partições SHA-256
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  Configuração efetiva: `esp32p4`, `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` desabilitado.
- Gravação final da aplicação: `idf.py -p COM8 app-flash`; esptool confirmou
  `Hash of data verified` e hard reset. O monitor `idf.py -p COM8 monitor`
  capturou o boot e foi encerrado para liberar a COM8. P4 v1.3, display EK79007,
  GT911, PSRAM 32 MiB, C6 ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR iniciaram;
  controles locais restauraram brilho/volume em 100/100 e a aplicação chegou à
  Home sem panic ou watchdog nos cerca de 13 s capturados. C6 permaneceu sem
  gravação; hash conhecido
  `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`.
- Teste host de preferências de notificação passou (restauração, coalescência,
  alteração durante gravação, falhas e som); o teste de provisionamento por
  toque passou (validação, máscara, revelação, limites e limpeza); e o teste de
  UI Wi-Fi passou (paginação, seleção, bloqueio por estado e 100 ciclos de vida).
  O script `tools/scripts/host_check.sh` não está presente neste checkout. O boot
  não exercita os controles por toque; a conferência interativa das telas permanece
  pendente.

### 2026-10-01 — correção da digitação no teclado Wi-Fi

- O evento de tecla era lido depois do callback padrão do LVGL, que pode trocar
  o mapa do teclado no mesmo toque. O índice selecionado passava então a apontar
  para outro caractere. Agora o texto da tecla é capturado no preprocessamento
  do evento, antes da troca de mapa, e consumido pelo callback da aplicação.
- Regressão host em `tools/run_settings_wifi_ui_host_test.ps1`: passou. Cobriu
  alternância para símbolos e letras, digitação de número, maiúscula/minúscula,
  máscara da senha e 100 ciclos de vida da tela; a sequência digitada foi
  preservada sem caractere extra. Nenhuma senha real foi usada ou registrada.
- Build ESP-IDF 5.5.4 para `esp32p4` aprovado; imagem P4 com 3.237.056 bytes
  (`0x3164c0`), SHA-256
  `734F270BF590BB564E4A2ECA8D6E78AD72C82482125F1356E2E34A3AC8240F54`.
  `sdkconfig` SHA-256
  `424C82D40949B6FA8A707166AE77FC1A65C7347C1E817177E85976108A82E31F`;
  tabela de partições SHA-256
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  Configuração efetiva: target `esp32p4`, XIP pela PSRAM habilitado e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` desabilitado.
- `idf.py -p COM8 app-flash monitor` gravou a aplicação P4; esptool confirmou
  `Hash of data verified`. Boot observado até `Home V2 visible`, com P4 v1.3,
  flash/PSRAM de 32 MiB, display EK79007, touch GT911 e C6 ESP-Hosted 3.0.6
  (RPC v2/SW_AGGR) iniciados. Monitor encerrado após a captura para liberar COM8.
  C6 não foi gravado; hash conhecido
  `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`.
- O teste confirma o fluxo do componente e o boot; ainda falta validar na tela
  física a digitação de uma senha de teste após esta gravação.

### 2026-10-01 — ícones em botões das telas de configuração

- Causa: rótulos de botões eram renderizados somente com a fonte Montserrat,
  sem fallback Material; caracteres de ícone nos textos de ação ficavam vazios.
  O estilo compartilhado dos botões agora usa uma cópia da fonte de texto com
  fallback para `NP_FONT_ICON`, para que o mesmo componente exiba texto e ícone.
- Regressão da UI Wi-Fi: `tools/run_settings_wifi_ui_host_test.ps1` passou,
  incluindo paginação, seleção, estados, máscara e 100 ciclos de vida. Build
  ESP-IDF 5.5.4, target `esp32p4`, aprovado; imagem P4 de 3.237.184 bytes
  (`0x316540`), SHA-256
  `162D6B271C5B0E87CFF9CBF65513FD610C5F9C2D4FD9789B79EE4032BABDF09B`.
  `sdkconfig` SHA-256
  `424C82D40949B6FA8A707166AE77FC1A65C7347C1E817177E85976108A82E31F`;
  tabela de partições SHA-256
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
- `idf.py -p COM8 app-flash monitor` gravou a aplicação P4 e esptool confirmou
  `Hash of data verified`. Boot chegou a `Home V2 visible`; P4 v1.3, flash e
  PSRAM 32 MiB, EK79007, GT911 e C6 Hosted 3.0.6 com RPC v2/SW_AGGR iniciaram.
  Monitor encerrado para liberar COM8. C6 não foi gravado. O boot não confirma
  visualmente cada ícone de botão; a correção altera o fallback compartilhado.

### 2026-10-01 — tela Pomodoro

- Adicionada a tela Pomodoro à navegação compartilhada. O serviço mantém o
  cronômetro monotônico no `app_loop`; os comandos da tela passam pelo EventBus.
  A interface oferece 5, 10, 25 e 50 minutos, duração personalizada de 1 a 99
  minutos, iniciar/pausar/zerar e resumo diário de ciclos e tempo focado.
- Teste host `tools/pomodoro_service_host_test.c` compilado com GCC C11 e
  `-Wall -Wextra -Werror`; passou. `git diff --check` sem erros.
- Build ESP-IDF 5.5.4 para `esp32p4` aprovado; imagem P4 de 3.239.344 bytes
  (`0x317db0`), SHA-256
  `CF98E9F3181F3B84D36A1DEF99583CE484EDDCFE166C5BD246F3C14FE9F8CA56`.
  Menor partição de app: `0x800000`, com `0x4e8250` bytes livres. `sdkconfig`
  SHA-256 `424C82D40949B6FA8A707166AE77FC1A65C7347C1E817177E85976108A82E31F`;
  tabela compilada SHA-256
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  Configuração efetiva: target `esp32p4`, XIP pela PSRAM habilitado e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` desabilitado.
- `idf.py -p COM8 app-flash monitor` gravou somente o app P4; esptool confirmou
  `Hash of data verified` e hard reset. Boot observado até `Home V2 visible`;
  display EK79007, touch GT911, PSRAM 32 MiB e C6 ESP-Hosted 3.0.6 com RPC v2 e
  SDIO SW_AGGR iniciaram. O Wi-Fi recebeu IP `192.168.1.16`; verificações de
  rede concluíram DNS, NTP e HTTPS sem erro. C6 não foi gravado.
- A navegação até a tela Pomodoro e os toques nos controles ainda precisam de
  conferência visual no display. Os contadores diários são voláteis e reiniciam
  após reboot, conforme ADR-059.

### 2026-10-01 — ajuste de layout e alarme do Pomodoro

- Reorganizados os botões em uma fileira compacta na ordem 5, 10, 25, 50 e
  Personalizar; o círculo e o contador agora têm posições calculadas para manter
  espaçamento com os botões. O botão principal alinha ícone e rótulo e alterna
  entre Iniciar/Pausar e play/pause.
- Ao concluir o ciclo, `app_loop` solicita uma vez o toque curto pelo serviço de
  controles de áudio. A reprodução segue na task do serviço e respeita o volume.
- Teste host Pomodoro passou com GCC C11 e warnings tratados como erros; build
  ESP-IDF 5.5.4 para `esp32p4` aprovado. Imagem P4 de 3.243.568 bytes
  (`0x317e30`), SHA-256
  `078F4DDAE38659A5412FB372658B5BF229C94EF7158ECB627A3119873DF37838`;
  menor partição de app com `0x4e81d0` bytes livres. `sdkconfig` e tabela
  compilada mantêm os hashes registrados acima; XIP pela PSRAM habilitado e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` desabilitado.
- Aplicação gravada em `ota_0` pela COM8. O monitor confirmou app em `0x20000`,
  display EK79007, touch GT911, PSRAM de 32 MiB, C6 Hosted 3.0.6 (RPC v2/SW_AGGR)
  e a transição para `Home V2 visible`; Wi-Fi recebeu IP. Monitor encerrado e
  COM8 liberada; C6 não foi gravado.
- Ainda falta conferir visualmente o novo espaçamento, rótulos e toque sonoro
  ao concluir um ciclo no display.

### 2026-10-01 — alinhamento do contador, cartões e botões do Pomodoro

- O contador passou a usar um único label centralizado pela largura do círculo.
  Ícone, valor e descrição dos dois cartões “Hoje” agora usam posições
  calculadas a partir da altura real das fontes e das caixas.
- Ícone e texto de Iniciar/Pausar agora ficam em um fluxo horizontal centralizado
  pelo LVGL. O callback do X do modal foi refeito após o retorno da view para
  apontar para o endereço persistente da tela.
- O alarme de conclusão usa três notas ascendentes com pequenos intervalos,
  enfileiradas para reprodução pela task de áudio existente; áudio desligado ou
  volume zero continuam em silêncio.
- Build ESP-IDF 5.5.4 para `esp32p4` e teste host do Pomodoro passaram. Imagem P4
  de 3.244.432 bytes (`0x318190`), SHA-256
  `40E04FC83502FEA05633A2B1344DAFF1B38C2796FE2968208112199F43F61C9B`; menor
  partição de app com `0x4e7e70` bytes livres. `sdkconfig` e tabela compilada
  mantêm os hashes registrados na evidência anterior.
- `idf.py -p COM8 app-flash monitor` gravou somente o app P4 e esptool confirmou
  `Hash of data verified`. Boot observado até `Home V2 visible`; display EK79007,
  touch GT911, PSRAM de 32 MiB e C6 Hosted 3.0.6 com RPC v2/SW_AGGR iniciaram.
  Wi-Fi recebeu `192.168.1.16`; DNS, NTP e HTTPS concluíram com `ESP_OK`.
  Monitor encerrado e COM8 liberada; C6 não foi gravado.
- Ainda falta conferir visualmente no display o contador, os cartões “Hoje”, o
  botão principal e o X do modal. O alarme requer esperar um ciclo completo ou
  encurtar a duração para testá-lo.

### 2026-10-01 — redesign da tela Pomodoro

- Refiz a hierarquia da página em duas áreas: timer circular ampliado e controles
  de duração/ação à esquerda; resumo diário à direita. Os cards de hoje usam
  painel sem borda, com ícone, número e legenda empilhados e centralizados.
- O estado da sessão agora fica explícito (pronto, em foco, pausado ou concluído),
  a duração personalizada recebe destaque quando selecionada e o botão secundário
  passou a exibir “Zerar”. O conteúdo do contador e sua fase são centralizados
  como um único bloco no círculo.
- Build limpo em diretório temporário com ESP-IDF 5.5.4, Python 3.14.4, target
  `esp32p4`; build incremental após o ajuste final também passou. Imagem de
  3.245.248 bytes (`0x3184c0`), SHA-256
  `930F853FA5DDDD672D8DD45FA7F040FC683D1FA3DD07ED2C3B18B64ECA0688C5`;
  `0x4e7b40` bytes livres na menor partição de app. `sdkconfig` e partição
  compilada mantêm os hashes registrados acima.
- O flash P4 foi tentado pela COM8, mas o esptool recebeu `PermissionError(13)`.
  A inspeção identificou um `idf_monitor.py -p COM8` ainda ativo em um terminal
  integrado do VS Code. O processo foi preservado; esta imagem não foi gravada.
  Sem boot novo, a tela Pomodoro ainda requer inspeção visual física. C6 não foi
  alterado.

### 2026-10-01 — correção de estouro de pilha na descoberta ONVIF

- O usuário observou `Stack overflow in task onvif_discovery` durante o uso do
  firmware. A task tinha 5 KiB e a rotina mantinha até 2 KiB de resposta XML,
  além do buffer da sonda e dados de parsing, na pilha. Aumentada para 10 KiB;
  após cada varredura, o serviço agora registra a margem livre e avisa quando
  ela fica abaixo de 1.5 KiB.
- Build ESP-IDF 5.5.4 para `esp32p4` passou usando o ambiente Python 3.14.4 que
  já configurava o diretório incremental. Imagem de 3.265.664 bytes
  (`0x31d480`), SHA-256
  `5042547F92BE505AAD68A50D2925185C9F107CA614AFE82EF63EB09748B149E3`;
  menor partição de app com `0x4e2b80` bytes livres. `sdkconfig` SHA-256
  `424C82D40949B6FA8A707166AE77FC1A65C7347C1E817177E85976108A82E31F` e
  tabela compilada SHA-256
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
- Flash solicitado pela COM8 falhou antes da escrita: esptool recebeu
  `PermissionError(13)`, porta ocupada ou indisponível. Portanto, a correção
  ainda não está gravada nem validada em hardware; repetir flash/monitor após
  liberar COM8 e conferir `task stack margin free` durante uma busca ONVIF.
  C6 não foi alterado.
- Nova tentativa em 2026-10-01: `idf.py -B C:\Users\Tauser\AppData\Local\Temp\np2_build_drawer_timer -p COM8 flash monitor` completou depois que a porta foi
  liberada. Esptool confirmou `Hash of data verified` para bootloader, app,
  tabela e `otadata`, e executou hard reset. O app `80c489d-dirty` iniciou no
  P4 v1.3, inicializou display triple-partial, LVGL, microSD e Hosted C6
  3.0.6/RPC v2/SW_AGGR; chegou a `Home V2 visible` e recebeu IP
  `192.168.1.16`. A busca ONVIF ainda não foi acionada nesta sessão, portanto
  a margem de pilha e a ausência de crash durante a busca permanecem pendentes.
- Flash da versão com verificação manual por IPv4 concluído na COM8. Esptool
  confirmou `Hash of data verified` para bootloader, aplicação, tabela de
  partições e `otadata`, seguido de hard reset. Aplicação P4 de 3.268.304 bytes
  (`0x31ded0`), SHA-256
  `3C053681C7646D1285E642F1483728103025004F8503F7C836466AB8A775F7A5`.
- Boot observado: display/LVGL e C6 Hosted 3.0.6 iniciaram; `Home V2 visible`,
  Wi-Fi conectado e endereço do painel `192.168.1.16`. O PC alcançou
  `192.168.1.8:2020`; esse teste não comprova a rota TCP a partir do P4.
- A interface permite informar o IPv4 manualmente quando a descoberta multicast
  não recebe respostas. A validação do P4 e a presença da Tapo na lista seguem
  pendentes até informar `192.168.1.8` e tocar em “Verificar IP”; não declarar
  câmera online antes desse resultado. O monitor atual permanece ligado na
  sessão de trabalho para capturar a busca; C6 não foi gravado.
- Verificação manual no painel concluída após informar `192.168.1.8`: log do P4
  `ONVIF discovery complete result=ESP_OK cameras=1 datagrams=0 probe_matches=0
  rejected=0 direct_endpoints_up=1`; margem da pilha da task: 3.632 bytes. O
  usuário confirmou que a câmera apareceu na lista. Isso comprova conectividade TCP
  do P4 ao serviço ONVIF na porta 2020 e estado online; autenticação ONVIF e vídeo
  RTSP ainda não foram testados. Nenhum estouro de pilha observado nesta busca.

### 2026-10-01 — ajuste do card da câmera IoT

- O card de câmera foi reduzido de 464×82 para 420×68 px, centralizado na grade
  de duas colunas. O ícone genérico foi trocado pelo Material Symbols Rounded
  `photo_camera` (U+E412), incluído na fonte LVGL de 24 px. Removido o texto
  “Online”; o estado permanece indicado somente pela bolinha colorida.
- Build limpo ESP-IDF 5.5.4, Python 3.14.4, target `esp32p4`: passou. Aplicação
  de 3.268.352 bytes (`0x31df00`), SHA-256
  `22AB583B734AC77CA3EF3A9F1BC94F68D0E6263526DE03F415ACF575690E1E7B`; menor
  partição de app com `0x4e2100` bytes livres.
- `idf.py -p COM8 app-flash monitor` gravou o app P4 em `0x20000`; esptool
  confirmou `Hash of data verified`. Boot chegou a `Home V2 visible`, C6
  Hosted 3.0.6/RPC v2/SW_AGGR iniciou e o painel recebeu `192.168.1.16`.
  Monitor encerrado; COM8 liberada. C6 não foi gravado. A inspeção visual do
  card após este flash ainda depende de conferência no display.

### 2026-10-02 — correção do início do RTSP da Tapo C200

- Ao tocar em “Iniciar vídeo” após preencher a Conta da Câmera, o stream não
  iniciava. A primeira requisição RTSP `OPTIONS` não enviava a linha em branco
  que encerra os cabeçalhos; a negociação podia ficar aguardando resposta antes
  de chegar à autenticação. Corrigida a montagem das requisições. A interface
  agora informa respostas RTSP 401 (credenciais rejeitadas), 403 (acesso
  negado), 461 (transporte não aceito) ou falta de resposta. Logs contêm apenas
  método e código/errno, nunca usuário, senha ou cabeçalhos.
- Build ESP-IDF 5.5.4, Python 3.14.4, target `esp32p4`, árvore baseada em
  `80c489d` com alterações locais: passou. Aplicação com 3.373.824 bytes
  (`0x337b00`), SHA-256
  `D009178C234631A221DDFCD1ECACAD54CF61D8720C01AF1948363A79B7FDA671`;
  menor partição de app com `0x4c8500` bytes livres. `sdkconfig` SHA-256
  `551CAD23C8343C9C18900A0863E20AFB40D62B2392F88760060D1E49183B3F04`;
  tabela de partições compilada SHA-256
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
- COM8 identificada por esptool como ESP32-P4 v1.3. Flash completado e hashes
  de bootloader, aplicação, tabela e `otadata` verificados. Boot observado com
  ESP-IDF 5.5.4, flash de 32 MiB, PSRAM de 32 MiB, display/LVGL, C6 Hosted
  3.0.6 com RPC v2/SW_AGGR, worker RTSP iniciado e Wi-Fi conectado em
  `192.168.1.16`. Monitor encerrado e COM8 liberada; C6 não foi gravado.
- Ainda falta testar “Iniciar vídeo” com a conta local no painel após esta
  correção. O boot valida o firmware e os serviços-base, mas não comprova
  autenticação RTSP, decodificação ou render de quadros da Tapo.

### 2026-10-02 — diagnóstico RTSP e tentativa de stream alternativo

- O monitor COM8 capturou duas tentativas no firmware anterior: cada uma
  recebeu `OPTIONS 200`, `DESCRIBE 401` seguido de `DESCRIBE 200` após Digest,
  `SETUP 200` e `PLAY 200`. A autenticação foi aceita. Logo depois, o decoder
  TinyH264 reportou `profile_idc is error` e rejeitou o SPS. O decoder
  `esp_h264` 1.4.1 usado pelo projeto documenta suporte somente a Constrained
  Baseline; portanto, o erro não é de usuário/senha nem de porta RTSP.
- Adicionada tentativa de `/stream8` seguida de `/stream2` se a decodificação
  falhar, além de mensagem explícita de perfil incompatível. Build incremental
  ESP-IDF 5.5.4, Python 3.14.4, target `esp32p4`: passou. Aplicação de
  3.374.496 bytes (`0x337da0`), SHA-256
  `9B9DB68530E84F9BC0BE8326DAC2B7D69F5D63AD3B3C40CDBB26BE0A397DA601`;
  menor partição com `0x4c8260` bytes livres.
- COM8 confirmou ESP32-P4 v1.3; `idf.py app-flash` concluiu com `Hash of data
  verified` e hard reset. Boot chegou a `Home V2 visible` com display/LVGL e
  C6 Hosted 3.0.6/RPC v2/SW_AGGR. O monitor foi reaberto para capturar o teste
  de `/stream8`; autenticação e render dessa nova tentativa ainda pendentes.
- A captura foi encerrada sem uma nova requisição RTSP após este flash; COM8
  liberada. Os dois logins confirmados nos logs ocorreram antes desta versão.
- Depois, o usuário iniciou o vídeo no firmware gravado e confirmou que a tela
  mostrou “perfil não compatível”. Isso confirma que a nova versão chegou ao
  caminho de rejeição do decoder após tentar o fluxo alternativo. Como o
  monitor não estava conectado durante essa tentativa, não há captura RTSP que
  identifique o SPS de cada URL nem confirme novamente os códigos de resposta.
- Adicionada instrumentação limitada para registrar uma vez por stream apenas
  `profile_idc`, flags de restrição e `level_idc` do SPS, junto de `/stream8` ou
  `/stream2`; nenhum dado de autenticação é impresso. Build incremental com
  ESP-IDF 5.5.4, target `esp32p4`, Python 3.14.4: passou; binário de
  3.374.720 bytes (`0x337e80`), 60% livres na menor partição de app, SHA-256
  `E25C11C5595EEA346C5080C1A43B812A88225494EEFDF75BEE6F658A5E6984A3`.
  `idf.py -p COM8 app-flash` gravou somente a aplicação P4 em `0x20000` e
  confirmou `Hash of data verified`; placa reconhecida como ESP32-P4 v1.3.
  Boot observado: Home visível, Wi-Fi em `192.168.1.16`, RTSP disponível.
- Na tentativa seguinte, `/stream8` respondeu `DESCRIBE 200` após Digest, mas
  não avançou até SETUP; `/stream2` respondeu `OPTIONS 200`, `DESCRIBE 401`
  seguido de `DESCRIBE 200`, `SETUP 200` e `PLAY 200`. O SPS de `/stream2`
  reportou `profile_idc=77`, `constraints=0x00`, `level_idc=31` (H.264 Main).
  TinyH264 rejeitou esse perfil, confirmando incompatibilidade do decoder; o
  usuário informou que um aplicativo no PC reproduz a câmera normalmente.
- No mesmo monitor houve um panic separado: Instruction access fault em
  `np_modal_hide` (PC `0x0000fffe`, retorno em `np_modal.c:81`) após tentativas
  de rede/C6 sem resposta. O painel reiniciou e voltou ao Home; após o reboot
  a sessão RTSP da câmera negociou e revelou o SPS acima. Esse crash de UI
  requer investigação independente antes de declarar o teste estável.
- Acrescentada uma consulta ONVIF somente de leitura antes da tentativa RTSP:
  `GetCapabilities` obtém e valida o XAddr Media na mesma câmera; em seguida,
  `GetVideoEncoderConfigurationOptions` é chamado sem tokens, solicitando as
  opções genéricas definidas pela especificação ONVIF. O log deve mostrar
  somente os perfis H.264 reconhecidos; nenhum XML bruto, usuário ou senha é
  registrado. Não existe chamada a `SetVideoEncoderConfiguration`. Os campos
  de credencial permanecem no modal em RAM para permitir nova tentativa e são
  apagados quando o modal fecha.
- Build ESP-IDF 5.5.4, target `esp32p4`, Python 3.14.4: passou; imagem de
  3.378.800 bytes (`0x338e70`), 60% livres na menor partição de app, SHA-256
  `622F44D98805307AEC133A5A7CA5B24D602B2D9E4CB537BBE7993702EDC4C66F`.
  `idf.py -p COM8 app-flash` gravou somente a aplicação P4 em `0x20000` e
  verificou o hash. Boot confirmado no P4 v1.3, display/LVGL, Hosted C6
  3.0.6/RPC v2/SW_AGGR, Wi-Fi `192.168.1.16` e Home visível. A consulta
  ONVIF aguarda nova ação no painel; ainda não há resultado da câmera.
- Na ação seguinte do usuário, o monitor registrou que RTSP passou por
  `OPTIONS 200`, Digest em `DESCRIBE` (`401` seguido de `200`), `SETUP 200` e
  `PLAY 200`. O SPS voltou a indicar H.264 Main (`profile_idc=77`,
  `constraints=0x00`, `level_idc=31`), rejeitado pelo TinyH264. A etapa ONVIF
  `GetCapabilities` obteve um XAddr Media válido, mas
  `GetVideoEncoderConfigurationOptions` retornou HTTP 400. Esse resultado não
  determina quais perfis a câmera suporta.
- Adicionados logs separados para sucesso do `GetCapabilities` e falha da
  consulta de opções, com reconhecimento em whitelist de fault codes SOAP
  (`ActionNotSupported`, `InvalidArgVal`, `NoProfile` e
  `AuthenticationFailed`). Nenhum XML ou texto livre da câmera é impresso.
  Build limpo ESP-IDF 5.5.4, target `esp32p4`, Python 3.14.4: passou; imagem
  de 3.379.264 bytes (`0x339040`), 60% livres na menor partição de app, SHA-256
  `072FB5A5534D5B4C34D2282F05580317171F1C55F15A9A4A7EBE25F4199F5ABE`.
  `idf.py -p COM8 app-flash monitor` gravou somente a aplicação P4 em
  `0x20000` e confirmou `Hash of data verified`. Boot chegou à Home; Hosted
  C6 3.0.6/RPC v2/SW_AGGR iniciou e o Wi-Fi recuperou `192.168.1.16` após
  tentativas iniciais de associação. C6 não foi gravado. A captura com os logs
  mais específicos ainda depende de repetir “Iniciar vídeo” nesta versão.
- Na repetição solicitada, a câmera apareceu após uma busca sem resposta e o
  teste foi executado sem novo crash. `GetCapabilities` passou e validou o
  endpoint Media; `GetVideoEncoderConfigurationOptions` respondeu HTTP 400
  com fault de autenticação SOAP. RTSP voltou a concluir Digest, `SETUP 200`
  e `PLAY 200`; `/stream2` reportou H.264 Main e o decoder rejeitou o SPS.
  O usuário confirmou que concluiu o teste.
- Alterada a consulta ONVIF de opções para enviar WS-Security
  `UsernameToken/PasswordDigest` com nonce aleatório e timestamp UTC. A senha
  não vai em texto claro, não é persistida e não é registrada. A consulta segue
  read-only. O ADR-063 registra a escolha e os limites. Build limpo ESP-IDF
  5.5.4, target `esp32p4`, Python 3.14.4: passou; imagem de 3.392.672 bytes
  (`0x33c4a0`), 60% livres na menor partição de app, SHA-256
  `635036EE3ED8DD82D75308CD2DCA4E7845A6D8C4A582960A2D619AECB119C7AF`.
  `idf.py -p COM8 app-flash monitor` gravou somente o app P4 em `0x20000` e
  confirmou `Hash of data verified`. Após encerrar o monitor obsoleto da sessão
  anterior, o boot chegou à Home; Hosted C6 3.0.6/RPC v2/SW_AGGR iniciou e o
  Wi-Fi recebeu `192.168.1.16`. A repetição desta consulta ainda está pendente.
  Nenhuma operação `Set*` foi implementada ou enviada.
- Repetição concluída em 2026-10-02 na COM8 após a gravação da versão com
  UsernameToken. A câmera foi descoberta (`cameras=1`, `direct_endpoints_up=1`)
  e a task terminou com margem livre de pilha de 3.636 bytes, sem crash.
  `GetCapabilities` validou o endpoint Media. A chamada de
  `GetVideoEncoderConfigurationOptions` recebeu HTTP 200, mas não foi
  reconhecida como resposta com `H264Options` nem como fault conhecido
  (`fault=unclassified`); portanto, os perfis suportados continuam
  desconhecidos. RTSP em `/stream2` completou `OPTIONS 200`, Digest
  (`DESCRIBE 401` seguido de `200`), `SETUP 200` e `PLAY 200`; o SPS indicou
  `profile_idc=77`, `constraints=0x00`, `level_idc=31` (H.264 Main), rejeitado
  pelo TinyH264. Monitor COM8 encerrado após a captura. Nenhuma operação
  `Set*` foi enviada.

### 2026-10-02 — margem de memória e diagnóstico da tela Mercado

- Board Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3; C6 permaneceu em
  ESP-Hosted 3.0.6/RPC v2/SW_AGGR, sem gravação. Imagem C6 de referência:
  `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`;
  esse hash não foi lido da placa nesta sessão. Worktree baseado em `80c489d`
  com alterações locais preservadas.
- Para a tela Mercado, a navegação passa a liberar caches até reservar 48 KiB
  do pool LVGL de 64 KiB; o gauge Fear & Greed agenda 24 arcos em vez de 64.
  Após construir a tela, o firmware registra memória LVGL livre/maior bloco.
  Desconexões Wi-Fi agora registram também `reason=`.
- Build limpo ESP-IDF 5.5.4, Python 3.14.4, target `esp32p4`, em
  `firmware/build/market-connectivity-fix-20261002`; imagem `0x33c570` bytes,
  com `0x4c3a90` bytes livres na menor partição de app. SHA-256 P4:
  `658668C2E8861F32F7AD15268F2FC0043A8A0DD5758EEEA1734C84917956C288`;
  `firmware/sdkconfig` SHA-256
  `551CAD23C8343C9C18900A0863E20AFB40D62B2392F88760060D1E49183B3F04`;
  tabela de partições SHA-256
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
- `idf.py -B build/market-connectivity-fix-20261002 -p COM8 app-flash monitor`
  gravou somente o app P4 em `0x20000`; esptool confirmou `Hash of data
  verified`. Boot capturado por cerca de 32 s: PSRAM 32 MiB, display/touch,
  C6 3.0.6/RPC v2/SW_AGGR e IP `192.168.1.16`; Home ficou visível. Não houve
  evento `STA_DISCONNECTED`, panic ou reboot na captura. Bitcoin, clima,
  Fear & Greed e índices retornaram `ESP_OK`; USD/BRL expirou uma vez em
  `ESP_ERR_TIMEOUT`, sem queda do link Wi-Fi observada. A tela Mercado não foi
  aberta por toque nesta sessão, então a correção de navegação ainda requer
  confirmação física. Monitor COM8 foi encerrado; C6 e dados de usuário não
  foram alterados.
- O usuário então reproduziu a falha ao entrar em Mercado: o log mostrou
  `free=21996`, `largest=20936`, `used=64%` no pool LVGL após construir a tela,
  seguido por `Load access fault`, `MTVAL=0x28`, no setter de estilo do LVGL.
  A pilha fornecida mistura deleção de objeto, criação do drawer, sparkline e
  atualização de Home/Wi-Fi; não prova uma única origem, mas aponta para uso de
  handle LVGL inválido ou corrupção, não para esgotamento simples do pool nesse
  instante. O SHA truncado do ELF (`3a104c2ba`) coincide com o prefixo do ELF
  gravado na captura anterior; o aviso de checksum do app, por si só, não
  localiza a causa.
- Foram adicionadas verificações de alocação nula às primitivas centrais de UI
  e validação de validade do objeto antes de reutilizar um chart sparkline.
  Build limpo ESP-IDF 5.5.4/Python 3.14.4, target `esp32p4`, em
  `firmware/build/market-crashfix-20261002`: passou; imagem `0x33c670` bytes,
  com `0x4c3990` bytes livres na menor partição. SHA-256 P4:
  `685F572CC21AEF01D90F63579964332C720D81CCF9D5BC08DA968FAEEA697240`;
  ELF SHA-256 `45B90F849174054A558CD2A46E55A370BBB27C4FCF36D57F2882C41DD24ED7F`;
  `sdkconfig` SHA-256
  `551CAD23C8343C9C18900A0863E20AFB40D62B2392F88760060D1E49183B3F04`;
  `partitions.csv` SHA-256
  `F33F93A1147944297C72DDA9BD1F92E074B8B8070171A5BB2D4301C52B6B25CC`.
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` permanece desabilitado/não definido.
- A tentativa de gravar somente o app P4 em `0x20000` falhou antes de abrir a
  sessão de flash: `esptool` recebeu acesso negado em COM8 e o Windows reportou
  que COM8 não está disponível; nenhuma porta serial foi enumerada. Logo, esta
  imagem não foi gravada e não há novo boot capturado. C6 não foi acessado.
  Repetir o flash e testar a navegação de Mercado com o ELF desta mesma versão
  quando a placa reaparecer no USB. A flutuação do Wi-Fi permanece sem
  diagnóstico conclusivo; o boot anterior teve uma expiração HTTPS isolada sem
  evento `STA_DISCONNECTED` observado.
- Após o usuário realizar o flash e testar a imagem
  `market-crashfix-20261002`, informou que o travamento parou. Este é um
  resultado de teste físico relatado pelo usuário; não foi enviada nova captura
  serial. Em seguida confirmou que a conexão Wi-Fi também permaneceu estável
  até o momento. A observação ainda é inicial e relatada pelo usuário, sem nova
  captura serial ou duração definida; não constitui gate de estabilidade.

### 2026-10-02 — scan Wi-Fi via ESP-Hosted sem RPC bloqueante

- O usuário relatou que, após perder a rede, novas buscas repetiam
  `eh_host_feat_rpc: request: no response ... msg_id=286 (5000 ms)` e
  `Wi-Fi scan request failed: ESP_FAIL`. A implementação anterior chamava
  `esp_wifi_scan_start(..., true)`, mantendo a RPC síncrona até concluir todos
  os canais; o ESP-Hosted tem timeout RPC padrão de 5 s. Isso pode conflitar
  com a duração real do scan e explica os logs sem provar sozinho que o SDIO
  esteja travado.
- Implementado ADR-064: scan inicia sem bloqueio e o worker espera
  `WIFI_EVENT_SCAN_DONE` até 15 s. A solicitação é recusada sem RPC quando o
  estado local informa Hosted/Wi-Fi não pronto. Não há recovery C6 automático;
  a recuperação continua pelo comando de manutenção e seu limite/cooldown.
- Build limpo ESP-IDF 5.5.4/Python 3.14.4, target `esp32p4`, em
  `firmware/build/wifi-async-scan-20261002`: passou; imagem `0x33c7b0` bytes,
  com `0x4c3850` bytes livres na menor partição. SHA-256 P4:
  `BB5C853F96C8F71C9D9AB073C73A24D677178ED3AC532F72D43ED1BEC0E33F75`;
  ELF SHA-256
  `3E656DEDDD81E4D004B1AE8330F3AAAC5EC674DBB3FC9A19B45DAD37E6CD5539`;
  `sdkconfig` SHA-256
  `551CAD23C8343C9C18900A0863E20AFB40D62B2392F88760060D1E49183B3F04`;
  tabela de partições SHA-256
  `F33F93A1147944297C72DDA9BD1F92E074B8B8070171A5BB2D4301C52B6B25CC`.
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` e
  `CONFIG_ESP_HOSTED_HOST_TRANSPORT_RESTART_ON_FAILURE` continuam desativados.
- A COM8 segue indisponível no Windows, sem portas seriais enumeradas. A
  imagem não foi gravada; boot, scan assíncrono real e estado após perda de
  SDIO aguardam flash e ensaio físico. C6 não foi acessado.

### 2026-10-02 — recuperação limitada para RPC Wi-Fi sem resposta

- Novos logs do usuário mostraram chamadas de `esp_wifi_disconnect` e
  `esp_wifi_connect` sem resposta RPC por 5 s, embora o estado local ainda
  marcasse Hosted ativo. Implementado ADR-065 inicialmente com duas falhas
  `ESP_FAIL` consecutivas agendando `recover_hosted_link()` no worker; evento Hosted
  `TRANSPORT_FAILURE/DOWN` agenda a mesma operação. O ciclo permanece limitado
  a três por 10 minutos, com cooldown de 5 minutos, não reinicia o P4 e deixa
  reset54 sob propriedade do ESP-Hosted. A varredura assíncrona permanece ativa.
- Build ESP-IDF 5.5.4/Python 3.14.4 com target explícito `esp32p4`, em
  `firmware/build/wifi-hosted-rpc-recovery-20261002`: passou. App P4
  `0x33c9b0` bytes; espaço livre na menor partição `0x4c3650` bytes (60%).
  SHA-256 da imagem: `3BBB39CBB1252979B09E65B8C3B37040D4A9C2AFA3B634138FA2E7B55AF4CD92`;
  ELF: `3DA8B748DB3E1C0F58960461D0CBF973A71446B2837BE50BEE036CC31F565F78`;
  configuração efetiva gerada (`config/sdkconfig.cmake`):
  `CCDD25C1BF226D6921A7E83FCBC768573B193E1521F8665AA6F7ADAE59DBABB0`;
  partições: `F33F93A1147944297C72DDA9BD1F92E074B8B8070171A5BB2D4301C52B6B25CC`.
  `IDF_TARGET=esp32p4`. `CONFIG_SPI_FLASH_AUTO_SUSPEND` e
  `CONFIG_ESP_HOSTED_HOST_TRANSPORT_RESTART_ON_FAILURE` estão vazios/desligados.
- O Windows continua sem enumerar portas seriais e `mode COM8` informa que a
  porta não está disponível. Esta imagem ainda não foi gravada; não há boot
  capturado nem ensaio físico do recovery. C6 não foi acessado.

### 2026-10-02 — antecipação do recovery após a primeira RPC sem resposta

- Ajustado o gatilho do ADR-065 para agendar recovery já na primeira chamada
  `esp_wifi` que retorne `ESP_FAIL` enquanto Hosted e Wi-Fi constam prontos.
  A falha já consumiu o timeout RPC de 5 s; a mudança evita esperar outra
  tentativa sem resposta. `WIFI_EVENT_STA_DISCONNECTED` normal continua
  disparando retry de associação, sem reiniciar Hosted/C6.
- Build limpo ESP-IDF 5.5.4, target explícito `esp32p4`, em
  `firmware/build/wifi-hosted-first-rpc-recovery-20261002`: passou. App P4
  `0x33c9b0` bytes; espaço livre na menor partição `0x4c3650` bytes (60%).
  SHA-256 da imagem: `2B389AE0E11D348069F662A8E0B14D36030E4FF15F4D70C749A834F4D04A0563`;
  ELF: `76FE2CAD34834AD618F4DFDBF8E8FD66C4A61062054EA8B560DADD9EAA74848C`;
  configuração efetiva gerada (`config/sdkconfig.cmake`):
  `CCDD25C1BF226D6921A7E83FCBC768573B193E1521F8665AA6F7ADAE59DBABB0`;
  partições: `F33F93A1147944297C72DDA9BD1F92E074B8B8070171A5BB2D4301C52B6B25CC`.
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` e
  `CONFIG_ESP_HOSTED_HOST_TRANSPORT_RESTART_ON_FAILURE` estão desligados.
- Análise da correlação relatada: CoinGecko busca Bitcoin e quatro altcoins em
  uma chamada; os três endpoints brapi para índices rodam sequencialmente no
  domínio `MARKET_INDICES`, normalmente a cada hora e com retry de dois minutos
  se falhar. O scheduler é independente da tela Mercado; abrir a tela não
  inicia essa sequência. Os logs enviados não incluem a linha
  `product refresh scheduled domain=4` imediatamente antes da queda, portanto
  a coincidência temporal ainda não foi confirmada.
- Na tentativa de gravação do agente, a COM8 estava indisponível e o Windows
  negou enumeração das portas seriais; a gravação não pôde ser feita nessa
  sessão. C6 não foi acessado.
- Depois, o usuário informou que gravou o build recomendado
  `wifi-hosted-first-rpc-recovery-20261002` (P4 SHA-256
  `2B389AE0E11D348069F662A8E0B14D36030E4FF15F4D70C749A834F4D04A0563`) e
  relatou que a conexão não está mais caindo ao entrar em Mercado. É uma
  observação física preliminar relatada pelo usuário; não há captura serial,
  duração do ensaio ou confirmação do log de recovery, portanto não fecha o
  gate de estabilidade de rede/Hosted. C6 não foi gravado.

### 2026-10-02 — reduzir consulta de Fear & Greed para frequência diária

- Alterada a cadência do domínio Fear & Greed para 24 horas após sucesso e
  retry após uma hora em caso de falha. A Alternative.me atualiza o índice
  diariamente; o polling anterior de 15 minutos gerava até 96 consultas por dia
  para esse dado. Decisão registrada no ADR-066.
- O build correspondente foi interrompido após o usuário solicitar também a
  cadência diária para S&P 500, Nasdaq e Ibovespa; nenhum resultado desse build
  parcial foi usado como evidência.

### 2026-10-02 — cadência diária para Fear & Greed e índices de mercado

- Aplicada a mesma política aos índices `^BVSP`, `^GSPC` e `^IXIC`: domínio
  consulta em série a cada 24 horas após sucesso e tenta novamente após uma
  hora em caso de falha. O domínio permanece separado e atualiza parcialmente
  por ativo. Decisão registrada no ADR-067.
- Bancada: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash NOR e PSRAM
  de 32 MiB; base Git `80c489d5ed2808c9501b2dd474baa7ad5b051dcb`, árvore de
  trabalho com alterações locais. C6 reportou ESP-Hosted 3.0.6; não foi
  gravado. Hash C6 de referência `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`
  não foi lido da placa nesta sessão.
- Build limpo ESP-IDF 5.5.4/Python 3.14.4, target explícito `esp32p4`, em
  `firmware/build/market-refresh-daily-20261002`: passou. App P4
  `0x33c9b0` bytes; `0x4c3650` bytes livres na menor partição (60%). SHA-256
  P4 `B39AFA25F15A792D0A656124D3D4EC34E8D0E979649490658761306F5CB2407D`;
  ELF `A2D29B7386C0FA371D8E6A551427988E37294C47DC52622B25731AE88B72A8DC`;
  configuração efetiva gerada `CCDD25C1BF226D6921A7E83FCBC768573B193E1521F8665AA6F7ADAE59DBABB0`;
  partições `F33F93A1147944297C72DDA9BD1F92E074B8B8070171A5BB2D4301C52B6B25CC`.
  `CONFIG_SPI_FLASH_AUTO_SUSPEND` e
  `CONFIG_ESP_HOSTED_HOST_TRANSPORT_RESTART_ON_FAILURE` estão desligados.
- `idf.py -B build/market-refresh-daily-20261002 -p COM8 -b 460800 app-flash monitor`
  gravou somente o app em `0x20000`; esptool confirmou `Hash of data verified`
  e hard reset. Boot observado por cerca de 44 s: Hosted/C6 3.0.6 subiu, a
  estação recebeu `192.168.1.16`, Home ficou visível e os domínios Bitcoin,
  clima, USD/BRL, Fear & Greed e índices (`domain=0..4`) concluíram com
  `dns=ESP_OK ntp=ESP_OK https=ESP_OK`. Sem panic ou desconexão Wi-Fi na janela
  capturada. A inicialização microSD expirou e o firmware usou o ícone estático
  de clima; não impediu UI ou rede. O monitor foi encerrado; C6 e partições não
  foram alterados.

### 2026-10-02 — busca automática ao entrar na tela Wi-Fi

- A tela sincronizava a projeção da rede e só solicitava scan no callback do
  botão; não havia solicitação de scan no evento de entrada da navegação. O scan
  de boot também é omitido quando credenciais salvas são restauradas para
  priorizar associação, DHCP e NTP. Por isso a lista podia continuar mostrando
  os resultados anteriores ao abrir Configurações > Wi-Fi.
- Na primeira tentativa de scan automático, a página Wi-Fi foi aberta enquanto
  o executor HTTPS ainda tratava `domain=3`. O RPC de `esp_wifi_scan_start`
  expirou (`msg_id=286` após 5 s); o recovery Hosted foi agendado e a
  renegociação SW_AGGR abortou por não alocar buffers de 15872 B, causando
  panic/reset do P4. Esta primeira imagem foi substituída. A implementação
  atual mantém o scan de entrada pendente até o executor HTTPS permanecer
  ocioso por 1 s; sair da tela cancela a pendência. Um `ESP_FAIL` isolado no
  início do scan não dispara recovery Hosted: a supervisão da estação ainda
  pode iniciá-lo após falha de associação/RPC observada.
- Os controles foram movidos para o cabeçalho do painel: adicionar rede é um
  botão com ícone `+`; buscar usa o ícone Material `refresh`, ao lado. O texto
  de estado vazio instrui a usar atualizar.
- Bancada: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash NOR e PSRAM
  de 32 MiB; base Git `80c489d5ed2808c9501b2dd474baa7ad5b051dcb`, árvore de
  trabalho com alterações locais. O boot anunciou C6/ESP-Hosted 3.0.6, RPC v2
  e SDIO SW_AGGR; C6 não foi gravado e seu hash não foi lido nesta sessão.
- Build limpo ESP-IDF 5.5.4/Python 3.14.4, target explícito `esp32p4`, em
  `firmware/build/wifi-screen-autoscan-20261002`, seguido de recompilação das
  proteções: passou. App `0x33cbd0` bytes, com `0x4c3430` bytes livres na menor
  partição de 8 MiB (60%). SHA-256 P4
  `9F0CE0BF7EFDA56D7C14823F37DDBDA98D13413C1E60CAB821E7EE5CE9FB691E`;
  ELF `DA0A631316325C8150844AA99FAF6A5F6297F5B9F92799343B9BD0C5BBA20627`;
  configuração efetiva gerada `CCDD25C1BF226D6921A7E83FCBC768573B193E1521F8665AA6F7ADAE59DBABB0`;
  partições `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  `CONFIG_IDF_TARGET=esp32p4`, RGB565, três framebuffers; auto-suspend da
  flash e restart automático de transporte continuam desligados.
- `idf.py -B build/wifi-screen-autoscan-20261002 -p COM8 -b 460800 app-flash`
  gravou somente a aplicação em `0x20000`; esptool confirmou `Hash of data
  verified` e hard reset. Captura de boot: P4 v1.3, display/touch e Hosted/C6
  3.0.6/RPC v2/SW_AGGR iniciaram; a estação recebeu `192.168.1.16`. Depois
  apareceram falhas DNS/HTTPS e uma falha de alocação DMA no SDIO. A tela Wi-Fi
  não foi aberta por toque nesta captura final, portanto scan automático e
  lista resultante ainda precisam de confirmação tátil. A inicialização da
  microSD expirou e o ícone estático de clima foi usado. C6 e tabela de
  partições não foram gravados.

### 2026-10-02 — limitar scans repetidos e proteger o link Hosted

- Após o usuário relatar que dois toques em atualizar derrubaram o Wi-Fi e
  reiniciaram o P4, o callback passou a coalescer solicitações e esperar o
  executor HTTPS ficar livre por 1 s antes de enfileirar o scan. A entrada
  automática usa a mesma fila. `ESP_FAIL` em `scan_start`, `scan_get_ap_num`
  ou `scan_get_ap_records` deixa de ser contado como falha RPC da estação e não
  dispara recovery Hosted isoladamente. Todo recovery Hosted também aguarda o
  executor HTTPS ficar ocioso por 1 s, com espera limitada a 60 s.
- Build limpo ESP-IDF 5.5.4/Python 3.14.4, target explícito `esp32p4`, em
  `firmware/build/wifi-screen-scan-recovery-clean-20261002`: passou. Binário
  `0x33ce10` bytes, `0x4c31f0` bytes livres na menor partição de 8 MiB (60%).
  SHA-256 app P4 `F4FD26E99F9BABA6FAB0BF162B28C4B0D927B01DB80AD52FDD1D2D3F20A048F2`;
  ELF `33FFF79BEBFD8481622C232DD08089F99FC9FA358DCADC3A305F09A764CDAC81`;
  configuração gerada `CCDD25C1BF226D6921A7E83FCBC768573B193E1521F8665AA6F7ADAE59DBABB0`;
  tabela de partições `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  RGB565, PSRAM 32 MiB, três framebuffers; `CONFIG_SPI_FLASH_AUTO_SUSPEND` e
  restart automático do transporte Hosted permanecem desligados.
- `idf.py -B build/wifi-screen-scan-recovery-clean-20261002 -p COM8 app-flash`
  gravou apenas o app em `0x20000`; esptool confirmou `Hash of data verified`
  e hard reset. Captura serial por cerca de 42 s: P4 v1.3 e display/touch
  iniciaram; Hosted/C6 3.0.6, RPC v2 e SW_AGGR subiram; a estação recebeu
  `192.168.1.16`. Depois houve `eh_sdio: dma_alloc(5120) failed`, seguido de
  erros de conexão HTTP/DNS nos domínios de dados. Não foi observado panic ou
  novo reset nesse intervalo. O usuário informou “agora resolveu” após testar;
  essa confirmação não contém duração nem quantidade de scans. Assim, registro
  a confirmação do usuário, mas não declaro fechado o gate de estabilidade de
  rede/SDIO. C6 e tabela de partições não foram gravados.

### 2026-10-02 — buffers SDIO DMA preferidos em PSRAM

- Motivação: a captura anterior manteve o ícone/associação de Wi-Fi, mas
  registrou `eh_sdio: dma_alloc(5120) failed` e `rx_get_buffer(4776) failed`,
  seguida de DNS/HTTPS sem resposta para todos os domínios de produto. O
  alocador alinhado do ESP-Hosted 3.0.6 já tem opção de preferência por PSRAM
  DMA-capable com fallback para SRAM DMA interna; habilitada no default do P4
  conforme ADR-068. Nenhuma fonte em `managed_components/` foi alterada.
- Bancada: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash NOR 32 MiB,
  PSRAM 32 MiB; commit-base `cf335d9f9c20773f0e3b599d22c85290de83f247`, com
  alterações locais. O C6 negociou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR;
  C6 não foi regravado e seu hash não foi lido nesta sessão (hash histórico de
  referência: `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`).
- Build limpo ESP-IDF 5.5.4/Python 3.14.4, target explícito `esp32p4`, em
  `firmware/build/wifi-dma-psram-clean-20261002`: passou. App `0x33ce10`
  bytes, `0x4c31f0` bytes livres na menor partição de 8 MiB (60%). SHA-256 app
  `68A1DB063D5C7076CE4EF69891EF875AB1F689E4E5FDD552E7661438B2596C7B`;
  ELF `BA5C34F24F64CCE99C7A9380AC3433A58DD12686A3BE2CCCD79465686DC4A7C1`;
  configuração efetiva `sdkconfig.cmake`
  `2021D399840C19A414F28BF5493B5FFDF960AB2CACEC8E5C16205FE95D083930`;
  arquivo local `firmware/sdkconfig` SHA-256
  `1DD9B3CA659AA29AF9F7C2414FB05D0A8802752CB928293AAE921258EC0DE065`;
  partições `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  A configuração efetiva confirma `CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM=y`,
  PSRAM, RGB565 e três framebuffers; auto-suspend de flash e restart automático
  do transporte Hosted continuam desligados.
- `idf.py -B build/wifi-dma-psram-clean-20261002 -p COM8 -b 460800 app-flash
  monitor` gravou somente o app P4 em `0x20000`. Esptool confirmou `Hash of data
  verified`, hard reset e boot. A captura mostrou duas desconexões de estação
  (razões 2 e 205) antes de obter IP `192.168.1.16`; então os domínios 0–4
  concluíram sequencialmente DNS, NTP e HTTPS com `ESP_OK`. Hosted/C6 subiram
  com versões 3.0.6/RPC v2/SW_AGGR. Não apareceu `dma_alloc`/`rx_get_buffer`
  failure nem reset durante cerca de 60 s após a janela de associação e
  atualização. Monitor encerrado, dispositivo deixado em execução. É um passe
  inicial de bancada; não fecha estabilidade/soak nem recuperação após falha
  forçada de AP ou C6.

### 2026-10-02 — preferência de tela inicial: build P4 e gravação bloqueada

- Adicionada em Perfil > Preferências pessoais a escolha de Home, Clima,
  Mercado, Dispositivos ou Pomodoro. A preferência é salva junto ao perfil no
  FlashCoordinator; registros antigos selecionam Home. O boot aguarda a
  restauração do perfil até 6 s e então usa Home como fallback.
- Base Git `cf335d9f9c20773f0e3b599d22c85290de83f247`, com alterações locais.
  Build limpa ESP-IDF 5.5.4, target explícito `esp32p4`, em
  `C:\Users\Tauser\AppData\Local\Temp\np2-startup-screen-build`: passou.
  Binário P4 `0x33d600` bytes; `0x4c2a00` bytes livres na menor partição de
  8 MiB (60%). SHA-256 app `AB20DBD7583E3465785545639EE3D50F3778F548EDEE08B0BFC3FA47F7466013`;
  ELF `021735731004639629A35AC4AB91AC6F5748D44EF55A4CC5BB936835DCEF5D58`;
  sdkconfig efetivo `1DD9B3CA659AA29AF9F7C2414FB05D0A8802752CB928293AAE921258EC0DE065`;
  tabela `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  Target, três framebuffers, preferência de DMA SDIO em PSRAM e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND=n` foram conferidos.
- `idf.py -B C:\Users\Tauser\AppData\Local\Temp\np2-startup-screen-build
  -p COM8 app-flash monitor` não conseguiu abrir COM8 (`PermissionError(13)`,
  porta ocupada ou inexistente). Nenhum byte foi gravado nesta tentativa e não
  há captura de boot desta imagem. O usuário relatou que o flash anterior
  continuou normal, mas não foi identificado o hash daquela imagem; isso não é
   atribuído a este build. C6, tabela de partições e eFuses não foram alterados.

### 2026-10-03 — gesto de páginas principais e indicador expanding-dot

- Implementada navegação por swipe horizontal na ordem Home, Clima, Mercado,
  Dispositivos e Pomodoro. O gesto usa o Navigation Manager existente, não
  atravessa os limites, ignora a área do header e não inicia em objetos
  clicáveis. O indicador do rodapé expande o ponto da tela ativa para uma
  cápsula curta. O padrão foi registrado no ADR-070.
- Placa observada: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash NOR
  32 MiB e PSRAM 32 MiB. C6 anunciou ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR;
  não foi regravado. Base Git `cf335d9f9c20773f0e3b599d22c85290de83f247`, com
  alterações locais.
- Build limpa ESP-IDF 5.5.4/Python 3.14.4, target explícito `esp32p4`, em
  `C:\Users\Tauser\AppData\Local\Temp\np2-page-slider-build`: passou; o
  delta final do expanding-dot recompilou `product_ui.c` e linkou o app. Binário
  `0x33d990` bytes; `0x4c2670` bytes livres na menor partição de 8 MiB (59%).
  SHA-256 app `89973E351C68BA5202832B3B2865300C3F3757CD814ACDEF3CD7CC6A21C13C6A`;
  ELF `7F903F27B4DBED403CBFC849D67942BF1638FCCBE1F6E8CFA85FD400FFA67631`;
  `sdkconfig` efetivo `1DD9B3CA659AA29AF9F7C2414FB05D0A8802752CB928293AAE921258EC0DE065`;
  tabela de partições `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  Configuração confirmou ESP32-P4, PSRAM 32 MiB a 200 MHz, RGB565,
  `CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM=y` e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`; a política de três framebuffers foi
  preservada.
- Primeira tentativa de `idf.py -B C:\Users\Tauser\AppData\Local\Temp\np2-page-slider-build
  -p COM8 app-flash monitor` falhou ao abrir COM8 (`PermissionError(13)`, porta
  ocupada ou inexistente), sem gravar dados. Após o operador liberar a porta,
  a mesma operação gravou somente o app no offset `0x20000`; esptool confirmou
  `Hash of data verified` e hard reset. Tabela de partições e C6 não foram
  gravados.
- Captura serial por cerca de 85 s após o boot: P4 v1.3, PSRAM, display, touch
  GT911 e Hosted/C6 iniciaram; a estação obteve `192.168.1.16`. Não apareceu
  panic nem reset nessa janela. HTTPS dos domínios 0, 1, 3 e 4 concluiu com
  `ESP_OK`; o domínio do BCB falhou em DNS nessa rodada. O swipe e o indicador
  expanding-dot ainda precisam de confirmação tátil na placa; build e boot não
  fecham esse teste funcional nem um soak de estabilidade.

### 2026-10-03 — correção do gesto de páginas após teste tátil

- O operador informou que o indicador apareceu, mas o swipe não navegava. A
  causa estava no filtro do início do gesto: `lv_obj_create()` marca objetos
  genéricos com `LV_OBJ_FLAG_CLICKABLE`, então a verificação descartava toques
  em praticamente toda a área de conteúdo. Removido esse filtro. Após um swipe
  reconhecido, a soltura normal do widget é preservada e LVGL consome os
  eventos de clique da interação para evitar acionar o controle tocado. O log `product_ui` agora
  registra página de origem/destino e deltas quando uma troca é aceita.
- Build limpa ESP-IDF 5.5.4/Python 3.14.4, target explícito `esp32p4`, em
  `C:\Users\Tauser\AppData\Local\Temp\np2-page-slider-fix-build`: passou.
  App `0x33dac0` bytes; `0x4c2540` bytes livres na menor partição de 8 MiB
  (59%). SHA-256 app
  `D809EC2467901196151BB0040ACC88B196306DE00AA2197A6FF0887B92139F4D`;
  ELF `FEC166CC5724A65EE8E0939BF6BFD61DEC67B4F59D2B5C038A3F551E13047A24`;
  `sdkconfig` efetivo
  `1DD9B3CA659AA29AF9F7C2414FB05D0A8802752CB928293AAE921258EC0DE065`;
  tabela de partições
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
- `idf.py -B C:\Users\Tauser\AppData\Local\Temp\np2-page-slider-fix-build
  -p COM8 app-flash monitor` gravou apenas o app P4 em `0x20000`; esptool
  confirmou `Hash of data verified` e hard reset. C6 e tabela não foram
  gravados. A captura passou por cerca de 149 s de uptime sem panic ou reset;
  P4 v1.3, display, touch GT911 e Hosted/C6 3.0.6/RPC v2/SW_AGGR iniciaram,
  e a estação obteve `192.168.1.16`. Os domínios 0, 1, 3 e 4 concluíram HTTPS;
  o domínio do BCB falhou em DNS nessa janela.
- A captura confirma boot, não a navegação pelo toque. Aguardando nova
  confirmação tátil do operador; o swipe deve gerar `page swipe origem ->
  destino` no serial quando aceito.
- Ajuste final após revisar o ciclo de evento: recompilado e regravado o mesmo
  app, preservando `LV_EVENT_RELEASED` para que o widget limpe o estado visual
  de pressionado e consumindo somente os eventos de clique. SHA-256 do app
  final `AA03F59FD35EB84126DE273154FE563DFCA603FC6C5AAB1DB612BB9F226CA534`;
  ELF `6FE710FD0869CEDC959862E67CF5E57BA730BA068A45EC400A348BB58DCC4B4B`.
  O flash app-only confirmou `Hash of data verified`; boot final sem panic/reset
  durante a captura inicial. O operador ainda precisa confirmar o swipe físico
  nesta última imagem.

### 2026-10-03 — navegação por swipe confirmada na placa

- O operador informou que o swipe continuava sem funcionar após as correções
  anteriores. A segunda causa era o uso de `lv_event_get_indev()` dentro de
  callback registrado diretamente na lista do indev: nesse contexto a API lê o
  parâmetro do evento (o objeto tocado). O handle do dispositivo é o alvo do
  evento, conforme já usado pelo diagnóstico de touch. Corrigido para
  `lv_event_get_target()`; adicionados logs do ponto inicial/final, delta e
  estado do Navigation Manager para arrastos acima de 32 px.
- Build limpa ESP-IDF 5.5.4/Python 3.14.4, target explícito `esp32p4`, em
  `C:\Users\Tauser\AppData\Local\Temp\np2-page-slider-indev-fix-build`:
  passou. App `0x33dba0` bytes; `0x4c2460` bytes livres na menor partição de
  8 MiB (59%). SHA-256 app
  `F197F6A8991853DA1DB3874AD1547B627A23C9169C215DA22F5A36C8FD6ADCC3`;
  ELF `900DD2AA8613C7D43A5948FB2DD7B368027CE567FA63F23180030AED71D51683`;
  `sdkconfig` efetivo
  `1DD9B3CA659AA29AF9F7C2414FB05D0A8802752CB928293AAE921258EC0DE065`;
  partições
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
- `idf.py -B C:\Users\Tauser\AppData\Local\Temp\np2-page-slider-indev-fix-build
  -p COM8 app-flash monitor` gravou apenas o app P4 no offset `0x20000`;
  esptool confirmou `Hash of data verified` e hard reset. Captura de boot de
  cerca de 69 s: P4 v1.3, display/touch, Hosted/C6 3.0.6/RPC v2/SW_AGGR
  iniciaram e o Wi-Fi recebeu `192.168.1.16`. Não houve panic ou reset durante
  esse período. C6 e tabela de partições não foram gravados.
- O operador realizou vários arrastos. O serial registrou `page swipe 1 -> 2`
  (Home → Clima), navegação sucessiva até Pomodoro e retorno `5 -> 4 -> 3`,
  com o Navigation Manager completando `ENTER`; também registrou retorno
  `2 -> 1`. A interação por swipe fica confirmada nos dois sentidos. Ao entrar
  em Mercado houve um aviso `LVGL reserve low` (40.816 B livres no intervalo
  de limpeza, limite configurado em 49.152 B); a tela foi montada e as trocas
  seguintes concluíram sem reset. Esse aviso permanece observação de memória,
  não falha de navegação.

### 2026-10-03 — ações do header e centro de notificações

- O header agora encaminha o ícone Wi-Fi para a cena de configuração. O sino
  abre sobre a tela atual um histórico limitado a oito eventos: transições de
  rede com credenciais ativas, novas cotações recebidas e eventos de erro ou
  recuperação do sistema/armazenamento. O badge mostra as não lidas; abrir um
  item ou marcar todas como lidas retira-as da contagem e conserva os itens no
  histórico. O modal é construído ao abrir e destruído ao fechar para preservar
  heap LVGL nas páginas de produto. A decisão está no ADR-071.
- Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash NOR 32 MiB e
  PSRAM 32 MiB. Base Git `cf335d9f9c20773f0e3b599d22c85290de83f247`, com
  alterações locais. C6 iniciou sem regravação, anunciando ESP-Hosted 3.0.6,
  RPC v2 e SDIO SW_AGGR. Hash da imagem C6 instalada não foi lido nesta sessão;
  a referência histórica da imagem reproduzida é
  `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`.
- Build limpa ESP-IDF 5.5.4/Python 3.14.4, target explícito `esp32p4`, em
  `C:\Users\Tauser\AppData\Local\Temp\np2-header-notification-20261003-build`:
  passou, sem warnings de compilação nos arquivos alterados. App `0x33efb0`
  bytes; `0x4c1040` bytes livres na menor partição de 8 MiB (59%). SHA-256 app
  `F299186253B5BDCE0F0EF805DD9B04E9A2A3FF1C1B9A8CFCF2F6B5D2AC053A05`;
  ELF `93C9E0E78508220E8AF1A83D2027AA1AF48AC2EF67C7DF2D10F7843C23091B05`;
  configuração efetiva `sdkconfig.cmake`
  `2021D399840C19A414F28BF5493B5FFDF960AB2CACEC8E5C16205FE95D083930`;
  `firmware/sdkconfig` `1DD9B3CA659AA29AF9F7C2414FB05D0A8802752CB928293AAE921258EC0DE065`;
  tabela de partições
  `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  Configuração observada: P4, XiP em PSRAM, três framebuffers,
  `CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM=y` e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`.
- `idf.py -B C:\Users\Tauser\AppData\Local\Temp\np2-header-notification-20261003-build
  -p COM8 app-flash monitor` gravou somente o app em `0x20000`; esptool
  confirmou `Hash of data verified` e hard reset. A captura final mostrou a UI
  iniciando, C6 pareado e Wi-Fi recebendo `192.168.1.16`, sem panic/reset em
  cerca de 35 s. Os domínios HTTPS 0, 1, 3 e 4 concluíram; o domínio 2 (`api.bcb.gov.br`)
  falhou ao resolver DNS. O teste físico dos toques nos ícones e nos itens do
  modal ainda depende de interação visual na placa. C6 e partições não foram
  gravados; monitor encerrado e COM8 liberada.
- A gravação e o boot foram confirmados; a interação física do ícone Wi-Fi,
  abertura do sino, leitura individual e leitura em lote ainda precisa de
  conferência tátil. O histórico implementado é volátil, começa vazio após
  reboot e mantém no máximo oito eventos.
- Ajuste de retorno da cena Wi-Fi: o botão Voltar passa a retornar à tela que
  originou a abertura pelo header; ao entrar pela linha de redes em Preferências,
  retorna a Preferências. Build limpo adicional ESP-IDF 5.5.4/Python 3.14.4,
  target `esp32p4`, em
  `C:\Users\Tauser\AppData\Local\Temp\np2-wifi-return-20261003-build`:
  passou. App `0x33f030` bytes, 59% livres na partição OTA de 8 MiB; SHA-256 app
  `A883744635D1096BBE3DB80CF45EAE33111E988219A5F7D90B32CC533F16D702`, ELF
  `9A2E8F9864FCDA200BF752C005E90280F1DE3E4D6D0482641B5157FEA7E3B77C`.
  `sdkconfig.cmake` e partições mantiveram os hashes acima; o `firmware/sdkconfig`
  original foi restaurado com SHA-256
  `1DD9B3CA659AA29AF9F7C2414FB05D0A8802752CB928293AAE921258EC0DE065`.
  `app-flash monitor` gravou apenas o app em `0x20000` e verificou o hash.
  Boot chegou à tela inicial, negociou C6/ESP-Hosted 3.0.6/RPC v2/SW_AGGR e
  conectou à rede (`192.168.1.16`), sem panic/reset em cerca de 37 s. HTTPS
  domínios 0, 1, 3 e 4 concluíram; a consulta do domínio 2 continuou falhando
  na resolução DNS. Monitor encerrado e COM8 liberada.

### 2026-10-03 — serviço de sincronização eWeLink e inventário persistente

- Board Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, NOR 32 MiB e PSRAM
  32 MiB. Base Git `cf335d9f9c20773f0e3b599d22c85290de83f247`, com alterações
  locais preservadas. C6 instalado iniciou com ESP-Hosted 3.0.6, RPC v2 e
  SDIO SW_AGGR; a imagem C6 não foi lida nesta sessão, referência histórica
  `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`.
- Build incremental ESP-IDF 5.5.4/Python 3.14.4, target `esp32p4`, em
  `firmware/build/ewelink-sync-20261003`: passou. App `0x345000` bytes, com
  `0x4bb000` bytes livres na partição OTA de 8 MiB (59%). SHA-256 do app
  `CA4FFD8859A9A18B56D1D0236DFC6225644868CA1CB4351C77640E7B97F08DD8`;
  ELF `C662764279C835CD65ECBF27EC4CACF3847C9E005AA0B29695FBBA332548D517`;
  configuração efetiva `sdkconfig.cmake`
  `2021D399840C19A414F28BF5493B5FFDF960AB2CACEC8E5C16205FE95D083930`;
  partições `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  Configuração observada: flash 32 MiB, `CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM=y`
  e `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`.
- A primeira imagem intermediária abortou no init de áudio (`i2s_alloc_dma_desc`,
  `ESP_ERR_NO_MEM`) após aumentar o BSS interno. Os workspaces de inventário
  foram movidos para PSRAM e os buffers grandes de autenticação/corpo HTTP
  passaram a ser temporários. A build e gravação finais abaixo são dessa
  correção; áudio inicializou e o executor HTTPS iniciou sem `ESP_ERR_NO_MEM`.
- `idf.py -B build\\ewelink-sync-20261003 -p COM8 -b 460800 app-flash monitor`
  gravou somente o app P4 em `0x20000`; esptool confirmou `Hash of data verified`
  e hard reset. Boot serial por cerca de 33 s: P4 v1.3, display/touch, áudio,
  C6/Hosted e Wi-Fi iniciaram; recebeu `192.168.1.16`. Os domínios HTTPS 0, 1,
  3 e 4 concluíram; domínio 2 (`api.bcb.gov.br`) falhou em DNS. Não houve
  panic/reset no intervalo. Monitor encerrado e COM8 liberada; C6, partições e
  dados locais não foram gravados.
- A sessão real de login eWeLink não foi executada nesta validação: nenhuma
  credencial foi fornecida ao firmware. A assinatura ainda precisa ser
  comparada com vetor conhecido do POC e a sincronização real precisa de uma
  conta de bancada. A persistência atual usa NVS sem proteção contra extração
  física; NVS Encryption/Flash Encryption permanece pendência de segurança.
- Revalidação solicitada em 2026-10-03: build ESP-IDF 5.5.4 passou novamente;
  app `0x345000` bytes, SHA-256
  `BAC21BC8CA152817CB4FB1DC8FDEDB34B94B506F845D9D797CB678877C3ABD5E`, ELF
  `B7481227C36E44AC0F47C4A2FD29D19CEA1A0A43C1627236933C617E6A90B3B6`. O app
  atual foi regravado apenas em `0x20000`, com `Hash of data verified`. Boot por
  cerca de 32 s chegou à UI, inicializou áudio/C6/Hosted/HTTPS e recebeu IP;
  não houve panic/reset. O domínio `api.bcb.gov.br` continuou sem resolver DNS.
  Não houve tentativa de login, fetch ou escrita de inventário nesta rodada.
- Integração visual eWeLink em 2026-10-03: a tela Dispositivos passou a expor
  “Importar inventário da conta eWeLink”, campos transitórios de email/telefone
  e senha, estado da sincronização e resumo local sem `devicekey`. O submit
  copia as credenciais para o serviço sob demanda e zera os buffers locais e
  campos; fechar o modal também limpa os campos. Build limpo ESP-IDF 5.5.4,
  Python 3.14.4, target `esp32p4`, em
  `firmware/build/ewelink-ui-20261003`: passou. App `0x345f90` bytes,
  `0x4ba070` livres (59%) na menor partição OTA; SHA-256
  `561BC2B3355E084008AFE396B35B893FA195468473328CA1AE4BCB8687C68975`, ELF
  `8A98A2CD841D3791010A599AC1413DA7DA3F2E113F5B0547233990F6ABF7906A`;
  configuração efetiva `2021D399840C19A414F28BF5493B5FFDF960AB2CACEC8E5C16205FE95D083930`,
  partições `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
- `app-flash monitor` gravou somente o app em `0x20000`; esptool confirmou
  `Hash of data verified`. Boot serial por cerca de 55 s: P4 v1.3, display/touch,
  áudio, C6/Hosted 3.0.6/RPC v2/SW_AGGR e Wi-Fi iniciaram; recebeu
  `192.168.1.16`, abriu a cena Dispositivos e não houve panic/reset. Domínios
  HTTPS 0, 1, 3 e 4 concluíram; `api.bcb.gov.br` falhou em DNS. Monitor
  encerrado e COM8 liberada; C6, partições e dados locais não foram gravados.
  O toque real dos campos/botão e uma autenticação eWeLink ainda aguardam teste
  na placa com uma conta de bancada; nenhuma credencial foi inserida nesta
  validação.
- Correção da tentativa de login eWeLink em 2026-10-03: o encoder Base64 da
  tabela de regiões recebia `sizeof(pointer)` (8 bytes) como capacidade, em vez
  dos 11.000 bytes alocados; a derivação da assinatura falhava antes de enviar
  o POST. Corrigido para usar a capacidade real e adicionados logs seguros de
  etapa, status HTTP, código da API e região, sem conta, senha, assinatura ou
  token. Build incremental ESP-IDF 5.5.4/Python 3.14.4, target `esp32p4`, em
  `firmware/build/ewelink-ui-20261003`: passou; app `0x346160` bytes, com
  `0x4b9ea0` bytes livres (59%) na menor partição OTA. SHA-256 app
  `581967B60E06C65A8B2BDEA9F54972DE1A1F2751ECB78918C5CDD38C7F37C347`, ELF
  `BC963CFF5CDC4EC7E0EEDF5CD05FAA83589EA40175537D6688B488F262797D71`;
  `sdkconfig.cmake` `2021D399840C19A414F28BF5493B5FFDF960AB2CACEC8E5C16205FE95D083930`,
  partições `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  `app-flash` gravou somente a app em `0x20000` e confirmou `Hash of data
  verified`. Boot serial por mais de 30 s chegou à UI, inicializou C6/Hosted
  3.0.6/RPC v2/SW_AGGR, obteve `192.168.1.16` e não apresentou panic/reset;
  HTTPS dos domínios 0, 1, 3 e 4 concluiu, com falha DNS conhecida no domínio 2.
  Esta rodada não refez login. Monitor serial permanece ativo para capturar a
  próxima tentativa; C6, partições e NVS não foram gravados.

### 2026-10-03 — imagem local de teste com formulário eWeLink pré-preenchido

- A pedido do usuário, foi criada uma configuração exclusivamente local para
  abrir o modal de importação com os campos de conta e senha já preenchidos.
  O cabeçalho local fica fora do repositório, mas os textos estão embutidos na
  imagem P4 atualmente gravada e são recuperáveis do flash sem Flash Encryption.
  Remover a configuração temporária e regravar uma imagem limpa após o teste.
  O fluxo normal continua limpando os campos ao enviar ou fechar o modal; os
  valores não são enviados a logs, estado ou NVS pelo código de UI.
- Board Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, NOR 32 MiB/PSRAM
  32 MiB. C6 não foi gravado; boot reportou ESP-Hosted 3.0.6, RPC v2 e SDIO
  SW_AGGR. Referência histórica de hash C6:
  `3EEC7C1E256BEFCE925ECCC661D4A4C730EC10F724433AB1001FC7D2965F43EE`.
- Build limpa ESP-IDF 5.5.4/Python 3.14.4, target `esp32p4`, em diretório
  externo ao repositório; `NP2_EWELINK_TEST_PREFILL_HEADER` aponta para
  configuração local externa. App `0x346180` bytes, `0x4b9e80` bytes livres
  (59%) na menor partição OTA. SHA-256 app
  `C85002B939D2331A37A3A1077F46952D13AD7D59818C11029E30C2CF5D9F4234`, ELF
  `669581114A778B46179B76C824063AB0F78BA84C71624F720AA3DAD62A9056F9`,
  `sdkconfig.cmake` `2021D399840C19A414F28BF5493B5FFDF960AB2CACEC8E5C16205FE95D083930`,
  `sdkconfig` `1DD9B3CA659AA29AF9F7C2414FB05D0A8802752CB928293AAE921258EC0DE065`,
  partições `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
- `idf.py -B <build externo> -p COM8 app-flash` gravou somente o app no offset
  `0x20000`; esptool confirmou `Hash of data verified` e hard reset. Boot serial
  chegou à UI, touch/display ativos e C6/Hosted negociado, sem panic/reset nos
  primeiros 36 s. Wi-Fi recebeu `192.168.1.16`; domínios HTTPS 0, 1, 3 e 4
  passaram e o domínio 2 continuou falhando em DNS. Monitor COM8 permanece ativo
  para observar a autenticação solicitada; toque na tela e tentativa de login
  ainda aguardam ação física do usuário.

- Reteste em 2026-10-03: a solicitação eWeLink chegou ao executor (`mode=9`),
  mas terminou como `ESP_ERR_NO_MEM` antes de surgir qualquer log de login HTTP.
  Isso localiza a falha antes de autenticar; ainda não permite concluir se a
  falta de memória ocorreu na criação do workspace ou em uma etapa posterior.
  Adicionado log de falha com etapa, resultado e heap livre/maior bloco (PSRAM e
  SRAM interna), sem credenciais. Build incremental ESP-IDF 5.5.4, target
  `esp32p4`, no mesmo diretório externo: passou; app `0x346360` bytes, 59% livres,
  SHA-256 `92D15952C2F181BAE468AF3264AB43BF4BC088D58F64345D3D9F7D556BE56738`,
  ELF `484C7915F4ADF6147F3D861F2FDE72DBF69CB9A0EF28FA48A5AD20BDD50C09BF`.
  App-only flash em `0x20000` foi verificado pelo esptool. Boot chegou à UI,
  Hosted 3.0.6/RPC v2/SW_AGGR e Wi-Fi com IP; monitor COM8 reaberto para uma nova
  tentativa e captura do estágio exato. Nenhum login bem-sucedido ou inventário
  foi confirmado até esta evidência.

- Comparação com `C:\\ewelink-poc\\ewelink_dump.py`: tabela tem 205 entradas e
  a representação Python `str(REGIONS)` coincide byte a byte com a tabela do
  firmware (5.905 bytes; SHA-256
  `97ECFBD8882A9A10766F08C095F74EE3E55DF042D472D00CA220FFE24C266B8A`). O
  arquivo de resposta raw do POC mede 9.401 bytes. Diagnóstico do teste anterior:
  o firmware reservava 49.153 bytes contíguos em SRAM interna para o corpo HTTP
  antes do POST; após abrir Dispositivos, o maior bloco interno observado era
  27.648 bytes. O login não chegava à rede. Correção ADR-073 move somente esse
  corpo limitado (máx. 48 KiB) para PSRAM; TLS mantém SRAM interna. Build
  incremental ESP-IDF 5.5.4/target `esp32p4` passou; app `0x346360` bytes, 59%
  livres, SHA-256 `5E0E39B58D259470E74A111357DEFCCF512B001263AF98277263B10731FD17B1`,
  ELF `F036ED84F052C9B3E1A35666043AFA215FEA0302F3BC34D19FF49E4D00A6B831`.
  App-only flash em `0x20000` verificado (`Hash of data verified`). Boot iniciou
  UI, Hosted C6 3.0.6/RPC v2/SW_AGGR, Wi-Fi e HTTPS nos domínios 0/1/3/4; DNS do
  domínio 2 continua falhando. Ainda sem nova tentativa eWeLink após esta
  correção. COM8 permanece monitorada.

- Login eWeLink após realocação do corpo HTTP: servidor respondeu HTTP 200,
  `api_error=10001`, região `us`; a tabela/região foi aceita e o erro já não é
  de alocação. Inspeção da configuração local de prefill encontrou diferença
  introduzida no teste: o email estava com o primeiro caractere em maiúscula,
  apesar de o usuário ter fornecido minúsculo. Corrigido para usar exatamente a
  caixa informada; nenhuma credencial foi registrada aqui. Build incremental
  ESP-IDF 5.5.4, `esp32p4`, passou; app `0x346360`, 59% livres, SHA-256
  `4A082437D6E8385E9FD7AE73FD334A5734BF610564863DAB9E3B09095EFC3C03`, ELF
  `0D5202D3F977B49D58B5BD76D5F4A72064FBF74CF9D1A365C01F1D86DBF46792`. App-only
  flash em `0x20000`, hash verificado. Boot desta última imagem negociou Hosted
  3.0.6/RPC v2/SW_AGGR, mas o AP rejeitou a autenticação Wi-Fi (`reason=202`);
  ainda sem login cloud após a correção de caixa. COM8 permanece monitorada.

- Reteste de diagnóstico em 2026-10-03: executei somente o login de
  `C:\\ewelink-poc\\ewelink_dump.py`, carregando os mesmos dados da imagem de
  teste, sem consultar a lista de devices. O POC também recebeu HTTP 200 e
  `api_error=10001` na região `us`; não houve token nem resposta de inventário.
  A imagem P4 registrou o mesmo HTTP/API error após o ajuste da caixa do email.
  Isso descarta diferença exclusiva no fluxo embarcado como causa demonstrada;
  os dados de autenticação/estado da conta foram rejeitados pelo endpoint nos
  dois clientes. O erro não aponta falha de região, TLS ou alocação de memória.
- Removido o prefill temporário da UI/CMake e apagados o header e o diretório
  externo de build que continham os dados de teste. A busca em `firmware/main`
  não encontra mais referências ao prefill; o binário limpo também não contém
  as strings de usuário/senha. Build limpo IDF 5.5.4, target `esp32p4`, no
  diretório `firmware/build/ewelink-clean-20261003`: app `0x3462c0`, 59% livres,
  SHA-256 `1DE3CD19ABFBBCABBEAA23BCD30BF54421F5DDEE004B7BCE298791DCD5384276`,
  ELF SHA-256 `E9BA0A78DECF5D2A5DF794C9F56CA0A4988280CAE374D1DDDCB3237B32EC6E82`.
  App-only flash em COM8/offset `0x20000` confirmou `Hash of data verified`,
  preservando NVS. Boot final confirmou P4 v1.3, display, PSRAM 32 MiB,
  ESP-Hosted C6 3.0.6/RPC v2/SW_AGGR e Wi-Fi com IP; HTTPS passou nos domínios
  0, 1, 3 e 4. Domínio 2 ainda falhou em DNS. Nenhum login eWeLink bem-sucedido
  ou inventário foi confirmado nesta imagem limpa.

- Evidência posterior recebida em 2026-10-03: os arquivos recém-gerados pelo
  POC (`C:\\ewelink-poc\\ewelink_devices.json` e
  `C:\\ewelink-poc\\ewelink_devices_raw.json`) contêm três devices e três
  `devicekey` distintos, todos não vazios; `apikey` e `devicekey` são campos
  separados. Os modelos TX1C, TX2C e TX3C estão dentro dos modelos conhecidos
  pelo parser do firmware; TX2C/TX3C trazem 2/3 nomes de canal e TX1C não traz
  nomes customizados. Esta execução confirma login e fetch bem-sucedidos no POC
  depois da tentativa host anterior com `10001`; portanto, a conclusão anterior
  de que a conta continuava rejeitada pelo POC está supersedida. O login no
  firmware ainda não foi repetido com essa execução/estado mais recente.
  Revisão dos tamanhos de `params`: os três devices têm JSON entre 462 e 835 B,
  acima do buffer atual de 256 B. `parse_device_item()` deixa `params_json`
  vazio quando o JSON não cabe; precisa ampliar/refatorar a representação e
  validar o orçamento da persistência NVS antes de declarar a importação
  completa. Nenhum valor de chave foi exibido ou copiado para o repositório.

- Correção da integração eWeLink aplicada em 2026-10-03 (ADR-074): o username
  agora remove whitespace ASCII nas bordas, como `input(...).strip()` do POC;
  senha permanece byte a byte sem normalização. `params` foi ampliado a 1 KiB
  no inventário transitório e qualquer valor maior falha com
  `ESP_ERR_INVALID_SIZE`, em vez de ser descartado. O snapshot UI foi movido
  para PSRAM. O formato A/B persistente ficou compacto e mantém os campos LAN,
  devicekey e settings locais; não grava `params` nem credenciais/tokens.
- Build limpo ESP-IDF 5.5.4 / target `esp32p4`, diretório
  `firmware/build/ewelink-fix-20261003`: passou, sem erro; app `0x346670` bytes,
  `0x4b9990` bytes livres (59%) no menor slot OTA. SHA-256 do binário:
  `D62C64A157E8F68E0088F0355A052EF9712646E17230E34637B5D87229D07FC2`.
- App-only flash em COM8 escreveu somente `0x346670` bytes a partir de
  `0x20000`; esptool confirmou `Hash of data verified`. Boot serial confirmou
  P4 v1.3, PSRAM, display/UI, ESP-Hosted C6 3.0.6, RPC v2, SW_AGGR e Wi-Fi com
  IP. NVS, tabela, C6 e eFuses não foram gravados. Captura no monitor COM8 até
  cerca de 13,5 s; refresh HTTPS iniciou normalmente.
- A execução POC recém-anexada prova login/fetch cloud e fornece três devices,
  mas não contém credenciais. Ainda não houve sincronização cloud end-to-end
  desta imagem do firmware; a autenticação no dispositivo precisa ser acionada
  pela tela com as credenciais informadas pelo usuário, sem persistir os dados.

### Diagnóstico de memória no fetch eWeLink — 2026-10-03

- Log recebido da placa confirma que o firmware autenticou (`login succeeded
  region=us`) e validou o certificado TLS. O fetch subsequente falhou dentro do
  AES/TLS (`esp-aes: Failed to allocate memory`, `ESP_ERR_HTTP_FETCH_HEADER`).
  Na falha havia 68.987 bytes de SRAM interna livres, mas o maior bloco era
  27.648 bytes; PSRAM livre era 15.245.420 bytes. Portanto, PSRAM disponível
  não satisfaz a alocação interna contígua requerida por AES/TLS.
- ADR-075 ajusta somente o cliente HTTP eWeLink: buffers RX/TX menores
  (2 KiB/1 KiB) e estratégia RX estática do TLS dinâmico, reduzindo
  realocações durante o fetch. O build P4 limpo ESP-IDF 5.5.4 passou no
  diretório `firmware/build/ewelink-aes-fix-20261003`; app `0x346670` bytes,
  `0x4b9990` bytes livres (59%) no menor slot OTA. SHA-256 do app:
  `0F8D0972D3A3F68134B926CBD76CD4E10ED99EA8B4615689835FA415999A2564`.
- App-only flash em COM8 gravou `0x346670` bytes no offset `0x20000` e o
  esptool confirmou `Hash of data verified`. O boot serial confirmou P4 v1.3,
  PSRAM de 32 MiB, display/UI, C6 ESP-Hosted 3.0.6, RPC v2 e SDIO SW_AGGR.
  NVS, tabela de partições, firmware C6 e eFuses não foram alterados.
- Nesta captura o Wi-Fi não associou (`StaDisconnected`, motivos 2 e 205), por
  isso não foi possível repetir o fetch cloud e comprovar ainda o efeito do
  ajuste. A compilação e gravação estão confirmadas; a correção de alocação
  permanece aguardando nova sincronização acionada pela tela com rede ativa.

### Reteste AES-DMA e mbedTLS software — 2026-10-03

- Novo log da placa repetiu login e validação do certificado, seguido pela
  mesma falha `esp-aes: Failed to allocate memory` no fetch. O teste demonstrou
  que reduzir buffers HTTP e fixar RX TLS não resolvia. Inspeção do ESP-IDF
  5.5.4 localizou a falha na alocação do caminho AES-DMA, que requer
  `MALLOC_CAP_DMA`; as métricas anteriores de heap interno não indicavam o
  maior bloco disponível com essas capabilities.
- Corrigido o ADR-075: removida a hipótese de que a estratégia RX TLS resolveria
  o problema; os buffers HTTP menores são apenas limites do cliente. ADR-076
  desabilita `CONFIG_MBEDTLS_HARDWARE_AES` e mantém os alocadores TLS em SRAM
  interna, eliminando para mbedTLS a dependência de buffers AES-DMA. O impacto
  se aplica a todo AES/GCM usado via mbedTLS no P4 e exige observar CPU/latência
  e heap sob a carga do produto.
- Build limpo ESP-IDF 5.5.4 / `esp32p4`, com `sdkconfig` isolado no diretório
  `firmware/build/ewelink-software-aes-final-20261003`: passou; app `0x346ad0`
  bytes, `0x4b9530` bytes livres (59%) no menor slot OTA. Configuração efetiva
  confirma `# CONFIG_MBEDTLS_HARDWARE_AES is not set` e
  `CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=y`. SHA-256 do app:
  `6552042396C7CD5569FDF6302DCF009584B076562CBDD9FBC50419827E085301`; ELF:
  `8587A204F2CAD535B9129ACDB72D0588083FBC7825FD823459892C6D9F309442`.
- App-only flash em COM8 escreveu `0x346ad0` bytes em `0x20000`; esptool
  confirmou `Hash of data verified`. Boot serial confirmou P4 v1.3, PSRAM,
  display/UI, Wi-Fi com IP, C6 ESP-Hosted 3.0.6/RPC v2/SW_AGGR. HTTPS de
  produto passou nos domínios 0, 1, 3 e 4; domínio 2 teve falha DNS. NVS,
  tabela de partições, C6 e eFuses não foram alterados.
- Ainda falta acionar uma sincronização eWeLink na tela com esta imagem para
  confirmar fetch, parse e reconciliação. A checagem host indicada pela skill
  (`tools/scripts/host_check.sh --app`) não pôde ser executada: esse arquivo
  não existe neste checkout; build IDF e boot serial passaram.

### Correção de pilha ao persistir inventário eWeLink — 2026-10-03

- Novo log recebido confirmou `cloud inventory fetched count=3`, seguido de
  stack protection fault na task `np2_netcheck`. O mesmo log avisou que o ELF
  local não correspondia ao app em execução; a sessão COM8 também tinha quatro
  monitores ligados ao ELF antigo, então os endereços e linhas simbolizados
  naquele panic não eram evidência confiável do call stack.
- A revisão da submissão localizou um envelope `flash_request_t` com chunk OTA
  de 4 KiB criado na pilha durante a escrita do inventário. A correção usa o
  buffer compartilhado serializado do `FlashCoordinator` e consulta o resultado
  da gravação com um getter pequeno, sem copiar o status completo a cada poll.
  A sincronização agora também registra o high-water mark da pilha da task.
  ADR-077 registra a decisão.
- Build limpo ESP-IDF 5.5.4 / `esp32p4`, configuração isolada em
  `firmware/build/ewelink-stack-fix-20261003`: passou; app `0x346b90` bytes,
  `0x4b9470` bytes livres (59%) no menor slot OTA. O sdkconfig efetivo mantém
  AES mbedTLS por software. SHA-256 do app:
  `FCB78326B4737EE1F2B227F2C89D682C304383CD90D54B27AFFF0D7A70845C54`; ELF:
  `7678AF52EC0449AF51976E10DD1F28322CB60170732F7F5AA906CA282A98E050`.
- App-only flash em COM8 gravou `0x346b90` bytes no offset `0x20000`; esptool
  confirmou `Hash of data verified`. A captura usou o ELF desta build e não
  acusou divergência de checksum. Boot confirmou P4 v1.3, PSRAM de 32 MiB,
  display/UI, C6 3.0.6 com RPC v2/SW_AGGR, Wi-Fi com IP e HTTPS bem-sucedido
  para os domínios de clima e mercado. Um endpoint do domínio 2 falhou ao abrir
  conexão. NVS, tabela de partições, C6 e eFuses não foram alterados.
- A sincronização eWeLink ainda precisa ser acionada na tela para confirmar o
  fetch, a gravação sem panic e o valor medido da pilha; não foi alegado um
  teste cloud end-to-end desta imagem.

### Conteúdo de modais rolável e fechamento do X — 2026-10-03

- O usuário confirmou que a lista eWeLink exibiu os devices, mas o conteúdo
  ultrapassou a área útil; o botão não pôde ser alcançado e o fechamento pelo X
  terminou em crash. O componente comum `np_modal` agora limita geometria à
  tela e deixa a área de conteúdo rolável verticalmente, com barra automática.
  O resumo eWeLink pode crescer conforme as linhas; removi o texto auxiliar que
  ficava depois da lista e o fazia sobrepor quando ela aumentava. O callback do
  X roda em preprocess para ocultar/desassociar o teclado antes de apagar os
  campos eWeLink. ADR-078 registra a decisão.
- Build incremental P4/ESP-IDF 5.5.4 no diretório isolado
  `firmware/build/ewelink-stack-fix-20261003`: passou; app `0x346b30` bytes,
  `0x4b94d0` bytes livres (59%) no menor slot OTA. SHA-256 do app:
  `E76A300719FEF4266C0ABC0F3CCC8081F050B16A3502064C55119CB38C0920A3`; ELF:
  `3D26F984AF9EF7B4CF2FD36C862E9041E85AB0EF4FD2D24A984BF098EF645926`.
- App-only flash em COM8 no offset `0x20000`; esptool confirmou
  `Hash of data verified`. Boot capturado com o ELF correspondente confirmou
  P4 v1.3, PSRAM 32 MiB, display/UI, C6 3.0.6 com RPC v2/SW_AGGR, Wi-Fi com IP
  e HTTPS de clima/mercado. NVS, tabela, C6 e eFuses não foram alterados.
- Falta confirmação de interação HIL na tela: rolar a lista comprida até o fim
  e fechar o modal pelo X com/sem teclado ativo. O build e o boot não validam
  gestos, posicionamento final nem ausência de crash ao toque.

### Projeção do inventário eWeLink na tela de dispositivos — 2026-10-03

- A tela principal projetava somente câmeras ONVIF, embora os três devices
  estivessem no inventário local mostrado pela modal. O AppState agora publica
  uma projeção sem devicekey/apikey/params; a tela apresenta nome, modelo,
  canais e presença na última sincronização. Cards de eWeLink e câmeras ficam
  numa lista vertical com barra automática quando o conteúdo ultrapassa o
  viewport. ADR-079 documenta a fronteira e a decisão de memória.
- A primeira imagem com a nova projeção compilou, mas o boot revelou stack
  protection fault em `app_loop`. O candidato grande da projeção passou a usar
  armazenamento estático do único escritor AppState. A imagem corrigida iniciou
  sem novo panic durante a captura de aproximadamente 35 s; a tela de startup,
  C6 3.0.6/RPC v2/SW_AGGR, Wi-Fi/IP e HTTPS clima/mercado funcionaram. BCB teve
  falha DNS isolada (`ESP_ERR_HTTP_CONNECT`).
- Build incremental ESP-IDF 5.5.4 no perfil isolado
  `firmware/build/ewelink-stack-fix-20261003`: passou; app `0x3471f0` bytes,
  `0x4b8e10` bytes livres (59%) no menor slot OTA. SHA-256 do app:
  `06C8435BF9528ADFD14A742EBF7A95EADFEB61D77F60C2EEB5C2FEF685CA5157`; ELF:
  `A0B32BA2B7344B670A4668E785001D80A59F2CFF85682237BC25E6464AABEC0F`.
- Gravação somente da aplicação em COM8, offset `0x20000`; esptool confirmou
  `Hash of data verified`. NVS, tabela de partições, C6 e eFuses ficaram
  intactos. A lista e a rolagem precisam de confirmação tátil na tela; a
  captura serial não comprova que os cards ficaram visíveis nem a interação.

### Fluxo guiado de importação e sincronização — 2026-10-03

- A tela de dispositivos agora apresenta o estado vazio com ação de importação,
  seletor Sonoff/eWeLink ou câmera ONVIF, página de login eWeLink e página de
  progresso com autenticação, busca, reconciliação e gravação. A ação de
  continuar em segundo plano retorna à lista sem alegar que cancelou a
  requisição HTTPS. Texto de inventário foi ajustado para não prometer controle
  LAN ainda não conectado.
- Build incremental ESP-IDF 5.5.4 / `esp32p4`, perfil isolado
  `firmware/build/ewelink-stack-fix-20261003`: passou; app `0x347f10` bytes,
  `0x4b80f0` bytes livres (59%) no menor slot OTA. SHA-256 do app:
  `BBA7B6A0156E7EE6FFD4AF3513027E9CDA1CEE238BF5F3EA13D464EAE7073EBE`; ELF:
  `0C9927A271D1B4C86BC41A43DB429FE6CD7EB70EE62B956BC0F8FD8F2C6EF251`.
- App-only flash em COM8 no offset `0x20000`; esptool escreveu 3.440.400 bytes
  e confirmou `Hash of data verified`. Boot serial com o ELF correspondente
  mostrou display/touch inicializados, C6 3.0.6/RPC v2/SW_AGGR, Wi-Fi com IP,
  tela de startup e HTTPS de clima/mercado; não houve stack fault durante a
  captura de aproximadamente 35 s. Um domínio externo retornou falha de DNS,
  sem afetar o boot. NVS, tabela de partições, firmware C6 e eFuses não foram
  alterados.
- A validação tátil ainda precisa confirmar as quatro telas, rolagem em painel,
  seleção de Sonoff/câmera, fluxo de login, estados de progresso e retorno à
  lista. Não foi executado novo login/sync eWeLink nesta revisão.

### Correção do congelamento ao abrir Dispositivos — 2026-10-03

- O watchdog reproduzido ao abrir Dispositivos manteve a task `lvgl` dentro de
  `lv_obj_add_style()` → `theme_apply()` → criação do botão em
  `np_devices_build_with_header()`. O assert padrão do LVGL para falha de
  alocação entra em loop; a tela reservava cartões para 8 devices eWeLink
  mesmo com somente 3 no inventário atual. A construção agora cria somente os
  cartões presentes e acrescenta outros quando uma sincronização aumentar o
  inventário.
- Build limpo ESP-IDF 5.5.4, target `esp32p4`, perfil isolado
  `firmware/build/lvgl-device-clean2-20261003`; imagem `0x348010` bytes,
  `0x4b7ff0` bytes livres (59%) no menor slot OTA. `ESP_IDF_VERSION=5.5`
  foi informado ao ambiente para selecionar a Kconfig Wi-Fi remota usada pelo
  ESP-Hosted. SHA-256 P4:
  `C4771591A92E5E9FEB91DAF1F2763E82ADCF62ABBADDC0E3F9A15FAE7F9308A9`;
  SHA-256 ELF:
  `9674BC4ECF1C970409E5D4224F404454A292E6C8D3CFE0C9CFE9D6506FCF1EBA`.
- Gravada somente a aplicação em COM8, offset `0x20000`; esptool confirmou
  `Hash of data verified`. Boot serial chegou à tela inicial; Wi-Fi recebeu
  IP, C6 3.0.6/RPC v2/SW_AGGR e HTTPS de clima/mercado iniciaram. Uma chamada
  externa BCB teve falha DNS isolada. Ainda aguarda confirmação tátil de que a
  página Dispositivos abre sem WDT e continua responsiva. Nenhuma outra partição,
  firmware C6 ou eFuse foi escrita.

### Construção das subpáginas de Dispositivos sem sobreposição — 2026-10-03

- Novo log reproduziu o watchdog ao abrir Dispositivos, durante `lv_textarea_create()` no trabalho da task `lvgl`, após a fase de limpeza da página anterior. As telas de adição/login/sincronização eram montadas ainda visíveis e só ocultadas depois de criar todos os controles. Agora cada subpágina inicia oculta antes de receber seus filhos; modais comuns também ocultam o scrim imediatamente ao criar a raiz, antes de construir o painel/conteúdo. Isto reduz trabalho de layout/invalidação enquanto a árvore está sendo montada. A tela Dispositivos ainda precisa de confirmação tátil após esta gravação.
- Build limpo ESP-IDF 5.5.4, `esp32p4`, pasta `firmware/build/lvgl-device-fix-20261003`: passou; app `0x347c30` bytes, `0x4b83d0` bytes livres (59%) no menor slot OTA. SHA-256 app: `089B38B8F238DCCCDDDED41CCF79B0AE57F9067E2B3766408C64E3D6220705E6`; ELF: `795914496BFD1CEBB954C7EAFAE736190310D42227CB5E1E4A6508F39660A97B`. Revisão fonte: `cf335d9f9c20773f0e3b599d22c85290de83f247` mais alterações locais.
- Gravados bootloader, app, tabela de partições e `otadata` inicial em COM8; esptool confirmou hashes. Boot observado em P4 v1.3, PSRAM 32 MiB, display/touch inicializados, C6 3.0.6 com RPC v2/SW_AGGR, app_main retornou e Wi-Fi iniciou tentativa de associação. O monitor foi encerrado depois da captura para liberar COM8. Não houve interação tátil com a tela Dispositivos nesta rodada; ausência de novo WDT nesse fluxo permanece pendente de confirmação do operador. NVS, partição storage, firmware C6 e eFuses não foram gravados.

### Expansão do pool LVGL para a tela Dispositivos — 2026-10-03

- O operador reproduziu o WDT após `CLEAN_WAIT_NEXT_PASS`, sem o evento `BUILD` da navegação. A task `lvgl` permaneceu no mesmo PC de `rgb888_image_blend` por dois períodos de watchdog. O pool LVGL tinha cerca de 40 KiB livres após limpar a página anterior; a falha ocorre durante a construção da árvore da tela.
- O primeiro build limpo em `firmware/build/lvgl-device-pool-20261003` passou, mas a primeira gravação em COM8 expôs um erro de configuração no boot: `LVGL could not register PSRAM pool` e `P4 local bring-up stopped: ESP_ERR_NO_MEM`. O TLSF de LVGL tinha `CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES=0`, limitando cada pool a 64 KiB. Essa imagem foi substituída imediatamente.
- ADR-082 fixa um pool adicional de 128 KiB em PSRAM e `CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES=128`. O build limpo ESP-IDF 5.5.4 / `esp32p4` em `firmware/build/lvgl-device-pool-v2-20261003` passou com a configuração efetiva 128, app `0x347df0` bytes e `0x4b8210` bytes livres no menor slot OTA. SHA-256 do app: `59FE9F59A5F5B3B02AEDE72A7EF52E5E391D0912626CD5340C8CCBD634FB82BC`; ELF: `B918C0AFC96FD99A04A4ECE96C8183F6DBADBF450695F6A3B7188EBD83B8EFE1`. Base Git `cf335d9f9c20773f0e3b599d22c85290de83f247` mais alterações locais preservadas.
- Gravados bootloader, app, tabela de partições e `otadata` inicial pela COM8; esptool verificou os hashes. Boot no P4 v1.3 / PSRAM 32 MiB confirmou `LVGL object pools total=194596 free=188168 largest=131064 PSRAM_added=131072`, display RGB565 / rotação 180° / triple partial / 3 framebuffers, touch GT911 e `P4 local bring-up ready`. C6 iniciou com firmware 3.0.6, RPC v2 e SW_AGGR. O hash da imagem C6 instalada não foi obtido nesta rodada. NVS, partição de storage, firmware C6 e eFuses não foram gravados. Monitor encerrado para liberar COM8.
- Após esta gravação, o operador confirmou que a tela Dispositivos está funcionando. Isto valida a entrada tátil e a resposta visual no uso observado, sem recorrência do congelamento reportado. Não foi fornecido log serial durante a navegação; portanto, a ausência de WDT é confirmada pelo operador, não por captura de monitor. A validação de outras telas e de uma sessão prolongada permanece fora desta observação.

### Controles de canal Sonoff pela LAN — 2026-10-03

- Os cards eWeLink agora recebem estado LAN e exibem switches por canal. A
  interação é postada pelo event bus e atendida pela task `sonoff_lan`; ela usa
  mDNS para achar o IP e a `devicekey` local para cifrar comandos
  `/zeroconf/switch` ou `/zeroconf/switches`. A UI não recebe nem registra a
  chave. Switches permanecem desabilitados enquanto não há estado conhecido.
  Detalhes da decisão em ADR-083.
- Build limpo ESP-IDF 5.5.4 / target `esp32p4`, perfil isolado
  `firmware/build/sonoff-lan-control-20261003`: passou sem warnings após
  recompilação incremental final. Configuração efetiva inclui
  `CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES=128`. App: `0x34a0e0` bytes, com
  `0x4b5f20` bytes livres (59%) na menor partição OTA. SHA-256 app:
  `B9B23BC28F73F3C0A519CB660D4873EFDCA7645F900C71EF6613E85DDCEEA84A`;
  SHA-256 ELF:
  `BCF857B7ED2A400D5FAA98E283C138A34C82554B21823830D76967585A853AC0`.
- Gravada somente a aplicação em COM8, offset `0x20000`; esptool reportou
  `Hash of data verified`. Captura serial curta avançou até ~70 s de uptime e
  mostrou `sonoff_lan: LAN discovery result=ESP_OK devices=3 imported=3`, sem
  panic/WDT nesse intervalo. A mesma captura também registrou falhas de
  alocação TLS de 4.437 bytes e falhas HTTPS de provedores externos; esse
  problema de pressão/fragmentação da SRAM interna requer acompanhamento, não
  foi atribuído ao comando LAN. Monitor/captura fechou COM8 após 25 s. NVS,
  tabela de partições, C6 e eFuses não foram escritos nesta operação.
- Confirmação tátil pendente: verificar que os três estados aparecem na tela,
  alternar um relé de teste e confirmar retorno visual do estado. O boot e a
  descoberta mDNS não comprovam que o endpoint/cifragem são compatíveis com
  cada modelo.

### Contraste visual dos cartões de dispositivos — 2026-10-03

- O operador informou que os dados apareciam como textos sem cartões visíveis.
  A causa era o fundo dos cartões eWeLink igual ao da lista, sem borda; os
  cartões de câmeras também não tinham contorno. Ambos agora usam fundo
  elevado e borda `np_c_hairline` de 1 px.
- Build incremental ESP-IDF 5.5.4 / `esp32p4` no perfil
  `firmware/build/sonoff-lan-control-20261003`: passou sem warnings; app
  `0x34a160` bytes, `0x4b5ea0` bytes livres (59%) no menor slot OTA. SHA-256
  app `6D2C37EA7DAC9C21D665585137CFB58DF96819E9129CBF2A00F508BCA3B379BD`;
  ELF `EDD39BFFEC16849CAA0CE0A1A1F588773627B9690840ED0C3A3891B1571695EA`.
- Gravada somente a aplicação em COM8, offset `0x20000`; esptool confirmou
  `Hash of data verified`. Captura serial chegou a `Boot transition complete;
  startup page visible` sem panic/WDT durante os 25 s de captura. As falhas de
  alocação TLS/HTTPS descritas na seção anterior voltaram a ocorrer. COM8 foi
  fechada ao terminar; partições, NVS, C6 e eFuses não foram escritos.
- Falta confirmação visual tátil de que a borda/fundo dos cartões agora
  aparecem com contraste correto.

### Subcards de canal clicáveis — 2026-10-03

- Cada canal agora é um card interno ao dispositivo e o próprio card alterna
  ligado/desligado ao toque; não há switch separado. O estado ligado usa fundo
  e contorno de destaque. O card mostra “Enviando...” e fica desabilitado
  durante a solicitação. Estados desconhecidos/offline também não aceitam
  comandos.
- Build incremental ESP-IDF 5.5.4 / `esp32p4`, perfil
  `firmware/build/sonoff-lan-control-20261003`: passou sem warnings; app
  `0x34a460` bytes, `0x4b5ba0` bytes livres (59%) no menor slot OTA. SHA-256
  app `F0AB1E59E31429FDC7EBBE0B74504C67DFDEEC27992DE45714953CEA6B2727EC`;
  ELF `EF64406EF7F872A9C347559274DA0557FD92D9E30EAC9FB7E9B30D9DE1EC1E5A`.
- Gravada somente a aplicação em COM8, offset `0x20000`; esptool confirmou
  `Hash of data verified`. Boot alcançou a tela inicial, associou ao Wi-Fi e
  recebeu IP `192.168.1.16`; sem panic/WDT durante a captura serial curta de
  25 s. COM8 fechada automaticamente; NVS, tabela, C6 e eFuses não foram
  escritos.
- Ainda é necessária a confirmação tátil do desenho final e de que tocar um
  subcard alterna o relé físico e atualiza seu estado.

### Buffer RX TLS de 8 KiB — build, flash e captura inicial — 2026-10-03

- Build limpo ESP-IDF 5.5.4, target explícito `esp32p4`, no perfil
  `firmware/build/tls-rx-buffer-20261003`: passou. Configuração efetiva:
  `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=8192` e
  `CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES=128`. App `0x34a460` bytes;
  `0x4b5ba0` bytes livres na menor partição de 8 MiB (59%). SHA-256 app
  `F60CB5FB6E600F13D9539E3B251EDFB45CF8D07747F1046DAA622D771971F5EC`;
  ELF `8B1F1C68A3784F9958CDF583526078175D415131FFF842AC6EAA2C7FB1C659EA`.
- A primeira tentativa de `app-flash` foi bloqueada por COM8; foram
  identificados e encerrados somente quatro processos `idf_monitor.py` que
  apontavam para COM8, conforme pedido anterior do operador. A segunda tentativa
  gravou somente o app em `0x20000`; esptool confirmou `Hash of data verified`
  e hard reset. Nenhuma partição, NVS, C6 ou eFuse foi escrita.
- Captura serial por 75 s, com COM8 fechada automaticamente ao final: o domínio
  inicial de produto concluiu DNS, NTP e HTTPS com `ESP_OK`; não apareceu a
  alocação RX de 17.058 bytes nem watchdog/panic. A descoberta LAN continuou
  retornando `devices=3 imported=3`. Esta janela validou apenas o domínio 0; a
  sincronização sob demanda eWeLink e os demais domínios HTTPS ainda precisam
  de validação específica. O log bruto ficou no diretório temporário do sistema,
  fora do repositório.

### Simplificação visual dos cartões de dispositivos — 2026-10-03

- Os cards eWeLink ficaram menores; o modelo aparece ao lado do nome, sem a
  contagem de canais. A disponibilidade LAN aparece somente como ponto verde
  ou vermelho alinhado ao nome no canto direito. Em aparelhos de um canal, o
  card do aparelho é o controle; em aparelhos multicanal, os subcards menores
  indicam o estado pela cor, sem texto de estado; os títulos podem ocupar duas
  linhas dentro da altura do subcard.
- Build limpa ESP-IDF 5.5.4 / alvo `esp32p4` em
  `firmware/build/device-layout-20261003`: passou; após corrigir a visibilidade
  do controle único, a build incremental final também passou. Configuração
  efetiva: PSRAM habilitada, pool LVGL extra de 128 KiB, Hosted ativo e três
  buffers RGB565. App final `0x34ae30` bytes; `0x4b51d0` bytes livres na menor
  partição OTA. SHA-256 app
  `A021439FE939053F05ECA54F1215B664CDAD714C09C1A989934D2A9C3B651D59`;
  ELF `9DE5F4245E94F5CE87C4294A312A70F781EE1AB20B621D46067E1FC292A47E8C`.
  Fonte baseada em `cf335d9f9c20773f0e3b599d22c85290de83f247` mais alterações
  locais.
- Gravada somente a aplicação P4 em COM8, offset `0x20000`; esptool confirmou
  `Hash of data verified`. NVS, tabela de partições, firmware C6 e eFuses não
  foram escritos. Após a gravação inicial, a COM8 ficou temporariamente ocupada
  por duas capturas externas; a versão final foi gravada depois que elas
  terminaram, novamente com hash confirmado. Captura serial de aproximadamente
  25 s da imagem final confirmou P4 v1.3,
  32 MiB de PSRAM, display RGB565/rotação 180°/triple-partial/3 FB, tela inicial,
  Hosted 3.0.6 com RPC v2 e SW_AGGR, Wi-Fi com IP e retorno de `app_main`, sem
  panic/WDT observado na janela. Um refresh HTTPS do Banco Central falhou por
  resolução do hostname; os domínios adjacentes concluíram HTTPS com sucesso.
- A aparência e a alternância tátil dos novos cards ainda precisam de
  confirmação visual na tela Dispositivos; a captura serial não prova esses
  detalhes de UX.

### Aumento da altura dos subcards de canal — 2026-10-03

- A altura dos subcards internos passou de 48 para 60 px; o card externo e a
  quebra dos títulos em até duas linhas foram mantidos.
- Build incremental ESP-IDF 5.5.4 / `esp32p4` no perfil
  `firmware/build/device-layout-20261003`: passou; app `0x34ae50` bytes, com
  `0x4b51b0` bytes livres na menor partição OTA. SHA-256 app
  `74C11E015CB7AD97739E8B5F40D3A7367543DBF18FEE57CA8C1E8855F6CBC582`.
- Gravada somente a aplicação P4 em COM8, offset `0x20000`; esptool confirmou
  `Hash of data verified`. Captura serial confirmou P4 v1.3, display/touch,
  PSRAM 32 MiB, C6/ESP-Hosted 3.0.6 com RPC v2 e SW_AGGR, Wi-Fi com IP e
  transição para a tela inicial; sem panic/WDT observado na janela. NVS,
  tabela de partições, C6 e eFuses não foram escritos.

### Padding inferior nos cards de dispositivo — 2026-10-03

- Aumentei a altura do card externo de 120 para 128 px. Com o subcard de 60 px
  começando em y=56, agora há 12 px até a borda inferior, igual à margem
  superior do conteúdo. O passo vertical continua em 132 px.
- Build incremental ESP-IDF 5.5.4 / `esp32p4` em
  `firmware/build/device-layout-20261003`: passou; app `0x34ae50` bytes,
  `0x4b51b0` bytes livres na menor partição OTA. SHA-256 app
  `B16054EA95549663854518DD830958F8131D410ECAE40EC652047C6313B4EBF8`.
- Gravada somente a aplicação em COM8, offset `0x20000`; esptool confirmou
  `Hash of data verified`. Captura serial confirmou P4 v1.3, display/touch,
  PSRAM 32 MiB, C6/ESP-Hosted 3.0.6 com RPC v2 e SW_AGGR, Wi-Fi com IP e
  transição para a tela inicial. NVS, tabela de partições, C6 e eFuses não
  foram escritos.

### Estado Sonoff LAN e comando criptografado — 2026-10-03

- Placa ESP32-P4 v1.3 com flash/PSRAM de 32 MiB; base Git
  `cf335d9f9c20773f0e3b599d22c85290de83f247` mais alterações locais.
  Build P4 ESP-IDF 5.5.4 no perfil limpo
  `firmware/build/sonoff-lan-iv-20261003`, seguido de builds incrementais;
  target efetivo `esp32p4`, `CONFIG_SPIRAM=1` e RX TLS de 8 KiB.
- A primeira imagem aumentou os buffers mDNS e corrigiu o IV AES, mas sofreu
  `Stack protection fault` na task `sonoff_lan` de 8 KiB. O binário anterior
  foi restaurado diretamente por esptool e teve hash verificado. O inventário
  público e o próximo snapshot da varredura foram movidos para uma área
  reutilizável em PSRAM; chaves e buffers criptográficos permaneceram internos.
- Imagem final: app `0x34ae50` bytes; `0x4b51b0` bytes livres (59%) no menor
  slot OTA. SHA-256 app
  `718696829AB1DD44F393841F186D04AF85F767160077E4B6477092DE908FF877`;
  ELF `41DD772713319B404801784BDA2CC76B2C7919A54FFFDFBB800CB85D26AA9D09`.
  Gravado somente o app P4 em COM8, offset `0x20000`, com `Hash of data
  verified` e hard reset. NVS, tabela de partições, C6 e eFuses não foram
  escritos; hash da imagem C6 em execução não foi obtido nesta sessão.
- Três varreduras da imagem final, até 103 s de uptime, retornaram
  `devices=3 imported=3 online=3 stateful=3 state_queries=0`,
  `max_mdns_b64=664` e margem mínima de pilha de 1.956 bytes. O contador
  bruto `channels_known=7 channels_expected=6` inclui uma posição extra
  anunciada: o POC mostra quatro entradas `switches` no TX2C, apesar de seus
  dois canais físicos. A contagem ainda não comprova cada canal individual.
- Houve um aviso de task watchdog em `np2_netcheck` / IDLE1 após a primeira
  varredura; o serviço LAN continuou a publicar varreduras. A causa desse
  aviso não foi atribuída ao Sonoff LAN. O acionamento físico e o retorno
  visual dos cards aguardam confirmação com toques na placa.

### Inclusão ONVIF da Tapo C200 e perfil de vídeo — 2026-10-03

- Na imagem anterior, o PC alcançou `192.168.1.8:2020` e `:554`. O operador
  informou o IP no painel e confirmou que a câmera apareceu na lista. O P4
  registrou `cameras=1 datagrams=0 direct_endpoints_up=1` e margem de pilha
  ONVIF de 3.644 bytes. A inclusão manual usa TCP porque não houve resposta
  multicast nesta rede.
- Ao iniciar vídeo com a conta local da câmera, `GetCapabilities` validou o
  endpoint Media. A consulta de opções retornou HTTP 200, mas foi classificada
  incorretamente como `unclassified` porque o parser exigia a string do tipo
  XML `H264Options`. RTSP autenticou por Digest: `/stream2` respondeu
  `OPTIONS 200`, `DESCRIBE 401/200`, `SETUP 200` e `PLAY 200`. O SPS recebido
  tinha `profile_idc=77`, restrições `0x00` e nível 31 (H.264 Main), rejeitado
  pelo decodificador atual. O operador confirmou a mensagem “Perfil
  incompatível”. Isto separa descoberta/autenticação bem-sucedidas da falha
  de decodificação.
- Corrigida a leitura read-only das opções para procurar o elemento
  `H264ProfilesSupported` definido pelo schema ONVIF e registrar apenas o
  tamanho do corpo quando a interpretação falhar. Build limpo ESP-IDF 5.5.4
  para P4 em `firmware/build/onvif-options-20261003`: passou. App `0x34aa80`
  bytes, `0x4b5580` bytes livres (59%) na menor partição OTA. SHA-256 app
  `8E02E09A2422B53156FBA40634A177B7CD2BA5A5576883F6CAD16FCAF3B08ADF`,
  ELF `6592DA74FC5FCCC44D0EF3CE63CDCCC74216689905C89D83303BADC6A0E199B6`,
  `sdkconfig` `0BE2019E74EA3B296BF7C2929C4BEDBF2EED573D5FF7BB2ABB1BB26DA3C52535`.
  Target efetivo `esp32p4`, PSRAM ativa, RX TLS 8 KiB e flash auto suspend
  desabilitado. Base Git `cf335d9f9c20773f0e3b599d22c85290de83f247` mais
  alterações locais.
- `idf.py -DIDF_TARGET=esp32p4 -B build/onvif-options-20261003 -p COM8
  app-flash` gravou somente a aplicação em `0x20000` e verificou o hash.
  Em nova captura após o operador iniciar o vídeo no painel, a consulta ONVIF
  retornou `H264Options ProfilesSupported=Main`. O `/stream8` respondeu
  `DESCRIBE 200`, mas não chegou a `SETUP`; o `/stream2` respondeu `SETUP 200`
  e `PLAY 200`, seguido de SPS `profile_idc=77`, restrições `0x00` e nível 31.
  O decodificador de software atual aceita somente Constrained Baseline e
  rejeitou o stream. Portanto, o problema não é a autenticação, o IP nem a
  sessão RTSP: esta C200 oferece somente Main nas opções ONVIF consultadas.
  A COM8 foi liberada após a captura; não houve nova gravação nesta medição.
  O IP manual da câmera está apenas em RAM nesta implementação e precisa ser
  informado novamente; isso permanece uma lacuna de persistência do inventário
  ONVIF. NVS, tabela, C6 e eFuses não foram escritos nesta gravação.

### Ícone de interruptor nos dispositivos eWeLink — 2026-10-03

- O ícone de roteador no card eWeLink foi substituído por um interruptor
  desenhado com formas LVGL; os demais usos do ícone de rede foram mantidos.
- Build incremental ESP-IDF 5.5.4 / `esp32p4` em
  `firmware/build/device-layout-20261003`: passou; app `0x34aed0` bytes,
  `0x4b5130` bytes livres na menor partição OTA. SHA-256 app
  `BB361101A20A98B3D4BE370075522D5BBC8A0FBB0C7E7D6B86C3DDD446379340`.
- Gravada somente a aplicação P4 em COM8, offset `0x20000`; esptool confirmou
  `Hash of data verified`. Captura serial confirmou P4 v1.3, display/touch,
  PSRAM 32 MiB, C6/ESP-Hosted 3.0.6 com RPC v2 e SW_AGGR, Wi-Fi com IP e
  transição para a tela inicial. A descoberta LAN publicou três dispositivos
  online. NVS, tabela de partições, C6 e eFuses não foram escritos.

### Ícone de lâmpada por canal — 2026-10-03

- Removido o ícone do cabeçalho junto ao nome do dispositivo. Cada subcard de
  canal agora identifica luz com uma lâmpada; para dispositivo de canal único,
  sem subcard conforme o layout, o símbolo aparece no próprio card sem ocupar
  a área do nome. O modelo deixa espaço horizontal para o ícone do canal.
- Build ESP-IDF 5.5.4 / `esp32p4` no perfil
  `firmware/build/ewelink-ui-20261003`: passou; app `0x34ac00` bytes,
  `0x4b5400` bytes livres na menor partição OTA. SHA-256 app
  `EA788A86C3340F4AD42E296B9A909CC34B7EA508BE621C5FF704D175CEE45402`.
- `app-flash` gravou somente a aplicação em COM8, offset `0x20000`; esptool
  confirmou `Hash of data verified` e hard reset. Boot serial confirmou P4 v1.3,
  display/touch, PSRAM 32 MiB, Hosted C6 3.0.6 com RPC v2 e SW_AGGR, Wi-Fi com
  IP e tela inicial visível; sem panic/WDT observado em aproximadamente 18 s.
  Um check HTTPS falhou por resolução de `api.bcb.gov.br`; outros checks
  HTTPS concluíram. NVS, tabela de partições, C6 e eFuses não foram escritos.

### Refinamento visual dos cards de dispositivo — 2026-10-04

- O título do dispositivo não concatena mais o modelo (ex.: TX1C). O indicador
  de rede segue como ponto à direita. Canais continuam sem texto de estado;
  o ícone de lâmpada agora tem estados visuais: amarelo ligado e cinza
  desligado.
- O canal único usa diretamente o card do dispositivo, sem subcard, e retorna
  à superfície natural do card quando desligado. O estado LVGL `DISABLED` foi
  removido dos cards de canal; a autorização do toque continua protegida pelo
  callback de controle do canal. Os subcards aumentaram de 60 para 76 px, o
  card externo de 128 para 144 px e o passo de linha para 150 px, mantendo
  12 px livres abaixo do subcard. Nomes de canal podem quebrar em mais linhas
  para caber nessa altura.
- Build ESP-IDF 5.5.4, target `esp32p4`, passou. `np2_p4.bin`: 0x34abf0 bytes,
  com 0x4b5410 bytes livres na menor partição OTA; SHA-256
  `3754A8450321499774410D58F88CF770256ED836DDB13AC70D1A6516775E4555`.
  ELF SHA-256 `2A0912CB19004922916AF98B4A5F566A747E17A91BB9AC3C9BE670C7F73AEE4E`.
  Base Git `cf335d9f9c20773f0e3b599d22c85290de83f247` mais alterações locais.
- `idf.py -p COM8 -b 460800 app-flash monitor` gravou somente a aplicação no
  offset `0x20000`; esptool confirmou `Hash of data verified`. Boot serial
  confirmou P4 v1.3, PSRAM de 32 MiB, display/touch inicializados, LVGL em
  RGB565/rotação 180°/triple-partial/3 FBs e Hosted C6 3.0.6 com RPC v2 e
  SW_AGGR. A aplicação chegou a `P4 local bring-up ready`, sem panic/WDT na
  captura. Wi-Fi registrou desconexão com reason=2 e retry agendado; esse evento
  não foi atribuído a esta mudança de UI. Não houve confirmação tátil/visual
  dos estados dos cards nesta captura. NVS, tabela, C6 e eFuses não foram
  escritos; COM8 foi liberada após a captura.

### Comando de canal sem toques repetidos — 2026-10-04

- Removido `LV_STATE_DISABLED` permanente após publicar um comando. O callback
  considera o alvo corrente do evento, bloqueia outros canais enquanto há uma
  solicitação em voo e mantém o card pendente até a projeção refletir o novo
  estado ou o serviço completar/falhar. Durante a transição, o card mostra
  `Ligando...` ou `Desligando...`; em falha, a projeção restaura o estado
  observado e libera nova tentativa.
- Build incremental ESP-IDF 5.5.4, target `esp32p4`, passou; app `0x34b090`
  bytes, com `0x4b4f70` bytes livres na menor partição OTA. SHA-256 do app
  `EB029EDED8F9126589F8371A7D655FDCD174CFDACB456E76F9501153D9FFD521`, ELF
  `AC4002369FBF6DBBE2924134A98DF32C94B1FF4A52D465BF8023F162991150F5` e
  `sdkconfig` `0BE2019E74EA3B296BF7C2929C4BEDBF2EED573D5FF7BB2ABB1BB26DA3C52535`.
  Base Git `cf335d9f9c20773f0e3b599d22c85290de83f247` mais alterações locais.
- `idf.py -p COM8 -b 460800 app-flash monitor` gravou somente o app no offset
  `0x20000`; esptool confirmou `Hash of data verified`. Boot confirmou P4 v1.3,
  PSRAM 32 MiB, display/touch inicializados, Hosted C6 3.0.6 com RPC v2/SW_AGGR,
  Wi-Fi com IP e `P4 local bring-up ready`, sem panic/WDT na captura. Houve uma
  primeira tentativa de montagem do microSD com timeout, seguida de montagem
  bem-sucedida; um domínio HTTPS reportou falha de DNS. Não houve confirmação
  tátil do comando de ligar/desligar nesta captura. NVS, tabela, C6 e eFuses não
  foram escritos; COM8 foi liberada.

### Faixa de status interna e espaçamento entre cards — 2026-10-04

- A mensagem de ação agora ocupa uma faixa própria na parte inferior do card
  de canal; o nome do canal e o nome do dispositivo permanecem visíveis. O
  card principal ficou com 156 px e o canal com 88 px de altura, conservando
  12 px inferiores. O passo vertical principal passou a 168 px (card + 12 px),
  igual ao intervalo horizontal. Cards de câmera usam passo vertical de 80 px
  para também manter 12 px de separação.
- Build incremental ESP-IDF 5.5.4, target `esp32p4`, passou; app `0x34b090`
  bytes, com `0x4b4f70` bytes livres na menor partição OTA. SHA-256 do app
  `53511AE24DC04EF9B977FE07A8EE5C121490A26F7EB3A680A6FA0A918ED26FA6`, ELF
  `3976C754E34DA2EAA1510CCFFAC504431DDD30F83558FAB15FB07EFA99E99AA9`.
- `idf.py -p COM8 -b 460800 app-flash monitor` gravou somente o app no offset
  `0x20000`; esptool confirmou `Hash of data verified`. Boot confirmou P4 v1.3,
  PSRAM 32 MiB, display/touch, Hosted C6 3.0.6 com RPC v2/SW_AGGR, Wi-Fi com IP
  e `P4 local bring-up ready`, sem panic/WDT. Nesta inicialização, as duas
  tentativas de montar o microSD falharam por timeout e o sistema usou o ícone
  estático de clima. O toque nos cards ainda não foi verificado nesta captura.
  NVS, tabela, C6 e eFuses não foram escritos; COM8 foi liberada.

### Cards e canais no layout de referência — 2026-10-04

- Os cards eWeLink agora ocupam uma, duas ou três colunas conforme a quantidade
  de canais; cards com um canal também mostram o subcard interno. Cada canal
  tem ícone circular de lâmpada, com lâmpada clara ligada e azul-cinza
  desligada. O card externo preserva o nome e indicador de rede. O resumo
  superior mostra dispositivos totais e online com ponto colorido; os estilos
  seguem as superfícies e cores dos componentes NP.
- Build incremental ESP-IDF 5.5.4, target `esp32p4`, passou. App `0x34ae90`
  bytes, com `0x4b5170` bytes livres na menor partição OTA. SHA-256 do app
  `ED2AADF20B21E32FACD7BAFFAD63FDAD3F4BD7CB1EAAE24B987E159991721598`, ELF
  `EDE4536925072E7DC00127BCB953AF675D38FCA2396B0B8B76396C4D4A4FD199`,
  `sdkconfig` `0BE2019E74EA3B296BF7C2929C4BEDBF2EED573D5FF7BB2ABB1BB26DA3C52535`.
- `idf.py -p COM8 -b 460800 app-flash monitor` gravou somente a aplicação em
  `0x20000`; esptool confirmou `Hash of data verified`. Boot em P4 v1.3 chegou
  a `P4 local bring-up ready`, com display/touch e Hosted C6 3.0.6, RPC v2 e
  SW_AGGR; sem panic/WDT. As duas tentativas de montagem do microSD expiraram;
  o Wi-Fi iniciou associação e depois registrou desconexões (reason 2/205).
  A disposição visual e o toque dos canais não foram verificados fisicamente.
  NVS, tabela, C6 e eFuses não foram gravados; COM8 foi liberada.

### Ícone Material dos canais — 2026-10-04

- Ícone de lâmpada trocado do desenho com formas LVGL para o glifo Material
  `lightbulb` U+E0F0 incluído na fonte Material 24 do produto. Desligado usa
  `np_c_text_3()` (cinza terciário mais apagado); ligado usa branco `np_c_text()`.
- Build incremental ESP-IDF 5.5.4, target `esp32p4`, passou. App `0x34ae10`
  bytes, `0x4b51f0` bytes livres na menor partição OTA. SHA-256 do app
  `74A41874C9351FFAF88A7FBAD3C6CDE81DBB2AD09F9960F616BC1A8C56B10FD9`, ELF
  `61EA749FC57AE7CC40FB1CAB690A3E95A3748546749758D411F1312DC9060525`,
  `sdkconfig` `0BE2019E74EA3B296BF7C2929C4BEDBF2EED573D5FF7BB2ABB1BB26DA3C52535`.
- `idf.py -p COM8 -b 460800 app-flash monitor` gravou somente a aplicação em
  `0x20000`; esptool confirmou `Hash of data verified`. Boot em P4 v1.3
  reconheceu PSRAM 32 MiB, display/touch e C6 3.0.6 com RPC v2/SW_AGGR; chegou
  a `P4 local bring-up ready`, depois obteve IP e concluiu DNS, NTP e HTTPS, sem
  panic/WDT. microSD ausente/timeout durante as duas tentativas de montagem.
  Render específico do novo glifo não foi conferido visualmente. NVS, tabela,
  C6 e eFuses não foram gravados; COM8 foi liberada.

### Diagnóstico dos cards amarelos e configuração TLS efetiva — 2026-10-04

- Unidade em uso: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash NOR
  32 MiB e PSRAM 32 MiB; base Git `cf335d9f9c20773f0e3b599d22c85290de83f247`
  mais alterações locais preservadas. Hash do firmware C6 instalado não foi
  obtido; C6 não foi atualizado. Captura passiva antes de gravar confirmou
  consultas BTC ainda sendo tentadas após mais de cinco horas, mas com timeout
  de conexão. O executor não estava parado. Descoberta LAN manteve três
  dispositivos com estado. A captura não gravou credenciais/headers/bodies.
- O sdkconfig do último build acima, hash `0BE2019E...C52535`, ainda tinha
  `CONFIG_MBEDTLS_HARDWARE_AES=y`. Header gerado e map do ELF confirmam AES/GCM
  de hardware, embora o default/ADR-076 tenham escolhido software. O CMake
  agora recusa essa configuração. Configure negativo real em
  `build/data-refresh-aes-negative-20261004`, usando cópia da configuração
  antiga, falhou no gate esperado; nenhuma imagem negativa foi gravada.
- Build P4 limpo, ESP-IDF 5.5.4, comando
  `idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=build/data-refresh-diag-20261004/sdkconfig -B build/data-refresh-diag-20261004 build`.
  A primeira imagem instrumentada registrou sucesso de todos os provedores no
  boot, mas depois reproduziu falha BTC no TLS (`0x8006`) e no recebimento de
  headers. Portanto, corrigir a divergência AES não resolve sozinho o sintoma.
  O operador confirmou somente Bitcoin amarelo. O snapshot foi aceito e a
  projeção marcou BTC stale; não há evidência de UI congelada nesse caso.
- A imagem final acrescenta uma sonda HEAD de controle, limitada ao orçamento
  remanescente e cooldown de dez minutos, conforme ADR-087. Build passou sem
  warnings de compilação. App `0x34ba80`, 59% livres no slot de 8 MiB; SHA-256
  app `B730DE7E19B0F1F1FC9C39A437FEC0BB74FC6A3C1F432F52E2920D6C04EFF806`,
  ELF `9AF7D23FE67E6EC513109F5D4CC912EA4246C9DDBCA82A72345600597CC09A82`,
  sdkconfig `2524F49D41654D1CBA9137177B4E979726865B622A43965DBBF71C2C4DEF67B8`,
  tabela `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
  AES por software, TLS interno/RX 8 KiB, PSRAM ativa, pool LVGL extra 128 KiB,
  três framebuffers e flash auto-suspend desligado. Não houve alteração de
  defaults, dependências, pins ou tabela de partições nesta investigação.
- `idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=build/data-refresh-diag-20261004/sdkconfig -B build/data-refresh-diag-20261004 -p COM8 -b 460800 app-flash`
  gravou somente a aplicação em `0x20000`, com hash verificado. Boot confirmou
  ELF `9af7d23fe...`, P4 v1.3, PSRAM 32 MiB, RPC v2/SW_AGGR e bring-up pronto.
  NVS, storage, tabela, firmware C6 e eFuses não foram escritos pelo flash.
- A captura final está registrada no [diagnóstico de refresh](DATA-REFRESH-DIAGNOSIS.md).
  Os logs mostram heap interno de apenas 44–55 KiB antes de alguns HTTPS e
  23–32 KiB com cliente ativo: abaixo do piso do plano. Este reteste não fecha
  margem de memória, recovery ou estabilidade por várias horas. Logs brutos
  filtrados ficaram no diretório temporário, fora do Git.

### EventBus compacto e reenvio de dados — 2026-10-04, 18h23–18h29 BRT

- Mesma Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, flash/PSRAM 32 MiB;
  base `cf335d9f9c20773f0e3b599d22c85290de83f247` mais alterações locais
  preservadas. C6 não atualizado; hash instalado não obtido. ADR-088 conserva
  32 entradas e usa quatro snapshots internos com índice/geração/cópia; o
  worker reenvia o mais recente se fila/pool rejeitarem a entrega.
- Teste host real passou: comandos, cópias, saturação/liberação, 1.000 ciclos
  de reciclagem, handles antigos/wrap, 8.000 snapshots concorrentes e reenvio
  sem persistência extra. Scheduler, codec e parsers também passaram.
- Build P4 limpo ESP-IDF 5.5.4, sem warnings, em
  `build/event-bus-pool-20261004`, app `0x34bf90`, 59% livres no slot OTA.
  SHA-256 app `06AAE6A5DE9FD9BC1D9995D237D359C574BC65FCBF345A14DB82B47F8B8EF9A1`,
  ELF `05A301A557374EBD71E19D01551267DF9C7DC4008EDD32F65AF9F10081E8F780`.
  Sdkconfig `2524F49D41654D1CBA9137177B4E979726865B622A43965DBBF71C2C4DEF67B8`
  e tabela `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`
  idênticos à imagem anterior; AES software, TLS interno/RX 8 KiB, três FB,
  LVGL extra 128 KiB, auto-suspend desligado e baseline Hosted preservados.
- `idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=build/event-bus-pool-20261004/sdkconfig -B build/event-bus-pool-20261004 -p COM8 -b 460800 app-flash`
  verificou hash, somente app `0x20000`. Boot confirmou ELF novo, P4 v1.3,
  PSRAM 32 MiB, RPC v2/SW_AGGR, bring-up pronto e queue/pool 1.792/1.984 B.
  DWARF confirmou a economia de 15.936 B. Sem gravação de tabela/NVS/C6/eFuses
  pelo comando de flash.
- Captura passiva de 330 s: 11 HTTPS com HTTP 200, sendo cinco BTC (inicial
  e quatro renovações), nove entregas aceitas, BTC stale=0, LAN três aparelhos
  stateful, sem falha HTTPS/entrega ou panic/WDT no log filtrado. Antes de BTC:
  64.143–65.447 B internos livres; fim do corpo com cliente ativo:
  40.963–42.227 B, maior bloco 27.648 B. São amostras pontuais, não o mínimo
  durante todo o TLS. A margem continua abaixo dos pisos do plano.
- COM8 fechada ao final; imagem permanece na placa. Logs fora do Git em
  `%TEMP%/np2-event-bus-pool-20261004.log`. Saturação foi validada no host;
  timeout não reapareceu nesta bancada. Causa, recovery e ensaio por horas
  continuam abertos. [Detalhes e comparação](DATA-REFRESH-DIAGNOSIS.md).
- Conferência solicitada depois: captura passiva de mais 180 s, sem reset,
  avançando dos uptimes 5.466.308 a 5.632.308 ms. Três novos HTTPS BTC
  (uptime ~91–93 min) responderam HTTP 200 em 1,86–2,10 s, stale=0 e entrega
  aceita. Heap pré-HTTPS 65,4 KiB, fim do corpo 42,2 KiB, maior bloco 27 KiB;
  nenhum evento adiado ou WDT/panic. Essa captura foi adicionada ao mesmo log
  filtrado. A causa original continua aberta até ensaio prolongado.

### Idade do evento no centro de notificações — 2026-10-04

- A modal mostra título e explicação do tipo, marcador de não lida, idade
  relativa e resumo com plural correto. Usa `timestamp_unix_s` mais o horário
  atual de `app_state`; com horário ausente informa `Horário indisponível`.
  Preserva oito itens, ordem mais recente primeiro, marcação individual,
  “Marcar todas como lidas” e histórico volátil. Nenhum campo novo foi
  adicionado ao `AppState` ou ao armazenamento. Registrado no ADR-089.
- Build limpo ESP-IDF 5.5.4/P4 concluiu sem warnings em
  `firmware/build/notification-modal-20261004`. App `0x34c160` (3.457.376 B),
  59% livres no menor slot OTA. SHA-256 app
  `F6E7855D69350600FDFB223F1A01E6E5268F4713AED0E04B0A651DB488BB30CA`,
  ELF `E1F92B4ACC461A68722DF90270EADE7E5B77ACF6C298F1F93E6552AC862EE38A`.
  Sdkconfig `2524F49D41654D1CBA9137177B4E979726865B622A43965DBBF71C2C4DEF67B8`
  e tabela `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`
  iguais à imagem EventBus anterior.
- `idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=build/notification-modal-20261004/sdkconfig -B build/notification-modal-20261004 -p COM8 -b 460800 app-flash`
  verificou o hash e gravou somente app em `0x20000`. Boot P4 v1.3, PSRAM
  32 MiB, Hosted RPC v2/SW_AGGR e bring-up concluído. O teste serial não
  abriu nem tocou a modal; confirmação visual das posições/tempo relativo na
  tela continua pendente. Log filtrado em
  `%TEMP%/np2-notification-modal-20261004.log`, COM8 fechada ao final.
- O boot também registrou Bitcoin HTTP 200/stale=0 e timeout de headers no
  BCB após ~10,3 s. Esse problema de rede reapareceu e permanece separado da
  mudança visual de notificação.

### Política do histórico de notificações — 2026-10-04

- O `app_loop` deixou de produzir eventos para atualizações normais de mercado;
  o tipo legado foi removido do contrato e a modal não oferece mais rótulo para
  esse evento. Permanecem transições de rede, falhas de armazenamento/reinício
  e recuperações. ADR-090 registra a política.
- Build P4 limpo ESP-IDF 5.5.4, sem warnings, em
  `firmware/build/notification-policy-20261004`, app `0x34bfa0` (3.456.928 B),
  59% livres na partição de aplicação. SHA-256 app
  `880735C9DD19909A0C03E4CC24221EC280ABA8253E14365A27478F92EFB0E582`, ELF
  `1D8C007E470C5E793FEAEA7B8ED48024193BC4FFFE6086896DE51911E719CB18`.
  Sdkconfig `2524F49D41654D1CBA9137177B4E979726865B622A43965DBBF71C2C4DEF67B8`
  e tabela `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`
  mantiveram-se idênticos.
- `idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=build/notification-policy-20261004/sdkconfig -B build/notification-policy-20261004 -p COM8 -b 460800 app-flash`
  verificou o hash e gravou somente o app em `0x20000`, seguido de reset por
  RTS. Captura serial passiva de 60 s confirmou execução, BTC HTTP 200,
  stale=0 e entregas aceitas para os cinco domínios consultados; o BCB também
  respondeu HTTP 200 nesta amostra. A captura começou em uptime de ~26 s, então
  não contém as primeiras linhas do banner de boot. A modal não foi aberta na
  tela; validação visual dos eventos permanece pendente. COM8 fechada ao final.
  Log filtrado fora do Git em `%TEMP%/np2-notification-policy-20261004.log`.

### Espaçamento da modal de notificações — 2026-10-04

- O painel passou a ocupar 880×570 px, com lista vertical rolável, cards de
  68 px em duas linhas (título/idade e descrição) e botão “Marcar todas como
  lidas” ampliado para 320×48 px. Mantém o limite de oito notificações.
- Build P4 limpo ESP-IDF 5.5.4 sem warnings em
  `firmware/build/notification-layout-20261004`, app `0x34c020` (3.457.056 B),
  59% livres na partição. SHA-256 app
  `44B685EDD6D3D9E30D563F346F44DA45AF166F936A8655BE538745872368B210`, ELF
  `B3E5F45FA07565C1E6380700D274156369BA385F1E153DC9E756CE807E66C329`.
  Sdkconfig `2524F49D41654D1CBA9137177B4E979726865B622A43965DBBF71C2C4DEF67B8`
  e tabela `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`
  idênticos às builds anteriores.
- Flash apenas do app em `0x20000` pela COM8, com hash verificado e reset por
  RTS. Captura serial passiva de 60 s a partir de uptime ~25 s: cinco domínios
  HTTPS responderam HTTP 200, entregas aceitas e LAN encontrou três devices;
  sem panic ou WDT no trecho filtrado. A modal não foi aberta na tela, portanto
  o enquadramento, a rolagem e o texto do botão ainda precisam de confirmação
  visual na placa. COM8 fechada ao final; log em
  `%TEMP%/np2-notification-layout-20261004.log`.

### Alinhamento do canal eWeLink com o ícone — 2026-10-04

- O rótulo do canal agora usa uma linha com reticências para nomes longos e é
  centralizado verticalmente na altura do ícone; o estado continua abaixo do
  nome. Build P4 limpa ESP-IDF 5.5.4 sem warnings em
  `firmware/build/notification-channel-align-20261004`, app `0x34c020`
  (3.457.056 B), 59% livres. SHA-256 app
  `4C43FBF365BAC4EF7E6A469CBEF8456A8307FB06B34EA38D418E07CFDBC3F813`, ELF
  `6D2C165772D462E64EA3C72FBEB532ED160454DD19756346E84736141843CD3A`.
  Sdkconfig e tabela de partições iguais às imagens anteriores.
- Flash apenas do app em `0x20000` pela COM8, hash verificado e reset por RTS.
  Captura serial passiva de 45 s: os cinco domínios HTTPS responderam HTTP 200,
  entregas aceitas e três devices LAN detectados; sem panic/WDT no trecho.
  A lista de canais não foi aberta na tela, então o alinhamento visual ainda
  depende de confirmação direta na placa. COM8 fechada; log em
  `%TEMP%/np2-notification-channel-align-20261004.log`.

### Panic ao abrir Preferências > Tela e som — 2026-10-04

- Evidência reportada pelo usuário: stack protection fault na task `lvgl`, SP
  680 bytes abaixo do limite inferior informado, durante
  `install_display_sound_callbacks()`. O fluxo mantinha uma projeção agregada
  durante `navigation_build` e copiava outra projeção completa no instalador
  para ler só brilho/volume.
- Correção preparada: getter lock-protected para a pequena projeção de
  controles, sem segunda cópia de `app_ui_projection_t`, e pilha LVGL elevada
  de 12 para 16 KiB conforme o plano. Registrado no ADR-091.
- Build P4 limpo ESP-IDF 5.5.4 sem warnings em
  `firmware/build/display-sound-stack-fix-20261004`, app `0x34c020`
  (3.457.056 B), 59% livres. SHA-256 app
  `51137B13CA8E4F221E268D947CDEDC2ABC2A06F53485BFE4A869387301C3F373`, ELF
  `0B81263E74B5EB0D973D66E31C6CEB7E75BA3A2C5CFEEFE177FC56E6C0B49D2C`.
  Sdkconfig e partições iguais às imagens anteriores.
- Flash não executado: COM8 enumerada, mas bloqueada por quatro processos
  `idf_monitor`/`esp_idf_monitor`; esptool recebeu `PermissionError: Acesso
  negado` antes de abrir a porta. Nenhum dado foi gravado e não foi possível
  capturar o boot dessa imagem. Fechar os monitores e repetir app-flash é o
  próximo passo; depois confirmar a navegação tátil e medir a pilha livre.
- **Reteste em 2026-10-05:** os quatro processos já não estavam ativos e a COM8
  abriu. `app-flash` gravou apenas os 3.457.056 B do app em `0x20000`; hash
  verificado `51137B13CA8E4F221E268D947CDEDC2ABC2A06F53485BFE4A869387301C3F373`,
  seguido de reset por RTS. Captura serial passiva por 60 s, iniciada em uptime
  ~23 s, mostrou HTTP 200 e entrega aceita nos cinco domínios, LAN com três
  devices, sem panic/WDT no trecho. Log em
  `%TEMP%/np2-display-sound-stack-fix-20261005.log`. A captura não reproduziu
  o toque em Tela e som; essa confirmação e o high-water mark sob navegação
  continuam pendentes.
