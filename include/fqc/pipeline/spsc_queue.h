#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <thread>
#include <utility>

namespace fqc::pipeline {

/// Single-producer single-consumer bounded ring buffer.
///
/// `close()` signals normal end-of-production: a consumer blocked on `pop()`
/// returns `std::nullopt` once the queue drains. `abort()` signals abnormal
/// shutdown: a producer blocked on a full `push()` returns `false` so it can
/// stop without deadlocking when the consumer has already failed.
template <typename T, std::size_t Capacity>
requires(Capacity > 0 && (Capacity & (Capacity - 1)) == 0)
class SpscQueue {
public:
    SpscQueue() = default;

    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    /// Enqueue an item. Returns false if the queue was aborted (item dropped);
    /// otherwise blocks until space is available and returns true.
    auto push(T item) -> bool {
        while (true) {
            if (aborted_.load(std::memory_order_acquire)) {
                return false;
            }
            const auto head = head_.load(std::memory_order_relaxed);
            const auto next = (head + 1) & kMask;
            if (next == tail_.load(std::memory_order_acquire)) {
                std::this_thread::yield();
                continue;
            }
            storage_[head] = std::move(item);
            head_.store(next, std::memory_order_release);
            return true;
        }
    }

    /// Dequeue an next item, or `std::nullopt` if the queue is empty and has
    /// been closed or aborted.
    ///
    /// `head_` and `closed_`/`aborted_` are distinct atomics, so the initial
    /// `head_` load may observe a stale value even after `closed_` becomes
    /// visible (acquire on `closed_` only orders writes sequenced-before the
    /// matching release store, not the earlier `head_` load in this loop). The
    /// re-check below relies on that acquire edge: once `closed_`/`aborted_` is
    /// observed, every prior producer write — including `head_` — is visible,
    /// so the second `head_` load cannot miss a just-pushed item.
    [[nodiscard]] auto pop() -> std::optional<T> {
        while (true) {
            const auto tail = tail_.load(std::memory_order_relaxed);
            if (tail != head_.load(std::memory_order_acquire)) {
                T item = std::move(storage_[tail]);
                tail_.store((tail + 1) & kMask, std::memory_order_release);
                return item;
            }
            if (aborted_.load(std::memory_order_acquire) ||
                closed_.load(std::memory_order_acquire)) {
                if (tail != head_.load(std::memory_order_acquire)) {
                    T item = std::move(storage_[tail]);
                    tail_.store((tail + 1) & kMask, std::memory_order_release);
                    return item;
                }
                return std::nullopt;
            }
            std::this_thread::yield();
        }
    }

    /// Signal that production is complete (no more items will be pushed).
    void close() {
        closed_.store(true, std::memory_order_release);
    }

    /// Signal abnormal shutdown: unblock a producer blocked on a full push and
    /// a consumer blocked on an empty pop.
    void abort() {
        aborted_.store(true, std::memory_order_release);
    }

    [[nodiscard]] auto isAborted() const noexcept -> bool {
        return aborted_.load(std::memory_order_acquire);
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) std::atomic<bool> closed_{false};
    alignas(64) std::atomic<bool> aborted_{false};
    std::array<T, Capacity> storage_;
};

}  // namespace fqc::pipeline
