# Hardware contract

This file is the short operational index. The complete, mandatory source of
truth is [RESTART-HARDWARE-BRINGUP.md](RESTART-HARDWARE-BRINGUP.md); a change
to this summary never overrides it.

| Item | Contract |
|---|---|
| Main SoC | ESP32-P4 revision v1.3, 32 MiB flash and 32 MiB PSRAM |
| Display | EK79007, MIPI-DSI two lanes, 1024x600, RGB565 |
| Touch/backlight | GT911 on GPIO8/GPIO7; backlight GPIO32; visual orientation 180 degrees |
| Wireless | ESP32-C6 over SDIO 4-bit: CLK18, CMD19, D0..D3 GPIO14..17; reset GPIO54 active low |
| Firmware pair | ESP-Hosted 3.0.6 on P4 and C6; RPC v2; SDIO SW_AGGR |
| Display policy | Three hardware framebuffers, triple partial mode, 50-line internal draw buffer |
| Flash policy | `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`; never erase/write flash during visible rendering |
| C6 update | Slave OTA through SDIO only; never use the P4 USB connection to update C6 |

Every physical test must append a dated record to
[BRINGUP-EVIDENCE.md](BRINGUP-EVIDENCE.md) with board/BOM, P4 and C6 hashes,
effective `sdkconfig`, command, full result and artifacts. A successful build
does not validate a hardware item.
