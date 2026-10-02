#include <assert.h>

#include "pomodoro_service.h"

int main(void)
{
    pomodoro_service_t service = {0};
    pomodoro_projection_t projection = {0};
    pomodoro_service_init(&service);
    pomodoro_service_copy(&service, &projection);
    assert(projection.duration_minutes == 25U);
    assert(projection.remaining_seconds == 1500U);

    pomodoro_service_apply(&service, POMODORO_COMMAND_SELECT_MINUTES, 5U, 0U);
    pomodoro_service_apply(&service, POMODORO_COMMAND_TOGGLE, 0U, 0U);
    pomodoro_service_tick(&service, 2500U, false, 0U);
    pomodoro_service_copy(&service, &projection);
    assert(projection.running);
    assert(projection.remaining_seconds == 298U);
    assert(projection.focused_seconds_today == 2U);

    pomodoro_service_apply(&service, POMODORO_COMMAND_TOGGLE, 0U, 2500U);
    pomodoro_service_copy(&service, &projection);
    assert(!projection.running);
    assert(projection.remaining_seconds == 298U);

    pomodoro_service_apply(&service, POMODORO_COMMAND_TOGGLE, 0U, 5000U);
    pomodoro_service_tick(&service, 302999U, false, 0U);
    pomodoro_service_copy(&service, &projection);
    assert(projection.running);
    assert(projection.remaining_seconds == 1U);
    pomodoro_service_tick(&service, 303000U, false, 0U);
    pomodoro_service_copy(&service, &projection);
    assert(!projection.running);
    assert(projection.completed);
    assert(projection.remaining_seconds == 0U);
    assert(projection.completed_today == 1U);
    assert(projection.focused_seconds_today == 300U);

    pomodoro_service_apply(&service, POMODORO_COMMAND_RESET, 0U, 303000U);
    pomodoro_service_copy(&service, &projection);
    assert(!projection.running);
    assert(!projection.completed);
    assert(projection.remaining_seconds == 300U);

    pomodoro_service_apply(&service, POMODORO_COMMAND_SELECT_MINUTES, 100U, 304000U);
    pomodoro_service_copy(&service, &projection);
    assert(projection.duration_minutes == 5U);
    assert(projection.remaining_seconds == 300U);
    return 0;
}
