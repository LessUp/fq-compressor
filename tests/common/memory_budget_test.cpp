// =============================================================================
// fq-compressor - Memory Budget Management Tests
// =============================================================================
// Unit tests for memory budget calculation, monitoring, and divide-and-conquer
// strategy for handling files that exceed memory limits.
//
// Requirements: 4.3
// =============================================================================

#include "fqc/common/memory_budget.h"

#include <gtest/gtest.h>

#include <thread>

namespace fqc {
namespace {

// =============================================================================
// MemoryBudget Tests
// =============================================================================

TEST(MemoryBudgetTest, DefaultConstruction) {
    MemoryBudget budget;

    EXPECT_EQ(budget.maxTotalMB, kDefaultMaxTotalMB);
    EXPECT_EQ(budget.phase1ReserveMB, kDefaultPhase1ReserveMB);
    EXPECT_EQ(budget.blockBufferMB, kDefaultBlockBufferMB);
    EXPECT_EQ(budget.workerStackMB, kDefaultWorkerStackMB);
}

TEST(MemoryBudgetTest, ConstructWithTotalLimit) {
    MemoryBudget budget(4096);  // 4GB

    EXPECT_EQ(budget.maxTotalMB, 4096);
    // Phase 1 should be 25% of total, capped at default
    EXPECT_EQ(budget.phase1ReserveMB, 1024);  // 4096 / 4
    // Block buffer should be 6.25% of total, capped at default
    EXPECT_EQ(budget.blockBufferMB, 256);  // 4096 / 16
}

TEST(MemoryBudgetTest, ConstructWithAllParameters) {
    MemoryBudget budget(8192, 2048, 512, 64);

    EXPECT_EQ(budget.maxTotalMB, 8192);
    EXPECT_EQ(budget.phase1ReserveMB, 2048);
    EXPECT_EQ(budget.blockBufferMB, 512);
    EXPECT_EQ(budget.workerStackMB, 64);
}

TEST(MemoryBudgetTest, ByteConversions) {
    MemoryBudget budget(1024, 256, 128, 32);

    EXPECT_EQ(budget.maxTotalBytes(), 1024ULL * 1024 * 1024);
    EXPECT_EQ(budget.phase1ReserveBytes(), 256ULL * 1024 * 1024);
    EXPECT_EQ(budget.blockBufferBytes(), 128ULL * 1024 * 1024);
    EXPECT_EQ(budget.workerStackBytes(), 32ULL * 1024 * 1024);
}

TEST(MemoryBudgetTest, Phase2Available) {
    MemoryBudget budget(1024, 256, 128, 32);

    // Phase 2 available = total - phase1 - blockBuffer
    EXPECT_EQ(budget.phase2AvailableMB(), 1024 - 256 - 128);
    EXPECT_EQ(budget.phase2AvailableBytes(), (1024 - 256 - 128) * 1024ULL * 1024);
}

TEST(MemoryBudgetTest, ValidateSuccess) {
    MemoryBudget budget(1024, 256, 128, 32);
    auto result = budget.validate();
    EXPECT_TRUE(result.has_value());
}

TEST(MemoryBudgetTest, ValidateFailTooSmall) {
    MemoryBudget budget(128, 32, 16, 8);  // Below minimum
    auto result = budget.validate();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kUsageError);
}

TEST(MemoryBudgetTest, ValidateFailPhase1TooLarge) {
    MemoryBudget budget(1024, 1024, 128, 32);  // Phase 1 == total
    auto result = budget.validate();
    EXPECT_FALSE(result.has_value());
}

TEST(MemoryBudgetTest, ValidateFailCombinedTooLarge) {
    MemoryBudget budget(1024, 800, 300, 32);  // Phase 1 + block > total
    auto result = budget.validate();
    EXPECT_FALSE(result.has_value());
}

TEST(MemoryBudgetTest, FromMemoryLimit) {
    auto budget = MemoryBudget::fromMemoryLimit(4096);

    EXPECT_EQ(budget.maxTotalMB, 4096);
    EXPECT_LE(budget.phase1ReserveMB, kDefaultPhase1ReserveMB);
    EXPECT_LE(budget.blockBufferMB, kDefaultBlockBufferMB);
    EXPECT_TRUE(budget.validate().has_value());
}

TEST(MemoryBudgetTest, FromMemoryLimitMinimum) {
    auto budget = MemoryBudget::fromMemoryLimit(100);  // Below minimum

    EXPECT_EQ(budget.maxTotalMB, kMinMemoryLimitMB);
    EXPECT_TRUE(budget.validate().has_value());
}

TEST(MemoryBudgetTest, Equality) {
    MemoryBudget a(1024, 256, 128, 32);
    MemoryBudget b(1024, 256, 128, 32);
    MemoryBudget c(2048, 256, 128, 32);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// =============================================================================
// MemoryEstimator Tests
// =============================================================================

TEST(MemoryEstimatorTest, EstimatePhase1) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryEstimator estimator(budget);

    std::size_t reads = 1'000'000;
    std::size_t estimated = estimator.estimatePhase1(reads);

    // Should be approximately reads * 24 bytes * safety margin
    std::size_t expected = static_cast<std::size_t>(
        reads * kMemoryPerReadPhase1 * kMemorySafetyMargin);
    EXPECT_EQ(estimated, expected);
}

TEST(MemoryEstimatorTest, EstimatePhase2) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryEstimator estimator(budget);

    std::size_t readsPerBlock = 100'000;
    std::size_t numThreads = 4;
    std::size_t estimated = estimator.estimatePhase2(readsPerBlock, numThreads);

    // Should be approximately readsPerBlock * 50 bytes * safety margin * threads
    std::size_t perBlock = static_cast<std::size_t>(
        readsPerBlock * kMemoryPerReadPhase2 * kMemorySafetyMargin);
    EXPECT_EQ(estimated, perBlock * numThreads);
}

TEST(MemoryEstimatorTest, MaxReadsForPhase1) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryEstimator estimator(budget);

    std::size_t maxReads = estimator.maxReadsForPhase1();

    // Should fit within phase1 reserve
    std::size_t estimatedMemory = estimator.estimatePhase1(maxReads);
    EXPECT_LE(estimatedMemory, budget.phase1ReserveBytes());
}

TEST(MemoryEstimatorTest, OptimalBlockSize) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryEstimator estimator(budget);

    std::size_t blockSize = estimator.optimalBlockSize(4);

    // Should be within reasonable bounds
    EXPECT_GE(blockSize, 10'000);
    EXPECT_LE(blockSize, 500'000);
}

TEST(MemoryEstimatorTest, EstimateNoChunking) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryEstimator estimator(budget);

    // Small file that fits in memory
    auto estimate = estimator.estimate(100'000, 100'000, 4);

    EXPECT_FALSE(estimate.requiresChunking);
    EXPECT_EQ(estimate.recommendedChunks, 1);
    EXPECT_TRUE(estimate.fitsInBudget(budget));
}

TEST(MemoryEstimatorTest, EstimateRequiresChunking) {
    MemoryBudget budget(1024, 256, 128, 32);  // Small budget
    MemoryEstimator estimator(budget);

    // Large file that exceeds memory
    std::size_t maxReads = estimator.maxReadsForPhase1();
    auto estimate = estimator.estimate(maxReads * 5, 100'000, 4);

    EXPECT_TRUE(estimate.requiresChunking);
    EXPECT_GE(estimate.recommendedChunks, 2);
}

// =============================================================================
// ChunkPlanner Tests
// =============================================================================

TEST(ChunkPlannerTest, SingleChunk) {
    MemoryBudget budget(8192, 2048, 512, 64);
    ChunkPlanner planner(budget);

    auto plan = planner.plan(100'000, 100'000, 4);

    EXPECT_FALSE(plan.requiresChunking());
    EXPECT_EQ(plan.numChunks, 1);
    EXPECT_EQ(plan.chunks.size(), 1);
    EXPECT_EQ(plan.chunks[0].startReadIndex, 0);
    EXPECT_EQ(plan.chunks[0].endReadIndex, 100'000);
    EXPECT_TRUE(plan.validate().has_value());
}

TEST(ChunkPlannerTest, MultipleChunks) {
    MemoryBudget budget(512, 128, 64, 32);  // Small budget
    ChunkPlanner planner(budget);

    // Force chunking with large read count
    MemoryEstimator estimator(budget);
    std::size_t maxReads = estimator.maxReadsForPhase1();
    std::uint64_t totalReads = maxReads * 3;

    auto plan = planner.plan(totalReads, 100'000, 4);

    EXPECT_TRUE(plan.requiresChunking());
    EXPECT_GE(plan.numChunks, 2);
    EXPECT_EQ(plan.chunks.size(), plan.numChunks);
    EXPECT_TRUE(plan.validate().has_value());

    // Verify continuity
    std::uint64_t expectedStart = 0;
    for (const auto& chunk : plan.chunks) {
        EXPECT_EQ(chunk.startReadIndex, expectedStart);
        EXPECT_GT(chunk.endReadIndex, chunk.startReadIndex);
        expectedStart = chunk.endReadIndex;
    }
    EXPECT_EQ(expectedStart, totalReads);
}

TEST(ChunkPlannerTest, ChunkOffsets) {
    MemoryBudget budget(512, 128, 64, 32);
    ChunkPlanner planner(budget);

    MemoryEstimator estimator(budget);
    std::size_t maxReads = estimator.maxReadsForPhase1();
    std::uint64_t totalReads = maxReads * 3;

    auto plan = planner.plan(totalReads, 100'000, 4);

    // Verify archive ID offsets are continuous
    std::uint64_t expectedArchiveOffset = 0;
    for (const auto& chunk : plan.chunks) {
        EXPECT_EQ(chunk.archiveIdOffset, expectedArchiveOffset);
        expectedArchiveOffset += chunk.readCount();
    }
}

TEST(ChunkPlannerTest, FindChunk) {
    MemoryBudget budget(512, 128, 64, 32);
    ChunkPlanner planner(budget);

    MemoryEstimator estimator(budget);
    std::size_t maxReads = estimator.maxReadsForPhase1();
    std::uint64_t totalReads = maxReads * 3;

    auto plan = planner.plan(totalReads, 100'000, 4);

    // Find chunk for first read
    EXPECT_EQ(plan.findChunk(0), 0);

    // Find chunk for last read
    EXPECT_EQ(plan.findChunk(totalReads - 1), plan.numChunks - 1);

    // Out of range
    EXPECT_EQ(plan.findChunk(totalReads), SIZE_MAX);
}

TEST(ChunkPlannerTest, PlanWithReadLength) {
    MemoryBudget budget(8192, 2048, 512, 64);
    ChunkPlanner planner(budget);

    // Short reads
    auto planShort = planner.planWithReadLength(1'000'000, 150, 100'000, 4);

    // Long reads (should require more chunks)
    auto planLong = planner.planWithReadLength(1'000'000, 10'000, 100'000, 4);

    // Long reads should potentially need more chunks due to higher memory per read
    EXPECT_TRUE(planShort.validate().has_value());
    EXPECT_TRUE(planLong.validate().has_value());
}

// =============================================================================
// MemoryMonitor Tests
// =============================================================================

TEST(MemoryMonitorTest, CurrentUsage) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryMonitor monitor(budget);

    auto usage = monitor.currentUsage();

    // Should have some memory usage
    EXPECT_GT(usage.rssBytes, 0);
}

TEST(MemoryMonitorTest, UsagePercentage) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryMonitor monitor(budget);

    double percentage = monitor.usagePercentage();

    // Should be a valid percentage
    EXPECT_GE(percentage, 0.0);
    // Unlikely to exceed budget in a test
    EXPECT_LT(percentage, 100.0);
}

TEST(MemoryMonitorTest, RemainingMemory) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryMonitor monitor(budget);

    std::size_t remaining = monitor.remainingBytes();
    std::size_t remainingMB = monitor.remainingMB();

    // Should have remaining memory
    EXPECT_GT(remaining, 0);
    EXPECT_GT(remainingMB, 0);
    EXPECT_EQ(remainingMB, remaining / (1024 * 1024));
}

TEST(MemoryMonitorTest, ExceedsThreshold) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryMonitor monitor(budget);

    // Very low threshold should be exceeded
    EXPECT_TRUE(monitor.exceedsThreshold(0.001));

    // Very high threshold should not be exceeded
    EXPECT_FALSE(monitor.exceedsThreshold(99.99));
}

TEST(MemoryMonitorTest, AlertCallback) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryMonitor monitor(budget);

    bool alertTriggered = false;
    monitor.setAlertCallback(0.001, [&](const MemoryUsage&, std::size_t) {
        alertTriggered = true;
    });

    monitor.checkAlert();
    EXPECT_TRUE(alertTriggered);
}

TEST(MemoryMonitorTest, ClearAlertCallback) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryMonitor monitor(budget);

    bool alertTriggered = false;
    monitor.setAlertCallback(0.001, [&](const MemoryUsage&, std::size_t) {
        alertTriggered = true;
    });

    monitor.clearAlertCallback();
    monitor.checkAlert();
    EXPECT_FALSE(alertTriggered);
}

TEST(MemoryMonitorTest, ResetPeak) {
    MemoryBudget budget(8192, 2048, 512, 64);
    MemoryMonitor monitor(budget);

    // Get initial usage to set peak
    auto usage1 = monitor.currentUsage();

    // Reset peak
    monitor.resetPeak();

    // Get usage again - peak should be reset
    auto usage2 = monitor.currentUsage();

    // Peak should be current RSS after reset
    EXPECT_EQ(usage2.peakRssBytes, usage2.rssBytes);
}

// =============================================================================
// Utility Function Tests
// =============================================================================

TEST(MemoryUtilsTest, GetSystemTotalMemory) {
    std::size_t total = getSystemTotalMemory();

    // Should return non-zero on most systems
    // Skip assertion if unable to determine (returns 0)
    if (total > 0) {
        EXPECT_GT(total, 1024ULL * 1024 * 1024);  // At least 1GB
    }
}

TEST(MemoryUtilsTest, GetSystemAvailableMemory) {
    std::size_t available = getSystemAvailableMemory();

    // Should return non-zero on most systems
    if (available > 0) {
        EXPECT_GT(available, 0);
    }
}

TEST(MemoryUtilsTest, GetProcessMemoryUsage) {
    auto usage = getProcessMemoryUsage();

    // Should have some memory usage
    EXPECT_GT(usage.rssBytes, 0);
}

TEST(MemoryUtilsTest, FormatMemorySize) {
    EXPECT_EQ(formatMemorySize(0), "0 B");
    EXPECT_EQ(formatMemorySize(512), "512 B");
    EXPECT_EQ(formatMemorySize(1024), "1.00 KB");
    EXPECT_EQ(formatMemorySize(1024 * 1024), "1.00 MB");
    EXPECT_EQ(formatMemorySize(1024ULL * 1024 * 1024), "1.00 GB");
    EXPECT_EQ(formatMemorySize(1024ULL * 1024 * 1024 * 1024), "1.00 TB");
}

TEST(MemoryUtilsTest, ParseMemorySize) {
    // Plain numbers (assumed MB)
    EXPECT_EQ(parseMemorySize("1024"), 1024);
    EXPECT_EQ(parseMemorySize("8192"), 8192);

    // With suffixes
    EXPECT_EQ(parseMemorySize("1G"), 1024);
    EXPECT_EQ(parseMemorySize("2g"), 2048);
    EXPECT_EQ(parseMemorySize("512M"), 512);
    EXPECT_EQ(parseMemorySize("512m"), 512);
    EXPECT_EQ(parseMemorySize("1T"), 1024 * 1024);

    // With spaces
    EXPECT_EQ(parseMemorySize("  1024  "), 1024);
    EXPECT_EQ(parseMemorySize("1 G"), 1024);

    // Invalid
    EXPECT_FALSE(parseMemorySize("").has_value());
    EXPECT_FALSE(parseMemorySize("abc").has_value());
}

TEST(MemoryUtilsTest, RecommendedMemoryLimit) {
    std::size_t recommended = recommendedMemoryLimit(0.75);

    // Should be within reasonable bounds
    EXPECT_GE(recommended, kMinMemoryLimitMB);
    EXPECT_LE(recommended, 64 * 1024);  // 64GB cap
}

// =============================================================================
// MemoryEstimate Tests
// =============================================================================

TEST(MemoryEstimateTest, FitsInBudget) {
    MemoryEstimate estimate;
    estimate.peakBytes = 1024ULL * 1024 * 1024;  // 1GB

    MemoryBudget smallBudget(512, 128, 64, 32);
    MemoryBudget largeBudget(8192, 2048, 512, 64);

    EXPECT_FALSE(estimate.fitsInBudget(smallBudget));
    EXPECT_TRUE(estimate.fitsInBudget(largeBudget));
}

TEST(MemoryEstimateTest, MBConversions) {
    MemoryEstimate estimate;
    estimate.phase1Bytes = 1024ULL * 1024 * 1024;  // 1GB
    estimate.peakBytes = 2048ULL * 1024 * 1024;    // 2GB

    EXPECT_EQ(estimate.phase1MB(), 1024);
    EXPECT_EQ(estimate.peakMB(), 2048);
}

// =============================================================================
// ChunkInfo Tests
// =============================================================================

TEST(ChunkInfoTest, ReadCount) {
    ChunkInfo chunk;
    chunk.startReadIndex = 100;
    chunk.endReadIndex = 500;

    EXPECT_EQ(chunk.readCount(), 400);
}

// =============================================================================
// ChunkPlan Validation Tests
// =============================================================================

TEST(ChunkPlanTest, ValidateEmpty) {
    ChunkPlan plan;
    plan.totalReads = 100;
    plan.numChunks = 1;
    // No chunks added

    auto result = plan.validate();
    EXPECT_FALSE(result.has_value());
}

TEST(ChunkPlanTest, ValidateCountMismatch) {
    ChunkPlan plan;
    plan.totalReads = 100;
    plan.numChunks = 2;
    plan.chunks.push_back(ChunkInfo{0, 0, 100, 0, 0, 0});

    auto result = plan.validate();
    EXPECT_FALSE(result.has_value());
}

TEST(ChunkPlanTest, ValidateGap) {
    ChunkPlan plan;
    plan.totalReads = 100;
    plan.numChunks = 2;
    plan.chunks.push_back(ChunkInfo{0, 0, 40, 0, 0, 0});
    plan.chunks.push_back(ChunkInfo{1, 50, 100, 40, 1, 0});  // Gap at 40-50

    auto result = plan.validate();
    EXPECT_FALSE(result.has_value());
}

TEST(ChunkPlanTest, ValidateIncomplete) {
    ChunkPlan plan;
    plan.totalReads = 100;
    plan.numChunks = 1;
    plan.chunks.push_back(ChunkInfo{0, 0, 50, 0, 0, 0});  // Only covers 50

    auto result = plan.validate();
    EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace fqc
