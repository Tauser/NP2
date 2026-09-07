# Roadmap and hardware gates

| Phase | Gate | Status | Exit evidence |
|---|---|---|---|
| 0 | Reproducible P4 project configuration | Complete on host | Locked dependencies and successful P4 build in `BASELINE-VALIDATION.md` |
| 1 | P4 local hardware baseline | Complete — scoped close | Reproducible build; P4 v1.3, flash 32 MiB and PSRAM 32 MiB observed; clean DSI first frame/backlight and repeated boot accepted by physical observation. See `BRINGUP-EVIDENCE.md`. |
| 2 | Display stability and storage discipline | Ready | Touch coordinate/orientation probe, tearing/glitch stress, memory trend, flash-write during render, thermal and long-duration checks were deferred from phase 1 and are mandatory exit evidence here. |
| 3 | C6 ESP-Hosted SDIO link and network | Blocked by phase 2 | C6 image identity, RPC v2/SW_AGGR handshake and managed network recovery |
| 4 | Secure persistence and OTA recovery | Blocked by phase 3 | P4 A/B rollback, C6 Slave OTA, interrupted-update recovery |
| 5 | Production qualification | Blocked by phase 4 | Soak, thermal, fault injection, security provisioning and acceptance record |

The detailed deliverables, risks and acceptance criteria are in
[PLANO-FIRMWARE-PREMIUM.md](PLANO-FIRMWARE-PREMIUM.md). No phase advances from
one-off behavior; it advances only with reproducible evidence.

## Phase 1 closure assessment — 2026-09-07

**Status:** complete — scoped by product-owner instruction.

**Evidence:** clean ESP-IDF 5.5.4 build; P4 baseline flashed to COM8 with
esptool hash verification; serial evidence for P4 v1.3, 32 MiB flash, 32 MiB
PSRAM, EK79007, GT911, backlight, three framebuffers, Hosted 3.0.6 / RPC v2 /
SW_AGGR; and physical confirmation of image/orientation and repeat boot. Full
attempt history, including the two corrected failures, is in
`BRINGUP-EVIDENCE.md`.

**Deferred risks:** touch position and gesture behavior, animation/tearing,
memory under workload, flash contention, Wi-Fi behavior, thermal and soak were
not represented by the baseline screen and remain unvalidated.

**Next checkpoint:** phase 2 starts with a diagnostic screen that makes touch
coordinates, continuous render timing and memory counters observable. It must
close the deferred display/stability gates before persistence or product UI.
