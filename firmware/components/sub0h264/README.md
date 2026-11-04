# Sub0h264 for the NP2 camera stream

Vendored from [CraigHutchinson/Sub0h264](https://github.com/CraigHutchinson/Sub0h264),
commit `15421eeade48774929bc0221ea0418a25462c222`, MIT (`LICENSE.md`; copyright
only, the upstream README notes H.264 patents are not licensed). Only
`components/sub0h264/{src,include}` are kept, without the unity TU and the
platform stubs. `CMakeLists.txt`, `np2_sub0h264.h` and `np2_sub0h264.cpp` are
local: upstream's component file targets the ESP-IDF cmakev2 system, and our
ESP-IDF 5.5.4 uses the classic one.

`camera_stream_service.c` uses it through `np2_sub0h264.h`. Malformed streams, render
under video (G2) and allocation failure (vectors inside `noexcept` abort on
out-of-memory, so the service admits a session first) are still open. See ADR-121.

## Local patches

`patches/0001-frame-num-wrap.patch` is already applied to `src/dpb.hpp`. It fixes
reference selection after `frame_num` wraps: eviction now follows decode order and
the L0 list orders short-term references by FrameNumWrap (H.264 8.2.4.1). Without
it, streams with `log2_max_frame_num_minus4=0` such as the Tapo C200 corrupt
silently after the first wrap. To check it against upstream, apply it with `-p1`
from upstream's `components/sub0h264`.

`patches/0002-panel-allocator.patch` is also applied. It adds
`src/panel_allocator.hpp` and routes every decoder vector through it, so large
buffers go to PSRAM and internal SRAM stays available for TLS, JSON and HTTP.

## Known divergences from FFmpeg

Seen with x264 test streams, with `errors=0`; none occur in the measured C200 sample:
QP below about 17 (cause unknown), several slices per frame (each is returned as a
frame) and explicit weighted prediction. The boundary rejects the last two as
unsupported before they reach the decoder; low QP cannot be detected there.
See `docs/BRINGUP-EVIDENCE.md`.
