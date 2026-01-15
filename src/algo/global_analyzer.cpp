// =============================================================================
// fq-compressor - Global Analyzer Implementation
// =============================================================================
// Implements Phase 1 of the two-phase compression strategy.
//
// Requirements: 1.1.2, 4.3
// =============================================================================

#include "fqc/algo/global_analyzer.h"

#include <algorithm>
#include <atomic>
#include <bitset>
#include <cstring>
#include <mutex>
#include <numeric>
#include <thread>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>
#include <tbb/spin_mutex.h>

#include "fqc/common/logger.h"

namespace fqc::algo {

// =============================================================================
// Constants for DNA Encoding
// =============================================================================

namespace {

/// @brief DNA base to 2-bit encoding lookup table
/// A=0, C=1, G=2, T=3, N=0 (treated as A)
constexpr std::uint8_t kBaseToInt[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-15
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 16-31
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 32-47
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 48-63
    0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0,  // 64-79  (A=65, C=67, G=71)
    0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80-95  (T=84)
    0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0,  // 96-111 (a=97, c=99, g=103)
    0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 112-127 (t=116)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 128-143
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 144-159
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 160-175
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 176-191
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 192-207
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 208-223
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 224-239
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // 240-255
};

/// @brief Complement lookup table
constexpr char kComplement[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   'T', 0,   'G', 0,   0,   0,   'C', 0,   0,   0,   0,   0,   0,   'N', 0,
    0,   0,   0,   0,   'A', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   't', 0,   'g', 0,   0,   0,   'c', 0,   0,   0,   0,   0,   0,   'n', 0,
    0,   0,   0,   0,   'a', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

}  // namespace


// =============================================================================
// Minimizer Bucket Entry
// =============================================================================

/// @brief Entry in the minimizer bucket
struct BucketEntry {
    std::uint64_t readId;      ///< Original read ID
    std::uint64_t minimizer;   ///< Minimizer hash
    std::uint16_t position;    ///< Position in read
    bool isRC;                 ///< Is reverse complement
};

// =============================================================================
// GlobalAnalyzerImpl - Implementation Class
// =============================================================================

class GlobalAnalyzerImpl {
public:
    explicit GlobalAnalyzerImpl(GlobalAnalyzerConfig config)
        : config_(std::move(config)), cancelled_(false) {}

    Result<GlobalAnalysisResult> analyze(const IReadDataProvider& provider);

    const GlobalAnalyzerConfig& config() const noexcept { return config_; }

    VoidResult setConfig(GlobalAnalyzerConfig config) {
        auto result = config.validate();
        if (!result) {
            return result;
        }
        config_ = std::move(config);
        return makeVoidSuccess();
    }

    void cancel() noexcept { cancelled_.store(true, std::memory_order_release); }
    bool isCancelled() const noexcept { return cancelled_.load(std::memory_order_acquire); }
    void resetCancellation() noexcept { cancelled_.store(false, std::memory_order_release); }

private:
    GlobalAnalyzerConfig config_;
    std::atomic<bool> cancelled_;

    // Internal methods
    Result<std::vector<BucketEntry>> extractAllMinimizers(const IReadDataProvider& provider);
    Result<std::vector<ReadId>> performReordering(
        const IReadDataProvider& provider,
        std::vector<BucketEntry>& buckets);
    std::vector<BlockBoundary> computeBlockBoundaries(std::uint64_t totalReads);
    void reportProgress(double progress);
};

// =============================================================================
// GlobalAnalysisResult Implementation
// =============================================================================

BlockId GlobalAnalysisResult::findBlock(ReadId archiveId) const noexcept {
    if (blockBoundaries.empty()) {
        return kInvalidBlockId;
    }

    // Binary search for the block containing archiveId
    auto it = std::upper_bound(
        blockBoundaries.begin(), blockBoundaries.end(), archiveId,
        [](ReadId id, const BlockBoundary& boundary) {
            return id < boundary.archiveIdStart;
        });

    if (it == blockBoundaries.begin()) {
        // archiveId is in the first block
        if (archiveId < blockBoundaries.front().archiveIdEnd) {
            return blockBoundaries.front().blockId;
        }
        return kInvalidBlockId;
    }

    --it;
    if (archiveId >= it->archiveIdStart && archiveId < it->archiveIdEnd) {
        return it->blockId;
    }

    return kInvalidBlockId;
}

// =============================================================================
// GlobalAnalyzerConfig Implementation
// =============================================================================

VoidResult GlobalAnalyzerConfig::validate() const {
    if (readsPerBlock == 0) {
        return makeVoidError(ErrorCode::kUsageError, "readsPerBlock must be > 0");
    }

    if (minimizerK == 0 || minimizerK > 31) {
        return makeVoidError(ErrorCode::kUsageError, "minimizerK must be in range [1, 31]");
    }

    if (minimizerW == 0) {
        return makeVoidError(ErrorCode::kUsageError, "minimizerW must be > 0");
    }

    if (numDictionaries == 0) {
        return makeVoidError(ErrorCode::kUsageError, "numDictionaries must be > 0");
    }

    return makeVoidSuccess();
}

// =============================================================================
// InMemoryReadProvider Implementation
// =============================================================================

InMemoryReadProvider::InMemoryReadProvider(std::span<const ReadRecord> reads)
    : reads_(reads), useReadRecords_(true), maxLength_(0), uniformLength_(0),
      hasUniformLength_(true) {
    computeStats();
}

InMemoryReadProvider::InMemoryReadProvider(std::span<const std::string> sequences)
    : sequences_(sequences), useReadRecords_(false), maxLength_(0), uniformLength_(0),
      hasUniformLength_(true) {
    computeStats();
}

void InMemoryReadProvider::computeStats() {
    if (useReadRecords_) {
        if (reads_.empty()) {
            return;
        }
        uniformLength_ = reads_[0].sequence.length();
        for (const auto& read : reads_) {
            std::size_t len = read.sequence.length();
            maxLength_ = std::max(maxLength_, len);
            if (len != uniformLength_) {
                hasUniformLength_ = false;
            }
        }
    } else {
        if (sequences_.empty()) {
            return;
        }
        uniformLength_ = sequences_[0].length();
        for (const auto& seq : sequences_) {
            std::size_t len = seq.length();
            maxLength_ = std::max(maxLength_, len);
            if (len != uniformLength_) {
                hasUniformLength_ = false;
            }
        }
    }
}

std::uint64_t InMemoryReadProvider::totalReads() const {
    return useReadRecords_ ? reads_.size() : sequences_.size();
}

std::string_view InMemoryReadProvider::getSequence(std::uint64_t index) const {
    if (useReadRecords_) {
        if (index >= reads_.size()) return {};
        return reads_[index].sequence;
    } else {
        if (index >= sequences_.size()) return {};
        return sequences_[index];
    }
}

std::size_t InMemoryReadProvider::getLength(std::uint64_t index) const {
    if (useReadRecords_) {
        if (index >= reads_.size()) return 0;
        return reads_[index].sequence.length();
    } else {
        if (index >= sequences_.size()) return 0;
        return sequences_[index].length();
    }
}

bool InMemoryReadProvider::hasUniformLength() const { return hasUniformLength_; }
std::size_t InMemoryReadProvider::uniformLength() const { return uniformLength_; }
std::size_t InMemoryReadProvider::maxLength() const { return maxLength_; }


// =============================================================================
// Minimizer Extraction Functions
// =============================================================================

std::string reverseComplement(std::string_view sequence) {
    std::string result;
    result.reserve(sequence.length());
    for (auto it = sequence.rbegin(); it != sequence.rend(); ++it) {
        char c = kComplement[static_cast<unsigned char>(*it)];
        result.push_back(c ? c : 'N');
    }
    return result;
}

std::uint64_t computeKmerHash(std::string_view sequence) {
    // Use a simple rolling hash for k-mers
    // This is a basic implementation; could be optimized with NTHash or similar
    std::uint64_t hash = 0;
    std::uint64_t rcHash = 0;
    const std::size_t k = sequence.length();

    for (std::size_t i = 0; i < k; ++i) {
        std::uint8_t base = kBaseToInt[static_cast<unsigned char>(sequence[i])];
        hash = (hash << 2) | base;

        // Reverse complement hash
        std::uint8_t rcBase = 3 - kBaseToInt[static_cast<unsigned char>(sequence[k - 1 - i])];
        rcHash = (rcHash << 2) | rcBase;
    }

    // Return canonical (minimum of forward and reverse complement)
    return std::min(hash, rcHash);
}

std::vector<Minimizer> extractMinimizers(std::string_view sequence,
                                          std::size_t k,
                                          std::size_t w) {
    std::vector<Minimizer> minimizers;

    if (sequence.length() < k) {
        return minimizers;
    }

    const std::size_t numKmers = sequence.length() - k + 1;
    if (numKmers == 0) {
        return minimizers;
    }

    // Compute all k-mer hashes
    std::vector<std::uint64_t> hashes(numKmers);
    for (std::size_t i = 0; i < numKmers; ++i) {
        hashes[i] = computeKmerHash(sequence.substr(i, k));
    }

    // Find minimizers using sliding window
    std::size_t windowSize = std::min(w, numKmers);
    std::uint64_t prevMinHash = UINT64_MAX;
    std::size_t prevMinPos = SIZE_MAX;

    for (std::size_t windowStart = 0; windowStart + windowSize <= numKmers; ++windowStart) {
        // Find minimum in current window
        std::uint64_t minHash = UINT64_MAX;
        std::size_t minPos = 0;

        for (std::size_t i = 0; i < windowSize; ++i) {
            std::size_t pos = windowStart + i;
            if (hashes[pos] < minHash) {
                minHash = hashes[pos];
                minPos = pos;
            }
        }

        // Add minimizer if it's new (different position from previous)
        if (minPos != prevMinPos) {
            // Check if this is from reverse complement
            std::uint64_t fwdHash = 0;
            for (std::size_t i = 0; i < k; ++i) {
                std::uint8_t base = kBaseToInt[static_cast<unsigned char>(sequence[minPos + i])];
                fwdHash = (fwdHash << 2) | base;
            }
            bool isRC = (minHash != fwdHash);

            minimizers.emplace_back(minHash, static_cast<std::uint16_t>(minPos), isRC);
            prevMinHash = minHash;
            prevMinPos = minPos;
        }
    }

    return minimizers;
}

// =============================================================================
// Read Length Classification
// =============================================================================

ReadLengthClass classifyReadLength(std::size_t medianLength, std::size_t maxLength) {
    // Priority order (high to low):
    // 1. max >= 100KB → Long (Ultra-long strategy)
    // 2. max >= 10KB → Long
    // 3. max > 511 → Medium (Spring compatibility protection)
    // 4. median >= 1KB → Medium
    // 5. Otherwise → Short

    if (maxLength >= kUltraLongReadThreshold) {
        return ReadLengthClass::kLong;
    }
    if (maxLength >= kLongReadThreshold) {
        return ReadLengthClass::kLong;
    }
    if (maxLength > kSpringMaxReadLength) {
        return ReadLengthClass::kMedium;
    }
    if (medianLength >= kMediumReadThreshold) {
        return ReadLengthClass::kMedium;
    }
    return ReadLengthClass::kShort;
}

ReadLengthClass classifyReadLength(const IReadDataProvider& provider, std::size_t sampleSize) {
    const std::uint64_t totalReads = provider.totalReads();
    if (totalReads == 0) {
        return ReadLengthClass::kShort;
    }

    // Determine sample size
    std::size_t actualSampleSize = sampleSize;
    if (sampleSize == 0 || sampleSize > totalReads) {
        actualSampleSize = static_cast<std::size_t>(totalReads);
    }

    // Collect lengths
    std::vector<std::size_t> lengths;
    lengths.reserve(actualSampleSize);

    std::size_t maxLength = 0;
    std::size_t step = totalReads / actualSampleSize;
    if (step == 0) step = 1;

    for (std::uint64_t i = 0; i < totalReads && lengths.size() < actualSampleSize; i += step) {
        std::size_t len = provider.getLength(i);
        lengths.push_back(len);
        maxLength = std::max(maxLength, len);
    }

    // Compute median
    std::sort(lengths.begin(), lengths.end());
    std::size_t medianLength = lengths[lengths.size() / 2];

    return classifyReadLength(medianLength, maxLength);
}

std::size_t recommendedBlockSize(ReadLengthClass lengthClass) noexcept {
    switch (lengthClass) {
        case ReadLengthClass::kShort:
            return kDefaultBlockSizeShort;
        case ReadLengthClass::kMedium:
            return kDefaultBlockSizeMedium;
        case ReadLengthClass::kLong:
            return kDefaultBlockSizeLong;
    }
    return kDefaultBlockSizeShort;
}

bool shouldEnableReorder(ReadLengthClass lengthClass) noexcept {
    // Only enable reordering for short reads (Spring ABC compatible)
    return lengthClass == ReadLengthClass::kShort;
}


// =============================================================================
// GlobalAnalyzerImpl - Core Analysis Methods
// =============================================================================

void GlobalAnalyzerImpl::reportProgress(double progress) {
    if (config_.progressCallback) {
        config_.progressCallback(progress);
    }
}

Result<std::vector<BucketEntry>> GlobalAnalyzerImpl::extractAllMinimizers(
    const IReadDataProvider& provider) {

    const std::uint64_t totalReads = provider.totalReads();
    std::vector<BucketEntry> allBuckets;

    // Estimate bucket count (typically 1-3 minimizers per read)
    allBuckets.reserve(totalReads * 2);

    // Thread-safe bucket collection
    std::mutex bucketMutex;
    std::atomic<std::uint64_t> processedReads{0};

    // Process reads in parallel
    tbb::parallel_for(
        tbb::blocked_range<std::uint64_t>(0, totalReads),
        [&](const tbb::blocked_range<std::uint64_t>& range) {
            if (isCancelled()) return;

            std::vector<BucketEntry> localBuckets;
            localBuckets.reserve((range.end() - range.begin()) * 2);

            for (std::uint64_t i = range.begin(); i < range.end(); ++i) {
                if (isCancelled()) break;

                std::string_view seq = provider.getSequence(i);
                if (seq.empty()) continue;

                auto minimizers = extractMinimizers(seq, config_.minimizerK, config_.minimizerW);

                for (const auto& m : minimizers) {
                    localBuckets.push_back({i, m.hash, m.position, m.isReverseComplement});
                }
            }

            // Merge local buckets into global
            {
                std::lock_guard<std::mutex> lock(bucketMutex);
                allBuckets.insert(allBuckets.end(), localBuckets.begin(), localBuckets.end());
            }

            processedReads.fetch_add(range.end() - range.begin(), std::memory_order_relaxed);
            reportProgress(0.3 * static_cast<double>(processedReads.load()) /
                           static_cast<double>(totalReads));
        });

    if (isCancelled()) {
        return makeError<std::vector<BucketEntry>>(ErrorCode::kIOError, "Analysis cancelled");
    }

    // Sort buckets by minimizer hash for efficient grouping
    tbb::parallel_sort(allBuckets.begin(), allBuckets.end(),
                       [](const BucketEntry& a, const BucketEntry& b) {
                           return a.minimizer < b.minimizer;
                       });

    reportProgress(0.4);
    return allBuckets;
}

Result<std::vector<ReadId>> GlobalAnalyzerImpl::performReordering(
    const IReadDataProvider& provider,
    std::vector<BucketEntry>& buckets) {

    const std::uint64_t totalReads = provider.totalReads();

    // Initialize reorder map with identity mapping
    std::vector<ReadId> reorderMap(totalReads);
    std::iota(reorderMap.begin(), reorderMap.end(), 0);

    if (buckets.empty() || totalReads <= 1) {
        return reorderMap;
    }

    // Build bucket index: minimizer -> [start, end) in sorted buckets array
    struct BucketRange {
        std::size_t start;
        std::size_t end;
    };

    std::vector<BucketRange> bucketRanges;
    std::uint64_t currentMinimizer = buckets[0].minimizer;
    std::size_t rangeStart = 0;

    for (std::size_t i = 1; i <= buckets.size(); ++i) {
        if (i == buckets.size() || buckets[i].minimizer != currentMinimizer) {
            bucketRanges.push_back({rangeStart, i});
            if (i < buckets.size()) {
                currentMinimizer = buckets[i].minimizer;
                rangeStart = i;
            }
        }
    }

    reportProgress(0.5);

    // Greedy reordering using Approximate Hamiltonian Path
    // This is a simplified version of Spring's reordering algorithm
    // For each read, we try to find a similar read in the same bucket

    std::vector<bool> used(totalReads, false);
    std::vector<ReadId> orderedReads;
    orderedReads.reserve(totalReads);

    // Start with the first read
    orderedReads.push_back(0);
    used[0] = true;

    std::atomic<std::uint64_t> orderedCount{1};

    // Process remaining reads
    while (orderedCount.load() < totalReads) {
        if (isCancelled()) {
            return makeError<std::vector<ReadId>>(ErrorCode::kIOError, "Analysis cancelled");
        }

        ReadId lastRead = orderedReads.back();
        std::string_view lastSeq = provider.getSequence(lastRead);

        // Find minimizers of the last read
        auto lastMinimizers = extractMinimizers(lastSeq, config_.minimizerK, config_.minimizerW);

        ReadId bestMatch = kInvalidReadId;
        std::size_t bestScore = SIZE_MAX;

        // Search for similar reads in the same buckets
        for (const auto& m : lastMinimizers) {
            // Binary search for bucket with this minimizer
            auto it = std::lower_bound(
                bucketRanges.begin(), bucketRanges.end(), m.hash,
                [&buckets](const BucketRange& range, std::uint64_t hash) {
                    return buckets[range.start].minimizer < hash;
                });

            if (it == bucketRanges.end() || buckets[it->start].minimizer != m.hash) {
                continue;
            }

            // Search within this bucket (limited search)
            std::size_t searchCount = 0;
            for (std::size_t i = it->start;
                 i < it->end && searchCount < config_.maxSearchReorder; ++i) {

                ReadId candidateId = buckets[i].readId;
                if (used[candidateId]) continue;

                searchCount++;

                // Compute simple similarity score (could use Hamming distance)
                std::string_view candidateSeq = provider.getSequence(candidateId);
                if (candidateSeq.empty()) continue;

                // Simple length-based score for now
                // A full implementation would compute Hamming distance
                std::size_t lenDiff = (lastSeq.length() > candidateSeq.length())
                                          ? lastSeq.length() - candidateSeq.length()
                                          : candidateSeq.length() - lastSeq.length();

                if (lenDiff < bestScore) {
                    bestScore = lenDiff;
                    bestMatch = candidateId;
                }
            }
        }

        // If no match found in buckets, take the next unused read
        if (bestMatch == kInvalidReadId) {
            for (std::uint64_t i = 0; i < totalReads; ++i) {
                if (!used[i]) {
                    bestMatch = i;
                    break;
                }
            }
        }

        if (bestMatch != kInvalidReadId) {
            orderedReads.push_back(bestMatch);
            used[bestMatch] = true;
            orderedCount.fetch_add(1, std::memory_order_relaxed);
        }

        // Report progress
        if (orderedCount.load() % 10000 == 0) {
            reportProgress(0.5 + 0.4 * static_cast<double>(orderedCount.load()) /
                                     static_cast<double>(totalReads));
        }
    }

    // Build reorder map: original_id -> archive_id
    for (std::size_t archiveId = 0; archiveId < orderedReads.size(); ++archiveId) {
        reorderMap[orderedReads[archiveId]] = archiveId;
    }

    reportProgress(0.95);
    return reorderMap;
}

std::vector<BlockBoundary> GlobalAnalyzerImpl::computeBlockBoundaries(std::uint64_t totalReads) {
    std::vector<BlockBoundary> boundaries;

    if (totalReads == 0) {
        return boundaries;
    }

    std::size_t readsPerBlock = config_.readsPerBlock;
    std::size_t numBlocks = (totalReads + readsPerBlock - 1) / readsPerBlock;

    boundaries.reserve(numBlocks);

    for (std::size_t blockId = 0; blockId < numBlocks; ++blockId) {
        ReadId start = blockId * readsPerBlock;
        ReadId end = std::min(start + readsPerBlock, totalReads);

        boundaries.push_back({
            static_cast<BlockId>(blockId),
            start,
            end
        });
    }

    return boundaries;
}


Result<GlobalAnalysisResult> GlobalAnalyzerImpl::analyze(const IReadDataProvider& provider) {
    GlobalAnalysisResult result;
    result.totalReads = provider.totalReads();
    result.maxReadLength = provider.maxLength();

    if (result.totalReads == 0) {
        result.lengthClass = ReadLengthClass::kShort;
        result.reorderingPerformed = false;
        result.numBlocks = 0;
        result.memoryUsed = 0;
        return result;
    }

    // Classify read length
    if (config_.readLengthClass.has_value()) {
        result.lengthClass = config_.readLengthClass.value();
    } else {
        result.lengthClass = classifyReadLength(provider);
    }

    FQC_LOG_INFO("Read length class: {}, max length: {}",
                 readLengthClassToString(result.lengthClass),
                 result.maxReadLength);

    // Determine if reordering should be performed
    bool shouldReorder = config_.enableReorder && shouldEnableReorder(result.lengthClass);

    // Check memory constraints
    std::size_t estimatedMemory = config_.estimateMemory(result.totalReads);
    if (config_.memoryLimit > 0 && estimatedMemory > config_.memoryLimit) {
        FQC_LOG_WARNING("Estimated memory {} exceeds limit {}, disabling reordering",
                        estimatedMemory, config_.memoryLimit);
        shouldReorder = false;
    }

    result.memoryUsed = estimatedMemory;

    if (shouldReorder) {
        FQC_LOG_INFO("Performing global reordering on {} reads", result.totalReads);

        // Step 1: Extract minimizers
        auto bucketsResult = extractAllMinimizers(provider);
        if (!bucketsResult) {
            return makeError<GlobalAnalysisResult>(bucketsResult.error());
        }

        FQC_LOG_DEBUG("Extracted {} minimizer entries", bucketsResult->size());

        // Step 2: Perform reordering
        auto reorderResult = performReordering(provider, *bucketsResult);
        if (!reorderResult) {
            return makeError<GlobalAnalysisResult>(reorderResult.error());
        }

        result.forwardMap = std::move(*reorderResult);
        result.reorderingPerformed = true;

        // Build reverse map: archive_id -> original_id
        result.reverseMap.resize(result.totalReads);
        for (std::uint64_t originalId = 0; originalId < result.totalReads; ++originalId) {
            ReadId archiveId = result.forwardMap[originalId];
            result.reverseMap[archiveId] = originalId;
        }

        FQC_LOG_INFO("Reordering complete");
    } else {
        FQC_LOG_INFO("Skipping reordering (length class: {}, enabled: {})",
                     readLengthClassToString(result.lengthClass),
                     config_.enableReorder);
        result.reorderingPerformed = false;
    }

    // Step 3: Compute block boundaries
    // Adjust block size based on read length class
    std::size_t effectiveBlockSize = config_.readsPerBlock;
    if (!config_.readLengthClass.has_value()) {
        effectiveBlockSize = recommendedBlockSize(result.lengthClass);
    }

    // Temporarily update config for block computation
    auto savedBlockSize = config_.readsPerBlock;
    config_.readsPerBlock = effectiveBlockSize;

    result.blockBoundaries = computeBlockBoundaries(result.totalReads);
    result.numBlocks = result.blockBoundaries.size();

    config_.readsPerBlock = savedBlockSize;

    FQC_LOG_INFO("Created {} blocks with {} reads per block",
                 result.numBlocks, effectiveBlockSize);

    reportProgress(1.0);
    return result;
}

// =============================================================================
// GlobalAnalyzer - Public Interface Implementation
// =============================================================================

GlobalAnalyzer::GlobalAnalyzer(GlobalAnalyzerConfig config)
    : impl_(std::make_unique<GlobalAnalyzerImpl>(std::move(config))) {}

GlobalAnalyzer::~GlobalAnalyzer() = default;

GlobalAnalyzer::GlobalAnalyzer(GlobalAnalyzer&&) noexcept = default;
GlobalAnalyzer& GlobalAnalyzer::operator=(GlobalAnalyzer&&) noexcept = default;

Result<GlobalAnalysisResult> GlobalAnalyzer::analyze(const IReadDataProvider& provider) {
    return impl_->analyze(provider);
}

Result<GlobalAnalysisResult> GlobalAnalyzer::analyze(std::span<const ReadRecord> reads) {
    InMemoryReadProvider provider(reads);
    return impl_->analyze(provider);
}

Result<GlobalAnalysisResult> GlobalAnalyzer::analyze(std::span<const std::string> sequences) {
    InMemoryReadProvider provider(sequences);
    return impl_->analyze(provider);
}

const GlobalAnalyzerConfig& GlobalAnalyzer::config() const noexcept {
    return impl_->config();
}

VoidResult GlobalAnalyzer::setConfig(GlobalAnalyzerConfig config) {
    return impl_->setConfig(std::move(config));
}

void GlobalAnalyzer::cancel() noexcept {
    impl_->cancel();
}

bool GlobalAnalyzer::isCancelled() const noexcept {
    return impl_->isCancelled();
}

void GlobalAnalyzer::resetCancellation() noexcept {
    impl_->resetCancellation();
}

}  // namespace fqc::algo
