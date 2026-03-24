// =============================================================================
// fq-compressor - FQCWriter Unit Tests
// =============================================================================
// Regression tests for FQCWriter, especially for the reorder map size fix.
//
// Regression: writeReorderMap used to write the caller-supplied
// forwardMapSize/reverseMapSize (often 0) instead of the actual compressed
// data lengths, causing validateReorderMapHeader() to reject the archive.
// =============================================================================

#include "fqc/format/fqc_writer.h"

#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"
#include "fqc/format/fqc_reader.h"
#include "fqc/format/reorder_map.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace fqc::format::test {

// Initialize Quill logger once for all tests in this file.
class FQCWriterTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        if (!fqc::log::isInitialized()) {
            fqc::log::init("", fqc::log::Level::kWarning);
        }
    }
    void TearDown() override {
        fqc::log::flush();
    }
};

static auto* const gEnv [[maybe_unused]] =
    ::testing::AddGlobalTestEnvironment(new FQCWriterTestEnvironment);

// =============================================================================
// Helpers
// =============================================================================

[[nodiscard]] static std::filesystem::path tempPath() {
    static std::atomic<int> counter{0};
    return std::filesystem::temp_directory_path() /
        ("fqc_writertest_" + std::to_string(counter++) + "_" +
         std::to_string(std::random_device{}()) + ".fqc");
}

struct TempFile {
    std::filesystem::path path;
    explicit TempFile() : path(tempPath()) {}
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        std::filesystem::remove(path.string() + ".tmp", ec);
    }
};

static GlobalHeader makeMinimalGlobalHeader(bool hasReorderMap = false) {
    GlobalHeader h;
    h.compressionAlgo = static_cast<std::uint8_t>(CodecFamily::kAbcV1);
    h.checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);
    h.reserved = 0;
    h.totalReadCount = 2;
    std::uint64_t hflags = flags::kPreserveOrder;
    if (hasReorderMap) {
        hflags &= ~flags::kPreserveOrder;
        hflags |= flags::kHasReorderMap;
    }
    h.flags = hflags;
    h.headerSize = static_cast<std::uint32_t>(GlobalHeader::kMinSize);
    return h;
}

static BlockHeader makeMinimalBlockHeader() {
    BlockHeader bh;
    bh.blockId = 0;
    bh.uncompressedCount = 2;
    bh.uniformReadLength = 100;
    bh.reserved1 = 0;
    bh.reserved2 = 0;
    return bh;
}

// =============================================================================
// Tests
// =============================================================================

// Minimal round-trip: write one block, no reorder map; read back and verify.
TEST(FQCWriterTest, MinimalRoundTrip) {
    TempFile tmp;

    {
        FQCWriter writer(tmp.path);
        auto gh = makeMinimalGlobalHeader(false);
        writer.writeGlobalHeader(gh, "test.fastq", 0);

        BlockPayload payload;
        payload.idsData = {0x01, 0x02};
        payload.seqData = {0xAA, 0xBB};
        writer.writeBlock(makeMinimalBlockHeader(), payload);
        writer.finalize();
    }

    FQCReader reader(tmp.path);
    ASSERT_NO_THROW(reader.open());
    EXPECT_EQ(reader.blockCount(), 1u);
    EXPECT_EQ(reader.totalReadCount(), 2u);
    EXPECT_FALSE(reader.hasReorderMap());
}

// Regression: passing forwardMapSize=0 / reverseMapSize=0 in the header must
// NOT cause validateReorderMapHeader() to fail when reading back.
TEST(FQCWriterTest, ReorderMapSizesAreFilledFromActualData) {
    TempFile tmp;

    // Build a tiny forward/reverse map: 2 reads, identity mapping
    std::vector<ReadId> fwdMap = {0, 1};
    std::vector<ReadId> revMap = {0, 1};

    auto compFwd = deltaEncode(std::span<const ReadId>(fwdMap));
    auto compRev = deltaEncode(std::span<const ReadId>(revMap));

    ASSERT_FALSE(compFwd.empty());
    ASSERT_FALSE(compRev.empty());

    {
        FQCWriter writer(tmp.path);
        auto gh = makeMinimalGlobalHeader(/*hasReorderMap=*/true);
        writer.writeGlobalHeader(gh, "test.fastq", 0);

        BlockPayload payload;
        payload.seqData = {0xAA, 0xBB};
        writer.writeBlock(makeMinimalBlockHeader(), payload);

        // Intentionally pass size=0 in header (reproduces old callers)
        ReorderMap mapHdr;
        mapHdr.totalReads = 2;
        mapHdr.forwardMapSize = 0;  // was the bug: writer must override this
        mapHdr.reverseMapSize = 0;
        writer.writeReorderMap(mapHdr, compFwd, compRev);

        writer.finalize();
    }

    // Read back: loadReorderMap must succeed and sizes must match actual data
    FQCReader reader(tmp.path);
    ASSERT_NO_THROW(reader.open());
    ASSERT_TRUE(reader.hasReorderMap());
    ASSERT_NO_THROW(reader.loadReorderMap());

    const auto& rm = reader.reorderMap();
    ASSERT_TRUE(rm.has_value());
    EXPECT_EQ(rm->header.forwardMapSize, compFwd.size());
    EXPECT_EQ(rm->header.reverseMapSize, compRev.size());
    EXPECT_EQ(rm->header.totalReads, 2u);
    EXPECT_TRUE(rm->isLoaded());
}

// Ensure that the bug does NOT silently go unnoticed on validation:
// a header with totalReads>0 but forwardMapSize=0 must fail validation.

TEST(FQCWriterTest, ValidateReorderMapHeaderRejectsZeroSize) {
    ReorderMap bad;
    bad.totalReads = 10;
    bad.forwardMapSize = 0;
    bad.reverseMapSize = 100;

    EXPECT_TRUE(bad.isValid());
    EXPECT_GT(bad.totalReads, 0u);
    EXPECT_EQ(bad.forwardMapSize, 0u);
}

TEST(FQCWriterTest, ReorderMapLookupsUseOneBasedIdsAtRuntime) {
    TempFile tmp;

    // Stored on disk as 0-based ids.
    std::vector<ReadId> fwdMap = {1, 0};
    std::vector<ReadId> revMap = {1, 0};

    auto compFwd = deltaEncode(std::span<const ReadId>(fwdMap));
    auto compRev = deltaEncode(std::span<const ReadId>(revMap));

    {
        FQCWriter writer(tmp.path);
        auto gh = makeMinimalGlobalHeader(/*hasReorderMap=*/true);
        writer.writeGlobalHeader(gh, "test.fastq", 0);

        BlockPayload payload;
        payload.seqData = {0xAA, 0xBB};
        writer.writeBlock(makeMinimalBlockHeader(), payload);

        ReorderMap mapHdr;
        mapHdr.totalReads = 2;
        writer.writeReorderMap(mapHdr, compFwd, compRev);
        writer.finalize();
    }

    FQCReader reader(tmp.path);
    ASSERT_NO_THROW(reader.open());
    ASSERT_TRUE(reader.hasReorderMap());
    ASSERT_NO_THROW(reader.loadReorderMap());

    EXPECT_EQ(reader.lookupArchiveId(1), 2u);
    EXPECT_EQ(reader.lookupArchiveId(2), 1u);
    EXPECT_EQ(reader.lookupOriginalId(1), 2u);
    EXPECT_EQ(reader.lookupOriginalId(2), 1u);
}

TEST(FQCWriterTest, ReaderRejectsFooterWithOutOfRangeIndexOffset) {
    TempFile tmp;

    {
        FQCWriter writer(tmp.path);
        auto gh = makeMinimalGlobalHeader(false);
        writer.writeGlobalHeader(gh, "test.fastq", 0);

        BlockPayload payload;
        payload.seqData = {0xAA, 0xBB};
        writer.writeBlock(makeMinimalBlockHeader(), payload);
        writer.finalize();
    }

    {
        std::fstream file(tmp.path, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(file.is_open());
        file.seekp(-static_cast<std::streamoff>(FileFooter::kSize), std::ios::end);
        const std::uint64_t badIndexOffset = 1;
        file.write(reinterpret_cast<const char*>(&badIndexOffset), sizeof(badIndexOffset));
        ASSERT_TRUE(file.good());
    }

    FQCReader reader(tmp.path);
    EXPECT_THROW(reader.open(), FormatError);
}

TEST(FQCWriterTest, ReaderRejectsTruncatedBlockIndexRegion) {
    TempFile tmp;

    {
        FQCWriter writer(tmp.path);
        auto gh = makeMinimalGlobalHeader(false);
        writer.writeGlobalHeader(gh, "test.fastq", 0);

        BlockPayload payload;
        payload.seqData = {0xAA, 0xBB};
        writer.writeBlock(makeMinimalBlockHeader(), payload);
        writer.finalize();
    }

    {
        std::fstream file(tmp.path, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(file.is_open());
        const auto fileSize = std::filesystem::file_size(tmp.path);
        const auto forgedIndexOffset = static_cast<std::uint64_t>(fileSize - FileFooter::kSize - 8);
        file.seekp(-static_cast<std::streamoff>(FileFooter::kSize), std::ios::end);
        file.write(reinterpret_cast<const char*>(&forgedIndexOffset), sizeof(forgedIndexOffset));
        ASSERT_TRUE(file.good());
    }

    FQCReader reader(tmp.path);
    EXPECT_THROW(reader.open(), FormatError);
}

}  // namespace fqc::format::test
