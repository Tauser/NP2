#pragma once
#include <limits>
#include <memory>
#include <new>
#include <vector>
#include <string>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
/* Audio/decoded protocol objects only. TLS, HTTP wire bodies, JSON and
 * credentials keep their internal allocations. No global allocator override. */
template<class T> struct SpotifyAudioAllocator {
    using value_type = T;
    SpotifyAudioAllocator() = default;
    template<class U> SpotifyAudioAllocator(const SpotifyAudioAllocator<U>&) {}
    T *allocate(size_t count) {
        if (count > 1024U * 1024U / sizeof(T)) throw std::bad_alloc();
        void *p = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }
    void deallocate(T *p, size_t) { heap_caps_free(p); }
    template<class U> bool operator==(const SpotifyAudioAllocator<U>&) const { return true; }
    template<class U> bool operator!=(const SpotifyAudioAllocator<U>&) const { return false; }
};
#else
template<class T> using SpotifyAudioAllocator = std::allocator<T>;
#endif
using SpotifyAudioBytes = std::vector<uint8_t, SpotifyAudioAllocator<uint8_t>>;
using SpotifyAudioString = std::basic_string<char, std::char_traits<char>, SpotifyAudioAllocator<char>>;
