/* Laboratory-only H.264 decoder comparison (ADR-121). OFF in normal builds. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Runs once after a settling delay, in its own task, then the task ends. */
void h264_decode_bench_start(void);

#ifdef __cplusplus
}
#endif
