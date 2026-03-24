// =============================================================================
// fq-compressor - Original Order Command Tests
// =============================================================================

#include "fqc/commands/compress_command.h"
#include "fqc/commands/decompress_command.h"
#include "fqc/common/logger.h"
#include "fqc/format/fqc_reader.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

namespace fqc::commands::test {

class OriginalOrderCommandTestEnvironment : public ::testing::Environment {
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
    ::testing::AddGlobalTestEnvironment(new OriginalOrderCommandTestEnvironment);

namespace {

using FastqTuple = std::tuple<std::string, std::string, std::string>;

[[nodiscard]] std::filesystem::path tempFilePath(std::string_view suffix) {
    static std::atomic<int> counter{0};
    return std::filesystem::temp_directory_path() /
        ("fqc_original_order_test_" + std::to_string(counter++) + "_" +
         std::to_string(std::random_device{}()) + std::string(suffix));
}

struct TempFileGuard {
    std::filesystem::path path;
    explicit TempFileGuard(std::filesystem::path p) : path(std::move(p)) {}
    ~TempFileGuard() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        std::filesystem::remove(path.string() + ".tmp", ec);
    }
};

void writeFastqFile(const std::filesystem::path& path, const std::vector<FastqTuple>& records) {
    std::ofstream out(path);
    ASSERT_TRUE(out.is_open());
    for (const auto& [id, seq, qual] : records) {
        out << '@' << id << '\n' << seq << "\n+\n" << qual << '\n';
    }
}

[[nodiscard]] std::vector<FastqTuple> readFastqFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    EXPECT_TRUE(in.is_open());

    std::vector<FastqTuple> records;
    std::string header;
    while (std::getline(in, header)) {
        std::string seq;
        std::string plus;
        std::string qual;
        if (!std::getline(in, seq) || !std::getline(in, plus) || !std::getline(in, qual)) {
            break;
        }
        if (!header.empty() && header.front() == '@') {
            header.erase(header.begin());
        }
        records.emplace_back(header, seq, qual);
    }
    return records;
}

[[nodiscard]] bool recordsEquivalent(const std::vector<FastqTuple>& expected,
                                     const std::vector<FastqTuple>& actual,
                                     bool preserveOrder) {
    if (expected.size() != actual.size()) {
        return false;
    }

    if (preserveOrder) {
        return expected == actual;
    }

    auto lhs = expected;
    auto rhs = actual;
    std::sort(lhs.begin(), lhs.end());
    std::sort(rhs.begin(), rhs.end());
    return lhs == rhs;
}

std::vector<FastqTuple> makeDeterministicRecords(std::size_t count) {
    std::vector<FastqTuple> records;
    records.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto len = 60 + (i % 7);
        std::string seq(static_cast<std::size_t>(len), "ACGTN"[i % 5]);
        std::string qual(static_cast<std::size_t>(len), static_cast<char>('!' + (i % 30)));
        records.emplace_back("read_" + std::to_string(i), std::move(seq), std::move(qual));
    }
    return records;
}

void compressWithReorder(const std::filesystem::path& inputPath,
                         const std::filesystem::path& compressedPath) {
    CompressOptions opts;
    opts.inputPath = inputPath;
    opts.outputPath = compressedPath;
    opts.forceOverwrite = true;
    opts.showProgress = false;
    opts.threads = 1;
    opts.streamingMode = false;
    opts.enableReordering = true;
    opts.saveReorderMap = true;
    opts.blockSize = 4;
    opts.autoDetectLongRead = false;
    opts.longReadMode = ReadLengthClass::kShort;

    CompressCommand command(std::move(opts));
    ASSERT_EQ(command.execute(), 0);
}

std::vector<FastqTuple> expectedOriginalSubset(const std::vector<FastqTuple>& records,
                                               const std::filesystem::path& compressedPath,
                                               ReadId archiveStart,
                                               ReadId archiveEnd) {
    format::FQCReader reader(compressedPath);
    reader.open();
    reader.loadReorderMap();

    std::vector<FastqTuple> expected;
    for (ReadId originalId = 1; originalId <= static_cast<ReadId>(records.size()); ++originalId) {
        ReadId archiveId = reader.lookupArchiveId(originalId);
        if (archiveId >= archiveStart && archiveId <= archiveEnd) {
            expected.push_back(records[static_cast<std::size_t>(originalId - 1)]);
        }
    }
    return expected;
}

}  // namespace

TEST(OriginalOrderCommandTest, RestoresOriginalOrderForReorderedArchive) {
    auto records = makeDeterministicRecords(12);

    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    TempFileGuard outputGuard(outputPath);

    writeFastqFile(inputPath, records);
    compressWithReorder(inputPath, compressedPath);

    DecompressOptions opts;
    opts.inputPath = compressedPath;
    opts.outputPath = outputPath;
    opts.originalOrder = true;
    opts.threads = 1;
    opts.showProgress = false;
    opts.forceOverwrite = true;

    DecompressCommand command(std::move(opts));
    ASSERT_EQ(command.execute(), 0);

    auto decompressed = readFastqFile(outputPath);
    EXPECT_TRUE(recordsEquivalent(records, decompressed, true));
}

TEST(OriginalOrderCommandTest, FallsBackFromMultiThreadRequest) {
    auto records = makeDeterministicRecords(12);

    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    TempFileGuard outputGuard(outputPath);

    writeFastqFile(inputPath, records);
    compressWithReorder(inputPath, compressedPath);

    DecompressOptions opts;
    opts.inputPath = compressedPath;
    opts.outputPath = outputPath;
    opts.originalOrder = true;
    opts.threads = 4;
    opts.showProgress = false;
    opts.forceOverwrite = true;

    DecompressCommand command(std::move(opts));
    ASSERT_EQ(command.execute(), 0);

    auto decompressed = readFastqFile(outputPath);
    EXPECT_TRUE(recordsEquivalent(records, decompressed, true));
}

TEST(OriginalOrderCommandTest, ReordersSelectedArchiveSubsetOnly) {
    auto records = makeDeterministicRecords(12);

    auto inputPath = tempFilePath(".fastq");
    auto compressedPath = tempFilePath(".fqc");
    auto originalOrderPath = tempFilePath(".original.fastq");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard compressedGuard(compressedPath);
    TempFileGuard originalGuard(originalOrderPath);

    writeFastqFile(inputPath, records);
    compressWithReorder(inputPath, compressedPath);

    DecompressOptions originalOpts;
    originalOpts.inputPath = compressedPath;
    originalOpts.outputPath = originalOrderPath;
    originalOpts.range = ReadRange{3, 8};
    originalOpts.originalOrder = true;
    originalOpts.threads = 1;
    originalOpts.showProgress = false;
    originalOpts.forceOverwrite = true;

    DecompressCommand originalCommand(std::move(originalOpts));
    ASSERT_EQ(originalCommand.execute(), 0);

    auto originalSubset = readFastqFile(originalOrderPath);
    auto expectedSubset = expectedOriginalSubset(records, compressedPath, 3, 8);

    ASSERT_EQ(expectedSubset.size(), 6u);
    EXPECT_TRUE(recordsEquivalent(expectedSubset, originalSubset, true));
}

TEST(OriginalOrderCommandTest, SplitPeDerivedOutputChecksDerivedR1Path) {
    auto archivePath = tempFilePath(".fqc");
    auto requestedOutputPath = tempFilePath(".fastq");
    auto derivedR1Path = requestedOutputPath.parent_path() /
        (requestedOutputPath.stem().string() + "_R1" + requestedOutputPath.extension().string());

    TempFileGuard archiveGuard(archivePath);
    TempFileGuard outputGuard(requestedOutputPath);
    TempFileGuard derivedR1Guard(derivedR1Path);

    {
        std::ofstream archiveOut(archivePath, std::ios::binary);
        ASSERT_TRUE(archiveOut.is_open());
        archiveOut << "stub";
    }

    {
        std::ofstream existingOut(derivedR1Path);
        ASSERT_TRUE(existingOut.is_open());
        existingOut << "already exists";
    }

    DecompressOptions opts;
    opts.inputPath = archivePath;
    opts.outputPath = requestedOutputPath;
    opts.splitPairedEnd = true;
    opts.showProgress = false;
    opts.forceOverwrite = false;

    DecompressCommand command(std::move(opts));
    EXPECT_EQ(command.execute(), 2);
}

TEST(OriginalOrderCommandTest, SplitPeDerivedOutputChecksDerivedR2Path) {
    auto archivePath = tempFilePath(".fqc");
    auto requestedOutputPath = tempFilePath(".fastq");
    auto derivedR2Path = requestedOutputPath.parent_path() /
        (requestedOutputPath.stem().string() + "_R2" + requestedOutputPath.extension().string());

    TempFileGuard archiveGuard(archivePath);
    TempFileGuard outputGuard(requestedOutputPath);
    TempFileGuard derivedR2Guard(derivedR2Path);

    {
        std::ofstream archiveOut(archivePath, std::ios::binary);
        ASSERT_TRUE(archiveOut.is_open());
        archiveOut << "stub";
    }

    {
        std::ofstream existingOut(derivedR2Path);
        ASSERT_TRUE(existingOut.is_open());
        existingOut << "already exists";
    }

    DecompressOptions opts;
    opts.inputPath = archivePath;
    opts.outputPath = requestedOutputPath;
    opts.splitPairedEnd = true;
    opts.showProgress = false;
    opts.forceOverwrite = false;

    DecompressCommand command(std::move(opts));
    EXPECT_EQ(command.execute(), 2);
}

}  // namespace fqc::commands::test
