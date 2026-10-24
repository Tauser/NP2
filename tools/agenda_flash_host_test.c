#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "agenda_service.h"
#include "esp_err.h"
#define MALLOC_CAP_SPIRAM 1U
#define MALLOC_CAP_8BIT 2U
#define pdTRUE 1
#define pdPASS 1
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
typedef struct {
    bool ready,littlefs_ready,busy,pending,p4_ota_active,p4_ota_finished,restart_pending;
    uint32_t last_sequence,agenda_completed_sequence;
    esp_err_t agenda_write_result;
} flash_coordinator_status_t;
enum { FLASH_REQUEST_AGENDA_WRITE };
typedef struct { int kind; uint32_t sequence; agenda_data_t *agenda; } flash_request_t;
static flash_request_t s_p4_ota_request,queued;
static flash_coordinator_status_t s_status;
static int s_status_lock;
static void *s_request_queue=(void *)1,*s_p4_ota_submission_lock=(void *)2;
static bool locked,fail_alloc,fail_queue;
static unsigned allocations,frees;
static esp_err_t write_result;
static agenda_data_t durable;
static int xSemaphoreTake(void *lock,int timeout)
{ (void)lock;(void)timeout; if (locked) return 0; locked=true;return 1; }
static void xSemaphoreGive(void *lock) { (void)lock;assert(locked);locked=false; }
static void flash_coordinator_get_status(flash_coordinator_status_t *out) { *out=s_status; }
static void set_busy(bool busy,bool pending) { s_status.busy=busy;s_status.pending=pending; }
static void *heap_caps_malloc(size_t size,unsigned caps)
{ assert(caps == (MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if (fail_alloc) return NULL;++allocations;return malloc(size); }
static void heap_caps_free(void *p) { assert(p);++frees;free(p); }
static int xQueueSend(void *queue,const flash_request_t *request,int timeout)
{ (void)queue;(void)timeout;if (fail_queue) return 0;queued=*request;return pdPASS; }
static esp_err_t agenda_storage_owner_write(const agenda_data_t *data)
{ assert(agenda_data_valid(data));durable=*data;return write_result; }
#include "agenda_flash_under_test.inc"
int main(void)
{
    agenda_data_t data;agenda_init(&data);
    agenda_command_t c={.request_id=1U,.action=AGENDA_UPSERT,
        .item={.kind=AGENDA_TASK,.reminder_minutes=AGENDA_REMINDER_NONE}};
    strcpy(c.item.title,"Snapshot mensal");
    while (data.count<AGENDA_MAX_ITEMS) assert(agenda_apply(&data,&c) == AGENDA_OK);
    s_status=(flash_coordinator_status_t){.ready=true,.littlefs_ready=true,.last_sequence=4U};
    uint32_t sequence=0U;
    fail_alloc=true;assert(flash_coordinator_request_agenda_write(&data,&sequence) == ESP_ERR_NO_MEM);
    assert(!locked && allocations == frees && !s_status.pending);
    fail_alloc=false;fail_queue=true;
    assert(flash_coordinator_request_agenda_write(&data,&sequence) == ESP_ERR_TIMEOUT);
    assert(!locked && allocations == frees && !s_status.pending);
    fail_queue=false;
    assert(flash_coordinator_request_agenda_write(&data,&sequence) == ESP_OK && sequence == 5U);
    assert(allocations == frees+1U && s_p4_ota_request.agenda == NULL);
    strcpy(data.items[0].title,"Edição após enqueue");
    assert(flash_coordinator_request_agenda_write(&data,&sequence) == ESP_ERR_INVALID_STATE);
    consume_agenda_request();
    assert(!strcmp(durable.items[0].title,"Snapshot mensal") && durable.count == AGENDA_MAX_ITEMS);
    assert(allocations == frees && s_status.agenda_completed_sequence == 5U && s_status.agenda_write_result == ESP_OK);
    s_status.pending=false;write_result=ESP_FAIL;
    assert(flash_coordinator_request_agenda_write(&data,&sequence) == ESP_OK);
    consume_agenda_request();
    assert(allocations == frees && s_status.agenda_write_result == ESP_FAIL);
    puts("Agenda flash mailbox: 64-item snapshot ownership, allocation/queue/write failures, busy admission PASS");
}
