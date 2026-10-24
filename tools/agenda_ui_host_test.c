#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui/screens/np_agenda.h"
#include "app_event_bus.h"

static agenda_command_t posted;
static unsigned posts;
static esp_err_t post_result;
static unsigned link_requests;
esp_err_t app_event_bus_post_agenda(const agenda_command_t *command)
{ posted=*command; ++posts; return post_result; }
esp_err_t app_event_bus_post_agenda_sync(void) { return ESP_OK; }
esp_err_t provisioning_service_arm_open_network(void) { ++link_requests; return ESP_OK; }
static uint8_t pixels[1024*600*2];
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }
static void advance(void) { for (unsigned i=0;i<20;++i) { lv_tick_inc(10); lv_timer_handler(); } }
static void screenshot(const char *path)
{ advance(); FILE *f=fopen(path,"wb"); assert(f != NULL); assert(fwrite(pixels,1,sizeof(pixels),f) == sizeof(pixels)); fclose(f); }
int main(int argc,char **argv)
{
    setbuf(stdout,NULL);
    lv_init(); lv_display_t *display=lv_display_create(1024,600);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,flush);
    np_keyboard_t keyboard=np_keyboard_create(lv_screen_active());
    lv_mem_monitor_t before_view,after_view;
    lv_mem_monitor(&before_view);
    np_agenda_view_t view=np_agenda_build(lv_screen_active());
    lv_mem_monitor(&after_view);
    const size_t view_bytes=before_view.free_size-after_view.free_size;
    printf("Host agenda LVGL allocation=%zu bytes (64-bit pointers; target measured separately)\n",view_bytes);
    assert(view_bytes <= 40U*1024U);
    np_header_t header=np_header(lv_screen_active()); np_header_set_drawer_agenda_active(&header);
    np_clock_set(&header.clock,"12:30");
    np_agenda_bind(&view,&keyboard);
    assert(view.editor.scrim == NULL && view.title == NULL);
    agenda_projection_t state={.ready=true,.storage_result=ESP_OK};
    np_agenda_sync(&view,&state); assert(!lv_obj_has_flag(view.empty,LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(view.link,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_state(view.sync,LV_STATE_DISABLED));
    state.google_linked=true; np_agenda_sync(&view,&state);
    assert(lv_obj_has_flag(view.link,LV_OBJ_FLAG_HIDDEN));
    lv_obj_send_event(view.link,LV_EVENT_CLICKED,NULL); assert(link_requests == 0U);
    state.sync_result=ESP_ERR_NO_MEM; np_agenda_sync(&view,&state);
    assert(lv_obj_has_flag(view.link,LV_OBJ_FLAG_HIDDEN));
    state.google_auth_required=true; state.sync_result=AGENDA_SYNC_AUTH_REQUIRED;
    np_agenda_sync(&view,&state); assert(!lv_obj_has_flag(view.link,LV_OBJ_FLAG_HIDDEN));
    lv_obj_send_event(view.link,LV_EVENT_CLICKED,NULL); assert(link_requests == 1U);
    state.sync_busy=true; np_agenda_sync(&view,&state);
    assert(!lv_obj_has_flag(view.link,LV_OBJ_FLAG_HIDDEN) && lv_obj_has_state(view.link,LV_STATE_DISABLED));
    state.sync_busy=false; state.google_auth_required=false; state.sync_result=ESP_OK;
    np_agenda_sync(&view,&state); assert(lv_obj_has_flag(view.link,LV_OBJ_FLAG_HIDDEN));
    lv_tick_inc(60001U); np_agenda_sync(&view,&state);
    assert(!view.notice_active && !lv_label_get_text(view.notice)[0]);
    lv_mem_monitor_t before_modal,after_modal;
    lv_mem_monitor(&before_modal);
    lv_obj_send_event(view.new_event,LV_EVENT_CLICKED,NULL);
    lv_mem_monitor(&after_modal);
    const size_t editor_bytes=before_modal.free_size-after_modal.free_size;
    printf("Host lazy modal allocation=%zu; combined view=%zu bytes\n",editor_bytes,view_bytes+editor_bytes);
    assert(view_bytes+editor_bytes <= 48U*1024U);
    assert(np_modal_is_visible(&view.editor));
    lv_textarea_set_text(view.title,"Reunião de projeto");
    lv_textarea_set_text(view.date,"31/02/2026"); lv_textarea_set_text(view.start,"10:00");
    lv_textarea_set_text(view.end_date,"19/10/2026"); lv_textarea_set_text(view.end,"11:00");
    lv_obj_send_event(view.save,LV_EVENT_CLICKED,NULL); assert(posts == 0U);
    lv_textarea_set_text(view.date,"19/10/2026");
    lv_obj_send_event(view.save,LV_EVENT_CLICKED,NULL); assert(posts == 1U && view.pending_request != 0U);
    const uint32_t pending_id=view.pending_request;
    lv_obj_send_event(view.cancel,LV_EVENT_CLICKED,NULL);
    assert(!np_modal_is_visible(&view.editor) && view.pending_request == pending_id);
    lv_obj_send_event(view.new_task,LV_EVENT_CLICKED,NULL);
    assert(view.draft.kind == AGENDA_EVENT && posts == 1U);
    state.completion_id=posted.request_id; state.command_result=AGENDA_OK; state.count=1U;
    state.items[0]=posted.item; state.items[0].id=1U; state.items[0].revision=1U;
    state.persistence_pending=true;
    np_agenda_sync(&view,&state); assert(view.pending_request == 0U && !view.creating);
    assert(!np_modal_is_visible(&view.editor));
    assert(view.persistence_feedback && strstr(lv_label_get_text(view.message),"Salvamento pendente"));
    np_agenda_sync(&view,&state);
    assert(strstr(lv_label_get_text(view.message),"Salvamento pendente"));
    lv_obj_send_event(view.filters[1],LV_EVENT_CLICKED,NULL);
    assert(view.persistence_feedback && strstr(lv_label_get_text(view.message),"Salvamento pendente"));
    lv_obj_send_event(view.filters[0],LV_EVENT_CLICKED,NULL);
    state.storage_result=-1; np_agenda_sync(&view,&state);
    assert(view.persistence_feedback && strstr(lv_label_get_text(view.message),"Falha ao salvar"));
    state.storage_result=ESP_OK; np_agenda_sync(&view,&state);
    assert(strstr(lv_label_get_text(view.message),"Salvamento pendente"));
    state.persistence_pending=false; state.storage_result=ESP_ERR_NOT_FOUND; np_agenda_sync(&view,&state);
    assert(view.persistence_feedback && strstr(lv_label_get_text(view.message),"Aguardando confirmação"));
    state.storage_result=ESP_OK; np_agenda_sync(&view,&state);
    assert(!view.persistence_feedback && !strcmp(lv_label_get_text(view.message),"Alteração salva no painel."));
    lv_obj_send_event(view.rows[0],LV_EVENT_CLICKED,NULL);
    lv_obj_send_event(view.remove,LV_EVENT_CLICKED,NULL); assert(posts == 1U);
    np_agenda_sync(&view,&state);
    assert(strstr(lv_label_get_text(view.message),"Confirme para excluir"));
    lv_obj_send_event(view.remove,LV_EVENT_CLICKED,NULL); assert(posts == 2U && posted.action == AGENDA_DELETE);
    state.completion_id=posted.request_id; state.count=0U; np_agenda_sync(&view,&state);
    lv_obj_send_event(view.new_task,LV_EVENT_CLICKED,NULL);
    assert(lv_obj_has_flag(view.end_date,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(view.end,LV_OBJ_FLAG_HIDDEN));
    assert(!strcmp(lv_label_get_text(view.editor_title),"Nova tarefa"));
    advance();
    assert(lv_obj_get_height(view.editor.panel) == 400);
    {
        lv_area_t task_date, keyboard_position;
        np_keyboard_focus(&keyboard,view.date,NP_KEYBOARD_MODE_TEXT); advance();
        lv_obj_get_coords(view.date,&task_date); lv_obj_get_coords(keyboard.root,&keyboard_position);
        assert(task_date.y2 < keyboard_position.y1);
        np_keyboard_hide(&keyboard);
    }
    lv_textarea_set_text(view.title,"Comprar componentes"); lv_textarea_set_text(view.date,"");
    lv_obj_send_event(view.save,LV_EVENT_CLICKED,NULL);
    assert(posts == 3U && posted.item.kind == AGENDA_TASK && posted.item.start_unix_s == 0U && posted.item.end_unix_s == 0U);
    state.completion_id=posted.request_id; state.count=1U; state.items[0]=posted.item;
    state.items[0].id=2U; state.items[0].revision=1U; np_agenda_sync(&view,&state);
    assert(!np_modal_is_visible(&view.editor));
    lv_obj_send_event(view.rows[0],LV_EVENT_CLICKED,NULL);
    lv_obj_send_event(view.complete,LV_EVENT_CLICKED,NULL); assert(posts == 4U && posted.item.completed == 1U);
    state.completion_id=posted.request_id; state.items[0].completed=1U; state.items[0].revision=2U; np_agenda_sync(&view,&state);
    state.google_linked=true; state.sync_busy=true; np_agenda_sync(&view,&state);
    assert(lv_obj_has_state(view.save,LV_STATE_DISABLED) && lv_obj_has_state(view.sync,LV_STATE_DISABLED));
    state.sync_busy=false; state.count=8U;
    for (unsigned i=0;i<state.count;++i) {
        state.items[i]=(agenda_item_t){.id=i+1U,.revision=1U,.kind= i%2 ? AGENDA_TASK : AGENDA_EVENT,
            .start_unix_s=1792414800U+i*3600U,.end_unix_s=i%2 ? 0U : 1792418400U+i*3600U,.reminder_minutes=15U};
        snprintf(state.items[i].title,sizeof(state.items[i].title),"%s %u",i%2 ? "Tarefa" : "Evento",i+1U);
    }
    strcpy(state.items[0].title,"Reunião de projeto"); state.items[0].reserved[1]=1U;
    np_agenda_sync(&view,&state);
    lv_obj_send_event(view.rows[0],LV_EVENT_CLICKED,NULL);
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(view.save,0)),"Usar Google"));
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(view.complete,0)),"Manter painel"));
    lv_obj_send_event(view.complete,LV_EVENT_CLICKED,NULL);
    assert(posted.action == AGENDA_KEEP_LOCAL && posted.item.id == 1U);
    state.completion_id=posted.request_id; state.items[0].reserved[1]=2U;
    np_agenda_sync(&view,&state); lv_obj_send_event(view.rows[0],LV_EVENT_CLICKED,NULL);
    assert(lv_obj_has_state(view.complete,LV_STATE_DISABLED));
    lv_obj_send_event(view.save,LV_EVENT_CLICKED,NULL);
    assert(posted.action == AGENDA_USE_GOOGLE && posted.item.id == 1U);
    state.completion_id=posted.request_id;
    state.items[0].reserved[1]=0U; np_agenda_sync(&view,&state);
    lv_obj_send_event(view.rows[0],LV_EVENT_CLICKED,NULL);
    lv_area_t field_area, keyboard_area;
    lv_obj_t *fields[]={view.title,view.date,view.start,view.end_date,view.end};
    for (unsigned i=0U;i<sizeof(fields)/sizeof(fields[0]);++i) {
        lv_obj_send_event(fields[i],LV_EVENT_FOCUSED,NULL); advance();
        lv_obj_get_coords(fields[i],&field_area); lv_obj_get_coords(keyboard.root,&keyboard_area);
        assert(field_area.y2 < keyboard_area.y1);
        lv_obj_remove_state(fields[i],LV_STATE_FOCUSED);
    }
    if (argc > 6) screenshot(argv[6]);
    np_keyboard_hide(&keyboard); np_agenda_sync(&view,&state); advance();
    if (argc > 5) screenshot(argv[5]);
    assert(lv_obj_has_state(view.rows[0],LV_STATE_CHECKED));
    np_keyboard_focus(&keyboard,view.title,NP_KEYBOARD_MODE_TEXT);
    assert(np_keyboard_is_visible(&keyboard));
    lv_obj_send_event(view.editor.close_button,LV_EVENT_CLICKED,NULL);
    np_agenda_sync(&view,&state);
    assert(!np_modal_is_visible(&view.editor) && !np_keyboard_is_visible(&keyboard));
    assert(!lv_obj_has_state(view.rows[0],LV_STATE_CHECKED));
    lv_tick_inc(8001U); np_agenda_sync(&view,&state);
    assert(!view.notice_active);
    if (argc > 1) screenshot(argv[1]);
    const unsigned commands_before_info=posts, windows_before_info=link_requests;
    lv_obj_send_event(view.status,LV_EVENT_CLICKED,NULL);
    assert(view.connection_info && np_modal_is_visible(&view.editor));
    assert(strstr(lv_label_get_text(view.message),"Conta Google vinculada"));
    if (argc > 8) screenshot(argv[8]);
    lv_obj_send_event(view.save,LV_EVENT_CLICKED,NULL);
    assert(!np_modal_is_visible(&view.editor) && posts == commands_before_info && link_requests == windows_before_info+1U);
    lv_tick_inc(60001U); np_agenda_sync(&view,&state);
    state.google_auth_required=true; np_agenda_sync(&view,&state);
    if (argc > 7) screenshot(argv[7]);
    state.google_auth_required=false; np_agenda_sync(&view,&state);
    advance();
    lv_area_t last_row, sync_button;
    lv_obj_get_coords(view.rows[NP_AGENDA_ROWS-1U],&last_row);
    lv_obj_get_coords(view.sync,&sync_button);
    assert(last_row.y2 < 580 && sync_button.y2 < 152);
    lv_obj_send_event(view.next,LV_EVENT_CLICKED,NULL); assert(view.page == 1U && view.row_ids[0] == 7U);
    state.count=7U; state.items[6].reserved[0]=1U; np_agenda_sync(&view,&state);
    assert(!strcmp(lv_label_get_text(view.times[0]),"Dia todo"));
    assert(!strcmp(lv_label_get_text(view.dates[0]),"19/10"));
    assert(lv_obj_has_flag(view.rows[3],LV_OBJ_FLAG_HIDDEN));
    if (argc > 2) screenshot(argv[2]);
    lv_obj_send_event(view.filters[1],LV_EVENT_CLICKED,NULL);
    assert(view.page == 0U && view.visible_count == 4U);
    assert(view.row_ids[0] == 1U && view.row_ids[1] == 3U && view.row_ids[3] == 7U);
    lv_obj_send_event(view.filters[2],LV_EVENT_CLICKED,NULL);
    assert(view.visible_count == 3U && view.row_ids[0] == 2U && view.row_ids[2] == 6U);
    lv_obj_send_event(view.rows[0],LV_EVENT_CLICKED,NULL);
    assert(lv_obj_has_flag(view.end_date,LV_OBJ_FLAG_HIDDEN));
    lv_textarea_set_text(view.title,"Rascunho preservado");
    const unsigned posts_before_cancel=posts;
    lv_obj_send_event(view.cancel,LV_EVENT_CLICKED,NULL);
    assert(posts == posts_before_cancel && !np_modal_is_visible(&view.editor));
    lv_obj_send_event(view.filters[0],LV_EVENT_CLICKED,NULL);
    lv_obj_send_event(view.rows[1],LV_EVENT_CLICKED,NULL);
    assert(strcmp(lv_textarea_get_text(view.title),"Rascunho preservado"));
    lv_obj_send_event(view.cancel,LV_EVENT_CLICKED,NULL);
    /* Sorting must not mutate storage order or bind the wrong command ID. */
    state.items[1].completed=1U;
    state.items[3].start_unix_s=0U;
    state.items[5].start_unix_s=state.items[0].start_unix_s-3600U;
    lv_obj_send_event(view.filters[2],LV_EVENT_CLICKED,NULL); np_agenda_sync(&view,&state);
    assert(view.row_ids[0] == 6U && view.row_ids[1] == 4U && view.row_ids[2] == 2U);
    assert(state.items[0].id == 1U && state.items[1].id == 2U);
    lv_obj_send_event(view.rows[0],LV_EVENT_CLICKED,NULL);
    lv_obj_send_event(view.complete,LV_EVENT_CLICKED,NULL);
    assert(posted.item.id == 6U && posted.action == AGENDA_SET_COMPLETED);
    state.completion_id=posted.request_id; np_agenda_sync(&view,&state);
    lv_obj_send_event(view.new_task,LV_EVENT_CLICKED,NULL);
    assert(!view.connection_info && np_modal_is_visible(&view.editor));
    if (argc > 3) screenshot(argv[3]);
    lv_obj_send_event(view.cancel,LV_EVENT_CLICKED,NULL);
    /* Every monthly record is reachable, including the final partial page. */
    state.count=AGENDA_MAX_ITEMS;
    for (unsigned i=0U;i<state.count;++i) {
        state.items[i]=(agenda_item_t){.id=i+1U,.revision=1U,.kind=AGENDA_EVENT,
            .start_unix_s=1792414800U+i*3600U,.end_unix_s=1792418400U+i*3600U,
            .reminder_minutes=AGENDA_REMINDER_NONE};
        snprintf(state.items[i].title,sizeof(state.items[i].title),"Compromisso %u",i+1U);
    }
    lv_obj_send_event(view.filters[0],LV_EVENT_CLICKED,NULL); np_agenda_sync(&view,&state);
    bool seen[AGENDA_MAX_ITEMS]={0};
    for (unsigned page=0U;page<(AGENDA_MAX_ITEMS+NP_AGENDA_ROWS-1U)/NP_AGENDA_ROWS;++page) {
        for (unsigned row=0U;row<NP_AGENDA_ROWS;++row) {
            const uint32_t id=view.row_ids[row];
            if (id) { assert(id <= AGENDA_MAX_ITEMS && !seen[id-1U]); seen[id-1U]=true; }
        }
        if (!lv_obj_has_state(view.next,LV_STATE_DISABLED)) lv_obj_send_event(view.next,LV_EVENT_CLICKED,NULL);
    }
    for (unsigned i=0U;i<AGENDA_MAX_ITEMS;++i) assert(seen[i]);
    assert(lv_obj_has_state(view.next,LV_STATE_DISABLED));
    state.count=0U; np_agenda_sync(&view,&state);
    assert(view.page == 0U);
    lv_obj_send_event(view.filters[2],LV_EVENT_CLICKED,NULL);
    assert(!lv_obj_has_flag(view.empty,LV_OBJ_FLAG_HIDDEN) && view.visible_count == 0U);
    assert(lv_obj_has_state(view.next,LV_STATE_DISABLED));
    assert(strstr(lv_label_get_text(view.empty),"Nenhuma tarefa"));
    if (argc > 4) screenshot(argv[4]);
    lv_obj_send_event(view.new_event,LV_EVENT_CLICKED,NULL);
    assert(!lv_obj_has_flag(view.end_date,LV_OBJ_FLAG_HIDDEN));
    lv_obj_delete(view.root); lv_obj_delete(header.drawer_scrim);
    lv_mem_monitor_t baseline,final;
    /* Warm the shared keyboard's first mode/focus allocation before comparing
     * equivalent lifecycle states, matching the firmware's lazy keyboard. */
    view=np_agenda_build(lv_screen_active()); np_agenda_bind(&view,&keyboard);
    np_agenda_sync(&view,&state); lv_obj_send_event(view.new_event,LV_EVENT_CLICKED,NULL);
    np_keyboard_focus(&keyboard,view.title,NP_KEYBOARD_MODE_TEXT);
    np_keyboard_hide(&keyboard); lv_obj_delete(view.root);
    advance(); lv_mem_monitor(&baseline);
    /* Destruction must release all five keyboard targets on each visit. */
    for (unsigned cycle=0;cycle<100U;++cycle) {
        view=np_agenda_build(lv_screen_active()); np_agenda_bind(&view,&keyboard);
        np_agenda_sync(&view,&state); lv_obj_send_event(view.new_event,LV_EVENT_CLICKED,NULL);
        np_keyboard_focus(&keyboard,view.title,NP_KEYBOARD_MODE_TEXT);
        if (cycle != 50U) np_keyboard_hide(&keyboard);
        lv_obj_delete(view.root); advance();
        assert(!np_keyboard_is_visible(&keyboard));
        for (unsigned i=0;i<NP_KEYBOARD_MAX_BINDINGS;++i) assert(keyboard.bindings[i].textarea == NULL);
    }
    lv_mem_monitor(&final);
    printf("Host lifecycle free baseline=%zu final=%zu\n",(size_t)baseline.free_size,(size_t)final.free_size);
    assert(final.free_size >= baseline.free_size);
    np_keyboard_destroy(&keyboard);
    puts("agenda UI: 6 rows/64 reachable, conditional auth/icons, notice expiry, modal/keyboard/conflict routing, CRUD, 100 lifecycles PASS");
    return 0;
}
