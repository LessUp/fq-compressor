#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <thread>
#include <utility>

namespace fqc::pipeline {

template <typename T, std::size_t Capacity>
requires(Capacity > 0 && (Capacity & (Capacity - 1)) == 0)
class SpscQueue {
public:
    SpscQueue() = default;

    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    void push(T item) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & kMask;
        while (next == tail_.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        storage_[head] = std::move(item);
        head_.store(next, std::memory_order_release);
    }

    [[nodiscard]] auto pop() -> std::optional<T> {
        const auto tail = tail_.load(std::memory_order_relaxed);
        while (tail == head_.load(std::memory_order_acquire)) {
            if (closed_.load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            std::this_thread::yield();
        }
        T item = std::move(storage_[tail]);
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return item;
    }

    void close() {
        closed_.store(true, std::memory_order_release);
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::atomic<bool> closed_{false};
    std::array<T, Capacity> storage_;
};

}  // namespace fqc::pipeline
