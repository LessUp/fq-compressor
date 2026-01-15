// =============================================================================
// fq-compressor - Memory Budget Management Implementation
// =============================================================================
// Implementation of memory budget calculation, monitoring, and divide-and-conquer
// strategy for handling files that exceed memory limits.
//
// Requirements: 4.3
// =============================================================================

#include "fqc/common/memory_budget.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#ifdef __linux__
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

namespace fqc {

// =============================================================================
// MemoryBudget Implementation
// =============================================================================

VoidResult MemoryBudget::validate() const {
    if (maxTotalMB < kMinMemoryLimitMB) {
        return makeVoidError(
            ErrorCode::kUsageError,
            "Memory limit must be at least " + std::to_string(kMinMemoryLimitMB) + " MB");
    }

    if (phase1ReserveMB >= maxTotalMB) {
        return makeVoidError(
            ErrorCode::kUsageError,
            "Phase 1 reserve (" + std::to_string(phase1ReserveMB) +
            " MB) must be less than total limit (" + std::to_string(maxTotalMB) + " MB)");
    }

    if (phase1ReserveMB + blockBufferMB >= maxTotalMB) {
        return makeVoidError(
            ErrorCode::kUsageError,
            "Phase 1 reserve + block buffer exceeds total memory limit");
    }

    return makeVoidSuccess();
}

MemoryBudget MemoryBudget::fromMemoryLimit(std::size_t memoryLimitMB) {
    // Ensure minimum limit
    std::size_t totalMB = std::max(memoryLimitMB, kMinMemoryLimitMB);

    // Calculate proportional allocations
    // Phase 1: ~25% of total (capped at 2GB)
    std::size_t phase1MB = std::min(totalMB / 4, kDefaultPhase1ReserveMB);

    // Block buffer: ~6% of total (capped at 512MB)
    std::size_t blockMB = std::min(totalMB / 16, kDefaultBlockBufferMB);

    // Worker stack: fixed at 64MB
    std::size_t workerMB = kDefaultWorkerStackMB;

    return MemoryBudget{totalMB, phase1MB, blockMB, workerMB};
}

// =============================================================================
// MemoryEstimator Implementation
// =============================================================================

MemoryEstimator::MemoryEstimator(MemoryBudget budget) noexcept : budget_(budget) {}

MemoryEstimate MemoryEstimator::estimate(std::size_t totalReads,
                                          std::size_t readsPerBlock,
                                          std::size_t numThreads) const noexcept {
    MemoryEstimate result;

    // Phase 1 memory: ~24 bytes/read
    result.phase1Bytes = estimatePhase1(totalReads);

    // Phase 2 memory: ~50 bytes/read × concurrent blocks
    result.phase2BytesPerBlock = estimatePhase2(readsPerBlock, 1);
    std::size_t phase2Total = estimatePhase2(readsPerBlock, numThreads);

    // Peak memory is max of Phase 1 or Phase 2 (they don't overlap significantly)
    // Plus some overhead for buffers
    result.peakBytes = static_cast<std::size_t>(
        std::max(result.phase1Bytes, phase2Total) * kMemorySafetyMargin);
    result.peakBytes += budget_.blockBufferBytes();

    // Calculate max reads for Phase 1
    result.maxReadsPhase1 = maxReadsForPhase1();

    // Store block size
    result.readsPerBlock = readsPerBlock;

    // Determine if chunking is required
    result.requiresChunking = totalReads > result.maxReadsPhase1;

    // Calculate recommended chunks
    if (result.requiresChunking) {
        result.recommendedChunks = (totalReads + result.maxReadsPhase1 - 1) / result.maxReadsPhase1;
        // Ensure at least 2 chunks if chunking is required
        result.recommendedChunks = std::max(result.recommendedChunks, std::size_t{2});
    } else {
        result.recommendedChunks = 1;
    }

    return result;
}

std::size_t MemoryEstimator::estimatePhase1(std::size_t totalReads) const noexcept {
    return static_cast<std::size_t>(totalReads * kMemoryPerReadPhase1 * kMemorySafetyMargin);
}

std::size_t MemoryEstimator::estimatePhase2(std::size_t readsPerBlock,
                                             std::size_t numThreads) const noexcept {
    // Each thread processes one block at a time
    std::size_t perBlock = static_cast<std::size_t>(
        readsPerBlock * kMemoryPerReadPhase2 * kMemorySafetyMargin);
    return perBlock * numThreads;
}

std::size_t MemoryEstimator::maxReadsForPhase1() const noexcept {
    std::size_t availableBytes = budget_.phase1ReserveBytes();
    // Account for safety margin
    std::size_t effectiveBytes = static_cast<std::size_t>(availableBytes / kMemorySafetyMargin);
    return effectiveBytes / kMemoryPerReadPhase1;
}

std::size_t MemoryEstimator::optimalBlockSize(std::size_t numThreads) const noexcept {
    std::size_t availableBytes = budget_.phase2AvailableBytes();
    // Account for safety margin and concurrent threads
    std::size_t perThread = static_cast<std::size_t>(
        availableBytes / (numThreads * kMemorySafetyMargin));
    std::size_t optimalReads = perThread / kMemoryPerReadPhase2;

    // Clamp to reasonable bounds
    optimalReads = std::max(optimalReads, std::size_t{10'000});
    optimalReads = std::min(optimalReads, std::size_t{500'000});

    return optimalReads;
}

// =============================================================================
// ChunkPlan Implementation
// =============================================================================

std::size_t ChunkPlan::findChunk(std::uint64_t readIndex) const noexcept {
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        if (readIndex >= chunks[i].startReadIndex && readIndex < chunks[i].endReadIndex) {
            return i;
        }
    }
    return SIZE_MAX;
}

VoidResult ChunkPlan::validate() const {
    if (chunks.empty()) {
        return makeVoidError(ErrorCode::kUsageError, "Chunk plan has no chunks");
    }

    if (chunks.size() != numChunks) {
        return makeVoidError(ErrorCode::kUsageError,
                             "Chunk count mismatch: expected " + std::to_string(numChunks) +
                             ", got " + std::to_string(chunks.size()));
    }

    // Verify continuity
    std::uint64_t expectedStart = 0;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        if (chunks[i].startReadIndex != expectedStart) {
            return makeVoidError(ErrorCode::kUsageError,
                                 "Chunk " + std::to_string(i) + " has gap: expected start " +
                                 std::to_string(expectedStart) + ", got " +
                                 std::to_string(chunks[i].startReadIndex));
        }
        if (chunks[i].endReadIndex <= chunks[i].startReadIndex) {
            return makeVoidError(ErrorCode::kUsageError,
                                 "Chunk " + std::to_string(i) + " has invalid range");
        }
        expectedStart = chunks[i].endReadIndex;
    }

    if (expectedStart != totalReads) {
        return makeVoidError(ErrorCode::kUsageError,
                             "Chunks don't cover all reads: expected " +
                             std::to_string(totalReads) + ", covered " +
                             std::to_string(expectedStart));
    }

    return makeVoidSuccess();
}

// =============================================================================
// ChunkPlanner Implementation
// =============================================================================

ChunkPlanner::ChunkPlanner(MemoryBudget budget) noexcept
    : budget_(budget), estimator_(budget) {}

ChunkPlan ChunkPlanner::plan(std::uint64_t totalReads,
                              std::size_t readsPerBlock,
                              std::size_t numThreads) const {
    ChunkPlan result;
    result.totalReads = totalReads;

    // Get memory estimate
    auto estimate = estimator_.estimate(static_cast<std::size_t>(totalReads),
                                         readsPerBlock, numThreads);

    if (!estimate.requiresChunking) {
        // Single chunk covers everything
        result.numChunks = 1;
        result.chunks.push_back(ChunkInfo{
            .chunkIndex = 0,
            .startReadIndex = 0,
            .endReadIndex = totalReads,
            .archiveIdOffset = 0,
            .blockIdOffset = 0,
            .estimatedMemory = estimate.peakBytes
        });
        return result;
    }

    // Calculate optimal chunk size
    std::size_t maxReadsPerChunk = estimator_.maxReadsForPhase1();
    maxReadsPerChunk = std::max(maxReadsPerChunk, kMinChunkReads);

    // Calculate number of chunks
    result.numChunks = (static_cast<std::size_t>(totalReads) + maxReadsPerChunk - 1) /
                       maxReadsPerChunk;

    // Distribute reads evenly across chunks
    std::size_t baseReadsPerChunk = static_cast<std::size_t>(totalReads) / result.numChunks;
    std::size_t remainder = static_cast<std::size_t>(totalReads) % result.numChunks;

    std::uint64_t currentStart = 0;
    std::uint64_t archiveIdOffset = 0;
    BlockId blockIdOffset = 0;

    for (std::size_t i = 0; i < result.numChunks; ++i) {
        // Add one extra read to first 'remainder' chunks for even distribution
        std::size_t chunkReads = baseReadsPerChunk + (i < remainder ? 1 : 0);
        std::uint64_t chunkEnd = currentStart + chunkReads;

        // Calculate estimated memory for this chunk
        auto chunkEstimate = estimator_.estimate(chunkReads, readsPerBlock, numThreads);

        // Calculate number of blocks in this chunk
        std::size_t blocksInChunk = (chunkReads + readsPerBlock - 1) / readsPerBlock;

        result.chunks.push_back(ChunkInfo{
            .chunkIndex = i,
            .startReadIndex = currentStart,
            .endReadIndex = chunkEnd,
            .archiveIdOffset = archiveIdOffset,
            .blockIdOffset = blockIdOffset,
            .estimatedMemory = chunkEstimate.peakBytes
        });

        currentStart = chunkEnd;
        archiveIdOffset += chunkReads;
        blockIdOffset += static_cast<BlockId>(blocksInChunk);
    }

    return result;
}

ChunkPlan ChunkPlanner::planWithReadLength(std::uint64_t totalReads,
                                            std::size_t avgReadLength,
                                            std::size_t readsPerBlock,
                                            std::size_t numThreads) const {
    // Adjust memory estimates based on read length
    // Longer reads require more memory per read
    double lengthFactor = 1.0;
    if (avgReadLength > 150) {
        // Scale memory estimate for longer reads
        lengthFactor = 1.0 + (static_cast<double>(avgReadLength) - 150.0) / 1000.0;
        lengthFactor = std::min(lengthFactor, 3.0);  // Cap at 3x
    }

    // Create adjusted budget
    MemoryBudget adjustedBudget = budget_;
    adjustedBudget.phase1ReserveMB = static_cast<std::size_t>(
        budget_.phase1ReserveMB / lengthFactor);

    // Use adjusted planner
    ChunkPlanner adjustedPlanner(adjustedBudget);
    return adjustedPlanner.plan(totalReads, readsPerBlock, numThreads);
}

std::size_t ChunkPlanner::optimalChunkSize(std::size_t numThreads) const noexcept {
    return estimator_.maxReadsForPhase1();
}

// =============================================================================
// MemoryMonitor Implementation
// =============================================================================

MemoryMonitor::MemoryMonitor(MemoryBudget budget) noexcept : budget_(budget) {}

MemoryUsage MemoryMonitor::currentUsage() const {
    MemoryUsage usage = getProcessMemoryUsage();

    // Track peak
    if (usage.rssBytes > peakRss_) {
        peakRss_ = usage.rssBytes;
    }
    usage.peakRssBytes = peakRss_;

    return usage;
}

bool MemoryMonitor::isOverBudget() const {
    return currentUsage().rssBytes > budget_.maxTotalBytes();
}

bool MemoryMonitor::exceedsThreshold(double percentage) const {
    if (percentage <= 0.0 || percentage > 100.0) {
        return false;
    }
    std::size_t threshold = static_cast<std::size_t>(
        budget_.maxTotalBytes() * percentage / 100.0);
    return currentUsage().rssBytes > threshold;
}

std::size_t MemoryMonitor::remainingBytes() const {
    auto usage = currentUsage();
    std::size_t limit = budget_.maxTotalBytes();
    return (usage.rssBytes < limit) ? (limit - usage.rssBytes) : 0;
}

std::size_t MemoryMonitor::remainingMB() const {
    return remainingBytes() / (1024 * 1024);
}

double MemoryMonitor::usagePercentage() const {
    auto usage = currentUsage();
    std::size_t limit = budget_.maxTotalBytes();
    if (limit == 0) {
        return 0.0;
    }
    return static_cast<double>(usage.rssBytes) * 100.0 / static_cast<double>(limit);
}

void MemoryMonitor::setAlertCallback(double threshold, MemoryAlertCallback callback) {
    alertThreshold_ = threshold;
    alertCallback_ = std::move(callback);
}

void MemoryMonitor::clearAlertCallback() noexcept {
    alertThreshold_.reset();
    alertCallback_ = nullptr;
}

void MemoryMonitor::checkAlert() {
    if (!alertThreshold_ || !alertCallback_) {
        return;
    }

    if (exceedsThreshold(*alertThreshold_)) {
        auto usage = currentUsage();
        std::size_t threshold = static_cast<std::size_t>(
            budget_.maxTotalBytes() * (*alertThreshold_) / 100.0);
        alertCallback_(usage, threshold);
    }
}

void MemoryMonitor::resetPeak() noexcept {
    peakRss_ = 0;
}

// =============================================================================
// System Memory Functions
// =============================================================================

std::size_t getSystemAvailableMemory() noexcept {
#ifdef __linux__
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo) {
        return 0;
    }

    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemAvailable:") == 0) {
            std::size_t kb = 0;
            if (std::sscanf(line.c_str(), "MemAvailable: %zu kB", &kb) == 1) {
                return kb * 1024;
            }
        }
    }

    // Fallback: try MemFree + Buffers + Cached
    meminfo.clear();
    meminfo.seekg(0);
    std::size_t memFree = 0, buffers = 0, cached = 0;
    while (std::getline(meminfo, line)) {
        if (line.find("MemFree:") == 0) {
            std::sscanf(line.c_str(), "MemFree: %zu kB", &memFree);
        } else if (line.find("Buffers:") == 0) {
            std::sscanf(line.c_str(), "Buffers: %zu kB", &buffers);
        } else if (line.find("Cached:") == 0 && line.find("SwapCached:") != 0) {
            std::sscanf(line.c_str(), "Cached: %zu kB", &cached);
        }
    }
    return (memFree + buffers + cached) * 1024;

#elif defined(__APPLE__)
    vm_size_t pageSize;
    mach_port_t machPort = mach_host_self();
    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t count = sizeof(vmStats) / sizeof(natural_t);

    if (host_page_size(machPort, &pageSize) != KERN_SUCCESS) {
        return 0;
    }

    if (host_statistics64(machPort, HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vmStats),
                          &count) != KERN_SUCCESS) {
        return 0;
    }

    return static_cast<std::size_t>(vmStats.free_count) * pageSize;

#elif defined(_WIN32)
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return static_cast<std::size_t>(memStatus.ullAvailPhys);
    }
    return 0;

#else
    return 0;
#endif
}

std::size_t getSystemTotalMemory() noexcept {
#ifdef __linux__
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<std::size_t>(info.totalram) * info.mem_unit;
    }
    return 0;

#elif defined(__APPLE__)
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    std::uint64_t memSize = 0;
    std::size_t len = sizeof(memSize);
    if (sysctl(mib, 2, &memSize, &len, nullptr, 0) == 0) {
        return static_cast<std::size_t>(memSize);
    }
    return 0;

#elif defined(_WIN32)
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return static_cast<std::size_t>(memStatus.ullTotalPhys);
    }
    return 0;

#else
    return 0;
#endif
}

MemoryUsage getProcessMemoryUsage() noexcept {
    MemoryUsage usage;

#ifdef __linux__
    std::ifstream statm("/proc/self/statm");
    if (statm) {
        std::size_t size, resident, shared, text, lib, data, dt;
        statm >> size >> resident >> shared >> text >> lib >> data >> dt;
        long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize > 0) {
            usage.virtualBytes = size * static_cast<std::size_t>(pageSize);
            usage.rssBytes = resident * static_cast<std::size_t>(pageSize);
        }
    }

    // Get peak RSS from /proc/self/status
    std::ifstream status("/proc/self/status");
    if (status) {
        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmHWM:") == 0) {
                std::size_t kb = 0;
                if (std::sscanf(line.c_str(), "VmHWM: %zu kB", &kb) == 1) {
                    usage.peakRssBytes = kb * 1024;
                }
                break;
            }
        }
    }

#elif defined(__APPLE__)
    struct rusage rusage;
    if (getrusage(RUSAGE_SELF, &rusage) == 0) {
        usage.rssBytes = static_cast<std::size_t>(rusage.ru_maxrss);
        usage.peakRssBytes = usage.rssBytes;
    }

    task_basic_info_data_t taskInfo;
    mach_msg_type_number_t infoCount = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&taskInfo),
                  &infoCount) == KERN_SUCCESS) {
        usage.virtualBytes = taskInfo.virtual_size;
        usage.rssBytes = taskInfo.resident_size;
    }

#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        usage.rssBytes = pmc.WorkingSetSize;
        usage.peakRssBytes = pmc.PeakWorkingSetSize;
        usage.virtualBytes = pmc.PrivateUsage;
    }
#endif

    return usage;
}

// =============================================================================
// Utility Functions
// =============================================================================

std::string formatMemorySize(std::size_t bytes) {
    constexpr std::size_t kKB = 1024;
    constexpr std::size_t kMB = kKB * 1024;
    constexpr std::size_t kGB = kMB * 1024;
    constexpr std::size_t kTB = kGB * 1024;

    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed;

    if (bytes >= kTB) {
        oss << static_cast<double>(bytes) / static_cast<double>(kTB) << " TB";
    } else if (bytes >= kGB) {
        oss << static_cast<double>(bytes) / static_cast<double>(kGB) << " GB";
    } else if (bytes >= kMB) {
        oss << static_cast<double>(bytes) / static_cast<double>(kMB) << " MB";
    } else if (bytes >= kKB) {
        oss << static_cast<double>(bytes) / static_cast<double>(kKB) << " KB";
    } else {
        oss << bytes << " B";
    }

    return oss.str();
}

std::optional<std::size_t> parseMemorySize(std::string_view str) {
    if (str.empty()) {
        return std::nullopt;
    }

    // Trim whitespace
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }

    if (str.empty()) {
        return std::nullopt;
    }

    // Find where the number ends
    std::size_t numEnd = 0;
    while (numEnd < str.size() &&
           (std::isdigit(static_cast<unsigned char>(str[numEnd])) || str[numEnd] == '.')) {
        ++numEnd;
    }

    if (numEnd == 0) {
        return std::nullopt;
    }

    // Parse the number
    double value = 0;
    auto numStr = str.substr(0, numEnd);
    auto [ptr, ec] = std::from_chars(numStr.data(), numStr.data() + numStr.size(), value);
    if (ec != std::errc{} || ptr != numStr.data() + numStr.size()) {
        // Try parsing as integer if double parsing failed
        std::size_t intValue = 0;
        auto [ptr2, ec2] = std::from_chars(numStr.data(), numStr.data() + numStr.size(), intValue);
        if (ec2 != std::errc{}) {
            return std::nullopt;
        }
        value = static_cast<double>(intValue);
    }

    // Get the suffix
    auto suffix = str.substr(numEnd);
    while (!suffix.empty() && std::isspace(static_cast<unsigned char>(suffix.front()))) {
        suffix.remove_prefix(1);
    }

    // Default to MB if no suffix
    std::size_t multiplier = 1;  // Result is in MB

    if (!suffix.empty()) {
        char unit = static_cast<char>(std::toupper(static_cast<unsigned char>(suffix[0])));
        switch (unit) {
            case 'T':
                multiplier = 1024 * 1024;  // TB to MB
                break;
            case 'G':
                multiplier = 1024;  // GB to MB
                break;
            case 'M':
                multiplier = 1;  // MB to MB
                break;
            case 'K':
                // KB to MB (may result in 0 for small values)
                return static_cast<std::size_t>(value / 1024.0);
            case 'B':
                // Bytes to MB
                return static_cast<std::size_t>(value / (1024.0 * 1024.0));
            default:
                // Unknown suffix, assume MB
                break;
        }
    }

    return static_cast<std::size_t>(value * static_cast<double>(multiplier));
}

std::size_t recommendedMemoryLimit(double maxPercentage) noexcept {
    std::size_t totalMemory = getSystemTotalMemory();
    if (totalMemory == 0) {
        // Fallback to default
        return kDefaultMaxTotalMB;
    }

    // Clamp percentage
    maxPercentage = std::clamp(maxPercentage, 0.1, 0.95);

    std::size_t recommended = static_cast<std::size_t>(
        static_cast<double>(totalMemory) * maxPercentage);

    // Convert to MB
    std::size_t recommendedMB = recommended / (1024 * 1024);

    // Clamp to reasonable bounds
    recommendedMB = std::max(recommendedMB, kMinMemoryLimitMB);
    recommendedMB = std::min(recommendedMB, std::size_t{64 * 1024});  // Cap at 64GB

    return recommendedMB;
}

}  // namespace fqc
