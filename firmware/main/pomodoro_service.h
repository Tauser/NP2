#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    POMODORO_COMMAND_TOGGLE = 0,
    POMODORO_COMMAND_RESET,
    POMODORO_COMMAND_SELECT_MINUTES,
} pomodoro_command_t;

typedef struct {
    bool running;
    bool completed;
    uint8_t duration_minutes;
    uint32_t remaining_seconds;
    uint32_t completed_today;
    uint32_t focused_seconds_today;
} pomodoro_projection_t;

typedef struct {
    pomodoro_projection_t projection;
    uint64_t deadline_ms;
    uint64_t last_accounted_ms;
    int32_t local_day_key;
} pomodoro_service_t;

void pomodoro_service_init(pomodoro_service_t *service);
void pomodoro_service_apply(pomodoro_service_t *service,
                            pomodoro_command_t command,
                            uint16_t value,
                            uint64_t now_ms);
void pomodoro_service_tick(pomodoro_service_t *service,
                           uint64_t now_ms,
                           bool time_trusted,
                           uint32_t current_unix_s);
void pomodoro_service_copy(const pomodoro_service_t *service,
                           pomodoro_projection_t *out_projection);
