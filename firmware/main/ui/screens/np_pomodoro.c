#include "np_pomodoro.h"

#include <stdio.h>

#include "app_event_bus.h"

#define POMODORO_LEFT_X          18
#define POMODORO_LEFT_Y          88
#define POMODORO_LEFT_W         558
#define POMODORO_LEFT_H         492
#define POMODORO_RIGHT_X        586
#define POMODORO_RIGHT_W        422
#define POMODORO_RIGHT_GAP       14
#define POMODORO_HEADER_H       182
#define POMODORO_GOAL_H         158
#define POMODORO_QUICK_H        138
#define POMODORO_RING_X         143
#define POMODORO_RING_Y         120
#define POMODORO_RING_SIZE      312
#define POMODORO_ACTION_Y       466
#define POMODORO_GOAL_COUNT       4U

static const uint8_t s_presets[] = {5U, 15U, 25U, 50U};

static void post_command(pomodoro_command_t command, uint16_t value)
{
    (void)app_event_bus_post_pomodoro(command, value);
}

static void toggle_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    np_pomodoro_view_t *const view = lv_event_get_user_data(event);
    if (view == NULL) return;
    const lv_obj_t *const target = lv_event_get_target(event);
    if (target == view->pause_button && view->state.running) {
        post_command(POMODORO_COMMAND_TOGGLE, 0U);
    } else if (target == view->play_button && !view->state.running) {
        post_command(POMODORO_COMMAND_TOGGLE, 0U);
    }
}

static void reset_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    post_command(POMODORO_COMMAND_RESET, 0U);
}

static void preset_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const uint16_t minutes = (uint16_t)(uintptr_t)lv_event_get_user_data(event);
    post_command(POMODORO_COMMAND_SELECT_MINUTES, minutes);
}

static lv_obj_t *icon_badge(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t size, const char *icon,
                            lv_color_t background, lv_color_t foreground)
{
    lv_obj_t *const badge = np_fill(parent, x, y, size, size,
        background, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    lv_obj_t *const label = np_label(badge, icon, NP_FONT_ICON_BADGE,
        foreground, 0, (size - NP_FONT_ICON_BADGE->line_height) / 2,
        size, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_height(label, NP_FONT_ICON_BADGE->line_height);
    return badge;
}

static lv_obj_t *action_button(lv_obj_t *parent, int32_t x, int32_t y,
                               int32_t size, const char *icon, bool primary,
                               lv_obj_t **icon_out)
{
    lv_obj_t *const button = np_button(parent, x, y, size, size, "", primary);
    lv_obj_t *const old_label = lv_obj_get_child(button, 0);
    if (old_label != NULL) lv_obj_delete(old_label);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);

    lv_obj_t *const icon_label = lv_label_create(button);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, NP_FONT_ICON_BADGE, 0);
    lv_obj_set_style_text_color(icon_label,
        primary ? np_c_text_on_accent() : np_c_text_2(), 0);
    lv_obj_set_style_text_align(icon_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(icon_label, size, NP_FONT_ICON_BADGE->line_height);
    lv_obj_align(icon_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(icon_label, LV_OBJ_FLAG_CLICKABLE);
    *icon_out = icon_label;
    return button;
}

static void set_action_enabled(lv_obj_t *button, lv_obj_t *icon,
                               bool enabled, bool primary)
{
    if (enabled) lv_obj_remove_state(button, LV_STATE_DISABLED);
    else lv_obj_add_state(button, LV_STATE_DISABLED);
    const lv_color_t color = primary ? np_c_text_on_accent() : np_c_text_2();
    np_set_text_color(icon, color);
}

static void goal_progress_set(np_pomodoro_view_t *view, uint32_t completed)
{
    const uint32_t capped = completed > POMODORO_GOAL_COUNT
        ? POMODORO_GOAL_COUNT : completed;
    const int32_t width = (int32_t)((uint32_t)view->goal_progress_width * capped /
                                    POMODORO_GOAL_COUNT);
    lv_obj_set_width(view->goal_progress_fill, width);
    np_set_visible(view->goal_progress_fill, width > 0);
    for (uint32_t i = 0U; i < POMODORO_GOAL_COUNT; ++i) {
        const bool done = i < capped;
        np_set_bg_color(view->goal_steps[i], done ? np_c_accent()
                                                  : np_c_surface_raised());
        np_set_text_color(view->goal_step_checks[i], done
            ? np_c_text_on_accent() : np_c_text_3());
        np_set_visible(view->goal_step_checks[i], done);
    }
    for (uint32_t i = 0U; i + 1U < POMODORO_GOAL_COUNT; ++i) {
        const bool done = i < capped;
        np_set_bg_color(view->goal_connectors[i], done ? np_c_accent()
                                                       : np_c_surface_raised());
    }
    char text[8] = {0};
    (void)snprintf(text, sizeof(text), "%u/%u", (unsigned)capped,
                   (unsigned)POMODORO_GOAL_COUNT);
    np_set_text(view->goal_value, text);
}

np_pomodoro_view_t np_pomodoro_build_with_header(lv_obj_t *parent,
                                                  const np_header_t *header)
{
    np_pomodoro_view_t view = {0};
    view.root = np_scene(parent);
    view.header = header != NULL ? *header : np_header(view.root);

    /* A composição segue as três faixas da referência: foco, resumo/meta e
     * escolhas rápidas. As superfícies usam os painéis sem borda do tema. */
    lv_obj_t *const focus_panel = np_panel(view.root, POMODORO_LEFT_X,
        POMODORO_LEFT_Y, POMODORO_LEFT_W, POMODORO_LEFT_H);
    lv_obj_t *const today_panel = np_panel(view.root, POMODORO_RIGHT_X,
        POMODORO_LEFT_Y, POMODORO_RIGHT_W, POMODORO_HEADER_H);
    lv_obj_t *const goal_panel = np_panel(view.root, POMODORO_RIGHT_X,
        POMODORO_LEFT_Y + POMODORO_HEADER_H + POMODORO_RIGHT_GAP,
        POMODORO_RIGHT_W, POMODORO_GOAL_H);
    lv_obj_t *const quick_panel = np_panel(view.root, POMODORO_RIGHT_X,
        POMODORO_LEFT_Y + POMODORO_HEADER_H + POMODORO_RIGHT_GAP +
        POMODORO_GOAL_H + POMODORO_RIGHT_GAP,
        POMODORO_RIGHT_W, POMODORO_QUICK_H);

    lv_obj_t *const arc = lv_arc_create(view.root);
    view.progress_arc = arc;
    lv_obj_remove_style_all(arc);
    lv_obj_set_pos(arc, POMODORO_RING_X, POMODORO_RING_Y);
    lv_obj_set_size(arc, POMODORO_RING_SIZE, POMODORO_RING_SIZE);
    lv_arc_set_range(arc, 0, 1000);
    lv_arc_set_bg_angles(arc, 0, 359);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_color(arc, np_c_surface_raised(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 15, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, np_c_accent(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 15, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, np_c_accent(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(arc, 0, LV_PART_KNOB);
    lv_obj_set_style_radius(arc, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 8, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *const timer_icon = np_label(focus_panel, NP_ICON_TIMER,
        NP_FONT_ICON_BADGE, np_c_accent(), 20, 88, POMODORO_LEFT_W - 40,
        LV_TEXT_ALIGN_CENTER);
    lv_obj_set_height(timer_icon, NP_FONT_ICON_BADGE->line_height);
    view.timer_label = np_label(focus_panel, "25:00", NP_FONT_BRAND,
        np_c_text(), 20, 155, POMODORO_LEFT_W - 40, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_height(view.timer_label, NP_FONT_BRAND->line_height);
    view.phase_label = np_label(focus_panel, "Foco", NP_FONT_TITLE,
        np_c_text(), 20, 225, POMODORO_LEFT_W - 40, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_height(view.phase_label, NP_FONT_TITLE->line_height);
    view.cycle_label = np_label(focus_panel, "Pomodoro 1 de 4", NP_FONT_MD,
        np_c_text_2(), 20, 261, POMODORO_LEFT_W - 40, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_height(view.cycle_label, NP_FONT_MD->line_height);

    /* Controles somente com ícones: pausa e reinício têm o mesmo tamanho;
     * play ganha um botão quadrado maior para indicar a ação principal. */
    view.pause_button = action_button(focus_panel, 139,
        POMODORO_ACTION_Y - POMODORO_LEFT_Y, 76, NP_ICON_PAUSE, false,
        &view.pause_icon);
    view.play_button = action_button(focus_panel, 235,
        POMODORO_ACTION_Y - POMODORO_LEFT_Y - 6, 88,
        NP_ICON_PLAY_ARROW, true, &view.play_icon);
    view.reset_button = action_button(focus_panel, 343,
        POMODORO_ACTION_Y - POMODORO_LEFT_Y, 76, NP_ICON_REPLAY, false,
        &view.reset_icon);

    /* Resumo do dia: os dois valores continuam vinculados ao serviço. */
    (void)icon_badge(today_panel, 20, 18, 54, NP_ICON_BAR_CHART,
                     np_c_positive_bg(), np_c_positive());
    np_label(today_panel, "Hoje", NP_FONT_LG, np_c_text(),
             92, 19, 250, LV_TEXT_ALIGN_LEFT);
    np_label(today_panel, "Seu progresso de foco", NP_FONT_MD, np_c_text_2(),
             92, 48, 300, LV_TEXT_ALIGN_LEFT);
    (void)icon_badge(today_panel, 20, 104, 42, NP_ICON_TIMER,
                     np_c_negative_bg(), np_c_negative());
    view.completed_value = np_label(today_panel, "0", NP_FONT_TITLE,
        np_c_text(), 72, 98, 104, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(view.completed_value, NP_FONT_TITLE->line_height);
    np_label(today_panel, "pomodoros", NP_FONT_MD, np_c_text_2(),
             72, 146, 112, LV_TEXT_ALIGN_LEFT);
    np_vline(today_panel, 192, 96, 58);
    (void)icon_badge(today_panel, 210, 104, 42, NP_ICON_SCHEDULE,
                     np_c_accent_bg(), np_c_accent());
    view.focused_value = np_label(today_panel, "0 min", NP_FONT_TITLE,
        np_c_text(), 260, 99, 150, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(view.focused_value, NP_FONT_TITLE->line_height);
    np_label(today_panel, "focado", NP_FONT_MD, np_c_text_2(),
             260, 146, 142, LV_TEXT_ALIGN_LEFT);

    /* Meta de quatro ciclos, com barra e quatro marcos de conclusão. */
    (void)icon_badge(goal_panel, 18, 14, 52, NP_ICON_TRACK_CHANGES,
                     np_c_surface_raised(), np_c_warning());
    np_label(goal_panel, "Meta do dia", NP_FONT_LG, np_c_text(),
             90, 15, 220, LV_TEXT_ALIGN_LEFT);
    np_label(goal_panel, "4 pomodoros", NP_FONT_MD, np_c_text_2(),
             90, 44, 220, LV_TEXT_ALIGN_LEFT);
    view.goal_value = np_label(goal_panel, "0/4", NP_FONT_TITLE, np_c_text(),
        338, 21, 66, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_height(view.goal_value, NP_FONT_TITLE->line_height);
    const int32_t goal_bar_x = 20;
    const int32_t goal_bar_y = 77;
    view.goal_progress_width = 382;
    (void)np_fill(goal_panel, goal_bar_x, goal_bar_y,
        view.goal_progress_width, 15, np_c_surface_raised(),
        LV_OPA_COVER, 8);
    view.goal_progress_fill = np_fill(goal_panel, goal_bar_x, goal_bar_y,
        1, 15, np_c_accent(), LV_OPA_COVER, 8);
    const int32_t step_centers[POMODORO_GOAL_COUNT] = {28, 150, 272, 394};
    for (uint32_t i = 0U; i + 1U < POMODORO_GOAL_COUNT; ++i) {
        view.goal_connectors[i] = np_fill(goal_panel,
            step_centers[i] + 14, 118,
            step_centers[i + 1U] - step_centers[i] - 28, 4,
            np_c_surface_raised(), LV_OPA_COVER, 2);
    }
    for (uint32_t i = 0U; i < POMODORO_GOAL_COUNT; ++i) {
        const int32_t x = step_centers[i] - 14;
        view.goal_steps[i] = np_fill(goal_panel, x, 106, 28, 28,
            np_c_surface_raised(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
        view.goal_step_checks[i] = np_label(view.goal_steps[i], NP_ICON_CHECK,
            NP_FONT_ICON, np_c_text_3(), 0,
            (28 - NP_FONT_ICON->line_height) / 2, 28,
            LV_TEXT_ALIGN_CENTER);
        lv_obj_set_height(view.goal_step_checks[i], NP_FONT_ICON->line_height);
    }

    /* Os quatro tempos da referência; o ativo usa o azul do produto. */
    (void)icon_badge(quick_panel, 18, 12, 54, NP_ICON_TIMER,
                     np_c_accent_bg(), np_c_accent());
    np_label(quick_panel, "Tempos rápidos", NP_FONT_LG, np_c_text(),
             90, 13, 300, LV_TEXT_ALIGN_LEFT);
    np_label(quick_panel, "Defina um tempo para o timer", NP_FONT_MD,
             np_c_text_2(), 90, 42, 320, LV_TEXT_ALIGN_LEFT);

    const int32_t preset_x[] = {15, 115, 215, 315};
    const int32_t preset_w = 92;
    for (size_t i = 0U; i < sizeof(s_presets) / sizeof(s_presets[0]); ++i) {
        char text[12] = {0};
        (void)snprintf(text, sizeof(text), "%u min", (unsigned)s_presets[i]);
        view.preset_buttons[i] = np_button(quick_panel, preset_x[i],
            82, preset_w, 45, text, false);
    }
    view.quick_preset_labels[0] = lv_obj_get_child(view.preset_buttons[0], 0);
    view.quick_preset_labels[1] = lv_obj_get_child(view.preset_buttons[1], 0);
    view.quick_preset_labels[2] = lv_obj_get_child(view.preset_buttons[2], 0);
    view.quick_preset_labels[3] = lv_obj_get_child(view.preset_buttons[3], 0);

    goal_progress_set(&view, 0U);

    return view;
}

void np_pomodoro_bind(np_pomodoro_view_t *view)
{
    if (view == NULL || view->root == NULL) return;
    for (size_t i = 0U; i < sizeof(s_presets) / sizeof(s_presets[0]); ++i) {
        lv_obj_add_event_cb(view->preset_buttons[i], preset_event_cb,
            LV_EVENT_CLICKED, (void *)(uintptr_t)s_presets[i]);
    }
    lv_obj_add_event_cb(view->pause_button, toggle_event_cb,
                        LV_EVENT_CLICKED, view);
    lv_obj_add_event_cb(view->play_button, toggle_event_cb,
                        LV_EVENT_CLICKED, view);
    lv_obj_add_event_cb(view->reset_button, reset_event_cb,
                        LV_EVENT_CLICKED, view);
}

void np_pomodoro_sync(np_pomodoro_view_t *view,
                      const pomodoro_projection_t *projection)
{
    if (view == NULL || projection == NULL) return;
    view->state = *projection;

    char clock_text[6] = {0};
    uint32_t minutes = projection->remaining_seconds / 60U;
    const uint32_t seconds = projection->remaining_seconds % 60U;
    if (minutes > 99U) minutes = 99U;
    (void)snprintf(clock_text, sizeof(clock_text), "%02u:%02u",
                   (unsigned)minutes, (unsigned)seconds);
    np_set_text(view->timer_label, clock_text);

    const uint32_t total_seconds = (uint32_t)projection->duration_minutes * 60U;
    uint16_t progress = 0U;
    if (total_seconds > 0U && projection->remaining_seconds <= total_seconds) {
        progress = (uint16_t)(((total_seconds - projection->remaining_seconds) * 1000U) /
                              total_seconds);
    }
    lv_arc_set_value(view->progress_arc, progress);
    const uint32_t configured_seconds =
        (uint32_t)projection->duration_minutes * 60U;
    const bool paused = !projection->running && !projection->completed &&
        projection->remaining_seconds < configured_seconds;
    np_set_text(view->phase_label, projection->completed ? "Concluído" :
                (paused ? "Pausado" : "Foco"));
    char cycle[32] = {0};
    if (projection->completed_today >= POMODORO_GOAL_COUNT) {
        (void)snprintf(cycle, sizeof(cycle), "Meta diária concluída");
    } else {
        (void)snprintf(cycle, sizeof(cycle), "Pomodoro %u de %u",
            (unsigned)(projection->completed_today + 1U),
            (unsigned)POMODORO_GOAL_COUNT);
    }
    np_set_text(view->cycle_label, cycle);

    set_action_enabled(view->pause_button, view->pause_icon,
                       projection->running, false);
    set_action_enabled(view->play_button, view->play_icon,
                       !projection->running, true);

    for (size_t i = 0U; i < sizeof(s_presets) / sizeof(s_presets[0]); ++i) {
        const bool selected = projection->duration_minutes == s_presets[i];
        if (projection->running) lv_obj_add_state(view->preset_buttons[i], LV_STATE_DISABLED);
        else lv_obj_remove_state(view->preset_buttons[i], LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(view->preset_buttons[i],
            selected ? np_c_accent() : np_c_surface_raised(), LV_PART_MAIN);
        lv_obj_set_style_border_width(view->preset_buttons[i],
            selected ? 0 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(view->preset_buttons[i],
            selected ? np_c_accent() : np_c_hairline(), LV_PART_MAIN);
        np_set_text_color(view->quick_preset_labels[i], selected
            ? np_c_text_on_accent() : np_c_text());
    }
    char count[12] = {0};
    (void)snprintf(count, sizeof(count), "%u",
                   (unsigned)projection->completed_today);
    np_set_text(view->completed_value, count);
    char focused[20] = {0};
    const uint32_t hours = projection->focused_seconds_today / 3600U;
    const uint32_t mins = (projection->focused_seconds_today % 3600U) / 60U;
    if (hours > 0U && mins > 0U) {
        (void)snprintf(focused, sizeof(focused), "%uh %u min",
                       (unsigned)hours, (unsigned)mins);
    } else if (hours > 0U) {
        (void)snprintf(focused, sizeof(focused), "%uh",
                       (unsigned)hours);
    } else {
        (void)snprintf(focused, sizeof(focused), "%u min", (unsigned)mins);
    }
    np_set_text(view->focused_value, focused);
    goal_progress_set(view, projection->completed_today);
}
