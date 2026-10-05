#include "np_notification_center.h"

#include <stdio.h>

static void format_notification_age(uint32_t timestamp_unix_s,
                                    uint32_t current_unix_s,
                                    char *out_text,
                                    size_t out_size)
{
    if (out_text == NULL || out_size == 0U) return;
    if (timestamp_unix_s == 0U || current_unix_s == 0U) {
        (void)snprintf(out_text, out_size, "Horário indisponível");
        return;
    }
    if (timestamp_unix_s >= current_unix_s) {
        (void)snprintf(out_text, out_size, "Agora");
        return;
    }

    const uint32_t age_s = current_unix_s - timestamp_unix_s;
    if (age_s < 60U) {
        (void)snprintf(out_text, out_size, "Agora");
    } else if (age_s < 3600U) {
        (void)snprintf(out_text, out_size, "há %u min", (unsigned int)(age_s / 60U));
    } else if (age_s < 86400U) {
        (void)snprintf(out_text, out_size, "há %u h", (unsigned int)(age_s / 3600U));
    } else {
        (void)snprintf(out_text, out_size, "há %u d", (unsigned int)(age_s / 86400U));
    }
}

static const char *notification_title(app_notification_kind_t kind)
{
    switch (kind) {
    case APP_NOTIFICATION_KIND_WIFI_DISCONNECTED: return "Wi-Fi desconectado";
    case APP_NOTIFICATION_KIND_WIFI_RESTORED: return "Wi-Fi conectado";
    case APP_NOTIFICATION_KIND_STORAGE_ERROR: return "Falha no armazenamento";
    case APP_NOTIFICATION_KIND_STORAGE_RECOVERED: return "Armazenamento recuperado";
    case APP_NOTIFICATION_KIND_RESTART_FAILED: return "Falha no reinício";
    default: return "Notificação do sistema";
    }
}

static const char *notification_detail(app_notification_kind_t kind)
{
    switch (kind) {
    case APP_NOTIFICATION_KIND_WIFI_DISCONNECTED:
        return "A conexão de rede foi perdida.";
    case APP_NOTIFICATION_KIND_WIFI_RESTORED:
        return "A conexão de rede foi restabelecida.";
    case APP_NOTIFICATION_KIND_STORAGE_ERROR:
        return "Não foi possível concluir uma operação local.";
    case APP_NOTIFICATION_KIND_STORAGE_RECOVERED:
        return "As operações locais voltaram ao normal.";
    case APP_NOTIFICATION_KIND_RESTART_FAILED:
        return "O painel não conseguiu reiniciar.";
    default:
        return "";
    }
}

np_notification_center_t np_notification_center_create(lv_obj_t *parent)
{
    np_notification_center_t view = {0};
    if (parent == NULL) return view;

    view.root = np_fill(parent, 0, 0, NP_SCREEN_W, NP_SCREEN_H,
                        np_c_bg(), LV_OPA_60, 0);
    if (view.root == NULL) return (np_notification_center_t){0};
    lv_obj_add_flag(view.root, LV_OBJ_FLAG_CLICKABLE);
    np_set_visible(view.root, false);

    lv_obj_t *const panel = np_panel(view.root, 72, 15, 880, 570);
    if (panel == NULL) {
        lv_obj_delete(view.root);
        return (np_notification_center_t){0};
    }
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_EVENT_BUBBLE);

    view.title = np_label(panel, "Notificações", NP_FONT_TITLE, np_c_text(),
                          28, 16, 480, LV_TEXT_ALIGN_LEFT);
    view.summary = np_label(panel, "Histórico recente", NP_FONT_SM, np_c_text_2(),
                            28, 56, 500, LV_TEXT_ALIGN_LEFT);
    view.close_button = np_button(panel, 756, 16, 96, 44, "Fechar", false);
    view.empty = np_label(panel, "Nenhuma notificação por enquanto.", NP_FONT_MD,
                          np_c_text_2(), 28, 270, 824, LV_TEXT_ALIGN_CENTER);

    view.list = np_group(panel, 20, 104, 840, 382);
    if (view.list == NULL) {
        lv_obj_delete(view.root);
        return (np_notification_center_t){0};
    }
    lv_obj_add_flag(view.list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(view.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(view.list, LV_SCROLLBAR_MODE_AUTO);

    for (uint8_t i = 0U; i < APP_NOTIFICATION_HISTORY_MAX; ++i) {
        const int32_t y = (int32_t)i * 76;
        view.rows[i] = np_fill(view.list, 0, y, 824, 68,
                               np_c_surface_raised(), LV_OPA_COVER,
                               NP_RADIUS_CONTROL);
        if (view.rows[i] == NULL) continue;
        lv_obj_add_flag(view.rows[i], LV_OBJ_FLAG_CLICKABLE);
        view.row_unread[i] = np_dot(view.rows[i], 14, 15, 9, np_c_accent());
        view.row_titles[i] = np_label(view.rows[i], "", NP_FONT_SM, np_c_text(),
                                      38, 8, 520, LV_TEXT_ALIGN_LEFT);
        view.row_times[i] = np_label(view.rows[i], "", NP_FONT_SM, np_c_text_2(),
                                     590, 8, 216, LV_TEXT_ALIGN_RIGHT);
        view.row_details[i] = np_label(view.rows[i], "", NP_FONT_SM, np_c_text_2(),
                                       38, 38, 768, LV_TEXT_ALIGN_LEFT);
        np_set_visible(view.rows[i], false);
    }
    view.mark_all_button = np_button(panel, 20, 506, 320, 48,
                                     "Marcar todas como lidas", false);
    return view;
}

void np_notification_center_sync(np_notification_center_t *view,
                                 const app_notification_center_projection_t *state,
                                 uint32_t current_unix_s)
{
    if (view == NULL || state == NULL || view->root == NULL) return;
    char summary[48] = {0};
    (void)snprintf(summary, sizeof(summary), "%u %s | %u no histórico",
                   (unsigned)state->unread_count,
                   state->unread_count == 1U ? "nova" : "novas",
                   (unsigned)state->count);
    np_set_text(view->summary, summary);
    np_set_visible(view->empty, state->count == 0U);
    np_set_visible(view->mark_all_button, state->unread_count != 0U);

    for (uint8_t row = 0U; row < APP_NOTIFICATION_HISTORY_MAX; ++row) {
        if (view->rows[row] == NULL) continue;
        const bool has_item = row < state->count;
        np_set_visible(view->rows[row], has_item);
        view->row_ids[row] = 0U;
        if (!has_item) continue;

        const app_notification_item_t *const item =
            &state->items[state->count - 1U - row];
        view->row_ids[row] = item->id;
        np_set_text(view->row_titles[row], notification_title(item->kind));
        char age[28] = {0};
        format_notification_age(item->timestamp_unix_s, current_unix_s,
                                age, sizeof(age));
        np_set_text(view->row_times[row], age);
        np_set_text(view->row_details[row], notification_detail(item->kind));
        np_set_visible(view->row_unread[row], item->unread);
        np_set_text_color(view->row_titles[row],
                          item->unread ? np_c_text() : np_c_text_2());
    }
}
