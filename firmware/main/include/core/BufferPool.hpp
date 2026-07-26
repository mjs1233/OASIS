//
// Created by tgian on 26. 7. 26..
//

#ifndef FIRMWARE_BUFFERPOOL_HPP
#define FIRMWARE_BUFFERPOOL_HPP
#include <array>
#include <cstddef>
#include <new>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"

static constexpr size_t kCacheLineSize = 32;

template <typename T, size_t Size>
struct alignas(kCacheLineSize) CacheAlignedBuffer {
    std::array<T, Size> data;
    T* raw() { return data.data(); }
};

template <typename T, size_t BufSize, size_t BufCount>
class BufferPool {
public:
    BufferPool() {
        storage_ = static_cast<CacheAlignedBuffer<T, BufSize>*>(
            heap_caps_aligned_alloc(kCacheLineSize,
                                     sizeof(CacheAlignedBuffer<T, BufSize>) * BufCount,
                                     MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
        configASSERT(storage_ != nullptr);

        free_ = xQueueCreate(BufCount, sizeof(T*));
        ready_ = xQueueCreate(BufCount, sizeof(T*));

        for (size_t i = 0; i < BufCount; ++i) {
            new (&storage_[i]) CacheAlignedBuffer<T, BufSize>();
            T* p = storage_[i].raw();
            xQueueSend(free_, &p, 0);
        }
    }

    ~BufferPool() {
        vQueueDelete(free_);
        vQueueDelete(ready_);
        for (size_t i = 0; i < BufCount; ++i) {
            storage_[i].~CacheAlignedBuffer<T, BufSize>();
        }
        heap_caps_free(storage_);
    }

    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    T* acquire_free(TickType_t timeout = portMAX_DELAY) {
        T* p = nullptr;
        xQueueReceive(free_, &p, timeout);
        return p;
    }

    bool publish_ready(T* buf, TickType_t timeout = portMAX_DELAY) {
        return xQueueSend(ready_, &buf, timeout) == pdPASS;
    }

    T* acquire_ready(TickType_t timeout = portMAX_DELAY) {
        T* p = nullptr;
        xQueueReceive(ready_, &p, timeout);
        return p;
    }

    bool release_free(T* buf, TickType_t timeout = portMAX_DELAY) {
        return xQueueSend(free_, &buf, timeout) == pdPASS;
    }

    static constexpr size_t buffer_size() { return BufSize; }
    static constexpr size_t buffer_count() { return BufCount; }

private:
    CacheAlignedBuffer<T, BufSize>* storage_ = nullptr;
    QueueHandle_t free_ = nullptr;
    QueueHandle_t ready_ = nullptr;
};

#endif //FIRMWARE_BUFFERPOOL_HPP
