// =============================================================================
// fq-compressor - Pipeline Property Tests
// =============================================================================
// Property-based tests for the TBB pipeline module.
//
// **Property 6: Complete compression round-trip consistency**
// For any valid FASTQ file, compress then decompress should produce
// equivalent output.
//
// **Validates: Requirements 1.1, 2.1, 2.2, 4.1**
// =============================================================================

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "fqc/common/types.h"
#include "fqc/io/fastq_parser.h"
#include "fqc/pipeline/pipeline.h"
#include "fqc/pipeline/pipeline_node.h"

namespace fqc::pipeline::test {

// =============================================================================
// Test Fixtures
// =============================================================================

class PipelinePropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

// =============================================================================
// Generators
// =============================================================================

namespace gen {

/// @brief Generate a valid DNA sequence
rc::Gen<std::string> dnaSequence(std::size_t minLen = 50, std::size_t maxLen = 300) {
    return rc::gen::mapcat(
        rc::gen::inRange<std::size_t>(minLen, maxLen + 1),
        [](std::size_t len) {
            return rc::gen::container<std::string>(
                len,
                rc::gen::elementOf('A', 'C', 'G', 'T', 'N'));
        });
}

/// @brief Generate a valid quality string
rc::Gen<std::string> qualityString(std::size_t len) {
    return rc::gen::container<std::string>(
        len,
        rc::gen::inRange<char>('!', 'J' + 1));  // Phred 0-41
}

/// @brief Generate a valid read ID
rc::Gen<std::string> readId() {
    return rc::gen::map(
        rc::gen::tuple(
            rc::gen::inRange<int>(1, 100),    // tile
            rc::gen::inRange<int>(1, 10000),  // x
            rc::gen::inRange<int>(1, 10000)   // y
        ),
        [](const std::tuple<int, int, int>& t) {
            return "SIM:1:FCX:1:" + std::to_string(std::get<0>(t)) + ":" +
                   std::to_string(std::get<1>(t)) + ":" +
                   std::to_string(std::get<2>(t));
        });
}


/// @brief Generate a valid ReadRecord
rc::Gen<ReadRecord> readRecord() {
    return rc::gen::mapcat(
        dnaSequence(),
        [](std::string seq) {
            return rc::gen::map(
                rc::gen::tuple(readId(), qualityString(seq.length())),
                [seq = std::move(seq)](std::tuple<std::string, std::string> t) {
                    return ReadRecord{
                        std::move(std::get<0>(t)),
                        seq,
                        std::move(std::get<1>(t))
                    };
                });
        });
}

/// @brief Generate a vector of ReadRecords
rc::Gen<std::vector<ReadRecord>> readRecords(std::size_t minCount = 1, std::size_t maxCount = 100) {
    return rc::gen::mapcat(
        rc::gen::inRange<std::size_t>(minCount, maxCount + 1),
        [](std::size_t count) {
            return rc::gen::container<std::vector<ReadRecord>>(count, readRecord());
        });
}

/// @brief Generate a ReadChunk
rc::Gen<ReadChunk> readChunk(std::uint32_t chunkId = 0) {
    return rc::gen::map(
        readRecords(10, 100),
        [chunkId](std::vector<ReadRecord> reads) {
            ReadChunk chunk;
            chunk.reads = std::move(reads);
            chunk.chunkId = chunkId;
            chunk.startReadId = 1;
            chunk.isLast = true;
            return chunk;
        });
}

}  // namespace gen

// =============================================================================
// Unit Tests
// =============================================================================

TEST_F(PipelinePropertyTest, ConfigValidation) {
    CompressionPipelineConfig config;

    // Valid config should pass
    auto result = config.validate();
    EXPECT_TRUE(result.has_value());

    // Invalid block size
    config.blockSize = 10;  // Too small
    result = config.validate();
    EXPECT_FALSE(result.has_value());

    // Reset and test compression level
    config.blockSize = kDefaultBlockSizeShort;
    config.compressionLevel = 0;  // Invalid
    result = config.validate();
    EXPECT_FALSE(result.has_value());
}

TEST_F(PipelinePropertyTest, DecompressionConfigValidation) {
    DecompressionPipelineConfig config;

    // Valid config should pass
    auto result = config.validate();
    EXPECT_TRUE(result.has_value());

    // Invalid range
    config.rangeStart = 100;
    config.rangeEnd = 50;
    result = config.validate();
    EXPECT_FALSE(result.has_value());
}

TEST_F(PipelinePropertyTest, RecommendedThreadCount) {
    auto threads = recommendedThreadCount();
    EXPECT_GT(threads, 0u);
    EXPECT_LE(threads, 32u);
}

TEST_F(PipelinePropertyTest, RecommendedBlockSize) {
    EXPECT_EQ(recommendedBlockSize(ReadLengthClass::kShort), kDefaultBlockSizeShort);
    EXPECT_EQ(recommendedBlockSize(ReadLengthClass::kMedium), kDefaultBlockSizeMedium);
    EXPECT_EQ(recommendedBlockSize(ReadLengthClass::kLong), kDefaultBlockSizeLong);
}

TEST_F(PipelinePropertyTest, MemoryEstimation) {
    CompressionPipelineConfig config;
    config.blockSize = 100000;
    config.maxInFlightBlocks = 8;
    config.streamingMode = true;  // No phase 1 memory

    auto estimate = estimateMemoryUsage(config, 1000000);
    EXPECT_GT(estimate, 0u);

    // With reordering enabled, should use more memory
    config.streamingMode = false;
    config.enableReorder = true;
    auto estimateWithReorder = estimateMemoryUsage(config, 1000000);
    EXPECT_GT(estimateWithReorder, estimate);
}


// =============================================================================
// Pipeline Node Tests
// =============================================================================

TEST_F(PipelinePropertyTest, ReaderNodeConfigValidation) {
    ReaderNodeConfig config;

    // Valid config
    auto result = config.validate();
    EXPECT_TRUE(result.has_value());

    // Invalid block size
    config.blockSize = 10;
    result = config.validate();
    EXPECT_FALSE(result.has_value());

    // Invalid buffer size
    config.blockSize = kDefaultBlockSizeShort;
    config.bufferSize = 0;
    result = config.validate();
    EXPECT_FALSE(result.has_value());
}

TEST_F(PipelinePropertyTest, CompressorNodeConfigValidation) {
    CompressorNodeConfig config;

    // Valid config
    auto result = config.validate();
    EXPECT_TRUE(result.has_value());

    // Invalid compression level
    config.compressionLevel = 0;
    result = config.validate();
    EXPECT_FALSE(result.has_value());
}

TEST_F(PipelinePropertyTest, BackpressureController) {
    BackpressureController controller(4);

    EXPECT_EQ(controller.maxInFlight(), 4u);
    EXPECT_EQ(controller.inFlight(), 0u);

    // Acquire slots
    EXPECT_TRUE(controller.tryAcquire());
    EXPECT_EQ(controller.inFlight(), 1u);

    EXPECT_TRUE(controller.tryAcquire());
    EXPECT_TRUE(controller.tryAcquire());
    EXPECT_TRUE(controller.tryAcquire());
    EXPECT_EQ(controller.inFlight(), 4u);

    // Should fail at limit
    EXPECT_FALSE(controller.tryAcquire());

    // Release and try again
    controller.release();
    EXPECT_EQ(controller.inFlight(), 3u);
    EXPECT_TRUE(controller.tryAcquire());

    // Reset
    controller.reset();
    EXPECT_EQ(controller.inFlight(), 0u);
}

TEST_F(PipelinePropertyTest, NodeStateToString) {
    EXPECT_EQ(nodeStateToString(NodeState::kIdle), "idle");
    EXPECT_EQ(nodeStateToString(NodeState::kRunning), "running");
    EXPECT_EQ(nodeStateToString(NodeState::kFinished), "finished");
    EXPECT_EQ(nodeStateToString(NodeState::kError), "error");
    EXPECT_EQ(nodeStateToString(NodeState::kCancelled), "cancelled");
}

// =============================================================================
// Progress Info Tests
// =============================================================================

TEST_F(PipelinePropertyTest, ProgressInfoRatio) {
    ProgressInfo info;

    // Unknown total
    info.readsProcessed = 100;
    info.totalReads = 0;
    EXPECT_DOUBLE_EQ(info.ratio(), 0.0);

    // Known total
    info.totalReads = 1000;
    EXPECT_DOUBLE_EQ(info.ratio(), 0.1);

    // Bytes fallback
    info.totalReads = 0;
    info.bytesProcessed = 500;
    info.totalBytes = 1000;
    EXPECT_DOUBLE_EQ(info.ratio(), 0.5);
}

TEST_F(PipelinePropertyTest, ProgressInfoEstimatedRemaining) {
    ProgressInfo info;
    info.readsProcessed = 500;
    info.totalReads = 1000;
    info.elapsedMs = 5000;

    // 50% done in 5s, should estimate 5s remaining
    auto remaining = info.estimatedRemainingMs();
    EXPECT_EQ(remaining, 5000u);
}

// =============================================================================
// Pipeline Stats Tests
// =============================================================================

TEST_F(PipelinePropertyTest, PipelineStatsCompressionRatio) {
    PipelineStats stats;
    stats.inputBytes = 1000;
    stats.outputBytes = 250;

    EXPECT_DOUBLE_EQ(stats.compressionRatio(), 0.25);

    // Zero input
    stats.inputBytes = 0;
    EXPECT_DOUBLE_EQ(stats.compressionRatio(), 1.0);
}

TEST_F(PipelinePropertyTest, PipelineStatsThroughput) {
    PipelineStats stats;
    stats.inputBytes = 100 * 1024 * 1024;  // 100 MB
    stats.processingTimeMs = 10000;         // 10 seconds

    // Should be ~10 MB/s
    EXPECT_NEAR(stats.throughputMBps(), 10.0, 0.1);

    // Zero time
    stats.processingTimeMs = 0;
    EXPECT_DOUBLE_EQ(stats.throughputMBps(), 0.0);
}

// =============================================================================
// Property 6: Complete Compression Round-Trip Consistency
// =============================================================================

namespace roundtrip {

/// @brief Generate a temporary file path for testing.
[[nodiscard]] std::filesystem::path tempFilePath(const std::string& suffix) {
    static std::atomic<int> counter{0};
    auto path = std::filesystem::temp_directory_path() /
                ("fqc_pipeline_test_" + std::to_string(counter++) + "_" +
                 std::to_string(std::random_device{}()) + suffix);
    return path;
}

/// @brief RAII cleanup for temporary files.
class TempFileGuard {
public:
    explicit TempFileGuard(std::filesystem::path path) : path_(std::move(path)) {}
    ~TempFileGuard() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

/// @brief Format a FASTQ record as a string.
[[nodiscard]] std::string formatFastqRecord(const std::string& id, const std::string& seq,
                                             const std::string& qual) {
    return "@" + id + "\n" + seq + "\n+\n" + qual + "\n";
}

/// @brief Write FASTQ records to a file.
void writeFastqFile(const std::filesystem::path& path,
                    const std::vector<std::tuple<std::string, std::string, std::string>>& records) {
    std::ofstream ofs(path, std::ios::binary);
    for (const auto& [id, seq, qual] : records) {
        ofs << formatFastqRecord(id, seq, qual);
    }
}

/// @brief Read FASTQ records from a file.
[[nodiscard]] std::vector<std::tuple<std::string, std::string, std::string>>
readFastqFile(const std::filesystem::path& path) {
    std::vector<std::tuple<std::string, std::string, std::string>> records;
    
    std::ifstream ifs(path);
    std::string line;
    
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] != '@') continue;
        
        std::string id = line.substr(1);  // Remove '@'
        // Remove comment if present
        auto spacePos = id.find(' ');
        if (spacePos != std::string::npos) {
            id = id.substr(0, spacePos);
        }
        
        std::string seq, plus, qual;
        if (!std::getline(ifs, seq)) break;
        if (!std::getline(ifs, plus)) break;
        if (!std::getline(ifs, qual)) break;
        
        records.emplace_back(id, seq, qual);
    }
    
    return records;
}

/// @brief Compare two sets of FASTQ records for equivalence.
/// Note: Order may differ if reordering is enabled, so we compare as sets.
[[nodiscard]] bool recordsEquivalent(
    const std::vector<std::tuple<std::string, std::string, std::string>>& original,
    const std::vector<std::tuple<std::string, std::string, std::string>>& decompressed,
    bool preserveOrder) {
    
    if (original.size() != decompressed.size()) {
        return false;
    }
    
    if (preserveOrder) {
        // Order must match exactly
        for (std::size_t i = 0; i < original.size(); ++i) {
            if (std::get<1>(original[i]) != std::get<1>(decompressed[i]) ||
                std::get<2>(original[i]) != std::get<2>(decompressed[i])) {
                return false;
            }
        }
        return true;
    }
    
    // Order may differ, compare as multisets by sequence+quality
    auto makeKey = [](const std::tuple<std::string, std::string, std::string>& rec) {
        return std::get<1>(rec) + "\t" + std::get<2>(rec);
    };
    
    std::multiset<std::string> origSet, decompSet;
    for (const auto& rec : original) {
        origSet.insert(makeKey(rec));
    }
    for (const auto& rec : decompressed) {
        decompSet.insert(makeKey(rec));
    }
    
    return origSet == decompSet;
}

namespace gen {

/// @brief Generate a valid DNA sequence.
[[nodiscard]] rc::Gen<std::string> dnaSequenceRoundTrip(std::size_t minLen = 50, 
                                                         std::size_t maxLen = 300) {
    return rc::gen::mapcat(
        rc::gen::inRange<std::size_t>(minLen, maxLen + 1),
        [](std::size_t len) {
            return rc::gen::container<std::string>(
                len,
                rc::gen::elementOf('A', 'C', 'G', 'T', 'N'));
        });
}

/// @brief Generate a valid quality string.
[[nodiscard]] rc::Gen<std::string> qualityStringRoundTrip(std::size_t len) {
    return rc::gen::container<std::string>(
        len,
        rc::gen::inRange<char>('!', 'J' + 1));  // Phred 0-41
}

/// @brief Generate an Illumina-style read ID.
[[nodiscard]] rc::Gen<std::string> illuminaReadIdRoundTrip(int index) {
    return rc::gen::map(
        rc::gen::tuple(
            rc::gen::inRange(1, 10),     // tile
            rc::gen::inRange(1, 10000),  // x
            rc::gen::inRange(1, 10000)   // y
        ),
        [index](const std::tuple<int, int, int>& t) {
            return "SIM:1:FCX:1:" + std::to_string(std::get<0>(t)) + ":" +
                   std::to_string(std::get<1>(t)) + ":" +
                   std::to_string(std::get<2>(t)) + ":" + std::to_string(index);
        });
}

/// @brief Generate a FASTQ record tuple (id, seq, qual).
[[nodiscard]] rc::Gen<std::tuple<std::string, std::string, std::string>> 
fastqRecordRoundTrip(int index) {
    return rc::gen::mapcat(
        dnaSequenceRoundTrip(50, 200),
        [index](std::string seq) {
            return rc::gen::map(
                rc::gen::tuple(illuminaReadIdRoundTrip(index), 
                               qualityStringRoundTrip(seq.length())),
                [seq = std::move(seq)](std::tuple<std::string, std::string> t) {
                    return std::make_tuple(std::move(std::get<0>(t)), seq, 
                                           std::move(std::get<1>(t)));
                });
        });
}

/// @brief Generate a vector of FASTQ records.
[[nodiscard]] rc::Gen<std::vector<std::tuple<std::string, std::string, std::string>>>
fastqRecordsRoundTrip(std::size_t minCount = 10, std::size_t maxCount = 100) {
    return rc::gen::mapcat(
        rc::gen::inRange<std::size_t>(minCount, maxCount + 1),
        [](std::size_t count) {
            return rc::gen::exec([count]() {
                std::vector<std::tuple<std::string, std::string, std::string>> records;
                records.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    records.push_back(*fastqRecordRoundTrip(static_cast<int>(i)));
                }
                return records;
            });
        });
}

/// @brief Generate long read FASTQ records (for Medium/Long read class testing).
[[nodiscard]] rc::Gen<std::tuple<std::string, std::string, std::string>> 
longReadRecordRoundTrip(int index, std::size_t minLen = 1000, std::size_t maxLen = 5000) {
    return rc::gen::mapcat(
        dnaSequenceRoundTrip(minLen, maxLen),
        [index](std::string seq) {
            return rc::gen::map(
                rc::gen::tuple(illuminaReadIdRoundTrip(index), 
                               qualityStringRoundTrip(seq.length())),
                [seq = std::move(seq)](std::tuple<std::string, std::string> t) {
                    return std::make_tuple(std::move(std::get<0>(t)), seq, 
                                           std::move(std::get<1>(t)));
                });
        });
}

}  // namespace gen

}  // namespace roundtrip

// =============================================================================
// Property 6 Tests: Complete Compression Round-Trip
// =============================================================================

/// @brief Property 6: Short read compression round-trip consistency (streaming mode).
/// For any valid short-read FASTQ data, compress then decompress should produce
/// equivalent output when using streaming mode (order preserved).
/// **Validates: Requirements 1.1, 2.1, 2.2**
RC_GTEST_PROP(PipelineRoundTripProperty, ShortReadStreamingRoundTrip, ()) {
    using namespace roundtrip;
    
    // Generate test data
    auto records = *gen::fastqRecordsRoundTrip(10, 50);
    RC_PRE(!records.empty());
    
    // Create temporary files
    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");
    
    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    TempFileGuard outputGuard(outputPath);
    
    // Write input FASTQ
    writeFastqFile(inputPath, records);
    
    // Configure compression (streaming mode - preserves order)
    CompressionPipelineConfig compressConfig;
    compressConfig.streamingMode = true;
    compressConfig.enableReorder = false;
    compressConfig.readLengthClass = ReadLengthClass::kShort;
    compressConfig.blockSize = 1000;  // Small blocks for testing
    compressConfig.numThreads = 1;
    
    // Compress
    CompressionPipeline compressor(compressConfig);
    auto compressResult = compressor.run(inputPath, compressedPath);
    RC_ASSERT(compressResult.has_value());
    
    // Configure decompression
    DecompressionPipelineConfig decompressConfig;
    decompressConfig.numThreads = 1;
    
    // Decompress
    DecompressionPipeline decompressor(decompressConfig);
    auto decompressResult = decompressor.run(compressedPath, outputPath);
    RC_ASSERT(decompressResult.has_value());
    
    // Read decompressed output
    auto decompressedRecords = readFastqFile(outputPath);
    
    // Verify round-trip consistency (order preserved in streaming mode)
    RC_ASSERT(recordsEquivalent(records, decompressedRecords, true));
}

/// @brief Property 6.1: Short read compression round-trip with reordering.
/// For any valid short-read FASTQ data, compress then decompress should produce
/// equivalent output (content matches, order may differ).
/// **Validates: Requirements 1.1, 2.1, 2.2**
RC_GTEST_PROP(PipelineRoundTripProperty, ShortReadReorderRoundTrip, ()) {
    using namespace roundtrip;
    
    // Generate test data
    auto records = *gen::fastqRecordsRoundTrip(20, 80);
    RC_PRE(!records.empty());
    
    // Create temporary files
    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");
    
    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    TempFileGuard outputGuard(outputPath);
    
    // Write input FASTQ
    writeFastqFile(inputPath, records);
    
    // Configure compression (with reordering)
    CompressionPipelineConfig compressConfig;
    compressConfig.streamingMode = false;
    compressConfig.enableReorder = true;
    compressConfig.saveReorderMap = true;
    compressConfig.readLengthClass = ReadLengthClass::kShort;
    compressConfig.blockSize = 1000;
    compressConfig.numThreads = 1;
    
    // Compress
    CompressionPipeline compressor(compressConfig);
    auto compressResult = compressor.run(inputPath, compressedPath);
    RC_ASSERT(compressResult.has_value());
    
    // Configure decompression
    DecompressionPipelineConfig decompressConfig;
    decompressConfig.numThreads = 1;
    
    // Decompress
    DecompressionPipeline decompressor(decompressConfig);
    auto decompressResult = decompressor.run(compressedPath, outputPath);
    RC_ASSERT(decompressResult.has_value());
    
    // Read decompressed output
    auto decompressedRecords = readFastqFile(outputPath);
    
    // Verify round-trip consistency (order may differ with reordering)
    RC_ASSERT(recordsEquivalent(records, decompressedRecords, false));
}

/// @brief Property 6.2: Medium read compression round-trip.
/// For any valid medium-length FASTQ data (>511bp), compress then decompress
/// should produce equivalent output.
/// **Validates: Requirements 1.1, 1.1.3, 2.1, 2.2**
RC_GTEST_PROP(PipelineRoundTripProperty, MediumReadRoundTrip, ()) {
    using namespace roundtrip;
    
    // Generate medium-length reads (>511bp triggers MEDIUM class)
    auto numRecords = *rc::gen::inRange(5, 20);
    std::vector<std::tuple<std::string, std::string, std::string>> records;
    records.reserve(numRecords);
    
    for (int i = 0; i < numRecords; ++i) {
        records.push_back(*gen::longReadRecordRoundTrip(i, 600, 2000));
    }
    
    RC_PRE(!records.empty());
    
    // Create temporary files
    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");
    
    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    TempFileGuard outputGuard(outputPath);
    
    // Write input FASTQ
    writeFastqFile(inputPath, records);
    
    // Configure compression (Medium read class - no reordering)
    CompressionPipelineConfig compressConfig;
    compressConfig.streamingMode = false;
    compressConfig.enableReorder = false;  // Disabled for medium reads
    compressConfig.readLengthClass = ReadLengthClass::kMedium;
    compressConfig.blockSize = 500;
    compressConfig.numThreads = 1;
    
    // Compress
    CompressionPipeline compressor(compressConfig);
    auto compressResult = compressor.run(inputPath, compressedPath);
    RC_ASSERT(compressResult.has_value());
    
    // Configure decompression
    DecompressionPipelineConfig decompressConfig;
    decompressConfig.numThreads = 1;
    
    // Decompress
    DecompressionPipeline decompressor(decompressConfig);
    auto decompressResult = decompressor.run(compressedPath, outputPath);
    RC_ASSERT(decompressResult.has_value());
    
    // Read decompressed output
    auto decompressedRecords = readFastqFile(outputPath);
    
    // Verify round-trip consistency (order preserved - no reordering for medium)
    RC_ASSERT(recordsEquivalent(records, decompressedRecords, true));
}

/// @brief Property 6.3: Empty file round-trip.
/// An empty FASTQ file should compress and decompress to an empty file.
/// **Validates: Requirements 1.1, 2.1**
TEST_F(PipelinePropertyTest, EmptyFileRoundTrip) {
    using namespace roundtrip;
    
    // Create temporary files
    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");
    
    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    TempFileGuard outputGuard(outputPath);
    
    // Write empty FASTQ
    std::ofstream(inputPath).close();
    
    // Configure compression
    CompressionPipelineConfig compressConfig;
    compressConfig.streamingMode = true;
    compressConfig.numThreads = 1;
    
    // Compress
    CompressionPipeline compressor(compressConfig);
    auto compressResult = compressor.run(inputPath, compressedPath);
    EXPECT_TRUE(compressResult.has_value());
    
    // Configure decompression
    DecompressionPipelineConfig decompressConfig;
    decompressConfig.numThreads = 1;
    
    // Decompress
    DecompressionPipeline decompressor(decompressConfig);
    auto decompressResult = decompressor.run(compressedPath, outputPath);
    EXPECT_TRUE(decompressResult.has_value());
    
    // Verify output is empty
    auto decompressedRecords = readFastqFile(outputPath);
    EXPECT_TRUE(decompressedRecords.empty());
}

/// @brief Property 6.4: Single record round-trip.
/// A single FASTQ record should compress and decompress correctly.
/// **Validates: Requirements 1.1, 2.1**
RC_GTEST_PROP(PipelineRoundTripProperty, SingleRecordRoundTrip, ()) {
    using namespace roundtrip;
    
    // Generate single record
    auto record = *gen::fastqRecordRoundTrip(0);
    std::vector<std::tuple<std::string, std::string, std::string>> records = {record};
    
    // Create temporary files
    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");
    
    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    TempFileGuard outputGuard(outputPath);
    
    // Write input FASTQ
    writeFastqFile(inputPath, records);
    
    // Configure compression
    CompressionPipelineConfig compressConfig;
    compressConfig.streamingMode = true;
    compressConfig.numThreads = 1;
    
    // Compress
    CompressionPipeline compressor(compressConfig);
    auto compressResult = compressor.run(inputPath, compressedPath);
    RC_ASSERT(compressResult.has_value());
    
    // Configure decompression
    DecompressionPipelineConfig decompressConfig;
    decompressConfig.numThreads = 1;
    
    // Decompress
    DecompressionPipeline decompressor(decompressConfig);
    auto decompressResult = decompressor.run(compressedPath, outputPath);
    RC_ASSERT(decompressResult.has_value());
    
    // Read decompressed output
    auto decompressedRecords = readFastqFile(outputPath);
    
    // Verify round-trip consistency
    RC_ASSERT(decompressedRecords.size() == 1);
    RC_ASSERT(std::get<1>(decompressedRecords[0]) == std::get<1>(record));
    RC_ASSERT(std::get<2>(decompressedRecords[0]) == std::get<2>(record));
}

/// @brief Property 6.5: Variable length reads round-trip.
/// FASTQ data with variable length reads should compress and decompress correctly.
/// **Validates: Requirements 1.1, 2.1**
RC_GTEST_PROP(PipelineRoundTripProperty, VariableLengthRoundTrip, ()) {
    using namespace roundtrip;
    
    // Generate records with varying lengths
    auto numRecords = *rc::gen::inRange(10, 30);
    std::vector<std::tuple<std::string, std::string, std::string>> records;
    records.reserve(numRecords);
    
    for (int i = 0; i < numRecords; ++i) {
        // Vary length significantly
        auto minLen = *rc::gen::inRange(30, 100);
        auto maxLen = minLen + *rc::gen::inRange(50, 200);
        
        auto seq = *gen::dnaSequenceRoundTrip(minLen, maxLen);
        auto qual = *gen::qualityStringRoundTrip(seq.length());
        auto id = *gen::illuminaReadIdRoundTrip(i);
        
        records.emplace_back(id, seq, qual);
    }
    
    RC_PRE(!records.empty());
    
    // Create temporary files
    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");
    
    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    TempFileGuard outputGuard(outputPath);
    
    // Write input FASTQ
    writeFastqFile(inputPath, records);
    
    // Configure compression
    CompressionPipelineConfig compressConfig;
    compressConfig.streamingMode = true;
    compressConfig.numThreads = 1;
    
    // Compress
    CompressionPipeline compressor(compressConfig);
    auto compressResult = compressor.run(inputPath, compressedPath);
    RC_ASSERT(compressResult.has_value());
    
    // Configure decompression
    DecompressionPipelineConfig decompressConfig;
    decompressConfig.numThreads = 1;
    
    // Decompress
    DecompressionPipeline decompressor(decompressConfig);
    auto decompressResult = decompressor.run(compressedPath, outputPath);
    RC_ASSERT(decompressResult.has_value());
    
    // Read decompressed output
    auto decompressedRecords = readFastqFile(outputPath);
    
    // Verify round-trip consistency
    RC_ASSERT(recordsEquivalent(records, decompressedRecords, true));
}

/// @brief Property 6.6: Compression statistics consistency.
/// After compression, statistics should reflect the actual data processed.
/// **Validates: Requirements 4.1**
RC_GTEST_PROP(PipelineRoundTripProperty, CompressionStatsConsistency, ()) {
    using namespace roundtrip;
    
    // Generate test data
    auto records = *gen::fastqRecordsRoundTrip(20, 60);
    RC_PRE(!records.empty());
    
    // Create temporary files
    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    
    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    
    // Write input FASTQ
    writeFastqFile(inputPath, records);
    
    // Configure compression
    CompressionPipelineConfig compressConfig;
    compressConfig.streamingMode = true;
    compressConfig.numThreads = 1;
    
    // Compress
    CompressionPipeline compressor(compressConfig);
    auto compressResult = compressor.run(inputPath, compressedPath);
    RC_ASSERT(compressResult.has_value());
    
    // Verify statistics
    const auto& stats = compressor.stats();
    RC_ASSERT(stats.totalReads == records.size());
    RC_ASSERT(stats.totalBlocks > 0);
    RC_ASSERT(stats.inputBytes > 0);
    RC_ASSERT(stats.outputBytes > 0);
    RC_ASSERT(stats.processingTimeMs > 0);
}

/// @brief Property 6.7: Cancellation handling.
/// Pipeline should handle cancellation gracefully.
/// **Validates: Requirements 4.1**
TEST_F(PipelinePropertyTest, CancellationHandling) {
    using namespace roundtrip;
    
    // Generate larger test data to allow time for cancellation
    std::vector<std::tuple<std::string, std::string, std::string>> records;
    for (int i = 0; i < 100; ++i) {
        std::string seq(150, 'A');
        std::string qual(150, 'I');
        records.emplace_back("read_" + std::to_string(i), seq, qual);
    }
    
    // Create temporary files
    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    
    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    
    // Write input FASTQ
    writeFastqFile(inputPath, records);
    
    // Configure compression with progress callback that cancels
    CompressionPipelineConfig compressConfig;
    compressConfig.streamingMode = true;
    compressConfig.numThreads = 1;
    compressConfig.progressIntervalMs = 1;  // Frequent callbacks
    
    std::atomic<int> callbackCount{0};
    compressConfig.progressCallback = [&callbackCount](const ProgressInfo&) {
        return ++callbackCount < 2;  // Cancel after 2 callbacks
    };
    
    // Compress (should be cancelled)
    CompressionPipeline compressor(compressConfig);
    auto compressResult = compressor.run(inputPath, compressedPath);
    
    // Should either complete (if fast enough) or be cancelled
    if (!compressResult.has_value()) {
        EXPECT_EQ(compressResult.error().code(), ErrorCode::kCancelled);
        EXPECT_TRUE(compressor.isCancelled());
    }
}

}  // namespace fqc::pipeline::test
