#include "np_agenda.h"
#include "agenda_time.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app_event_bus.h"
#include "provisioning_service.h"
#include "np_styles.h"

static uint32_t s_next_request = 1U;
static const uint16_t s_reminders[] = {AGENDA_REMINDER_NONE,0U,5U,15U,30U,60U,1440U};
static void build_editor(np_agenda_view_t *v);
static void enabled(lv_obj_t *obj, bool allow)
{
    if (obj == NULL) return;
    if (allow) lv_obj_remove_state(obj, LV_STATE_DISABLED);
    else lv_obj_add_state(obj, LV_STATE_DISABLED);
}
static void button_text(lv_obj_t *button, const char *text)
{ if (button != NULL) np_set_text(lv_obj_get_child(button, 0), text); }
static void notice(np_agenda_view_t *v,const char *text,uint32_t duration_ms)
{
    np_set_text(v->notice,text);
    v->notice_expiry=lv_tick_get()+duration_ms;
    v->notice_active=true;
}
static const agenda_item_t *find(const np_agenda_view_t *v, uint32_t id)
{
    for (uint8_t i = 0U; i < v->state.count; ++i) if (v->state.items[i].id == id) return &v->state.items[i];
    return NULL;
}
/* Presentation order only: domain/storage order and item identity stay intact. */
static bool comes_before(const agenda_item_t *a, const agenda_item_t *b)
{
    const bool a_done = a->kind == AGENDA_TASK && a->completed;
    const bool b_done = b->kind == AGENDA_TASK && b->completed;
    if (a_done != b_done) return !a_done;
    if ((a->start_unix_s == 0U) != (b->start_unix_s == 0U)) return a->start_unix_s != 0U;
    if (a->start_unix_s != b->start_unix_s) return a->start_unix_s < b->start_unix_s;
    return a->id < b->id;
}
static uint8_t display_order(const np_agenda_view_t *v, uint8_t order[AGENDA_MAX_ITEMS])
{
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < v->state.count && i < AGENDA_MAX_ITEMS; ++i) {
        const agenda_item_t *item = &v->state.items[i];
        if ((v->filter == 1U && item->kind != AGENDA_EVENT) ||
            (v->filter == 2U && item->kind != AGENDA_TASK)) continue;
        uint8_t position = count;
        while (position > 0U && comes_before(item, &v->state.items[order[position-1U]])) {
            order[position] = order[position-1U]; --position;
        }
        order[position] = i; ++count;
    }
    return count;
}
static void put_time(lv_obj_t *date, lv_obj_t *clock, uint32_t epoch)
{
    char text[24] = {0};
    if (epoch != 0U) {
        time_t t = (time_t)epoch; struct tm local = {0}; localtime_r(&t, &local);
        strftime(text, sizeof(text), "%d/%m/%Y", &local); lv_textarea_set_text(date, text);
        strftime(text, sizeof(text), "%H:%M", &local);
    } else lv_textarea_set_text(date, "");
    lv_textarea_set_text(clock, text);
}
static void select_item(np_agenda_view_t *v, const agenda_item_t *item, agenda_kind_t kind)
{
    v->persistence_feedback = false;
    np_keyboard_hide(v->keyboard);
    v->creating = item == NULL;
    v->draft = item != NULL ? *item : (agenda_item_t){.kind = (uint8_t)kind, .reminder_minutes = 15U};
    v->delete_armed = false; button_text(v->remove, "Excluir");
    lv_textarea_set_text(v->title, v->draft.title);
    uint32_t start = v->draft.start_unix_s;
    if (v->creating) {
        time_t now = time(NULL);
        if (now >= (time_t)AGENDA_MIN_EPOCH && (uint64_t)now+7200U <= UINT32_MAX)
            start = ((uint32_t)now/3600U+1U)*3600U;
    }
    put_time(v->date, v->start, start);
    put_time(v->end_date, v->end, v->creating && start != 0U ? start+3600U : v->draft.end_unix_s);
    uint16_t selected = 0U;
    for (uint16_t i = 0U; i < sizeof(s_reminders)/sizeof(s_reminders[0]); ++i)
        if (s_reminders[i] == v->draft.reminder_minutes) selected = i;
    lv_dropdown_set_selected(v->reminder, selected);
    const bool task = v->draft.kind == AGENDA_TASK;
    np_set_text(v->editor.icon,task ? NP_ICON_CHECK_CIRCLE : NP_ICON_CALENDAR);
    np_set_text_color(v->editor.icon,task ? np_c_positive() : np_c_accent());
    lv_textarea_set_placeholder_text(v->title,task ? "Ex.: enviar relatório" : "Ex.: reunião de equipe");
    np_set_text(v->editor_title, task ? v->creating ? "Nova tarefa" : "Editar tarefa" :
        v->creating ? "Novo evento" : "Editar evento");
    np_set_text(v->start_label, task ? "Prazo (opcional)" : "Início");
    np_set_visible(v->end_label, !task); np_set_visible(v->end_time_label, !task);
    np_set_visible(v->end_date, !task); np_set_visible(v->end, !task);
    lv_obj_set_y(v->editor.panel,task ? 100 : 60);
    lv_obj_set_height(v->editor.panel,task ? 400 : 480);
    lv_obj_set_height(v->editor.content,task ? 323 : 403);
    lv_obj_set_pos(v->reminder_label,task ? 376 : 24,task ? 94 : 182);
    lv_obj_set_pos(v->reminder,task ? 376 : 24,task ? 118 : 206);
    lv_obj_set_y(v->message,task ? 190 : 270);
    lv_obj_t *actions[]={v->cancel,v->remove,v->complete,v->save};
    for (unsigned i=0U;i<sizeof(actions)/sizeof(actions[0]);++i) lv_obj_set_y(actions[i],task ? 258 : 338);
    np_set_text(v->editor.subtitle,task ? "Organize o prazo e o lembrete" : "Defina quando o compromisso acontece");
    np_set_text_color(v->message, np_c_text_2());
    np_set_text(v->message, v->draft.reserved[0] ? "Dia inteiro: somente leitura." :
        task ? "Sem prazo? Deixe a data vazia. Hora e lembrete são locais." : "Horários no fuso do painel.");
}
static void editor_closed(void *user_data)
{
    np_agenda_view_t *v=user_data;
    np_keyboard_hide(v->keyboard);
    v->delete_armed=false;
    button_text(v->remove,"Excluir");
}
static void open_editor(np_agenda_view_t *v, const agenda_item_t *item, agenda_kind_t kind)
{
    if (v->editor.scrim == NULL) build_editor(v);
    v->connection_info=false;
    for (uint32_t i=0U;i<lv_obj_get_child_count(v->editor.content);++i)
        np_set_visible(lv_obj_get_child(v->editor.content,i),true);
    lv_obj_set_pos(v->message,24,270); lv_obj_set_height(v->message,48);
    button_text(v->cancel,"Cancelar");
    select_item(v,item,kind);
    lv_obj_scroll_to_y(v->editor.content,0,LV_ANIM_OFF);
    np_modal_show(&v->editor);
}
static void open_connection(np_agenda_view_t *v)
{
    if (v->editor.scrim == NULL) build_editor(v);
    np_keyboard_hide(v->keyboard);
    v->connection_info=true;
    for (uint32_t i=0U;i<lv_obj_get_child_count(v->editor.content);++i)
        np_set_visible(lv_obj_get_child(v->editor.content,i),false);
    np_set_visible(v->message,true); np_set_visible(v->cancel,true); np_set_visible(v->save,true);
    np_set_text(v->editor.title,"Conta Google");
    np_set_text(v->editor.subtitle,"Conexão e sincronização");
    np_set_text(v->editor.icon,NP_ICON_LINK);
    np_set_text_color(v->editor.icon,np_c_accent());
    lv_obj_set_y(v->editor.panel,140); lv_obj_set_height(v->editor.panel,320);
    lv_obj_set_height(v->editor.content,243);
    lv_obj_set_pos(v->message,24,16); lv_obj_set_height(v->message,104);
    lv_obj_set_pos(v->cancel,24,178); lv_obj_set_pos(v->save,472,178); lv_obj_set_width(v->save,224);
    button_text(v->cancel,"Fechar"); button_text(v->save,"Liberar USB");
    lv_obj_scroll_to_y(v->editor.content,0,LV_ANIM_OFF);
    np_modal_show(&v->editor);
}
static bool read_time(lv_obj_t *date, lv_obj_t *clock, uint32_t *out)
{
    const char *d = lv_textarea_get_text(date), *c = lv_textarea_get_text(clock);
    if (strlen(d) != 10U || strlen(c) != 5U || d[2] != '/' || d[5] != '/' || c[2] != ':') return false;
    const unsigned pos[] = {0,1,3,4,6,7,8,9};
    for (unsigned i = 0U; i < sizeof(pos)/sizeof(pos[0]); ++i) if (d[pos[i]] < '0' || d[pos[i]] > '9') return false;
    if (c[0] < '0' || c[0] > '9' || c[1] < '0' || c[1] > '9' || c[3] < '0' || c[3] > '9' || c[4] < '0' || c[4] > '9') return false;
    unsigned day,month,year,hour,minute;
    if (sscanf(d,"%2u/%2u/%4u",&day,&month,&year) != 3 || sscanf(c,"%2u:%2u",&hour,&minute) != 2 ||
        year < 2025U || year > 2105U || month < 1U || month > 12U || day < 1U || day > 31U || hour > 23U || minute > 59U) return false;
    struct tm local = {.tm_year=(int)year-1900,.tm_mon=(int)month-1,.tm_mday=(int)day,
        .tm_hour=(int)hour,.tm_min=(int)minute,.tm_isdst=-1};
    time_t t = mktime(&local);
    if (t < (time_t)AGENDA_MIN_EPOCH || (uint64_t)t > UINT32_MAX) return false;
    struct tm check = {0}; localtime_r(&t, &check);
    if (check.tm_year != (int)year-1900 || check.tm_mon != (int)month-1 || check.tm_mday != (int)day ||
        check.tm_hour != (int)hour || check.tm_min != (int)minute) return false;
    *out = (uint32_t)t; return true;
}
static void submit(np_agenda_view_t *v, agenda_action_t action)
{
    agenda_command_t command = {.request_id=s_next_request++,.action=action,.item=v->draft};
    if (command.request_id == 0U) command.request_id = s_next_request++;
    if (action == AGENDA_UPSERT) {
        const char *title = lv_textarea_get_text(v->title);
        if (strlen(title) >= sizeof(command.item.title)) { np_set_text(v->message,"Título: máximo de 48 bytes."); return; }
        memset(command.item.title,0,sizeof(command.item.title)); strcpy(command.item.title,title);
        command.item.reminder_minutes = s_reminders[lv_dropdown_get_selected(v->reminder)];
        if (command.item.kind == AGENDA_TASK && lv_textarea_get_text(v->date)[0] == 0) {
            command.item.start_unix_s=0U; command.item.reminder_minutes=AGENDA_REMINDER_NONE;
        } else if (!read_time(v->date,v->start,&command.item.start_unix_s)) {
            np_set_text(v->message,"Confira a data e a hora de início."); return;
        }
        if (command.item.kind == AGENDA_EVENT && !read_time(v->end_date,v->end,&command.item.end_unix_s)) {
            np_set_text(v->message,"Confira a data e a hora de término."); return;
        }
        if (command.item.kind == AGENDA_TASK) command.item.end_unix_s=0U;
        if (!agenda_item_valid(&command.item)) { np_set_text(v->message,"Título obrigatório; término após o início."); return; }
    }
    if (action == AGENDA_SET_COMPLETED) command.item.completed=!v->draft.completed;
    if (app_event_bus_post_agenda(&command) != ESP_OK) { np_set_text(v->message,"Painel ocupado. Tente novamente."); return; }
    np_keyboard_hide(v->keyboard); v->pending_request=command.request_id;
    np_set_text(v->message,"Aplicando alteração...");
    enabled(v->save,false); enabled(v->remove,false); enabled(v->complete,false);
}
static void click(lv_event_t *event)
{
    np_agenda_view_t *v=lv_event_get_user_data(event);
    if (v == NULL) return;
    lv_obj_t *target=lv_event_get_target(event);
    if (target == v->cancel) { np_modal_hide(&v->editor); return; }
    if (target == v->link && v->state.google_linked && !v->state.google_auth_required) return;
    if (v->pending_request != 0U || v->state.sync_busy) return;
    if (target == v->status) { open_connection(v); np_agenda_sync(v,&v->state); return; }
    if (v->connection_info && target == v->save) {
        const bool armed=provisioning_service_arm_open_network() == ESP_OK;
        notice(v,armed ? "USB liberada por 60 s.\nExecute o vinculador no PC." : "USB indisponível.",armed ? 60000U : 8000U);
        np_modal_hide(&v->editor); return;
    }
    /* List navigation preserves the draft and its outstanding save feedback.
     * Other interactions own the editor's validation/confirmation message. */
    const bool list_navigation=target == v->previous || target == v->next ||
        target == v->filters[0] || target == v->filters[1] || target == v->filters[2];
    if (!list_navigation) {
        v->persistence_feedback = false;
        np_set_text_color(v->message, np_c_text_2());
    }
    if (target == v->new_event || target == v->new_task) {
        v->filter = 0U; v->page = 0U;
        open_editor(v,NULL,target == v->new_task ? AGENDA_TASK : AGENDA_EVENT);
    }
    else if (target == v->save || target == v->complete) {
        const agenda_item_t *selected=find(v,v->draft.id);
        const bool conflict=selected != NULL && selected->reserved[1] != 0U;
        submit(v,target == v->save ? conflict ? AGENDA_USE_GOOGLE : AGENDA_UPSERT :
            conflict ? AGENDA_KEEP_LOCAL : AGENDA_SET_COMPLETED);
    }
    else if (target == v->remove) {
        if (v->delete_armed) submit(v,AGENDA_DELETE);
        else { v->delete_armed=true; button_text(v->remove,"Confirmar"); np_set_text(v->message,"Confirme para excluir também no Google."); }
    } else if (target == v->sync) notice(v,app_event_bus_post_agenda_sync() == ESP_OK ? "Sincronização solicitada." : "Painel ocupado.",8000U);
    else if (target == v->link) {
        const bool armed=provisioning_service_arm_open_network() == ESP_OK;
        notice(v,armed ? "USB liberada por 60 s.\nExecute o vinculador no PC." : "Vinculação USB indisponível.",armed ? 60000U : 8000U);
    }
    else if (target == v->previous) { if (v->page > 0U) --v->page; }
    else if (target == v->next) { if ((v->page+1U)*NP_AGENDA_ROWS < v->visible_count) ++v->page; }
    else if (target == v->filters[0] || target == v->filters[1] || target == v->filters[2]) {
        for (uint8_t i=0U; i<3U; ++i) if (target == v->filters[i]) v->filter=i;
        v->page=0U;
    }
    else for (unsigned row=0U;row<NP_AGENDA_ROWS;++row)
        if (target == v->rows[row]) { const agenda_item_t *item=find(v,v->row_ids[row]); if (item != NULL) open_editor(v,item,(agenda_kind_t)item->kind); }
    np_agenda_sync(v,&v->state);
}
/* The modal is built on first use; the information screen has no form cost. */
static void build_editor(np_agenda_view_t *v)
{
    np_modal_create(&v->editor,v->root,152,60,720,480,
        NP_ICON_CALENDAR,np_c_accent(),"Novo evento","Defina quando o compromisso acontece");
    np_modal_rebind(&v->editor);
    np_modal_set_close_callback(&v->editor,editor_closed,v);
    lv_obj_set_size(v->editor.close_button,48,48);
    v->editor_title=v->editor.title;
    lv_obj_t *form=v->editor.content;
    np_label(form,"Título",NP_FONT_SM,np_c_text_2(),24,10,672,LV_TEXT_ALIGN_LEFT);
    v->title=np_form_text_input(form,24,34,672,48,"Ex.: reunião de equipe",NULL);
    lv_textarea_set_max_length(v->title,48U);
    v->start_label=np_label(form,"Início",NP_FONT_SM,np_c_text_2(),24,94,196,LV_TEXT_ALIGN_LEFT);
    np_label(form,"Hora",NP_FONT_SM,np_c_text_2(),232,94,112,LV_TEXT_ALIGN_LEFT);
    v->date=np_form_text_input(form,24,118,196,48,"DD/MM/AAAA",NULL);
    v->start=np_form_text_input(form,232,118,112,48,"HH:MM",NULL);
    v->end_label=np_label(form,"Término",NP_FONT_SM,np_c_text_2(),376,94,196,LV_TEXT_ALIGN_LEFT);
    v->end_time_label=np_label(form,"Hora",NP_FONT_SM,np_c_text_2(),584,94,112,LV_TEXT_ALIGN_LEFT);
    v->end_date=np_form_text_input(form,376,118,196,48,"DD/MM/AAAA",NULL);
    v->end=np_form_text_input(form,584,118,112,48,"HH:MM",NULL);
    lv_textarea_set_max_length(v->date,10U); lv_textarea_set_max_length(v->end_date,10U);
    lv_textarea_set_max_length(v->start,5U); lv_textarea_set_max_length(v->end,5U);
    v->reminder_label=np_label(form,"Lembrete",NP_FONT_SM,np_c_text_2(),24,182,320,LV_TEXT_ALIGN_LEFT);
    v->reminder=np_form_dropdown(form,24,206,320,48);
    lv_dropdown_set_options(v->reminder,"Sem lembrete\nNo horário\n5 min antes\n15 min antes\n30 min antes\n1 hora antes\n1 dia antes");
    v->message=np_label(form,"",NP_FONT_SM,np_c_text_2(),24,270,672,LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(v->message,LV_LABEL_LONG_MODE_WRAP); lv_obj_set_height(v->message,48);
    v->cancel=np_button(form,24,338,132,48,"Cancelar",false);
    v->remove=np_form_button(form,168,338,140,48,"Excluir",NP_FORM_BUTTON_DESTRUCTIVE);
    v->complete=np_button(form,320,338,140,48,"Concluir",false);
    v->save=np_button(form,472,338,224,48,"Salvar evento",true);
    lv_obj_t *buttons[]={v->cancel,v->save,v->remove,v->complete};
    for (unsigned i=0U;i<sizeof(buttons)/sizeof(buttons[0]);++i)
        lv_obj_add_event_cb(buttons[i],click,LV_EVENT_CLICKED,v);
    lv_obj_t *fields[]={v->title,v->date,v->end_date,v->start,v->end};
    for (unsigned i=0U;i<sizeof(fields)/sizeof(fields[0]);++i) {
        np_keyboard_bind(v->keyboard,fields[i],NP_KEYBOARD_MODE_TEXT);
        lv_obj_set_style_border_opa(fields[i],LV_OPA_TRANSP,LV_PART_CURSOR);
        lv_obj_set_style_border_opa(fields[i],LV_OPA_COVER,LV_PART_CURSOR|LV_STATE_FOCUSED);
    }
}
np_agenda_view_t np_agenda_build(lv_obj_t *parent)
{
    np_agenda_view_t v={0}; v.root=np_scene(parent); v.creating=true;
    v.draft.kind=AGENDA_EVENT;
    lv_obj_t *list=np_panel(v.root,18,88,988,492);
    np_label(list,"Agenda",NP_FONT_TITLE,np_c_text(),24,6,144,LV_TEXT_ALIGN_LEFT);
    v.count=np_label(list,"",NP_FONT_SM,np_c_text_2(),24,46,144,LV_TEXT_ALIGN_LEFT);
    v.new_event=np_button(list,556,12,140,48,"+ Evento",true);
    v.new_task=np_button(list,708,12,140,48,"+ Tarefa",false);
    v.link=np_icon_button(list,860,12,48,NP_ICON_LINK);
    v.sync=np_icon_button(list,916,12,48,NP_ICON_REFRESH);
    v.status_dot=np_dot(list,24,89,8,np_c_text_3());
    v.status=np_label(list,"Carregando agenda...",NP_FONT_SM,np_c_text_2(),40,64,412,LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(v.status,48); lv_obj_set_style_pad_top(v.status,18,0);
    lv_obj_add_flag(v.status,LV_OBJ_FLAG_CLICKABLE);
    const char *const filter_names[]={"Todos","Eventos","Tarefas"};
    for (uint8_t i=0U;i<3U;++i) {
        v.filters[i]=np_button(list,176+(int)i*120,12,112,48,filter_names[i],false);
        lv_obj_set_style_text_font(lv_obj_get_child(v.filters[i],0),NP_FONT_SM,0);
        lv_obj_set_style_bg_color(v.filters[i],np_c_accent_bg(),LV_STATE_CHECKED);
        lv_obj_set_style_border_color(v.filters[i],np_c_accent(),LV_STATE_CHECKED);
    }
    v.empty_icon=np_label(list,NP_ICON_CALENDAR,NP_FONT_ICON,np_c_accent(),24,230,940,LV_TEXT_ALIGN_CENTER);
    v.empty=np_label(list,"Sua agenda começa aqui",NP_FONT_LG,np_c_text(),24,270,940,LV_TEXT_ALIGN_CENTER);
    v.empty_hint=np_label(list,"Crie um evento ou uma tarefa para começar.",NP_FONT_SM,np_c_text_2(),24,312,940,LV_TEXT_ALIGN_CENTER);
    for (unsigned i=0U;i<NP_AGENDA_ROWS;++i) {
        v.rows[i]=np_group(list,24,120+(int)i*60,940,56);
        lv_obj_add_style(v.rows[i],np_st_row(),0);
        lv_obj_add_flag(v.rows[i],LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(v.rows[i],np_c_accent_bg(),LV_STATE_CHECKED);
        lv_obj_set_style_border_color(v.rows[i],np_c_accent(),LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(v.rows[i],np_c_surface_subtle(),LV_STATE_PRESSED);
        v.kinds[i]=np_label(v.rows[i],"",NP_FONT_ICON,np_c_accent(),16,16,32,LV_TEXT_ALIGN_CENTER);
        v.times[i]=np_label(v.rows[i],"",NP_FONT_MD,np_c_text(),64,3,88,LV_TEXT_ALIGN_CENTER);
        v.dates[i]=np_label(v.rows[i],"",NP_FONT_SM,np_c_text_2(),64,30,88,LV_TEXT_ALIGN_CENTER);
        v.titles[i]=np_label(v.rows[i],"",NP_FONT_MD,np_c_text(),172,3,748,LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(v.titles[i],LV_LABEL_LONG_MODE_DOTS);
        v.details[i]=np_label(v.rows[i],"",NP_FONT_SM,np_c_text_2(),172,30,748,LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(v.details[i],LV_LABEL_LONG_MODE_DOTS);
    }
    v.previous=np_form_icon_button(list,804,64,48,NP_ICON_ARROW_LEFT);
    v.next=np_form_icon_button(list,916,64,48,NP_ICON_ARROW_RIGHT);
    v.page_label=np_label(list,"1 / 1",NP_FONT_SM,np_c_text_2(),854,80,60,LV_TEXT_ALIGN_CENTER);
    v.notice=np_label(list,"",NP_FONT_SM,np_c_text_2(),468,68,308,LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(v.notice,LV_LABEL_LONG_MODE_WRAP); lv_obj_set_height(v.notice,44);
    return v;
}
/* Layout-only entry point retained for the reference catalog. */
lv_obj_t *np_agenda_create(lv_obj_t *parent) { return np_agenda_build(parent).root; }
void np_agenda_bind(np_agenda_view_t *v,np_keyboard_t *keyboard)
{
    v->keyboard=keyboard;
    lv_obj_t *buttons[]={v->new_event,v->new_task,v->sync,v->link,v->previous,v->next,v->status};
    for (unsigned i=0U;i<sizeof(buttons)/sizeof(buttons[0]);++i) lv_obj_add_event_cb(buttons[i],click,LV_EVENT_CLICKED,v);
    for (unsigned i=0U;i<3U;++i) lv_obj_add_event_cb(v->filters[i],click,LV_EVENT_CLICKED,v);
    for (unsigned i=0U;i<NP_AGENDA_ROWS;++i) lv_obj_add_event_cb(v->rows[i],click,LV_EVENT_CLICKED,v);
}
void np_agenda_sync(np_agenda_view_t *v,const agenda_projection_t *state)
{
    if (v == NULL || v->root == NULL || state == NULL) return;
    if (v->notice_active && (int32_t)(lv_tick_get()-v->notice_expiry) >= 0) {
        np_set_text(v->notice,""); v->notice_active=false;
    }
    if (state != &v->state) v->state=*state;
    if (v->pending_request != 0U && state->completion_id == v->pending_request) {
        v->pending_request=0U;
        if (state->command_result == AGENDA_OK) {
            if (v->creating && state->count > 0U) v->draft=state->items[state->count-1U];
            const agenda_item_t *item=find(v,v->draft.id);
            if (item != NULL) select_item(v,item,(agenda_kind_t)item->kind);
            else select_item(v,NULL,AGENDA_EVENT);
            v->persistence_feedback = true;
            np_modal_hide(&v->editor);
            notice(v,"Alteração aplicada à agenda.",8000U);
        } else {
            const char *error=state->command_result == AGENDA_FULL ? "Agenda cheia: limite de 64 itens." :
                state->command_result == AGENDA_CONFLICT ? "Item alterado. Selecione novamente." : "Não foi possível aplicar a alteração.";
            np_set_text(v->message,error); notice(v,error,8000U);
            np_set_text_color(v->message,np_c_negative());
        }
    }
    const bool storage_failed = state->storage_result != ESP_OK && state->storage_result != ESP_ERR_NOT_FOUND;
    if (v->persistence_feedback) {
        if (storage_failed) {
            np_set_text_color(v->message,np_c_negative());
            np_set_text(v->message,"Falha ao salvar. Tentaremos novamente.");
        } else if (state->persistence_pending) {
            np_set_text_color(v->message,np_c_text_2());
            np_set_text(v->message,"Alteração aplicada. Salvamento pendente.");
        } else if (state->storage_result == ESP_OK) {
            np_set_text_color(v->message,np_c_positive());
            np_set_text(v->message,"Alteração salva no painel.");
            v->persistence_feedback = false;
        } else
            np_set_text(v->message,"Aguardando confirmação do salvamento.");
    }
    const bool needs_auth=!state->google_linked || state->google_auth_required;
    const char *status = !state->ready ? "Não foi possível carregar" : state->sync_busy ? "Sincronizando com Google..." :
        storage_failed ? "Falha no armazenamento" :
        state->persistence_pending ? "Salvamento pendente" : needs_auth ? "Conecte sua conta Google" :
        state->sync_result == ESP_ERR_NO_MEM ? "Google aguardando recursos" :
        state->sync_result == ESP_ERR_INVALID_SIZE ? "Google: limite de dados atingido" :
        state->sync_result != ESP_OK ? "Google requer atenção" : "Google vinculado · Detalhes";
    np_set_text(v->status,status);
    const bool failed=!state->ready || storage_failed;
    const bool attention=state->persistence_pending || state->sync_busy || needs_auth || state->sync_result != ESP_OK;
    np_set_bg_color(v->status_dot,failed ? np_c_negative() : attention ? np_c_warning() : np_c_positive());
    uint8_t order[AGENDA_MAX_ITEMS]; v->visible_count=display_order(v,order);
    char today[32]="Visão geral";
    time_t now=time(NULL); struct tm local_today={0};
    if (now >= (time_t)AGENDA_MIN_EPOCH && localtime_r(&now,&local_today) != NULL) {
        const char *const months[]={"janeiro","fevereiro","março","abril","maio","junho","julho","agosto","setembro","outubro","novembro","dezembro"};
        snprintf(today,sizeof(today),"%s · %d",months[local_today.tm_mon],local_today.tm_year+1900);
    }
    np_set_text(v->count,today);
    uint8_t type_counts[3]={state->count,0U,0U};
    for (uint8_t i=0U;i<state->count && i<AGENDA_MAX_ITEMS;++i)
        ++type_counts[state->items[i].kind == AGENDA_TASK ? 2U : 1U];
    const char *const filter_names[]={"Todos","Eventos","Tarefas"};
    for (uint8_t i=0U;i<3U;++i) {
        char label[32]; snprintf(label,sizeof(label),"%s · %u",filter_names[i],(unsigned)type_counts[i]);
        button_text(v->filters[i],label);
        if (i == v->filter) lv_obj_add_state(v->filters[i],LV_STATE_CHECKED);
        else lv_obj_remove_state(v->filters[i],LV_STATE_CHECKED);
    }
    const bool empty=v->visible_count == 0U;
    np_set_visible(v->empty,empty); np_set_visible(v->empty_hint,empty); np_set_visible(v->empty_icon,empty);
    np_set_text(v->empty,v->filter == 1U ? "Nenhum evento por aqui" : v->filter == 2U ? "Nenhuma tarefa por aqui" : "Sua agenda começa aqui");
    np_set_text(v->empty_hint,v->filter == 1U ? "Toque em + Evento para agendar." : v->filter == 2U ? "Toque em + Tarefa para organizar seu dia." : "Crie um evento ou uma tarefa para começar.");
    const uint8_t pages=empty ? 1U : (v->visible_count+NP_AGENDA_ROWS-1U)/NP_AGENDA_ROWS;
    if (v->page >= pages) v->page=pages-1U;
    char page[24]; snprintf(page,sizeof(page),"%u / %u",(unsigned)v->page+1U,(unsigned)pages); np_set_text(v->page_label,page);
    for (uint8_t row=0U;row<NP_AGENDA_ROWS;++row) {
        uint8_t index=v->page*NP_AGENDA_ROWS+row; bool visible=index < v->visible_count;
        np_set_visible(v->rows[row],visible); v->row_ids[row]=0U; if (!visible) continue;
        const agenda_item_t *item=&state->items[order[index]]; v->row_ids[row]=item->id; np_set_text(v->titles[row],item->title);
        const bool selected=np_modal_is_visible(&v->editor) && !v->connection_info && !v->creating && item->id == v->draft.id;
        if (selected) lv_obj_add_state(v->rows[row],LV_STATE_CHECKED);
        else lv_obj_remove_state(v->rows[row],LV_STATE_CHECKED);
        char when[16]="--:--", date[16]="Sem prazo";
        if (item->start_unix_s != 0U) {
            time_t t=(time_t)item->start_unix_s; struct tm local={0}; localtime_r(&t,&local);
            strftime(when,sizeof(when),"%H:%M",&local); strftime(date,sizeof(date),"%d/%m",&local);
            if (item->reserved[0]) strcpy(when,"Dia todo");
        }
        np_set_text(v->times[row],when); np_set_text(v->dates[row],date);
        const bool done=item->kind == AGENDA_TASK && item->completed;
        const bool task=item->kind == AGENDA_TASK;
        np_set_text(v->kinds[row],task ? NP_ICON_CHECK_CIRCLE : NP_ICON_CALENDAR);
        np_set_text_color(v->kinds[row],task ? np_c_positive() : np_c_accent());
        np_set_text_color(v->titles[row],done ? np_c_text_2() : np_c_text());
        np_set_text_color(v->times[row],selected ? np_c_accent() : np_c_text());
        char reminder[40]=" · Sem lembrete";
        if (item->reminder_minutes == 0U) strcpy(reminder," · Lembrete no horário");
        else if (item->reminder_minutes != AGENDA_REMINDER_NONE)
            snprintf(reminder,sizeof(reminder)," · Lembrete %u min antes",(unsigned)item->reminder_minutes);
        char description[40]="Evento";
        if (task) strcpy(description,done ? "Concluída" : "Pendente");
        else if (item->reserved[0]) strcpy(description,"Dia inteiro · Somente leitura");
        else if (item->end_unix_s != 0U) {
            time_t end=(time_t)item->end_unix_s; struct tm local={0}; localtime_r(&end,&local);
            strftime(description,sizeof(description),"Término às %H:%M",&local);
        }
        char detail[128]; snprintf(detail,sizeof(detail),"%s · %s%s",task ? "Tarefa" : "Evento",
            item->reserved[1] ? "Conflito com Google" : description,reminder);
        np_set_text(v->details[row],detail);
        np_set_text_color(v->details[row],item->reserved[1] ? np_c_warning() : done ? np_c_positive() : np_c_text_2());
    }
    bool allow=state->ready && !state->sync_busy && v->pending_request == 0U;
    enabled(v->new_event,allow); enabled(v->new_task,allow); enabled(v->sync,allow && state->google_linked && !state->persistence_pending);
    enabled(v->link,allow); enabled(v->previous,allow && v->page > 0U); enabled(v->next,allow && v->page+1U < pages);
    for (uint8_t i=0U;i<3U;++i) enabled(v->filters[i],allow);
    np_set_visible(v->link,needs_auth);
    lv_obj_set_x(v->new_event,needs_auth ? 556 : 612);
    lv_obj_set_x(v->new_task,needs_auth ? 708 : 764);
    np_set_text_color(lv_obj_get_child(v->link,0),allow ? np_c_warning() : np_c_text_disabled());
    np_set_text_color(lv_obj_get_child(v->sync,0),state->sync_busy ? np_c_accent() :
        allow && state->google_linked && !state->persistence_pending ? np_c_text() : np_c_text_disabled());
    if (v->editor.scrim == NULL) return;
    if (v->connection_info) {
        char last[32]="Ainda não sincronizado";
        if (state->last_sync_unix_s != 0U) {
            time_t timestamp=(time_t)state->last_sync_unix_s; struct tm local={0}; localtime_r(&timestamp,&local);
            strftime(last,sizeof(last),"%d/%m às %H:%M",&local);
        }
        char info[256];
        snprintf(info,sizeof(info),"%s\nÚltimo sincronismo: %s\nPara confirmar o vínculo no computador, libere a USB por 60 s.",
            needs_auth ? "Autorização Google necessária." : "Conta Google vinculada.",last);
        np_set_text(v->message,info); np_set_text_color(v->message,np_c_text_2());
        enabled(v->save,allow); return;
    }
    const agenda_item_t *selected=find(v,v->draft.id); bool conflict=selected != NULL && selected->reserved[1] != 0U;
    np_set_visible(v->remove,!conflict && !v->creating);
    button_text(v->save,conflict ? "Usar Google" : v->draft.kind == AGENDA_TASK ? "Salvar tarefa" : "Salvar evento");
    lv_obj_set_x(v->save,conflict ? 320 : 472); lv_obj_set_width(v->save,conflict ? 176 : 224);
    lv_obj_set_x(v->complete,conflict ? 508 : 320); lv_obj_set_width(v->complete,conflict ? 188 : 140);
    if (conflict && selected->reserved[1] == 2U)
        np_set_text(v->message,"Excluído no Google. Use Google para remover localmente.");
    enabled(v->save,allow && (conflict || !v->draft.reserved[0])); enabled(v->remove,allow && !v->creating && !v->draft.reserved[0]);
    np_set_visible(v->complete,conflict || (!v->creating && v->draft.kind == AGENDA_TASK));
    enabled(v->complete,allow && (!conflict || selected->reserved[1] != 2U));
    button_text(v->complete,conflict ? "Manter painel" : v->draft.completed ? "Reabrir" : "Concluir");
    enabled(v->title,allow && !v->draft.reserved[0]); enabled(v->date,allow && !v->draft.reserved[0]);
    enabled(v->start,allow && !v->draft.reserved[0]); enabled(v->reminder,allow && !v->draft.reserved[0]);
    enabled(v->end_date,v->draft.kind == AGENDA_EVENT && allow && !v->draft.reserved[0]);
    enabled(v->end,v->draft.kind == AGENDA_EVENT && allow && !v->draft.reserved[0]);
}
