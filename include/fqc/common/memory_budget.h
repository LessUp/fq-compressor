// =============================================================================
// fq-compressor - Memory Budget Management Module
// =============================================================================
// Implements memory budget calculation, monitoring, and divide-and-conquer
// strategy for handling files that exceed memory limits.
//
// This module provides:
// - MemoryBudget: Configuration for memory allocation limits
// - MemoryEstimator: Estimates memory requirements for compression phases
// - ChunkPlanner: Plans file chunking for divide-and-conquer mode
// - MemoryMonitor: Runtime memory usage monitoring
//
// Memory Model (from design.md):
// - Phase 1 (Global Analysis): ~24 bytes/read (Minimizer index + Reorder Map)
// - Phase 2 (Block Compression): ~50 bytes/read × block_size
//
// Divide-and-Conquer Strategy:
// When file exceeds memory budget:
// 1. Split input into N chunks (each fits in memory)
// 2. Each chunk independently executes two-phase compression
// 3. Archive order concatenates by chunk order
// 4. Reorder Map concatenates with offset accumulation
//
// Requirements: 4.3
// =============================================================================

#ifndef FQC_COMMON_MEMORY_BUDGET_H
#define FQC_COMMON_MEMORY_BUDGET_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc {

// =============================================================================
// Constants
// =============================================================================

/// @brief Memory per read for Phase 1 (bytes)
/// Includes: Minimizer index (~16 bytes) + Reorder Map (~8 bytes)
inline constexpr std::size_t kMemoryPerReadPhase1 = 24;

/// @brief Memory per read for Phase 2 (bytes)
/// Includes: Read data + encoding buffers
inline constexpr std::size_t kMemoryPerReadPhase2 = 50;

/// @brief Default total memory limit (MB)
inline constexpr std::size_t kDefaultMaxTotalMB = 8192;  // 8GB

/// @brief Default Phase 1 reserve (MB)
inline constexpr std::size_t kDefaultPhase1ReserveMB = 2048;  // 2GB

/// @brief Default block buffer pool size (MB)
inline constexpr std::size_t kDefaultBlockBufferMB = 512;

/// @brief Default per-worker stack space (MB)
inline constexpr std::size_t kDefaultWorkerStackMB = 64;

/// @brief Minimum memory limit (MB)
inline constexpr std::size_t kMinMemoryLimitMB = 256;

/// @brief Minimum chunk size (reads) to avoid excessive overhead
inline constexpr std::size_t kMinChunkReads = 100'000;

/// @brief Safety margin factor for memory estimation (1.1 = 10% overhead)
inline constexpr double kMemorySafetyMargin = 1.1;

// =============================================================================
// MemoryBudget Structure
// =============================================================================

/// @brief Configuration for memory allocation limits
/// @note All sizes are in megabytes (MB)
struct MemoryBudget {
    /// @brief Total memory upper limit (default: 8GB)
    std::size_t maxTotalMB = kDefaultMaxTotalMB;

    /// @brief Phase 1 reserve for Minimizer index (default: 2GB)
    std::size_t phase1ReserveMB = kDefaultPhase1ReserveMB;

    /// @brief Block buffer pool size (default: 512MB)
    std::size_t blockBufferMB = kDefaultBlockBufferMB;

    /// @brief Per-worker stack space (default: 64MB)
    std::size_t workerStackMB = kDefaultWorkerStackMB;

    /// @brief Default constructor with default values
    constexpr MemoryBudget() noexcept = default;

    /// @brief Construct with custom total memory limit
    /// @param totalMB Total memory limit in MB
    explicit constexpr MemoryBudget(std::size_t totalMB) noexcept
        : maxTotalMB(totalMB),
          phase1ReserveMB(std::min(totalMB / 4, kDefaultPhase1ReserveMB)),
          blockBufferMB(std::min(totalMB / 16, kDefaultBlockBufferMB)),
          workerStackMB(kDefaultWorkerStackMB) {}

    /// @brief Construct with all parameters
    /// @param totalMB Total memory limit in MB
    /// @param phase1MB Phase 1 reserve in MB
    /// @param blockMB Block buffer pool in MB
    /// @param workerMB Per-worker stack in MB
    constexpr MemoryBudget(std::size_t totalMB, std::size_t phase1MB,
                           std::size_t blockMB, std::size_t workerMB) noexcept
        : maxTotalMB(totalMB),
          phase1ReserveMB(phase1MB),
          blockBufferMB(blockMB),
          workerStackMB(workerMB) {}

    /// @brief Get total memory limit in bytes
    [[nodiscard]] constexpr std::size_t maxTotalBytes() const noexcept {
        return maxTotalMB * 1024 * 1024;
    }

    /// @brief Get Phase 1 reserve in bytes
    [[nodiscard]] constexpr std::size_t phase1ReserveBytes() const noexcept {
        return phase1ReserveMB * 1024 * 1024;
    }

    /// @brief Get block buffer pool size in bytes
    [[nodiscard]] constexpr std::size_t blockBufferBytes() const noexcept {
        return blockBufferMB * 1024 * 1024;
    }

    /// @brief Get per-worker stack space in bytes
    [[nodiscard]] constexpr std::size_t workerStackBytes() const noexcept {
        return workerStackMB * 1024 * 1024;
    }

    /// @brief Get available memory for Phase 2 (total - phase1 - buffers)
    [[nodiscard]] constexpr std::size_t phase2AvailableMB() const noexcept {
        std::size_t reserved = phase1ReserveMB + blockBufferMB;
        return (maxTotalMB > reserved) ? (maxTotalMB - reserved) : 0;
    }

    /// @brief Get available memory for Phase 2 in bytes
    [[nodiscard]] constexpr std::size_t phase2AvailableBytes() const noexcept {
        return phase2AvailableMB() * 1024 * 1024;
    }

    /// @brief Validate the memory budget configuration
    /// @return VoidResult indicating success or validation error
    [[nodiscard]] VoidResult validate() const;

    /// @brief Create a budget from command-line memory limit
    /// @param memoryLimitMB Memory limit from --memory-limit parameter
    /// @return Configured MemoryBudget
    [[nodiscard]] static MemoryBudget fromMemoryLimit(std::size_t memoryLimitMB);

    /// @brief Equality comparison
    [[nodiscard]] constexpr bool operator==(const MemoryBudget& other) const noexcept = default;
};

// =============================================================================
// Memory Estimation
// =============================================================================

/// @brief Estimated memory requirements for a compression operation
struct MemoryEstimate {
    /// @brief Phase 1 memory requirement (bytes)
    std::size_t phase1Bytes = 0;

    /// @brief Phase 2 memory requirement per block (bytes)
    std::size_t phase2BytesPerBlock = 0;

    /// @brief Total estimated peak memory (bytes)
    std::size_t peakBytes = 0;

    /// @brief Number of reads that can fit in memory for Phase 1
    std::size_t maxReadsPhase1 = 0;

    /// @brief Number of reads per block for Phase 2
    std::size_t readsPerBlock = 0;

    /// @brief Whether divide-and-conquer mode is required
    bool requiresChunking = false;

    /// @brief Recommended number of chunks (1 if no chunking needed)
    std::size_t recommendedChunks = 1;

    /// @brief Get Phase 1 memory in MB
    [[nodiscard]] constexpr std::size_t phase1MB() const noexcept {
        return phase1Bytes / (1024 * 1024);
    }

    /// @brief Get peak memory in MB
    [[nodiscard]] constexpr std::size_t peakMB() const noexcept {
        return peakBytes / (1024 * 1024);
    }

    /// @brief Check if the estimate fits within a budget
    /// @param budget The memory budget to check against
    /// @return true if the estimate fits within the budget
    [[nodiscard]] bool fitsInBudget(const MemoryBudget& budget) const noexcept {
        return peakBytes <= budget.maxTotalBytes();
    }
};

/// @brief Estimates memory requirements for compression operations
class MemoryEstimator {
public:
    /// @brief Construct with memory budget
    /// @param budget Memory budget configuration
    explicit MemoryEstimator(MemoryBudget budget = {}) noexcept;

    /// @brief Estimate memory for compressing a given number of reads
    /// @param totalReads Total number of reads to compress
    /// @param readsPerBlock Number of reads per block
    /// @param numThreads Number of compression threads
    /// @return Memory estimate
    [[nodiscard]] MemoryEstimate estimate(std::size_t totalReads,
                                           std::size_t readsPerBlock,
                                           std::size_t numThreads = 1) const noexcept;

    /// @brief Estimate memory for Phase 1 only
    /// @param totalReads Total number of reads
    /// @return Phase 1 memory requirement in bytes
    [[nodiscard]] std::size_t estimatePhase1(std::size_t totalReads) const noexcept;

    /// @brief Estimate memory for Phase 2 (per block)
    /// @param readsPerBlock Number of reads per block
    /// @param numThreads Number of concurrent blocks being processed
    /// @return Phase 2 memory requirement in bytes
    [[nodiscard]] std::size_t estimatePhase2(std::size_t readsPerBlock,
                                              std::size_t numThreads = 1) const noexcept;

    /// @brief Calculate maximum reads that fit in Phase 1 memory
    /// @return Maximum number of reads
    [[nodiscard]] std::size_t maxReadsForPhase1() const noexcept;

    /// @brief Calculate optimal block size for given memory constraints
    /// @param numThreads Number of compression threads
    /// @return Recommended reads per block
    [[nodiscard]] std::size_t optimalBlockSize(std::size_t numThreads = 1) const noexcept;

    /// @brief Get the memory budget
    [[nodiscard]] const MemoryBudget& budget() const noexcept { return budget_; }

    /// @brief Update the memory budget
    /// @param budget New memory budget
    void setBudget(MemoryBudget budget) noexcept { budget_ = budget; }

private:
    MemoryBudget budget_;
};

// =============================================================================
// Chunk Planning for Divide-and-Conquer
// =============================================================================

/// @brief Represents a chunk of reads for divide-and-conquer processing
struct ChunkInfo {
    /// @brief Chunk index (0-based)
    std::size_t chunkIndex = 0;

    /// @brief Start read index in original file (0-based)
    std::uint64_t startReadIndex = 0;

    /// @brief End read index in original file (exclusive)
    std::uint64_t endReadIndex = 0;

    /// @brief Number of reads in this chunk
    [[nodiscard]] constexpr std::uint64_t readCount() const noexcept {
        return endReadIndex - startReadIndex;
    }

    /// @brief Starting archive ID for this chunk (for global continuity)
    std::uint64_t archiveIdOffset = 0;

    /// @brief Starting block ID for this chunk (for global continuity)
    BlockId blockIdOffset = 0;

    /// @brief Estimated memory requirement for this chunk (bytes)
    std::size_t estimatedMemory = 0;
};

/// @brief Plan for dividing a file into chunks
struct ChunkPlan {
    /// @brief Total number of reads in the file
    std::uint64_t totalReads = 0;

    /// @brief Number of chunks
    std::size_t numChunks = 1;

    /// @brief Individual chunk information
    std::vector<ChunkInfo> chunks;

    /// @brief Whether chunking is required
    [[nodiscard]] bool requiresChunking() const noexcept { return numChunks > 1; }

    /// @brief Get chunk containing a specific read index
    /// @param readIndex Read index (0-based)
    /// @return Chunk index, or SIZE_MAX if not found
    [[nodiscard]] std::size_t findChunk(std::uint64_t readIndex) const noexcept;

    /// @brief Validate the chunk plan
    /// @return VoidResult indicating success or validation error
    [[nodiscard]] VoidResult validate() const;
};

/// @brief Plans file chunking for divide-and-conquer mode
class ChunkPlanner {
public:
    /// @brief Construct with memory budget
    /// @param budget Memory budget configuration
    explicit ChunkPlanner(MemoryBudget budget = {}) noexcept;

    /// @brief Plan chunks for a file with given number of reads
    /// @param totalReads Total number of reads in the file
    /// @param readsPerBlock Number of reads per block
    /// @param numThreads Number of compression threads
    /// @return Chunk plan
    [[nodiscard]] ChunkPlan plan(std::uint64_t totalReads,
                                  std::size_t readsPerBlock,
                                  std::size_t numThreads = 1) const;

    /// @brief Plan chunks with estimated average read length
    /// @param totalReads Total number of reads
    /// @param avgReadLength Average read length in bases
    /// @param readsPerBlock Number of reads per block
    /// @param numThreads Number of compression threads
    /// @return Chunk plan
    [[nodiscard]] ChunkPlan planWithReadLength(std::uint64_t totalReads,
                                                std::size_t avgReadLength,
                                                std::size_t readsPerBlock,
                                                std::size_t numThreads = 1) const;

    /// @brief Calculate optimal chunk size
    /// @param numThreads Number of compression threads
    /// @return Optimal number of reads per chunk
    [[nodiscard]] std::size_t optimalChunkSize(std::size_t numThreads = 1) const noexcept;

    /// @brief Get the memory budget
    [[nodiscard]] const MemoryBudget& budget() const noexcept { return budget_; }

    /// @brief Update the memory budget
    /// @param budget New memory budget
    void setBudget(MemoryBudget budget) noexcept { budget_ = budget; }

private:
    MemoryBudget budget_;
    MemoryEstimator estimator_;
};

// =============================================================================
// Memory Monitor
// =============================================================================

/// @brief Current memory usage snapshot
struct MemoryUsage {
    /// @brief Current resident set size (RSS) in bytes
    std::size_t rssBytes = 0;

    /// @brief Peak RSS in bytes
    std::size_t peakRssBytes = 0;

    /// @brief Virtual memory size in bytes
    std::size_t virtualBytes = 0;

    /// @brief Get RSS in MB
    [[nodiscard]] constexpr std::size_t rssMB() const noexcept {
        return rssBytes / (1024 * 1024);
    }

    /// @brief Get peak RSS in MB
    [[nodiscard]] constexpr std::size_t peakRssMB() const noexcept {
        return peakRssBytes / (1024 * 1024);
    }

    /// @brief Get virtual memory in MB
    [[nodiscard]] constexpr std::size_t virtualMB() const noexcept {
        return virtualBytes / (1024 * 1024);
    }
};

/// @brief Callback type for memory threshold alerts
using MemoryAlertCallback = std::function<void(const MemoryUsage&, std::size_t threshold)>;

/// @brief Monitors runtime memory usage
class MemoryMonitor {
public:
    /// @brief Construct with memory budget
    /// @param budget Memory budget configuration
    explicit MemoryMonitor(MemoryBudget budget = {}) noexcept;

    /// @brief Get current memory usage
    /// @return Current memory usage snapshot
    [[nodiscard]] MemoryUsage currentUsage() const;

    /// @brief Check if current usage exceeds budget
    /// @return true if memory usage exceeds the budget
    [[nodiscard]] bool isOverBudget() const;

    /// @brief Check if current usage exceeds a percentage of budget
    /// @param percentage Percentage threshold (0-100)
    /// @return true if usage exceeds the threshold
    [[nodiscard]] bool exceedsThreshold(double percentage) const;

    /// @brief Get remaining memory budget
    /// @return Remaining bytes available, or 0 if over budget
    [[nodiscard]] std::size_t remainingBytes() const;

    /// @brief Get remaining memory budget in MB
    [[nodiscard]] std::size_t remainingMB() const;

    /// @brief Get usage as percentage of budget
    /// @return Usage percentage (0-100+)
    [[nodiscard]] double usagePercentage() const;

    /// @brief Set alert callback for memory threshold
    /// @param threshold Threshold percentage (0-100)
    /// @param callback Callback to invoke when threshold is exceeded
    void setAlertCallback(double threshold, MemoryAlertCallback callback);

    /// @brief Clear alert callback
    void clearAlertCallback() noexcept;

    /// @brief Check and trigger alert if threshold exceeded
    void checkAlert();

    /// @brief Get the memory budget
    [[nodiscard]] const MemoryBudget& budget() const noexcept { return budget_; }

    /// @brief Update the memory budget
    /// @param budget New memory budget
    void setBudget(MemoryBudget budget) noexcept { budget_ = budget; }

    /// @brief Reset peak memory tracking
    void resetPeak() noexcept;

private:
    MemoryBudget budget_;
    std::optional<double> alertThreshold_;
    MemoryAlertCallback alertCallback_;
    mutable std::size_t peakRss_ = 0;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Get system available memory
/// @return Available memory in bytes, or 0 if unable to determine
[[nodiscard]] std::size_t getSystemAvailableMemory() noexcept;

/// @brief Get system total memory
/// @return Total memory in bytes, or 0 if unable to determine
[[nodiscard]] std::size_t getSystemTotalMemory() noexcept;

/// @brief Get current process memory usage
/// @return Current memory usage snapshot
[[nodiscard]] MemoryUsage getProcessMemoryUsage() noexcept;

/// @brief Format memory size as human-readable string
/// @param bytes Memory size in bytes
/// @return Formatted string (e.g., "1.5 GB", "256 MB")
[[nodiscard]] std::string formatMemorySize(std::size_t bytes);

/// @brief Parse memory size from string
/// @param str Memory size string (e.g., "8G", "512M", "8192")
/// @return Memory size in MB, or nullopt if parsing fails
[[nodiscard]] std::optional<std::size_t> parseMemorySize(std::string_view str);

/// @brief Calculate recommended memory limit based on system resources
/// @param maxPercentage Maximum percentage of system memory to use (default: 75%)
/// @return Recommended memory limit in MB
[[nodiscard]] std::size_t recommendedMemoryLimit(double maxPercentage = 0.75) noexcept;

}  // namespace fqc

#endif  // FQC_COMMON_MEMORY_BUDGET_H
