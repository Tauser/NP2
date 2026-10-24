#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "agenda_service.h"
#include "google_agenda_codec.h"

static agenda_command_t event(void)
{
    agenda_command_t c = {.request_id=1U,.action=AGENDA_UPSERT,
        .item={.kind=AGENDA_EVENT,.start_unix_s=1800000000U,.end_unix_s=1800003600U,.reminder_minutes=15U}};
    strcpy(c.item.title,"Reunião de projeto"); return c;
}
static void domain(void)
{
    agenda_data_t data; agenda_init(&data);
    assert(agenda_data_valid(&data));
    agenda_command_t c=event(); assert(agenda_apply(&data,&c) == AGENDA_OK);
    assert(data.items[0].id == 1U && data.items[0].revision == 1U && data.remote[0].dirty);
    agenda_item_t reminder; bool changed;
    const uint32_t due=c.item.start_unix_s-900U;
    assert(!agenda_take_reminder(&data,false,due,&reminder,&changed) && !changed);
    assert(!agenda_take_reminder(&data,true,due-1U,&reminder,&changed));
    assert(agenda_take_reminder(&data,true,due,&reminder,&changed) && changed && reminder.id == 1U);
    assert(!agenda_take_reminder(&data,true,due+1U,&reminder,&changed) && !changed);
    c.item=data.items[0]; strcpy(c.item.title,"Título corrigido");
    assert(agenda_apply(&data,&c) == AGENDA_OK && data.items[0].reminded);
    assert(agenda_apply(&data,&c) == AGENDA_CONFLICT);
    c.item=data.items[0]; c.item.start_unix_s+=3600U; c.item.end_unix_s+=3600U;
    assert(agenda_apply(&data,&c) == AGENDA_OK && !data.items[0].reminded);
    assert(!agenda_take_reminder(&data,true,data.items[0].start_unix_s+100U,&reminder,&changed) && changed);
    c=event(); c.item.kind=AGENDA_TASK; c.item.end_unix_s=0U; c.item.start_unix_s=0U;
    assert(agenda_apply(&data,&c) == AGENDA_INVALID);
    c.item.reminder_minutes=AGENDA_REMINDER_NONE;
    assert(agenda_apply(&data,&c) == AGENDA_OK);
    c.item=data.items[1]; c.item.completed=1U; c.action=AGENDA_SET_COMPLETED;
    assert(agenda_apply(&data,&c) == AGENDA_OK && data.items[1].completed);
    c.item=data.items[1]; c.action=AGENDA_DELETE;
    assert(agenda_apply(&data,&c) == AGENDA_OK && data.remote[1].deleted);
    assert(agenda_apply(&data,&c) == AGENDA_NOT_FOUND);
    c=event(); memset(c.item.title,'a',sizeof(c.item.title)); assert(agenda_apply(&data,&c) == AGENDA_INVALID);
    c=event(); strcpy(c.item.title," \t"); assert(agenda_apply(&data,&c) == AGENDA_INVALID);
    c=event(); strcpy(c.item.title,"\xc0\xaf"); assert(agenda_apply(&data,&c) == AGENDA_INVALID);
    c=event(); strcpy(c.item.title,"Evento 🙂"); assert(agenda_apply(&data,&c) == AGENDA_OK);
    while (data.count < AGENDA_MAX_ITEMS) { c=event(); assert(agenda_apply(&data,&c) == AGENDA_OK); }
    c=event(); assert(agenda_apply(&data,&c) == AGENDA_FULL);
    assert(agenda_data_valid(&data));
    data.remote[0].reserved[0]=1U; strcpy(data.remote[0].etag,"new");
    c.item=data.items[0]; c.action=AGENDA_USE_GOOGLE;
    assert(agenda_apply(&data,&c) == AGENDA_OK && !data.remote[0].dirty && !data.remote[0].reserved[0]);
    c.item=data.items[0]; c.action=AGENDA_UPSERT; data.items[0].reserved[0]=1U;
    assert(agenda_apply(&data,&c) == AGENDA_INVALID);
}
static void codec(void)
{
    static google_json_t doc;
    const char *json="{\"items\":[{\"title\":\"Reuni\\u00e3o \\ud83d\\ude42\",\"done\":true}],\"n\":12}";
    assert(google_json_parse(&doc,json,strlen(json)));
    int array=google_json_field(&doc,0,"items"), object=array+1;
    char text[64]; assert(google_json_string(&doc,google_json_field(&doc,object,"title"),text,sizeof(text)));
    assert(strcmp(text,"Reunião 🙂") == 0);
    assert(google_json_true(&doc,google_json_field(&doc,object,"done")));
    const char *bad[]={"{\"a\":1,\"a\":2}","{\"a\":1,\"\\u0061\":2}","{\"a\":01}","{\"a\":1,}","{\"a\":1}x","[]","{\"a\":1e}","{\"a\":\"\\uZZZZ\"}"};
    for (unsigned i=0U;i<sizeof(bad)/sizeof(bad[0]);++i) assert(!google_json_parse(&doc,bad[i],strlen(bad[i])));
    char nested[128]="{\"a\":"; for (unsigned i=0U;i<10U;++i) strcat(nested,"["); strcat(nested,"0"); for (unsigned i=0U;i<10U;++i) strcat(nested,"]"); strcat(nested,"}");
    assert(!google_json_parse(&doc,nested,strlen(nested)));
    uint32_t epoch, equivalent;
    assert(google_agenda_parse_time("2026-10-19T12:30:00Z",&epoch));
    assert(google_agenda_parse_time("2026-10-19T09:30:00.123-03:00",&equivalent) && epoch == equivalent);
    assert(!google_agenda_parse_time("2026-02-29T00:00:00Z",&equivalent));
    assert(!google_agenda_parse_time("2026-10-19T24:00:00Z",&equivalent));
    assert(!google_agenda_parse_time("2026-10-19T12:30:00+99:00",&equivalent));
    assert(google_agenda_parse_time("2028-02-29T00:00:00Z",&equivalent));
    char formatted[21]; assert(google_agenda_format_time(epoch,formatted) && strcmp(formatted,"2026-10-19T12:30:00Z") == 0);
    assert(google_agenda_json_escape("A \"B\" \\",text,sizeof(text)) && strcmp(text,"A \\\"B\\\" \\\\") == 0);
    assert(!google_agenda_json_escape("line\nbreak",text,sizeof(text)));
}
static void months(void)
{
    _putenv("TZ=BRT3"); _tzset();
    const char *dates[]={"2026-10-21T12:00:00Z","2028-02-29T12:00:00Z","2026-12-31T23:00:00Z",
                         "2026-11-01T01:00:00Z"};
    const char *first[]={"2026-10-01T03:00:00Z","2028-02-01T03:00:00Z","2026-12-01T03:00:00Z",
                         "2026-10-01T03:00:00Z"};
    const char *next[]={"2026-11-01T03:00:00Z","2028-03-01T03:00:00Z","2027-01-01T03:00:00Z",
                        "2026-11-01T03:00:00Z"};
    for (unsigned i=0U;i<4U;++i) {
        uint32_t now,from,until,a,b;
        assert(google_agenda_parse_time(dates[i],&now));
        assert(google_agenda_parse_time(first[i],&a) && google_agenda_parse_time(next[i],&b));
        assert(google_agenda_month_window(now,&from,&until) && from == a && until == b);
    }
    uint32_t from,until;
    assert(!google_agenda_month_window(0U,&from,&until));
}
int main(void) { domain(); codec(); months(); puts("agenda domain, monthly timezone boundaries and Google codec: PASS"); return 0; }
