# Roadmap and hardware gates

| Phase | Gate | Status | Exit evidence |
|---|---|---|---|
| 0 | Reproducible P4 project configuration | Complete on host | Locked dependencies and successful P4 build in `BASELINE-VALIDATION.md` |
| 1 | P4 local hardware baseline | In progress | PSRAM capacity, clean DSI first frame, 180-degree GT911 touch and controlled backlight recorded on the board |
| 2 | Display stability and storage discipline | Blocked by phase 1 | Tearing/glitch stress evidence with flash-write policy enforced |
| 3 | C6 ESP-Hosted SDIO link and network | Blocked by phase 2 | C6 image identity, RPC v2/SW_AGGR handshake and managed network recovery |
| 4 | Secure persistence and OTA recovery | Blocked by phase 3 | P4 A/B rollback, C6 Slave OTA, interrupted-update recovery |
| 5 | Production qualification | Blocked by phase 4 | Soak, thermal, fault injection, security provisioning and acceptance record |

The detailed deliverables, risks and acceptance criteria are in
[PLANO-FIRMWARE-PREMIUM.md](PLANO-FIRMWARE-PREMIUM.md). No phase advances from
one-off behavior; it advances only with reproducible evidence.
