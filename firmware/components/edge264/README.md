# edge264 for ESP32-P4

Vendored from [tvlabs/edge264](https://github.com/tvlabs/edge264), commit
`2c2ab95d63c1ad89c5687f9e50b57cad772c871b`, BSD-3-Clause. The public
header, decoder source translation units, and license are preserved; upstream
tests and desktop build scripts are omitted. See `LICENSE_BSD.txt`.

The ESP32-P4 build compiles `src/edge264.c` with ESP Clang and links the object
into the ESP-IDF firmware. It uses the single-thread mode. The local build
definition maps the optional CPU-time clock to `CLOCK_MONOTONIC`, which ESP-IDF
provides. No camera credentials or media are built into the component.

This decoder is being evaluated for H.264 Main from a Tapo C200. Passing a
build does not establish real-time performance, memory safety under malformed
streams, or display stability; these require bounded on-device measurements.
