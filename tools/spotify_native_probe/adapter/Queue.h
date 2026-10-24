#ifndef BELL_QUEUE_H
#define BELL_QUEUE_H
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <utility>
namespace bell {
/* Private Mercury mailbox: at most two authenticated packets (8 KiB each).
 * Saturation closes the session through its existing exception path. */
template<class T> class Queue {
    std::queue<T> queue;
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable space;
    bool stopped = false;
    bool take(T& out) {
        if (stopped || queue.empty()) return false;
        out = std::move(queue.front()); queue.pop(); space.notify_one(); return true;
    }
public:
    void push(const T& value) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!space.wait_for(lock, std::chrono::milliseconds(200), [&]{return queue.size() < 2;}))
            throw std::runtime_error("Spotify packet queue full");
        stopped = false; queue.push(value); lock.unlock(); ready.notify_one();
    }
    bool pop(T& out) { std::lock_guard<std::mutex> lock(mutex); return take(out); }
    bool wpop(T& out) {
        std::unique_lock<std::mutex> lock(mutex);
        ready.wait(lock, [&]{return stopped || !queue.empty();}); return take(out);
    }
    bool wtpop(T& out, long ms = 1000) {
        std::unique_lock<std::mutex> lock(mutex);
        ready.wait_for(lock, std::chrono::milliseconds(ms), [&]{return stopped || !queue.empty();});
        return take(out);
    }
    bool isEmpty() const { std::lock_guard<std::mutex> lock(mutex); return queue.empty(); }
    bool isExit() const { std::lock_guard<std::mutex> lock(mutex); return stopped; }
    int size() { std::lock_guard<std::mutex> lock(mutex); return static_cast<int>(queue.size()); }
    void clear() {
        std::unique_lock<std::mutex> lock(mutex);
        stopped = true; while (!queue.empty()) queue.pop(); lock.unlock(); ready.notify_all(); space.notify_all();
    }
};
}
#endif
