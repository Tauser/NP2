#ifndef BELL_TASK_H
#define BELL_TASK_H
#include <string>
#include <atomic>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
extern "C" void np_spotify_task_fault(void);
namespace bell {
class Task {
    int priority;
    std::atomic<bool> started{false};
    static void entry(void *arg) {
        Task *self = static_cast<Task*>(arg);
        try { self->runTask(); } catch (...) { np_spotify_task_fault(); }
        self->started.store(false);
        /* IDF owns both allocations, including deferred self-deletion. */
        vTaskDeleteWithCaps(nullptr);
    }
protected:
    virtual void runTask() = 0;
public:
    std::string TASK;
    int stackSize, core;
    bool runOnPSRAM;
    Task(std::string name, int size, int relativePriority, int cpu, bool external = true)
      : priority(relativePriority > 1 ? 3 : 2), TASK(std::move(name)), stackSize(size),
        core(cpu), runOnPSRAM(external) {}
    virtual ~Task() = default; /* Runtime keeps worker objects alive until reboot. */
    bool startTask() {
        bool expected = false;
        if (!started.compare_exchange_strong(expected, true)) return false;
        TaskHandle_t handle = nullptr;
        const unsigned caps = MALLOC_CAP_8BIT | (runOnPSRAM ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL);
        if (xTaskCreatePinnedToCoreWithCaps(entry, TASK.c_str(), stackSize, this, priority,
            &handle, core, caps) != pdPASS) { started.store(false); np_spotify_task_fault(); return false; }
        return true;
    }
};
}
#endif
