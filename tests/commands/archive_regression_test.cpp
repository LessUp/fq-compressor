#include "fqc/commands/compress_command.h"
#include "fqc/commands/decompress_command.h"
#include "fqc/commands/verify_command.h"
#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"
#include "fqc/format/fqc_reader.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace fqc::commands::test {

class ArchiveRegressionTestEnvironment : public ::testing::Environment {
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

static auto* const gArchiveRegressionEnv [[maybe_unused]] =
    ::testing::AddGlobalTestEnvironment(new ArchiveRegressionTestEnvironment);

namespace {

struct FastqRecordSpec {
    std::string id;
    std::string comment;
    std::string sequence;
    std::string quality;

    [[nodiscard]] bool operator==(const FastqRecordSpec& other) const noexcept = default;
};

[[nodiscard]] std::filesystem::path tempFilePath(std::string_view suffix) {
    static std::atomic<int> counter{0};
    return std::filesystem::temp_directory_path() /
        ("fqc_archive_regression_" + std::to_string(counter++) + "_" +
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

void writeFastqFile(const std::filesystem::path& path,
                    const std::vector<FastqRecordSpec>& records) {
    std::ofstream out(path);
    ASSERT_TRUE(out.is_open());
    for (const auto& record : records) {
        out << '@' << record.id;
        if (!record.comment.empty()) {
            out << ' ' << record.comment;
        }
        out << '\n' << record.sequence << "\n+\n" << record.quality << '\n';
    }
}

[[nodiscard]] std::vector<FastqRecordSpec> readFastqFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    EXPECT_TRUE(in.is_open());

    std::vector<FastqRecordSpec> records;
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

        FastqRecordSpec record;
        auto spacePos = header.find(' ');
        if (spacePos == std::string::npos) {
            record.id = header;
        } else {
            record.id = header.substr(0, spacePos);
            record.comment = header.substr(spacePos + 1);
        }
        record.sequence = std::move(seq);
        record.quality = std::move(qual);
        records.push_back(std::move(record));
    }

    return records;
}

std::vector<FastqRecordSpec> makeShortReadsWithComments() {
    return {
        {"read_1", "1:N:0:ACGT", "ACGTACGTACGT", "FFFFFFFFFFFF"},
        {"read_2", "2:N:0:ACGT", "TGCATGCATGCA", "HHHHHHHHHHHH"},
        {"read_3", "3:N:0:ACGT", "GGGGAAAACCCC", "IIIIIIIIIIII"},
        {"read_4", "4:N:0:ACGT", "TTTTCCCCAAAA", "JJJJJJJJJJJJ"},
    };
}

std::vector<FastqRecordSpec> makeShortReadsNoComments(std::size_t count) {
    std::vector<FastqRecordSpec> records;
    records.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::string seq(80, "ACGTN"[i % 5]);
        std::string qual(80, static_cast<char>('!' + (i % 30)));
        records.push_back({"read_" + std::to_string(i), "", std::move(seq), std::move(qual)});
    }
    return records;
}

std::pair<std::vector<FastqRecordSpec>, std::vector<FastqRecordSpec>> makePairedShortReads(
    std::size_t pairCount) {
    std::vector<FastqRecordSpec> r1;
    std::vector<FastqRecordSpec> r2;
    r1.reserve(pairCount);
    r2.reserve(pairCount);
    for (std::size_t i = 0; i < pairCount; ++i) {
        r1.push_back({"pair_" + std::to_string(i) + "/1",
                      "1:N:0:ACGT",
                      std::string(80, "ACGT"[i % 4]),
                      std::string(80, static_cast<char>('!' + (i % 30)))});
        r2.push_back({"pair_" + std::to_string(i) + "/2",
                      "2:N:0:ACGT",
                      std::string(80, "TGCA"[i % 4]),
                      std::string(80, static_cast<char>('#' + (i % 30)))});
    }
    return {std::move(r1), std::move(r2)};
}

std::vector<FastqRecordSpec> makeVariableLongReads() {
    return {
        {"long_1", "", std::string(12000, 'A'), std::string(12000, 'I')},
        {"long_2", "", std::string(13500, 'C'), std::string(13500, 'J')},
        {"long_3", "", std::string(15000, 'G'), std::string(15000, 'K')},
    };
}

std::vector<FastqRecordSpec> makeNonMonotonicVariableLongReads() {
    return {
        {"long_1", "", std::string(15000, 'A'), std::string(15000, 'I')},
        {"long_2", "", std::string(12000, 'C'), std::string(12000, 'J')},
        {"long_3", "", std::string(13500, 'G'), std::string(13500, 'K')},
    };
}

int compressArchive(const std::filesystem::path& inputPath,
                    const std::filesystem::path& outputPath,
                    int threads,
                    bool enableReordering,
                    bool saveReorderMap,
                    ReadLengthClass lengthClass,
                    bool autoDetectLongRead = false) {
    CompressOptions opts;
    opts.inputPath = inputPath;
    opts.outputPath = outputPath;
    opts.forceOverwrite = true;
    opts.showProgress = false;
    opts.threads = threads;
    opts.enableReordering = enableReordering;
    opts.saveReorderMap = saveReorderMap;
    opts.streamingMode = !enableReordering;
    opts.autoDetectLongRead = autoDetectLongRead;
    opts.longReadMode = lengthClass;
    opts.blockSize = (lengthClass == ReadLengthClass::kLong) ? 2 : 4;

    CompressCommand command(std::move(opts));
    return command.execute();
}

int compressPairedArchive(const std::filesystem::path& input1Path,
                          const std::filesystem::path& input2Path,
                          const std::filesystem::path& outputPath,
                          int threads,
                          std::size_t blockSize) {
    CompressOptions opts;
    opts.inputPath = input1Path;
    opts.input2Path = input2Path;
    opts.outputPath = outputPath;
    opts.forceOverwrite = true;
    opts.showProgress = false;
    opts.threads = threads;
    opts.paired = true;
    opts.enableReordering = false;
    opts.saveReorderMap = false;
    opts.streamingMode = true;
    opts.autoDetectLongRead = false;
    opts.longReadMode = ReadLengthClass::kShort;
    opts.blockSize = blockSize;
    opts.blockSizeExplicit = true;

    CompressCommand command(std::move(opts));
    return command.execute();
}

int decompressArchive(const std::filesystem::path& inputPath,
                      const std::filesystem::path& outputPath,
                      int threads,
                      bool originalOrder = false) {
    DecompressOptions opts;
    opts.inputPath = inputPath;
    opts.outputPath = outputPath;
    opts.forceOverwrite = true;
    opts.showProgress = false;
    opts.threads = threads;
    opts.originalOrder = originalOrder;

    DecompressCommand command(std::move(opts));
    return command.execute();
}

}  // namespace

TEST(ArchiveRegressionTest, VerifyAcceptsOneBasedArchiveIds) {
    auto inputPath = tempFilePath(".fastq");
    auto archivePath = tempFilePath(".fqc");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard archiveGuard(archivePath);

    writeFastqFile(inputPath, makeShortReadsNoComments(6));
    ASSERT_EQ(compressArchive(inputPath, archivePath, 1, false, false, ReadLengthClass::kShort), 0);

    VerifyOptions verifyOpts;
    verifyOpts.inputPath = archivePath;
    verifyOpts.verbose = false;
    verifyOpts.jsonOutput = false;

    VerifyCommand verify(std::move(verifyOpts));
    EXPECT_EQ(verify.execute(), 0);
}

TEST(ArchiveRegressionTest, SingleThreadRoundTripPreservesHeaderComments) {
    auto inputPath = tempFilePath(".fastq");
    auto archivePath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard archiveGuard(archivePath);
    TempFileGuard outputGuard(outputPath);

    auto records = makeShortReadsWithComments();
    writeFastqFile(inputPath, records);

    ASSERT_EQ(compressArchive(inputPath, archivePath, 1, false, false, ReadLengthClass::kShort), 0);
    ASSERT_EQ(decompressArchive(archivePath, outputPath, 1), 0);

    EXPECT_EQ(readFastqFile(outputPath), records);
}

TEST(ArchiveRegressionTest, ParallelDecompressionPreservesHeaderComments) {
    auto inputPath = tempFilePath(".fastq");
    auto archivePath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard archiveGuard(archivePath);
    TempFileGuard outputGuard(outputPath);

    auto records = makeShortReadsWithComments();
    writeFastqFile(inputPath, records);

    ASSERT_EQ(compressArchive(inputPath, archivePath, 4, false, false, ReadLengthClass::kShort), 0);
    ASSERT_EQ(decompressArchive(archivePath, outputPath, 4), 0);

    EXPECT_EQ(readFastqFile(outputPath), records);
}

TEST(ArchiveRegressionTest, ParallelCompressionWritesHonestOrderMetadata) {
    auto inputPath = tempFilePath(".fastq");
    auto archivePath = tempFilePath(".fqc");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard archiveGuard(archivePath);

    writeFastqFile(inputPath, makeShortReadsNoComments(12));
    ASSERT_EQ(compressArchive(inputPath, archivePath, 4, true, true, ReadLengthClass::kShort), 0);

    format::FQCReader reader(archivePath);
    reader.open();

    EXPECT_EQ(reader.originalFilename(), inputPath.filename().string());
    EXPECT_TRUE(format::isPreserveOrder(reader.globalHeader().flags));
    EXPECT_FALSE(reader.hasReorderMap());
}

TEST(ArchiveRegressionTest, ParallelPairedCompressionHonorsExplicitBlockSize) {
    auto input1Path = tempFilePath(".R1.fastq");
    auto input2Path = tempFilePath(".R2.fastq");
    auto archivePath = tempFilePath(".fqc");
    constexpr std::size_t kBlockSize = 100;
    constexpr std::size_t kPairCount = 100;

    TempFileGuard input1Guard(input1Path);
    TempFileGuard input2Guard(input2Path);
    TempFileGuard archiveGuard(archivePath);

    auto [r1Records, r2Records] = makePairedShortReads(kPairCount);
    writeFastqFile(input1Path, r1Records);
    writeFastqFile(input2Path, r2Records);

    ASSERT_EQ(compressPairedArchive(input1Path, input2Path, archivePath, 4, kBlockSize), 0);

    format::FQCReader reader(archivePath);
    reader.open();

    ASSERT_EQ(reader.blockCount(), 2u);
    EXPECT_EQ(reader.getBlockReadRange(0), (std::pair<ReadId, ReadId>{1, 101}));
    EXPECT_EQ(reader.getBlockReadRange(1), (std::pair<ReadId, ReadId>{101, 201}));
}

TEST(ArchiveRegressionTest, SingleThreadLongReadArchiveReportsZstdCodec) {
    auto inputPath = tempFilePath(".fastq");
    auto archivePath = tempFilePath(".fqc");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard archiveGuard(archivePath);

    writeFastqFile(inputPath, makeVariableLongReads());
    ASSERT_EQ(compressArchive(inputPath, archivePath, 1, false, false, ReadLengthClass::kLong), 0);

    format::FQCReader reader(archivePath);
    reader.open();

    EXPECT_EQ(static_cast<CodecFamily>(reader.globalHeader().compressionAlgo),
              CodecFamily::kZstdPlain);
}

TEST(ArchiveRegressionTest, ParallelLongReadRoundTripHandlesVariableLengths) {
    auto inputPath = tempFilePath(".fastq");
    auto archivePath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard archiveGuard(archivePath);
    TempFileGuard outputGuard(outputPath);

    auto records = makeVariableLongReads();
    writeFastqFile(inputPath, records);

    ASSERT_EQ(compressArchive(inputPath, archivePath, 4, false, false, ReadLengthClass::kLong), 0);
    ASSERT_EQ(decompressArchive(archivePath, outputPath, 1), 0);

    EXPECT_EQ(readFastqFile(outputPath), records);
}

TEST(ArchiveRegressionTest, ParallelLongReadRoundTripHandlesNonMonotonicVariableLengths) {
    auto inputPath = tempFilePath(".fastq");
    auto archivePath = tempFilePath(".fqc");
    auto outputPath = tempFilePath(".out.fastq");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard archiveGuard(archivePath);
    TempFileGuard outputGuard(outputPath);

    auto records = makeNonMonotonicVariableLongReads();
    writeFastqFile(inputPath, records);

    ASSERT_EQ(compressArchive(inputPath, archivePath, 4, false, false, ReadLengthClass::kLong), 0);
    ASSERT_EQ(decompressArchive(archivePath, outputPath, 1), 0);

    EXPECT_EQ(readFastqFile(outputPath), records);
}

TEST(ArchiveRegressionTest, SingleThreadSplitPeRangePairsWritesBothOutputsAndHonorsRange) {
    auto input1Path = tempFilePath(".R1.fastq");
    auto input2Path = tempFilePath(".R2.fastq");
    auto archivePath = tempFilePath(".fqc");
    auto requestedOutputPath = tempFilePath(".fastq");
    auto derivedR1Path = requestedOutputPath.parent_path() /
        (requestedOutputPath.stem().string() + "_R1" + requestedOutputPath.extension().string());
    auto derivedR2Path = requestedOutputPath.parent_path() /
        (requestedOutputPath.stem().string() + "_R2" + requestedOutputPath.extension().string());

    TempFileGuard input1Guard(input1Path);
    TempFileGuard input2Guard(input2Path);
    TempFileGuard archiveGuard(archivePath);
    TempFileGuard requestedOutputGuard(requestedOutputPath);
    TempFileGuard derivedR1Guard(derivedR1Path);
    TempFileGuard derivedR2Guard(derivedR2Path);

    auto [r1Records, r2Records] = makePairedShortReads(6);
    writeFastqFile(input1Path, r1Records);
    writeFastqFile(input2Path, r2Records);

    ASSERT_EQ(compressPairedArchive(input1Path, input2Path, archivePath, 1, 100), 0);

    DecompressOptions opts;
    opts.inputPath = archivePath;
    opts.outputPath = requestedOutputPath;
    opts.splitPairedEnd = true;
    opts.rangePairs = ReadRange{1, 2};
    opts.threads = 1;
    opts.showProgress = false;
    opts.forceOverwrite = true;

    DecompressCommand command(std::move(opts));
    ASSERT_EQ(command.execute(), 0);

    ASSERT_TRUE(std::filesystem::exists(derivedR1Path));
    ASSERT_TRUE(std::filesystem::exists(derivedR2Path));

    std::vector<FastqRecordSpec> expectedR1{r1Records.begin(), r1Records.begin() + 2};
    std::vector<FastqRecordSpec> expectedR2{r2Records.begin(), r2Records.begin() + 2};

    EXPECT_EQ(readFastqFile(derivedR1Path), expectedR1);
    EXPECT_EQ(readFastqFile(derivedR2Path), expectedR2);
}

TEST(ArchiveRegressionTest, VerifyDetectsTamperedBlockPayload) {
    auto inputPath = tempFilePath(".fastq");
    auto archivePath = tempFilePath(".fqc");

    TempFileGuard inputGuard(inputPath);
    TempFileGuard archiveGuard(archivePath);

    auto records = makeShortReadsNoComments(100);
    writeFastqFile(inputPath, records);
    ASSERT_EQ(compressArchive(inputPath, archivePath, 1, true, false, ReadLengthClass::kShort), 0);

    std::ifstream inFile(archivePath, std::ios::binary);
    ASSERT_TRUE(inFile.is_open());
    std::vector<char> archiveData((std::istreambuf_iterator<char>(inFile)),
                                  std::istreambuf_iterator<char>());
    inFile.close();

    ASSERT_GT(archiveData.size(), 200u);
    std::size_t tamperOffset = 100;
    archiveData[tamperOffset] ^= 0xFF;

    std::ofstream outFile(archivePath, std::ios::binary);
    ASSERT_TRUE(outFile.is_open());
    outFile.write(archiveData.data(), static_cast<std::streamsize>(archiveData.size()));
    outFile.close();

    VerifyOptions opts;
    opts.inputPath = archivePath;
    opts.verifyBlocks = true;
    opts.failFast = true;
    opts.verbose = false;

    VerifyCommand command(std::move(opts));
    EXPECT_NE(command.execute(), 0);
}

}  // namespace fqc::commands::test
