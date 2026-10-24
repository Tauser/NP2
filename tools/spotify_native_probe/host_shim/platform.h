#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_TIMEOUT 0x107
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_SPIRAM 4
typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(lock) pthread_mutex_lock(lock)
#define portEXIT_CRITICAL(lock) pthread_mutex_unlock(lock)
#define pdMS_TO_TICKS(ms) (ms)
typedef struct test_semaphore *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreTake(SemaphoreHandle_t, unsigned);
void xSemaphoreGive(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);
void *heap_caps_calloc(size_t, size_t, unsigned);
void *heap_caps_malloc(size_t, unsigned);
void heap_caps_free(void *);
size_t heap_caps_get_free_size(unsigned);
size_t heap_caps_get_largest_free_block(unsigned);
int64_t esp_timer_get_time(void);
void vTaskDelay(unsigned ticks);
esp_err_t heap_caps_monitor_local_minimum_free_size_start(void);
esp_err_t heap_caps_monitor_local_minimum_free_size_stop(void);
size_t heap_caps_get_minimum_free_size(unsigned);
unsigned uxTaskGetStackHighWaterMark(void *);
typedef void *esp_http_client_handle_t;
typedef struct {
    int event_id;
    void *user_data;
    char *header_key, *header_value;
} esp_http_client_event_t;
#define HTTP_EVENT_ON_HEADER 3
#define HTTP_METHOD_GET 0
#define HTTP_METHOD_POST 1
typedef struct {
    const char *url;
    int method;
    esp_err_t (*crt_bundle_attach)(void *);
    bool disable_auto_redirect;
    int max_redirection_count, timeout_ms, buffer_size, buffer_size_tx;
    esp_err_t (*event_handler)(esp_http_client_event_t *);
    void *user_data;
} esp_http_client_config_t;
esp_err_t esp_crt_bundle_attach(void *);
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t, const char *, const char *);
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t, int);
esp_err_t esp_http_client_open(esp_http_client_handle_t, int);
int esp_http_client_write(esp_http_client_handle_t, const char *, int);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
int esp_http_client_read(esp_http_client_handle_t, char *, int);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t);
bool esp_http_client_is_chunked_response(esp_http_client_handle_t);
esp_err_t esp_http_client_close(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
