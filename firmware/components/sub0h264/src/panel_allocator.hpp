/** NP2 local addition: keeps decoder working memory out of internal SRAM.
 *
 *  The panel reserves SRAM for TLS, JSON and HTTP. Small transient vectors stay
 *  in the default heap; anything larger goes to PSRAM, which is the placement
 *  measured in ADR-121. Allocation failure still terminates, because the decoder
 *  vectors live inside noexcept functions: callers must admit the session first.
 */
#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include <vector>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace sub0h264
{

inline constexpr std::size_t cPanelSmallAllocBytes = 1024U;

template <typename T>
struct PanelAllocator
{
    using value_type = T;

    PanelAllocator() noexcept = default;
    template <typename U>
    PanelAllocator(const PanelAllocator<U>&) noexcept {}

    T* allocate(std::size_t count)
    {
        const std::size_t bytes = count * sizeof(T);
#ifdef ESP_PLATFORM
        void* const memory = bytes > cPanelSmallAllocBytes
            ? heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
            : std::malloc(bytes);
#else
        void* const memory = std::malloc(bytes);
#endif
        if (memory == nullptr)
            std::abort();
        return static_cast<T*>(memory);
    }

    void deallocate(T* memory, std::size_t) noexcept { std::free(memory); }

    template <typename U>
    bool operator==(const PanelAllocator<U>&) const noexcept { return true; }
};

template <typename T>
using Vector = std::vector<T, PanelAllocator<T>>;

} // namespace sub0h264
