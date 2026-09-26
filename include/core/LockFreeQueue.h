#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <utility>
#include <optional>
#include <new>

namespace Recorder::Core {

    /**
     * @brief High-performance Single-Producer Single-Consumer (SPSC) lock-free circular queue.
     * Guaranteed wait-free enqueue and dequeue without locks or heap allocation during the hot path.
     */
    template <typename T, size_t Capacity = 1024>
    class LockFreeQueue {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

    public:
        LockFreeQueue() : m_head(0), m_tail(0) {
            m_ringBuffer.resize(Capacity);
        }

        ~LockFreeQueue() = default;

        LockFreeQueue(const LockFreeQueue&) = delete;
        LockFreeQueue& operator=(const LockFreeQueue&) = delete;

        bool TryPush(T&& item) {
            const size_t currentHead = m_head.load(std::memory_order_relaxed);
            const size_t currentTail = m_tail.load(std::memory_order_acquire);

            if ((currentHead - currentTail) >= Capacity) {
                // Buffer is full (producer must not block the capture thread)
                return false;
            }

            m_ringBuffer[currentHead & (Capacity - 1)] = std::move(item);
            m_head.store(currentHead + 1, std::memory_order_release);
            return true;
        }

        bool TryPop(T& item) {
            const size_t currentTail = m_tail.load(std::memory_order_relaxed);
            const size_t currentHead = m_head.load(std::memory_order_acquire);

            if (currentTail == currentHead) {
                // Buffer is empty
                return false;
            }

            item = std::move(m_ringBuffer[currentTail & (Capacity - 1)]);
            m_tail.store(currentTail + 1, std::memory_order_release);
            return true;
        }

        size_t ApproximateSize() const {
            const size_t currentHead = m_head.load(std::memory_order_relaxed);
            const size_t currentTail = m_tail.load(std::memory_order_relaxed);
            return (currentHead >= currentTail) ? (currentHead - currentTail) : 0;
        }

        bool Empty() const {
            return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
        }

    private:
        // Separate cache lines to avoid false sharing between producer and consumer cores
        alignas(64) std::atomic<size_t> m_head;
        alignas(64) std::atomic<size_t> m_tail;
        std::vector<T> m_ringBuffer;
    };

} // namespace Recorder::Core
