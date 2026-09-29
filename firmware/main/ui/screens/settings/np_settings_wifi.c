#include "np_settings_wifi.h"

#include <stdio.h>
#include <string.h>

#define SCENE_WIFI_ROWS 4U

#define SETTINGS_WIFI_MODAL_X 160
#define SETTINGS_WIFI_MODAL_Y 42
#define SETTINGS_WIFI_MODAL_W 704
#define SETTINGS_WIFI_MODAL_H 516

static void row_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        np_settings_wifi_show(lv_event_get_user_data(event));
    }
}

static void scene_details(np_settings_wifi_view_t *view);

static void select_row(np_settings_wifi_t *wifi, uint8_t selected)
{
    if (wifi->scene == NULL && wifi->selected_index == selected) return;
    wifi->selected_index = selected;
    if (selected < NP_SETTINGS_WIFI_VISIBLE_RESULTS) {
        (void)snprintf(wifi->selected_ssid, sizeof(wifi->selected_ssid), "%s",
                       lv_label_get_text(wifi->network_names[selected]));
        lv_obj_remove_state(wifi->connect_button, LV_STATE_DISABLED);
    } else {
        wifi->selected_ssid[0] = '\0';
        lv_obj_add_state(wifi->connect_button, LV_STATE_DISABLED);
    }
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        if (wifi->network_rows[i] == NULL) continue;
        const bool active = i == selected;
        np_set_bg_color(wifi->network_rows[i], active ? np_c_accent_bg() : np_c_surface_raised());
        lv_obj_set_style_border_color(wifi->network_rows[i],
                                      active ? np_c_accent() : np_c_hairline(), 0);
        lv_obj_set_style_border_width(wifi->network_rows[i], active ? 2 : 1, 0);
    }
    if (wifi->scene != NULL) {
        scene_details(wifi->scene);
        return;
    }
    np_set_text(lv_obj_get_child(wifi->connect_button, 0),
                selected < NP_SETTINGS_WIFI_VISIBLE_RESULTS ? "Conectar rede" : "Selecione uma rede");
}

static void network_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    np_settings_wifi_t *const wifi = lv_event_get_user_data(event);
    if (wifi == NULL) return;
    lv_obj_t *const row = lv_event_get_target(event);
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        if (wifi->network_rows[i] == row) {
            select_row(wifi, i);
            return;
        }
    }
}

static lv_obj_t *network_lock(lv_obj_t *parent)
{
    return np_label(parent, NP_ICON_LOCK, NP_FONT_ICON, np_c_text_2(),
                    246, 12, 20, LV_TEXT_ALIGN_CENTER);
}

static lv_obj_t *network_row(np_settings_wifi_t *wifi, uint8_t index)
{
    lv_obj_t *const row = np_fill(wifi->modal.content, 360, 70 + index * 49,
                                  320, 48, np_c_surface_raised(), LV_OPA_COVER,
                                  NP_RADIUS_CONTROL);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, np_c_hairline(), 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    wifi->network_names[index] = np_label(row, "", NP_FONT_SM, np_c_text(),
                                          14, 16, 224, LV_TEXT_ALIGN_LEFT);
    wifi->network_locks[index] = network_lock(row);
    wifi->network_signals[index] = np_label(row, "", NP_FONT_ICON, np_c_text_2(),
                                            278, 12, 28, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_event_cb(row, network_event_cb, LV_EVENT_CLICKED, wifi);
    return row;
}

void np_settings_wifi_create(np_settings_wifi_t *wifi, lv_obj_t *parent)
{
    if (wifi == NULL || parent == NULL) return;
    *wifi = (np_settings_wifi_t){0};
    wifi->selected_index = NP_SETTINGS_WIFI_VISIBLE_RESULTS;
    np_modal_create(&wifi->modal, parent,
                    SETTINGS_WIFI_MODAL_X, SETTINGS_WIFI_MODAL_Y,
                    SETTINGS_WIFI_MODAL_W, SETTINGS_WIFI_MODAL_H,
                    NP_ICON_WIFI, np_c_positive(), "Wi-Fi", "Rede e conectividade");

    lv_obj_t *const content = wifi->modal.content;
    if (content == NULL) return;

    lv_obj_t *const status = np_fill(content, 24, 20, 316, 306,
                                     np_c_surface_raised(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    np_label(status, "Status", NP_FONT_SM, np_c_text_3(), 28, 28, 220, LV_TEXT_ALIGN_LEFT);
    wifi->status_icon = np_label(status, NP_ICON_WIFI_OFF, NP_FONT_ICON, np_c_text_3(),
                                 28, 64, 28, LV_TEXT_ALIGN_CENTER);
    wifi->detail_value = np_label(status, "Nenhuma rede associada", NP_FONT_MD,
                                  np_c_text_2(), 68, 69, 220, LV_TEXT_ALIGN_LEFT);
    np_hline(status, 28, 119, 260);
    np_label(status, "Credenciais protegidas pelo servico", NP_FONT_SM,
             np_c_text_3(), 28, 145, 260, LV_TEXT_ALIGN_LEFT);
    wifi->manage_button = np_button(status, 28, 204, 260, 52, "Gerenciar redes", false);
    wifi->forget_button = np_button(content, 24, 342, 316, 52, "Esquecer rede", false);

    lv_obj_t *const networks = np_fill(content, 360, 20, 320, 306,
                                       np_c_surface_raised(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    np_label(networks, "Redes disponiveis", NP_FONT_MD, np_c_text_2(),
             16, 21, 230, LV_TEXT_ALIGN_LEFT);
    wifi->scan_button = np_icon_button(networks, 264, 12, 42, NP_ICON_SEARCH);
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        wifi->network_rows[i] = network_row(wifi, i);
        np_set_visible(wifi->network_rows[i], false);
    }
    wifi->connect_button = np_button(content, 360, 342, 320, 52, "Selecione uma rede", true);
    lv_obj_add_state(wifi->connect_button, LV_STATE_DISABLED);
    lv_obj_set_style_opa(wifi->connect_button, LV_OPA_40, LV_STATE_DISABLED);
}

void np_settings_wifi_show(np_settings_wifi_t *wifi)
{
    if (wifi != NULL) np_modal_show(&wifi->modal);
}

void np_settings_wifi_hide(np_settings_wifi_t *wifi)
{
    if (wifi != NULL) np_modal_hide(&wifi->modal);
}

void np_settings_wifi_bind_row(np_settings_wifi_t *wifi, lv_obj_t *row)
{
    if (wifi != NULL && row != NULL) {
        lv_obj_add_event_cb(row, row_event_cb, LV_EVENT_CLICKED, wifi);
    }
}

void np_settings_wifi_sync(np_settings_wifi_t *wifi, np_wifi_status_t status,
                           const char *connected_ssid,
                           uint8_t scan_results_count,
                           const connectivity_scan_result_t *scan_results)
{
    if (wifi == NULL) return;
    const char *status_icon = NP_ICON_WIFI_OFF;
    const char *status_text = "Nenhuma rede associada";
    lv_color_t status_color = np_c_text_3();
    switch (status) {
    case NP_WIFI_STATUS_ONLINE:
        status_icon = NP_ICON_WIFI;
        status_text = connected_ssid != NULL && connected_ssid[0] != '\0'
                          ? connected_ssid : "Conectado";
        status_color = np_c_positive();
        break;
    case NP_WIFI_STATUS_ASSOCIATED_PENDING_IP:
        status_icon = NP_ICON_WIFI_OFF;
        status_text = connected_ssid != NULL && connected_ssid[0] != '\0'
                          ? connected_ssid : "Aguardando IP";
        status_color = np_c_warning();
        break;
    case NP_WIFI_STATUS_CONNECTING:
        status_icon = NP_ICON_SEARCH;
        status_text = "Conectando";
        status_color = np_c_accent();
        break;
    case NP_WIFI_STATUS_FAILED:
        status_icon = NP_ICON_WARNING;
        status_text = "Conexao indisponivel";
        status_color = np_c_negative();
        break;
    case NP_WIFI_STATUS_OFFLINE:
    default:
        break;
    }
    np_set_text(wifi->status_icon, status_icon);
    np_set_text_color(wifi->status_icon, status_color);
    np_set_text(wifi->detail_value, status_text);
    np_set_text_color(wifi->detail_value, status_color);

    /* Settings projects network state before this lazy modal exists. */
    if (wifi->network_rows[0] == NULL) return;

    const uint8_t count = scan_results_count < NP_SETTINGS_WIFI_VISIBLE_RESULTS
                              ? scan_results_count : NP_SETTINGS_WIFI_VISIBLE_RESULTS;
    uint8_t selected = NP_SETTINGS_WIFI_VISIBLE_RESULTS;
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        const bool visible = scan_results != NULL && i < count &&
                             scan_results[i].ssid[0] != '\0';
        np_set_visible(wifi->network_rows[i], visible);
        if (!visible) continue;

        np_set_text(wifi->network_names[i], scan_results[i].ssid);
        np_set_visible(wifi->network_locks[i], scan_results[i].secure);
        np_set_text(wifi->network_signals[i], np_wifi_signal_icon(scan_results[i].rssi));
        wifi->network_secure[i] = scan_results[i].secure;
        if (strcmp(wifi->selected_ssid, scan_results[i].ssid) == 0 &&
            (selected == NP_SETTINGS_WIFI_VISIBLE_RESULTS || i == wifi->selected_index)) selected = i;
    }
    select_row(wifi, selected);
}

bool np_settings_wifi_copy_selected_ssid(const np_settings_wifi_t *wifi,
                                         char *out_ssid, size_t out_size)
{
    if (wifi == NULL || out_ssid == NULL || out_size == 0U ||
        wifi->selected_index >= NP_SETTINGS_WIFI_VISIBLE_RESULTS ||
        wifi->network_names[wifi->selected_index] == NULL ||
        lv_obj_has_flag(wifi->network_rows[wifi->selected_index], LV_OBJ_FLAG_HIDDEN)) return false;
    const char *const ssid = wifi->selected_ssid;
    const size_t length = strnlen(ssid, out_size);
    if (length == 0U || length >= out_size) return false;
    memcpy(out_ssid, ssid, length + 1U);
    return true;
}

/* Dedicated management scene. Only four rows exist, even with 40 scan results. */
static const connectivity_scan_result_t *selected_result(const np_settings_wifi_view_t *view)
{
    const uint8_t index = view->page * SCENE_WIFI_ROWS + view->wifi.selected_index;
    if (view->wifi.selected_index < SCENE_WIFI_ROWS && index < view->count &&
        strcmp(view->wifi.selected_ssid, view->results[index].ssid) == 0)
        return &view->results[index];
    for (uint8_t i = 0; i < view->count; ++i) {
        if (strcmp(view->wifi.selected_ssid, view->results[i].ssid) == 0)
            return &view->results[i];
    }
    return NULL;
}

static void scene_details(np_settings_wifi_view_t *view)
{
    const connectivity_scan_result_t *result = selected_result(view);
    const char *ssid = view->wifi.selected_ssid;
    const bool selected = ssid[0] != '\0';
    const bool current = selected && strcmp(ssid, view->connected_ssid) == 0;
    np_set_text(view->selected_name, selected ? ssid : "Selecione uma rede");
    const char *state = !selected ? "Toque em uma rede da lista"
        : current && view->status == NP_WIFI_STATUS_ONLINE ? "Conectado"
        : current ? "Aguardando IP"
        : view->status == NP_WIFI_STATUS_CONNECTING ? "Conexão em andamento"
        : view->status == NP_WIFI_STATUS_FAILED ? "Falha na conexão" : "Desconectado";
    np_set_text(view->selected_status, state);
    np_set_text_color(view->selected_status, current && view->status == NP_WIFI_STATUS_ONLINE
        ? np_c_positive() : view->status == NP_WIFI_STATUS_FAILED ? np_c_negative() : np_c_text_2());
    np_set_text(view->selected_icon, result != NULL ? np_wifi_signal_icon(result->rssi) : NP_ICON_WIFI_OFF);
    np_set_text_color(view->selected_icon, np_c_accent());
    char signal[64] = {0};
    if (result != NULL) snprintf(signal, sizeof(signal), "%d dBm · última busca", (int)result->rssi);
    np_set_text(view->signal, result != NULL ? signal : "Não disponível");
    np_set_text(view->security, result != NULL ? (result->secure ? "Protegida" : "Aberta") : "Não disponível");
    np_set_text(view->ip, current && view->status == NP_WIFI_STATUS_ONLINE && view->ip_address[0]
        ? view->ip_address : "Não disponível");
    np_set_text(view->gateway, current && view->status == NP_WIFI_STATUS_ONLINE && view->gateway_address[0]
        ? view->gateway_address : "Não disponível");
    if (result != NULL && !current && view->status != NP_WIFI_STATUS_CONNECTING)
        lv_obj_remove_state(view->wifi.connect_button, LV_STATE_DISABLED);
    else lv_obj_add_state(view->wifi.connect_button, LV_STATE_DISABLED);
    /* FORGET acts on the active station, never an arbitrary scanned SSID. */
    if (current) lv_obj_remove_state(view->wifi.forget_button, LV_STATE_DISABLED);
    else lv_obj_add_state(view->wifi.forget_button, LV_STATE_DISABLED);
}

static void scene_page(np_settings_wifi_view_t *view)
{
    const uint8_t pages = view->count == 0U ? 1U :
        (view->count + SCENE_WIFI_ROWS - 1U) / SCENE_WIFI_ROWS;
    if (view->page >= pages) view->page = pages - 1U;
    const uint8_t start = view->page * SCENE_WIFI_ROWS;
    const uint8_t remaining = view->count > start ? (uint8_t)(view->count - start) : 0U;
    const uint8_t count = remaining < SCENE_WIFI_ROWS ? remaining : SCENE_WIFI_ROWS;
    const bool selected_current = view->connected_ssid[0] &&
        strcmp(view->wifi.selected_ssid, view->connected_ssid) == 0;
    np_settings_wifi_sync(&view->wifi, view->status, view->connected_ssid, count, view->results + start);
    if (selected_current) snprintf(view->wifi.selected_ssid, sizeof(view->wifi.selected_ssid),
        "%s", view->connected_ssid);
    char page[40] = {0};
    snprintf(page, sizeof(page), "%u/%u · %u redes", (unsigned)view->page + 1U,
        (unsigned)pages, (unsigned)view->count);
    np_set_text(view->page_label, page);
    np_set_visible(view->empty_label, view->count == 0U);
    if (view->page == 0U) lv_obj_add_state(view->previous_page, LV_STATE_DISABLED);
    else lv_obj_remove_state(view->previous_page, LV_STATE_DISABLED);
    if (view->page + 1U >= pages) lv_obj_add_state(view->next_page, LV_STATE_DISABLED);
    else lv_obj_remove_state(view->next_page, LV_STATE_DISABLED);
    np_set_text(view->current_status, view->status == NP_WIFI_STATUS_ONLINE ? "Conectado"
        : view->connected_ssid[0] ? "Aguardando IP"
        : view->status == NP_WIFI_STATUS_CONNECTING ? "Conectando..." : "Sem conexão");
    np_set_text_color(view->current_status, view->status == NP_WIFI_STATUS_ONLINE
        ? np_c_positive() : np_c_text_2());
    /* The tile always identifies the actual association, not the selection. */
    np_set_text(view->current_name, view->connected_ssid[0] ? view->connected_ssid : "Nenhuma rede associada");
    np_set_text_color(view->current_name, np_c_text());
    for (uint8_t i = 0; i < count; ++i) {
        np_set_text_color(view->wifi.network_signals[i], view->connected_ssid[0] &&
            strcmp(view->results[start + i].ssid, view->connected_ssid) == 0
                ? np_c_positive() : np_c_text_2());
    }
    for (uint8_t i = 0; i < view->count; ++i) {
        if (view->connected_ssid[0] && strcmp(view->connected_ssid, view->results[i].ssid) == 0) {
            np_set_text(view->current_icon, np_wifi_signal_icon(view->results[i].rssi));
            break;
        }
    }
    scene_details(view);
}

static void page_event(lv_event_t *event)
{
    np_settings_wifi_view_t *view = lv_event_get_user_data(event);
    if (lv_event_get_target(event) == view->previous_page) {
        if (view->page == 0U) return;
        --view->page;
    } else {
        if ((view->page + 1U) * SCENE_WIFI_ROWS >= view->count) return;
        ++view->page;
    }
    view->wifi.selected_ssid[0] = '\0';
    scene_page(view);
}

static void current_event(lv_event_t *event)
{
    np_settings_wifi_view_t *view = lv_event_get_user_data(event);
    if (!view->connected_ssid[0]) return;
    for (uint8_t i = 0; i < view->count; ++i) {
        if (strcmp(view->connected_ssid, view->results[i].ssid) == 0) {
            view->page = i / SCENE_WIFI_ROWS;
            break;
        }
    }
    snprintf(view->wifi.selected_ssid, sizeof(view->wifi.selected_ssid), "%s", view->connected_ssid);
    scene_page(view);
    /* The current AP may not appear in the last scan; its IP remains real. */
    snprintf(view->wifi.selected_ssid, sizeof(view->wifi.selected_ssid), "%s", view->connected_ssid);
    scene_details(view);
}

static lv_obj_t *detail_row(lv_obj_t *parent, int32_t y, const char *icon, const char *title)
{
    np_label(parent, icon, NP_FONT_ICON, np_c_text_2(), 24, y + 10, 32, LV_TEXT_ALIGN_CENTER);
    np_label(parent, title, NP_FONT_SM, np_c_text_2(), 80, y, 256, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *value = np_label(parent, "Não disponível", NP_FONT_SM, np_c_text_2(),
        80, y + 25, 256, LV_TEXT_ALIGN_LEFT);
    return value;
}

np_settings_wifi_view_t np_settings_wifi_scene_build(lv_obj_t *parent)
{
    np_settings_wifi_view_t view = {0};
    view.root = np_scene(parent);
    view.header = np_header(view.root);
    view.back_button = np_button(view.root, 250, 12, 160, NP_TOUCH_TARGET, "Voltar", false);
    view.wifi.selected_index = NP_SETTINGS_WIFI_VISIBLE_RESULTS;
    lv_obj_t *main = np_panel(view.root, 24, 80, 600, 496);
    np_label(main, NP_ICON_WIFI, NP_FONT_ICON_BADGE, np_c_accent(), 24, 26, 56, LV_TEXT_ALIGN_CENTER);
    np_label(main, "Wi-Fi", NP_FONT_TITLE, np_c_text(), 96, 16, 480, LV_TEXT_ALIGN_LEFT);
    view.add_button = np_button(main, 420, 16, 164, 44, "Adicionar rede", false);
    np_label(main, "Conexão e gerenciamento de redes", NP_FONT_SM, np_c_text_2(),
        96, 60, 480, LV_TEXT_ALIGN_LEFT);
    np_hline(main, 16, 96, 568);
    np_label(main, "Rede atual", NP_FONT_SM, np_c_text_2(), 20, 110, 400, LV_TEXT_ALIGN_LEFT);
    view.current_row = np_fill(main, 16, 138, 568, 68, np_c_surface_raised(), LV_OPA_COVER, NP_RADIUS_CONTROL);
    lv_obj_add_flag(view.current_row, LV_OBJ_FLAG_CLICKABLE);
    view.current_icon = np_label(view.current_row, NP_ICON_WIFI_OFF, NP_FONT_ICON,
        np_c_positive(), 24, 20, 36, LV_TEXT_ALIGN_CENTER);
    view.current_name = np_label(view.current_row, "Nenhuma rede associada", NP_FONT_LG,
        np_c_text(), 88, 9, 416, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(view.current_name, NP_FONT_LG->line_height);
    view.current_status = np_label(view.current_row, "Sem conexão", NP_FONT_SM,
        np_c_text_2(), 88, 38, 416, LV_TEXT_ALIGN_LEFT);
    np_label(view.current_row, NP_ICON_ARROW_RIGHT, NP_FONT_ICON, np_c_text_2(), 522, 22, 28, LV_TEXT_ALIGN_CENTER);
    np_label(main, "Redes disponíveis", NP_FONT_SM, np_c_text_2(), 20, 222, 400, LV_TEXT_ALIGN_LEFT);
    view.wifi.scan_button = np_icon_button(main, 538, 211, NP_TOUCH_TARGET, NP_ICON_SEARCH);
    for (uint8_t i = 0; i < SCENE_WIFI_ROWS; ++i) {
        lv_obj_t *row = np_fill(main, 16, 250 + i * 48, 568, 48,
            np_c_surface_raised(), LV_OPA_COVER, NP_RADIUS_CONTROL);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        view.wifi.network_rows[i] = row;
        view.wifi.network_signals[i] = np_label(row, "", NP_FONT_ICON, np_c_text_2(), 20, 11, 36, LV_TEXT_ALIGN_CENTER);
        view.wifi.network_names[i] = np_label(row, "", NP_FONT_MD, np_c_text(), 88, 11, 416, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_height(view.wifi.network_names[i], NP_FONT_MD->line_height);
        view.wifi.network_locks[i] = np_label(row, NP_ICON_LOCK, NP_FONT_ICON, np_c_text_2(), 524, 11, 28, LV_TEXT_ALIGN_CENTER);
        np_set_visible(row, false);
    }
    view.empty_label = np_label(main, "Toque na lupa para buscar redes", NP_FONT_SM,
        np_c_text_3(), 20, 280, 540, LV_TEXT_ALIGN_CENTER);
    view.previous_page = np_icon_button(main, 16, 448, NP_TOUCH_TARGET, NP_ICON_ARROW_LEFT);
    view.next_page = np_icon_button(main, 538, 448, NP_TOUCH_TARGET, NP_ICON_ARROW_RIGHT);
    /* Footer buttons fit inside the card; no unnecessary final divider. */
    view.page_label = np_label(main, "", NP_FONT_SM, np_c_text_3(), 80, 470, 440, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *details = np_panel(view.root, 640, 80, 360, 496);
    view.selected_icon = np_label(details, NP_ICON_WIFI_OFF, NP_FONT_ICON_BADGE,
        np_c_accent(), 24, 32, 48, LV_TEXT_ALIGN_CENTER);
    view.selected_name = np_label(details, "Selecione uma rede", NP_FONT_LG, np_c_text(),
        88, 28, 248, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(view.selected_name, NP_FONT_LG->line_height);
    view.selected_status = np_label(details, "", NP_FONT_SM, np_c_text_2(), 88, 60, 248, LV_TEXT_ALIGN_LEFT);
    np_hline(details, 16, 96, 328);
    view.signal = detail_row(details, 112, NP_ICON_SIGNAL, "Sinal");
    np_hline(details, 80, 172, 256);
    view.security = detail_row(details, 184, NP_ICON_LOCK, "Segurança");
    np_hline(details, 80, 244, 256);
    view.ip = detail_row(details, 256, NP_ICON_INFO, "Endereço IP");
    np_hline(details, 80, 316, 256);
    view.gateway = detail_row(details, 328, NP_ICON_ROUTER, "Gateway");
    view.wifi.connect_button = np_button(details, 16, 396, 328, 44, NP_ICON_LINK "  Conectar", true);
    view.wifi.forget_button = np_button(details, 16, 448, 328, 44, NP_ICON_DELETE "  Esquecer", false);
    lv_obj_set_style_opa(view.wifi.connect_button, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_set_style_opa(view.wifi.forget_button, LV_OPA_40, LV_STATE_DISABLED);
    view.wifi.status_icon = view.current_icon;
    view.wifi.detail_value = view.current_name;
    return view;
}

void np_settings_wifi_scene_bind(np_settings_wifi_view_t *view)
{
    if (view == NULL || view->root == NULL || view->wifi.scene != NULL) return;
    view->wifi.scene = view;
    for (uint8_t i = 0; i < SCENE_WIFI_ROWS; ++i)
        lv_obj_add_event_cb(view->wifi.network_rows[i], network_event_cb, LV_EVENT_CLICKED, &view->wifi);
    lv_obj_add_event_cb(view->previous_page, page_event, LV_EVENT_CLICKED, view);
    lv_obj_add_event_cb(view->next_page, page_event, LV_EVENT_CLICKED, view);
    lv_obj_add_event_cb(view->current_row, current_event, LV_EVENT_CLICKED, view);
}

void np_settings_wifi_scene_sync(np_settings_wifi_view_t *view, np_wifi_status_t status,
    const char *connected_ssid, uint8_t count, const connectivity_scan_result_t *results,
    const char *ip, const char *gateway, bool scanning)
{
    if (view == NULL || view->root == NULL) return;
    view->status = status;
    view->count = results == NULL ? 0U : count < CONNECTIVITY_DIAGNOSTIC_MAX_SCAN_RESULTS
        ? count : CONNECTIVITY_DIAGNOSTIC_MAX_SCAN_RESULTS;
    if (view->count) memcpy(view->results, results, view->count * sizeof(view->results[0]));
    snprintf(view->connected_ssid, sizeof(view->connected_ssid), "%s", connected_ssid != NULL ? connected_ssid : "");
    snprintf(view->ip_address, sizeof(view->ip_address), "%s", ip != NULL ? ip : "");
    snprintf(view->gateway_address, sizeof(view->gateway_address), "%s", gateway != NULL ? gateway : "");
    scene_page(view);
    np_set_text(view->empty_label, scanning ? "Buscando redes..." : "Nenhuma rede na última busca. Toque na lupa.");
    if (scanning || status == NP_WIFI_STATUS_CONNECTING) lv_obj_add_state(view->wifi.scan_button, LV_STATE_DISABLED);
    else lv_obj_remove_state(view->wifi.scan_button, LV_STATE_DISABLED);
}

static void add_closed(void *user_data)
{
    np_settings_wifi_add_t *view = user_data;
    np_keyboard_hide(view->keyboard);
    lv_textarea_set_text(view->ssid, "");
}

static void add_ssid_clicked(lv_event_t *event)
{
    np_settings_wifi_add_t *view = lv_event_get_user_data(event);
    np_keyboard_focus(view->keyboard, view->ssid, NP_KEYBOARD_MODE_TEXT);
}

void np_settings_wifi_add_create(np_settings_wifi_add_t *view, lv_obj_t *parent,
                                 np_keyboard_t *keyboard)
{
    *view = (np_settings_wifi_add_t){.keyboard = keyboard};
    np_modal_create(&view->modal, parent, 288, 82, 448, 280,
        NP_ICON_WIFI, np_c_accent(), "Adicionar rede", "Informe o nome exato da rede");
    view->ssid = np_form_text_input(view->modal.content, 24, 12, 400, 48, "SSID", NULL);
    lv_textarea_set_max_length(view->ssid, 32);
    np_keyboard_bind(keyboard, view->ssid, NP_KEYBOARD_MODE_TEXT);
    lv_obj_add_event_cb(view->ssid, add_ssid_clicked, LV_EVENT_CLICKED, view);
    np_label(view->modal.content, "Rede protegida (WPA2)", NP_FONT_SM,
        np_c_text_2(), 24, 86, 304, LV_TEXT_ALIGN_LEFT);
    view->secure_switch = lv_switch_create(view->modal.content);
    lv_obj_set_pos(view->secure_switch, 352, 74);
    lv_obj_set_size(view->secure_switch, 64, 36);
    lv_obj_add_state(view->secure_switch, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(view->secure_switch, np_c_accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    view->continue_button = np_button(view->modal.content, 24, 136, 400, 48, "Continuar", true);
    np_modal_set_close_callback(&view->modal, add_closed, view);
    np_modal_show(&view->modal);
    np_keyboard_focus(keyboard, view->ssid, NP_KEYBOARD_MODE_TEXT);
}
