#include "np_settings_notifications.h"
#include "np_form.h"

static lv_obj_t *notification_switch(lv_obj_t *parent)
{
    lv_obj_t *sw=lv_switch_create(parent);
    lv_obj_set_pos(sw,228,21); lv_obj_set_size(sw,52,28);
    lv_obj_set_style_bg_color(sw,np_c_hairline(),LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw,np_c_accent(),LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw,np_c_text(),LV_PART_KNOB);
    return sw;
}
static void set_switch(lv_obj_t *sw,bool enabled)
{
    if (enabled) lv_obj_add_state(sw,LV_STATE_CHECKED);
    else lv_obj_remove_state(sw,LV_STATE_CHECKED);
}
void np_settings_notifications_sync(np_settings_notifications_t *v,bool general,bool sound,bool system,
                                    const uint8_t melodies[SOUND_CATEGORY_COUNT])
{
    if (v == NULL) return;
    set_switch(v->general_switch,general);set_switch(v->sound_switch,sound);set_switch(v->system_switch,system);
    for (unsigned i=0U;i<SOUND_CATEGORY_COUNT;++i) {
        if (v->melodies[i] != NULL && melodies != NULL && melodies[i]<SOUND_VARIANT_COUNT &&
            lv_dropdown_get_selected(v->melodies[i]) != melodies[i]) lv_dropdown_set_selected(v->melodies[i],melodies[i]);
    }
}
np_settings_notifications_view_t np_settings_notifications_scene_build(lv_obj_t *parent)
{
    np_settings_notifications_view_t v={0};v.root=np_scene(parent);np_set_visible(v.root,false);
    v.header=np_header(v.root);
    v.back_button=np_button(v.root,NP_HEADER_NAV_X,12,NP_HEADER_NAV_W,NP_TOUCH_TARGET,"Voltar",false);
    lv_obj_t *panel=np_panel(v.root,24,80,976,496);
    lv_obj_t *badge=np_fill(panel,24,18,48,48,np_c_accent(),LV_OPA_COVER,LV_RADIUS_CIRCLE);
    np_label(badge,NP_ICON_NOTIFICATIONS,NP_FONT_ICON,np_c_text_on_accent(),0,10,48,LV_TEXT_ALIGN_CENTER);
    np_label(panel,"Notificações e sons",NP_FONT_TITLE,np_c_text(),92,12,820,LV_TEXT_ALIGN_LEFT);
    np_label(panel,"Escolha como o NovaPanel chama sua atenção",NP_FONT_SM,np_c_text_2(),92,51,820,LV_TEXT_ALIGN_LEFT);
    const char *titles[]={"Notificações","Sons do painel","Alertas do sistema"};
    const char *details[]={"Agenda e avisos gerais","Usa o volume geral","Rede e armazenamento"};
    lv_obj_t **switches[]={&v.notifications.general_switch,&v.notifications.sound_switch,&v.notifications.system_switch};
    for (unsigned i=0U;i<3U;++i) {
        lv_obj_t *card=np_fill(panel,24+(int)i*316,88,296,70,np_c_surface_raised(),LV_OPA_COVER,NP_RADIUS_CONTROL);
        np_label(card,titles[i],NP_FONT_SM,np_c_text(),14,10,206,LV_TEXT_ALIGN_LEFT);
        np_label(card,details[i],NP_FONT_SM,np_c_text_2(),14,40,266,LV_TEXT_ALIGN_LEFT);
        *switches[i]=notification_switch(card);
    }
    const char *categories[]={"Notificações","Alarmes","Inicialização"};
    const char *hints[]={"Avisos rápidos","Agenda e Pomodoro","Uma vez ao ligar"};
    const char *options[]={"Sem som\nCristal\nGotas\nDueto","Sem som\nAurora\nSinos\nPulso","Sem som\nBoas-vindas\nAscendente\nMinimal"};
    const char *icons[]={NP_ICON_NOTIFICATIONS,NP_ICON_CLOCK,NP_ICON_POWER};
    for (unsigned i=0U;i<SOUND_CATEGORY_COUNT;++i) {
        lv_obj_t *row=np_fill(panel,24,180+(int)i*78,928,68,np_c_surface_raised(),LV_OPA_COVER,NP_RADIUS_CONTROL);
        np_label(row,icons[i],NP_FONT_ICON,np_c_accent(),16,21,40,LV_TEXT_ALIGN_CENTER);
        np_label(row,categories[i],NP_FONT_MD,np_c_text(),72,8,230,LV_TEXT_ALIGN_LEFT);
        np_label(row,hints[i],NP_FONT_SM,np_c_text_2(),72,37,230,LV_TEXT_ALIGN_LEFT);
        v.notifications.melodies[i]=np_form_dropdown(row,316,10,400,48);
        lv_dropdown_set_options(v.notifications.melodies[i],options[i]);
        v.notifications.previews[i]=np_button(row,740,10,164,48,"Ouvir",false);
    }
    np_label(panel,"Sem som silencia apenas a categoria. Volume em Tela e som.",NP_FONT_SM,np_c_text_3(),24,422,928,LV_TEXT_ALIGN_LEFT);
    v.persistence_status=np_label(panel,"Carregando preferências...",NP_FONT_SM,np_c_text_3(),24,460,928,LV_TEXT_ALIGN_LEFT);
    return v;
}
