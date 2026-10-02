#include "pomodoro_service.h"

#include <time.h>

#define POMODORO_DEFAULT_MINUTES 25U
#define POMODORO_MAX_MINUTES 99U

static bool local_time_copy(const time_t *timestamp, struct tm *out)
{
#if defined(_WIN32) || defined(__MINGW32__)
    return localtime_s(out, timestamp) == 0;
#else
    return localtime_r(timestamp, out) != NULL;
#endif
}

static void advance_timer(pomodoro_service_t *service, uint64_t now_ms)
{
    pomodoro_projection_t *const state = &service->projection;
    if (!state->running) return;

    uint64_t account_until_ms = now_ms;
    if (account_until_ms > service->deadline_ms) {
        account_until_ms = service->deadline_ms;
    }
    if (account_until_ms > service->last_accounted_ms) {
        const uint64_t elapsed_seconds =
            (account_until_ms - service->last_accounted_ms) / UINT64_C(1000);
        if (elapsed_seconds > 0U) {
            const uint32_t add_seconds = elapsed_seconds > UINT32_MAX
                ? UINT32_MAX : (uint32_t)elapsed_seconds;
            state->focused_seconds_today += add_seconds;
            service->last_accounted_ms += elapsed_seconds * UINT64_C(1000);
        }
    }

    if (now_ms >= service->deadline_ms) {
        state->running = false;
        state->completed = true;
        state->remaining_seconds = 0U;
        ++state->completed_today;
        return;
    }

    const uint64_t remaining_ms = service->deadline_ms - now_ms;
    state->remaining_seconds = (uint32_t)((remaining_ms + 999U) / 1000U);
}

void pomodoro_service_init(pomodoro_service_t *service)
{
    if (service == NULL) return;
    *service = (pomodoro_service_t){
        .projection = {
            .duration_minutes = POMODORO_DEFAULT_MINUTES,
            .remaining_seconds = POMODORO_DEFAULT_MINUTES * 60U,
        },
    };
}

void pomodoro_service_apply(pomodoro_service_t *service,
                            pomodoro_command_t command,
                            uint16_t value,
                            uint64_t now_ms)
{
    if (service == NULL) return;
    pomodoro_projection_t *const state = &service->projection;
    advance_timer(service, now_ms);

    switch (command) {
    case POMODORO_COMMAND_TOGGLE:
        if (state->running) {
            state->running = false;
            service->deadline_ms = 0U;
            service->last_accounted_ms = 0U;
        } else {
            if (state->remaining_seconds == 0U) {
                state->remaining_seconds = (uint32_t)state->duration_minutes * 60U;
                state->completed = false;
            }
            service->last_accounted_ms = now_ms;
            service->deadline_ms = now_ms +
                (uint64_t)state->remaining_seconds * UINT64_C(1000);
            state->running = true;
        }
        break;
    case POMODORO_COMMAND_RESET:
        state->running = false;
        state->completed = false;
        state->remaining_seconds = (uint32_t)state->duration_minutes * 60U;
        service->deadline_ms = 0U;
        service->last_accounted_ms = 0U;
        break;
    case POMODORO_COMMAND_SELECT_MINUTES:
        if (!state->running && value > 0U && value <= POMODORO_MAX_MINUTES) {
            state->duration_minutes = (uint8_t)value;
            state->remaining_seconds = (uint32_t)value * 60U;
            state->completed = false;
        }
        break;
    default:
        break;
    }
}

void pomodoro_service_tick(pomodoro_service_t *service,
                           uint64_t now_ms,
                           bool time_trusted,
                           uint32_t current_unix_s)
{
    if (service == NULL) return;
    if (time_trusted && current_unix_s != 0U) {
        const time_t timestamp = (time_t)current_unix_s;
        struct tm local = {0};
        if (local_time_copy(&timestamp, &local)) {
            const int32_t day_key = (int32_t)(local.tm_year + 1900) * 366 +
                                    (int32_t)local.tm_yday;
            if (service->local_day_key != 0 && service->local_day_key != day_key) {
                service->projection.completed_today = 0U;
                service->projection.focused_seconds_today = 0U;
            }
            service->local_day_key = day_key;
        }
    }
    advance_timer(service, now_ms);
}

void pomodoro_service_copy(const pomodoro_service_t *service,
                           pomodoro_projection_t *out_projection)
{
    if (service == NULL || out_projection == NULL) return;
    *out_projection = service->projection;
}
