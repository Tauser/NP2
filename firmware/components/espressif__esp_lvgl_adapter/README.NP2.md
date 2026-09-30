# NovaPanel local override of esp_lvgl_adapter

This directory is a versioned copy of Espressif `esp_lvgl_adapter` 0.6.4
(component registry hash
`bcb316bedda9e934d32567082a969a8e8f8f406dd32d23c359a299249dc18e81`,
upstream `repository_info.commit_sha` `5d75f3f0dc499d9ed4b69284a3741187c2b75a70`).
The copied `src/adapter/esp_lv_adapter.c` had SHA-256
`3AAF0E1B79C8F89AFB1224F5F08957AEE3E76E1AE2D9D3AE626D717A6B42B2F5`
before the local change. The original component was resolved in the NP2
baseline; no generated `managed_components` file is edited.

The NP2 delta is limited to two optional hooks in `esp_lv_adapter_config_t`
and calls around the worker's `lv_timer_handler()` under the adapter lock:
`ui_cycle_begin` and `ui_cycle_end`. They carry an opaque user context. The
adapter does not know about pages, navigation phases, or product state. The
display, flush, touch, and tearing code remains the upstream 0.6.4 code.

`firmware/main/idf_component.yml` selects this directory through
`override_path`. ESP-IDF Component Manager serializes the resolved local
source as an absolute path in `dependencies.lock`; another checkout will
regenerate that entry for its own path. Review this delta against 0.6.4 before updating the
component, and validate the display pipeline on the physical P4 before
promoting the navigation pilot.
