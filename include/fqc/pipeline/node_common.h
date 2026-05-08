// =============================================================================
// fq-compressor - Pipeline Node Common Types
// =============================================================================
// Shared node state and helpers for pipeline node modules.
// =============================================================================

#ifndef FQC_PIPELINE_NODE_COMMON_H
#define FQC_PIPELINE_NODE_COMMON_H

#include "fqc/pipeline/pipeline.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>

namespace fqc::pipeline {

// =============================================================================
// Node State
// =============================================================================

/// @brief State of a pipeline node
enum class NodeState : std::uint8_t {
    /// @brief Node is idle, ready to start
    kIdle = 0,

    /// @brief Node is running
    kRunning = 1,

    /// @brief Node has finished processing
    kFinished = 2,

    /// @brief Node encountered an error
    kError = 3,

    /// @brief Node was cancelled
    kCancelled = 4
};

/// @brief Convert NodeState to string
[[nodiscard]] constexpr std::string_view nodeStateToString(NodeState state) noexcept {
    switch (state) {
        case NodeState::kIdle:
            return "idle";
        case NodeState::kRunning:
            return "running";
        case NodeState::kFinished:
            return "finished";
        case NodeState::kError:
            return "error";
        case NodeState::kCancelled:
            return "cancelled";
    }
    return "unknown";
}

// =============================================================================
// Block Ordering Queue
// =============================================================================

/// @brief Thread-safe queue for maintaining block order
template <typename T>
class OrderedQueue {
public:
    /// @brief Construct with expected starting ID
    explicit OrderedQueue(std::uint32_t startId = 0) : nextExpectedId_(startId) {}

    /// @brief Push an item with its ID
    void push(std::uint32_t id, T item) {
        std::lock_guard lock(mutex_);
        pending_[id] = std::move(item);
    }

    /// @brief Try to pop the next expected item
    [[nodiscard]] std::optional<T> tryPop() {
        std::lock_guard lock(mutex_);
        auto it = pending_.find(nextExpectedId_);
        if (it == pending_.end()) {
            return std::nullopt;
        }
        T item = std::move(it->second);
        pending_.erase(it);
        ++nextExpectedId_;
        return item;
    }

    /// @brief Get next expected ID
    [[nodiscard]] std::uint32_t nextExpectedId() const noexcept {
        std::lock_guard lock(mutex_);
        return nextExpectedId_;
    }

    /// @brief Check if queue is empty
    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard lock(mutex_);
        return pending_.empty();
    }

    /// @brief Get number of pending items
    [[nodiscard]] std::size_t size() const noexcept {
        std::lock_guard lock(mutex_);
        return pending_.size();
    }

    /// @brief Clear the queue
    void clear() noexcept {
        std::lock_guard lock(mutex_);
        pending_.clear();
        nextExpectedId_ = 0;
    }

private:
    mutable std::mutex mutex_;
    std::map<std::uint32_t, T> pending_;
    std::uint32_t nextExpectedId_ = 0;
};

// =============================================================================
// Backpressure Controller
// =============================================================================

/// @brief Controls backpressure in the pipeline
class BackpressureController {
public:
    /// @brief Construct with limit
    explicit BackpressureController(std::size_t maxInFlight = kDefaultMaxInFlightBlocks);

    /// @brief Acquire a slot (blocks if at limit)
    void acquire();

    /// @brief Try to acquire a slot (non-blocking)
    [[nodiscard]] bool tryAcquire();

    /// @brief Release a slot
    void release();

    /// @brief Get current in-flight count
    [[nodiscard]] std::size_t inFlight() const noexcept;

    /// @brief Get maximum in-flight limit
    [[nodiscard]] std::size_t maxInFlight() const noexcept;

    /// @brief Reset controller
    void reset() noexcept;

private:
    std::size_t maxInFlight_;
    std::atomic<std::size_t> inFlight_{0};
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_NODE_COMMON_H
