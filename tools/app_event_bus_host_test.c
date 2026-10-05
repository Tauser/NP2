/* Real bus implementation; synchronized host queue models task concurrency. */
#include <winsock2.h>
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_event_bus.c"

struct test_queue {
    SRWLOCK lock;
    unsigned capacity, count, head;
    size_t item_size;
    unsigned char *data;
};
static bool fail_queue_allocation;

QueueHandle_t xQueueCreate(unsigned count, size_t item_size)
{
    if (fail_queue_allocation) return NULL;
    QueueHandle_t queue = calloc(1, sizeof(*queue));
    assert(queue != NULL);
    InitializeSRWLock(&queue->lock);
    queue->capacity = count;
    queue->item_size = item_size;
    queue->data = calloc(count, item_size);
    assert(queue->data != NULL);
    return queue;
}

int xQueueSend(QueueHandle_t queue, const void *item, unsigned ticks)
{
    assert(ticks == 0U);
    AcquireSRWLockExclusive(&queue->lock);
    const bool space = queue->count < queue->capacity;
    if (space) {
        const unsigned tail = (queue->head + queue->count) % queue->capacity;
        memcpy(queue->data + tail * queue->item_size, item, queue->item_size);
        ++queue->count;
    }
    ReleaseSRWLockExclusive(&queue->lock);
    return space ? pdPASS : 0;
}

int xQueueReceive(QueueHandle_t queue, void *item, unsigned ticks)
{
    (void)ticks;
    AcquireSRWLockExclusive(&queue->lock);
    const bool available = queue->count != 0U;
    if (available) {
        memcpy(item, queue->data + queue->head * queue->item_size, queue->item_size);
        queue->head = (queue->head + 1U) % queue->capacity;
        --queue->count;
    }
    ReleaseSRWLockExclusive(&queue->lock);
    return available ? pdPASS : 0;
}

static offline_data_snapshot_t snapshot(uint32_t sample)
{
    const offline_data_snapshot_t value = {
        .schema_version = OFFLINE_DATA_SCHEMA_VERSION,
        .origin = OFFLINE_DATA_ORIGIN_LIVE,
        .market = {.available = true, .bitcoin_usd_cents = sample,
                   .observed_at_unix_s = sample + 100000U},
    };
    assert(offline_data_snapshot_is_valid(&value));
    return value;
}

static app_event_t receive(void)
{
    app_event_t result;
    memset(&result, 0xa5, sizeof(result));
    assert(app_event_bus_receive(&result, 0) == ESP_OK);
    return result;
}

static void expect_empty(void)
{
    app_event_t event;
    assert(app_event_bus_receive(&event, 0) == ESP_ERR_TIMEOUT);
    for (unsigned i = 0; i < APP_EVENT_BUS_SNAPSHOT_SLOTS; ++i)
        assert(!s_snapshot_slots[i].in_use);
}

static void test_lifecycle_and_commands(void)
{
    offline_data_snapshot_t data = snapshot(100U);
    app_event_t event = {.type = APP_EVENT_REFRESH_PLATFORM};
    assert(app_event_bus_post(&event) == ESP_ERR_INVALID_STATE);
    assert(app_event_bus_post_product_data(&data) == ESP_ERR_INVALID_STATE);
    assert(app_event_bus_receive(&event, 0) == ESP_ERR_INVALID_STATE);
    fail_queue_allocation = true;
    assert(app_event_bus_start() == ESP_ERR_NO_MEM);
    fail_queue_allocation = false;
    assert(app_event_bus_start() == ESP_OK);
    assert(app_event_bus_start() == ESP_ERR_INVALID_STATE);
    app_event_bus_status_t status;
    app_event_bus_get_status(&status);
    assert(status.ready && status.posted_count == 0 && status.dropped_count == 0);
    assert(app_event_bus_post(NULL) == ESP_ERR_INVALID_STATE);
    assert(app_event_bus_receive(NULL, 0) == ESP_ERR_INVALID_STATE);
    assert(app_event_bus_post_product_data(NULL) == ESP_ERR_INVALID_ARG);
    assert(app_event_bus_post(&event) == ESP_OK);
    assert(receive().type == APP_EVENT_REFRESH_PLATFORM);
    event.type = (app_event_type_t)99;
    assert(app_event_bus_post(&event) == ESP_ERR_INVALID_ARG);

    event = (app_event_t){.type = APP_EVENT_USER_PROFILE_UPDATED,
        .user_profile = {.name = "Teste", .avatar_color = 3, .initial_screen = 4}};
    assert(app_event_bus_post(&event) == ESP_OK);
    memset(&event.user_profile, 0, sizeof(event.user_profile));
    app_event_t result = receive();
    assert(result.type == APP_EVENT_USER_PROFILE_UPDATED);
    assert(strcmp(result.user_profile.name, "Teste") == 0);
    assert(result.user_profile.avatar_color == 3 && result.user_profile.initial_screen == 4);
    assert(app_event_bus_post_pomodoro(POMODORO_COMMAND_SELECT_MINUTES, 25) == ESP_OK);
    result = receive();
    assert(result.type == APP_EVENT_POMODORO_COMMAND && result.pomodoro_value == 25);
    assert(result.pomodoro_command == POMODORO_COMMAND_SELECT_MINUTES);
    assert(app_event_bus_post_pomodoro(POMODORO_COMMAND_SELECT_MINUTES, 100) == ESP_ERR_INVALID_ARG);
    assert(app_event_bus_post_pomodoro((pomodoro_command_t)99, 0) == ESP_ERR_INVALID_ARG);
    assert(app_event_bus_post_onvif_scan_request() == ESP_OK);
    assert(receive().type == APP_EVENT_ONVIF_SCAN_REQUEST);
    assert(app_event_bus_post_onvif_address_request("192.168.1.20") == ESP_OK);
    result = receive();
    assert(result.type == APP_EVENT_ONVIF_ADDRESS_REQUEST);
    assert(strcmp(result.onvif_address, "192.168.1.20") == 0);
    assert(app_event_bus_post_onvif_address_request("8.8.8.8") == ESP_ERR_INVALID_ARG);
    assert(app_event_bus_post_sonoff_switch_request("123456abcd", 2, true) == ESP_OK);
    result = receive();
    assert(result.type == APP_EVENT_SONOFF_SWITCH_REQUEST && result.sonoff_enabled);
    assert(result.sonoff_channel == 2 && strcmp(result.sonoff_device_id, "123456abcd") == 0);
    assert(app_event_bus_post_sonoff_switch_request("123456abcd", 3, true) == ESP_ERR_INVALID_ARG);
    assert(app_event_bus_post_notification_mark_read(0) == ESP_ERR_INVALID_ARG);
    assert(app_event_bus_post_notification_mark_read(1234) == ESP_OK);
    result = receive();
    assert(result.type == APP_EVENT_NOTIFICATION_MARK_READ && result.notification_id == 1234);
    assert(app_event_bus_post_notification_mark_all_read() == ESP_OK);
    assert(receive().type == APP_EVENT_NOTIFICATION_MARK_ALL_READ);
    expect_empty();
}

static void test_snapshot_ownership_and_saturation(void)
{
    offline_data_snapshot_t data = snapshot(111U);
    app_event_t generic = {.type = APP_EVENT_PRODUCT_DATA_UPDATED, .offline_data = data};
    assert(app_event_bus_post(&generic) == ESP_OK);
    generic.offline_data.market.bitcoin_usd_cents = 999;
    app_event_t retained = receive();
    assert(retained.offline_data.market.bitcoin_usd_cents == 111);

    /* A full command queue must return the reserved snapshot slot. */
    for (unsigned i = 0; i < APP_EVENT_BUS_QUEUE_LENGTH; ++i)
        assert(app_event_bus_post_notification_mark_read(i + 1) == ESP_OK);
    app_event_bus_status_t before, after;
    app_event_bus_get_status(&before);
    assert(app_event_bus_post_product_data(&data) == ESP_ERR_TIMEOUT);
    assert(app_event_bus_post_onvif_scan_request() == ESP_ERR_TIMEOUT);
    app_event_bus_get_status(&after);
    assert(after.dropped_count == before.dropped_count + 2);
    for (unsigned i = 0; i < APP_EVENT_BUS_QUEUE_LENGTH; ++i)
        assert(receive().notification_id == i + 1);
    expect_empty();

    for (unsigned cycle = 0; cycle < 1000; ++cycle) {
        for (unsigned i = 0; i < APP_EVENT_BUS_SNAPSHOT_SLOTS; ++i) {
            data = snapshot(i + 1);
            assert(app_event_bus_post_product_data(&data) == ESP_OK);
            data.market.bitcoin_usd_cents = 888;
        }
        assert(app_event_bus_post_product_data(&data) == ESP_ERR_TIMEOUT);
        /* Pool saturation does not exhaust the remaining command capacity. */
        assert(app_event_bus_post_notification_mark_read(777) == ESP_OK);
        assert(receive().offline_data.market.bitcoin_usd_cents == 1);
        data = snapshot(5);
        assert(app_event_bus_post_product_data(&data) == ESP_OK);
        for (unsigned i = 2; i <= 4; ++i)
            assert(receive().offline_data.market.bitcoin_usd_cents == i);
        assert(receive().notification_id == 777);
        assert(receive().offline_data.market.bitcoin_usd_cents == 5);
        assert(retained.offline_data.market.bitcoin_usd_cents == 111);
        expect_empty();
    }
}

static void test_stale_handle_and_generation_wrap(void)
{
    offline_data_snapshot_t data = snapshot(10);
    assert(app_event_bus_post_product_data(&data) == ESP_OK);
    app_queued_event_t old_handle, new_handle;
    assert(xQueueReceive(s_queue, &old_handle, 0) == pdPASS);
    assert(xQueueSend(s_queue, &old_handle, 0) == pdPASS);
    assert(receive().offline_data.market.bitcoin_usd_cents == 10);
    data = snapshot(20);
    assert(app_event_bus_post_product_data(&data) == ESP_OK);
    assert(xQueueReceive(s_queue, &new_handle, 0) == pdPASS);
    assert(new_handle.payload.product.slot == old_handle.payload.product.slot);
    assert(xQueueSend(s_queue, &old_handle, 0) == pdPASS);
    assert(xQueueSend(s_queue, &new_handle, 0) == pdPASS);
    app_event_t rejected;
    assert(app_event_bus_receive(&rejected, 0) == ESP_ERR_INVALID_STATE);
    assert(s_snapshot_slots[new_handle.payload.product.slot].in_use);
    assert(receive().offline_data.market.bitcoin_usd_cents == 20);
    expect_empty();
    s_snapshot_slots[0].generation = UINT32_MAX;
    assert(app_event_bus_post_product_data(&data) == ESP_OK);
    assert(s_snapshot_slots[0].generation == 1);
    assert(receive().offline_data.market.bitcoin_usd_cents == 20);
    expect_empty();
}

/* Mock only delivery I/O; functions below are extracted from production. */
static offline_data_snapshot_t s_product_snapshot;
static bool s_product_delivery_pending;
static int64_t s_last_product_cache_write_us;
#define NP2_PRODUCT_CACHE_WRITE_INTERVAL_US INT64_C(1800000000)
static unsigned persistence_calls;
static int64_t esp_timer_get_time(void) { return 1000000; }
static esp_err_t flash_coordinator_request_offline_data_write(const offline_data_snapshot_t *data)
{
    assert(offline_data_snapshot_is_valid(data));
    ++persistence_calls;
    return ESP_OK;
}
#include "product_delivery_under_test.inc"

static void test_latest_delivery_retry(void)
{
    for (unsigned i = 0; i < APP_EVENT_BUS_SNAPSHOT_SLOTS; ++i) {
        offline_data_snapshot_t data = snapshot(i + 1);
        assert(app_event_bus_post_product_data(&data) == ESP_OK);
    }
    s_product_snapshot = snapshot(500);
    publish_product_snapshot();
    assert(s_product_delivery_pending && persistence_calls == 1);
    retry_pending_product_delivery();
    assert(s_product_delivery_pending && persistence_calls == 1);
    s_product_snapshot = snapshot(600);
    publish_product_snapshot();
    assert(s_product_delivery_pending && persistence_calls == 1);
    assert(receive().offline_data.market.bitcoin_usd_cents == 1);
    retry_pending_product_delivery();
    assert(!s_product_delivery_pending && persistence_calls == 1);
    for (unsigned i = 2; i <= 4; ++i)
        assert(receive().offline_data.market.bitcoin_usd_cents == i);
    assert(receive().offline_data.market.bitcoin_usd_cents == 600);
    retry_pending_product_delivery();
    expect_empty();
    s_product_snapshot.schema_version = 0;
    s_product_delivery_pending = true;
    publish_product_snapshot();
    assert(!s_product_delivery_pending && persistence_calls == 1);
}

#define PRODUCERS 4U
#define SAMPLES_PER_PRODUCER 2000U
static DWORD WINAPI producer(void *arg)
{
    const unsigned id = (unsigned)(uintptr_t)arg;
    for (unsigned i = 0; i < SAMPLES_PER_PRODUCER; ++i) {
        offline_data_snapshot_t data = snapshot(id * SAMPLES_PER_PRODUCER + i + 1);
        esp_err_t result;
        while ((result = app_event_bus_post_product_data(&data)) == ESP_ERR_TIMEOUT) Sleep(0);
        assert(result == ESP_OK);
        data.market.bitcoin_usd_cents = 0;
    }
    return 0;
}

static void test_concurrent_ownership(void)
{
    HANDLE threads[PRODUCERS];
    bool seen[PRODUCERS * SAMPLES_PER_PRODUCER] = {0};
    app_event_bus_status_t before, after;
    app_event_bus_get_status(&before);
    for (unsigned i = 0; i < PRODUCERS; ++i) {
        threads[i] = CreateThread(NULL, 0, producer, (void *)(uintptr_t)i, 0, NULL);
        assert(threads[i] != NULL);
    }
    const ULONGLONG deadline = GetTickCount64() + 30000;
    unsigned count = 0;
    while (count < PRODUCERS * SAMPLES_PER_PRODUCER) {
        assert(GetTickCount64() < deadline);
        app_event_t event;
        const esp_err_t result = app_event_bus_receive(&event, 0);
        if (result == ESP_ERR_TIMEOUT) { Sleep(0); continue; }
        assert(result == ESP_OK);
        assert(event.type == APP_EVENT_PRODUCT_DATA_UPDATED);
        const uint32_t value = event.offline_data.market.bitcoin_usd_cents;
        assert(value > 0 && value <= PRODUCERS * SAMPLES_PER_PRODUCER);
        assert(!seen[value - 1]);
        seen[value - 1] = true;
        assert(event.offline_data.market.observed_at_unix_s == value + 100000);
        ++count;
    }
    assert(WaitForMultipleObjects(PRODUCERS, threads, TRUE, 30000) == WAIT_OBJECT_0);
    for (unsigned i = 0; i < PRODUCERS; ++i) CloseHandle(threads[i]);
    app_event_bus_get_status(&after);
    assert(after.posted_count == before.posted_count + count);
    expect_empty();
}

int main(void)
{
    test_lifecycle_and_commands();
    test_snapshot_ownership_and_saturation();
    test_stale_handle_and_generation_wrap();
    test_latest_delivery_retry();
    test_concurrent_ownership();
    const size_t current = APP_EVENT_BUS_QUEUE_LENGTH * sizeof(app_queued_event_t) + sizeof(s_snapshot_slots);
    const size_t previous = APP_EVENT_BUS_QUEUE_LENGTH * sizeof(app_event_t);
    assert(current < previous && s_queue->item_size == sizeof(app_queued_event_t));
    printf("PASS lifecycle, commands, ownership, queue/pool saturation, newest retry, 8000 concurrent snapshots\n");
    printf("Host layout: public=%zu queued=%zu pool=%zu total=%zu saved=%zu bytes (verify target separately)\n",
           sizeof(app_event_t), sizeof(app_queued_event_t), sizeof(s_snapshot_slots), current, previous - current);
    free(s_queue->data);
    free(s_queue);
    return 0;
}
