#include "notification_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static portMUX_TYPE l=portMUX_INITIALIZER_UNLOCKED; static TaskHandle_t t; static bool started,pending; static notification_service_status_t s={true,true,true,0,ESP_OK};
static void run(void *x){(void)x;for(;;){(void)ulTaskNotifyTake(pdTRUE,portMAX_DELAY);portENTER_CRITICAL(&l);if(pending){pending=false;++s.generation;s.last_result=ESP_OK;}portEXIT_CRITICAL(&l);}}
esp_err_t notification_service_start(void){portENTER_CRITICAL(&l);if(started){portEXIT_CRITICAL(&l);return ESP_ERR_INVALID_STATE;}started=true;portEXIT_CRITICAL(&l);if(xTaskCreate(run,"notifications",3072U,NULL,2U,&t)!=pdPASS){portENTER_CRITICAL(&l);started=false;portEXIT_CRITICAL(&l);return ESP_ERR_NO_MEM;}return ESP_OK;}
static esp_err_t set(unsigned f,bool v){portENTER_CRITICAL(&l);if(!started||t==NULL){portEXIT_CRITICAL(&l);return ESP_ERR_INVALID_STATE;}if(f==0)s.general_enabled=v;else if(f==1)s.sound_enabled=v;else s.system_alerts_enabled=v;pending=true;TaskHandle_t q=t;portEXIT_CRITICAL(&l);xTaskNotifyGive(q);return ESP_OK;}
esp_err_t notification_service_set_general_enabled(bool v){return set(0,v);} esp_err_t notification_service_set_sound_enabled(bool v){return set(1,v);} esp_err_t notification_service_set_system_alerts_enabled(bool v){return set(2,v);} void notification_service_get_status(notification_service_status_t *o){if(!o)return;portENTER_CRITICAL(&l);*o=s;portEXIT_CRITICAL(&l);}
