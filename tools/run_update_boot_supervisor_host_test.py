"""Exercise the real supervisor loop with deterministic platform faults."""
from pathlib import Path
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]
PLATFORM = r'''
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NOT_FOUND 2
#define ESP_ERR_NO_MEM 3
#define ESP_OTA_IMG_UNDEFINED 0
#define ESP_OTA_IMG_PENDING_VERIFY 1
#define ESP_OTA_IMG_VALID 2
typedef int esp_ota_img_states_t;
typedef struct { const char *label; } esp_partition_t;
typedef struct {bool display_ready,first_frame_presented; uint64_t last_ui_progress_ms;} board_bringup_health_t;
typedef struct {bool ready; uint64_t last_progress_ms;} app_state_health_t;
typedef struct {bool ready,busy,pending; uint32_t last_sequence; esp_err_t last_result;} flash_coordinator_status_t;
#define pdMS_TO_TICKS(ms) (ms)
#define pdPASS 1
/* Evaluate log arguments to catch format/type and unused-variable errors. */
#define ESP_LOGI(tag, ...) do {(void)(tag); if (false) printf(__VA_ARGS__);} while(0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
const char *esp_err_to_name(esp_err_t e);
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(const void *p);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p,esp_ota_img_states_t *s);
int64_t esp_timer_get_time(void);
void board_bringup_get_health(board_bringup_health_t *s);
void app_state_get_health(app_state_health_t *s);
void flash_coordinator_get_status(flash_coordinator_status_t *s);
esp_err_t flash_coordinator_request_p4_ota_confirm(void);
esp_err_t flash_coordinator_request_p4_ota_rollback(void);
int xTaskCreate(void (*fn)(void *), const char *, unsigned, void *, unsigned, void *);
void vTaskDelay(unsigned ms);
void vTaskDelete(void *p);
'''
TEST = r'''
#include <assert.h>
#include "update_boot_supervisor.c"
static uint64_t now_ms, submitted_ms;
static int running_state, lookup_result, confirms, rollbacks;
static bool healthy, fallback_valid, fail_confirm, hang_flash;
static flash_coordinator_status_t status;
static void (*task_fn)(void *);
static const esp_partition_t running={"ota_0"}, fallback={"ota_1"};
const char *esp_err_to_name(esp_err_t e){(void)e;return "mock";}
const esp_partition_t *esp_ota_get_running_partition(void){return &running;}
const esp_partition_t *esp_ota_get_next_update_partition(const void *p){(void)p;return &fallback;}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p,int *s){
    *s=p==&running?running_state:(fallback_valid?ESP_OTA_IMG_VALID:ESP_OTA_IMG_UNDEFINED);
    return p==&running?lookup_result:ESP_OK;
}
int64_t esp_timer_get_time(void){return (int64_t)now_ms*1000;}
void board_bringup_get_health(board_bringup_health_t *s){
    *s=(board_bringup_health_t){healthy,healthy,now_ms};
}
void app_state_get_health(app_state_health_t *s){*s=(app_state_health_t){healthy,now_ms};}
void flash_coordinator_get_status(flash_coordinator_status_t *s){*s=status;}
esp_err_t flash_coordinator_request_p4_ota_confirm(void){
    ++confirms; status.pending=true; submitted_ms=now_ms; return ESP_OK;
}
esp_err_t flash_coordinator_request_p4_ota_rollback(void){
    ++rollbacks; status.pending=true; submitted_ms=now_ms; return ESP_OK;
}
int xTaskCreate(void (*fn)(void *),const char *n,unsigned b,void *a,unsigned p,void *h){
    (void)n;(void)b;(void)a;(void)p;(void)h;task_fn=fn;return pdPASS;
}
void vTaskDelete(void *p){(void)p;}
void vTaskDelay(unsigned ms){
    now_ms+=ms; assert(now_ms<70000U);
    if(status.pending && !hang_flash && now_ms-submitted_ms>=1000U){
        status.pending=false;++status.last_sequence;
        status.last_result=ESP_FAIL;
        if(confirms && !rollbacks && !fail_confirm){
            running_state=ESP_OTA_IMG_VALID;status.last_result=ESP_OK;
        }
        /* Rollback returns failure in this harness instead of rebooting host. */
    }
}
static void reset(void){
    now_ms=100;submitted_ms=0;confirms=rollbacks=0;running_state=ESP_OTA_IMG_PENDING_VERIFY;
    lookup_result=ESP_OK;healthy=fallback_valid=true;fail_confirm=hang_flash=false;
    status=(flash_coordinator_status_t){.ready=true};task_fn=NULL;s_started=false;
}
static void run(void){assert(update_boot_supervisor_start()==ESP_OK);assert(task_fn);task_fn(NULL);}
int main(void){
    reset();running_state=ESP_OTA_IMG_VALID;assert(update_boot_supervisor_start()==ESP_OK);assert(!task_fn);
    reset();lookup_result=ESP_ERR_NOT_FOUND;assert(update_boot_supervisor_start()==ESP_OK);assert(!task_fn);
    reset();run();assert(confirms==1 && rollbacks==0 && now_ms>=16100U);
    reset();healthy=false;run();assert(confirms==0 && rollbacks==1 && submitted_ms==60100U);
    reset();fail_confirm=true;run();assert(confirms==1 && rollbacks==1 && submitted_ms==60100U);
    reset();healthy=false;fallback_valid=false;run();assert(confirms==0 && rollbacks==0 && now_ms==60100U);
    reset();hang_flash=true;run();assert(confirms==1 && rollbacks==0 && now_ms==65100U);
    puts("update_boot_supervisor_host_test: 7 scenarios passed");return 0;
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix='np2-boot-supervisor-') as tmp:
        root = Path(tmp)
        (root / 'test_platform.h').write_text(PLATFORM)
        headers = ('update_boot_supervisor.h', 'app_state.h', 'board_bringup.h',
                   'esp_log.h', 'esp_ota_ops.h', 'esp_timer.h', 'flash_coordinator.h',
                   'freertos/FreeRTOS.h', 'freertos/task.h')
        for header in headers:
            path = root / header
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#include "test_platform.h"\n')
        for name in ('update_boot_supervisor.c', 'update_policy.c', 'update_policy.h'):
            (root / name).write_bytes((REPO / 'firmware/main' / name).read_bytes())
        (root / 'test.c').write_text(TEST)
        exe = root / 'test.exe'
        subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                        '-I', str(root), str(root / 'test.c'), str(root / 'update_policy.c'),
                        '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
