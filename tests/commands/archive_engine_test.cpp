#include "fqc/commands/archive_engine.h"

#include "fqc/io/fastq_parser.h"

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace fqc::commands::test {

namespace {

constexpr std::string_view kShortFastq =
    "@short_1 1:N:0:ACGT\nACGTNacgt\n+\n!#IJKLMNO\n"
    "@short_2 2:N:0:TGCA\nTGCARYSWK\n+\nJKLMNOPQR\n";

[[nodiscard]] auto makeFactory() -> std::shared_ptr<io::MemoryStreamFactory> {
    return std::make_shared<io::MemoryStreamFactory>();
}

}  // namespace

TEST(ArchiveEngineTest, CompressesAndDecompressesCanonicalFastq) {
    auto factory = makeFactory();
    factory->setFileContent("reads.fastq", kShortFastq);
    ArchiveEngine engine(factory);

    auto compressed = engine.compress({.inputPath = "reads.fastq",
                                       .matePath = {},
                                       .outputPath = "reads.fqc",
                                       .profile = format::DatasetProfile::kIllumina,
                                       .memoryLimitBytes = 64 * 1024 * 1024,
                                       .targetFrameBytes = 64,
                                       .forceOverwrite = true});
    ASSERT_TRUE(compressed) << compressed.error().message();
    EXPECT_EQ(compressed->recordCount, 2U);
    EXPECT_GE(compressed->frameCount, 1U);

    auto decompressed = engine.decompress({.inputPath = "reads.fqc",
                                           .outputPath = "restored.fastq",
                                           .memoryLimitBytes = 64 * 1024 * 1024,
                                           .forceOverwrite = true});
    ASSERT_TRUE(decompressed);
    EXPECT_EQ(factory->getFileContent("restored.fastq"), kShortFastq);

    auto verified = engine.verify("reads.fqc", 64 * 1024 * 1024);
    ASSERT_TRUE(verified);
    EXPECT_EQ(verified->recordCount, 2U);
}

TEST(ArchiveEngineTest, InterleavesPairedFilesAndKeepsPairsAtomic) {
    auto factory = makeFactory();
    factory->setFileContent("r1.fastq", "@pair/1\nACGT\n+\nIIII\n");
    factory->setFileContent("r2.fastq", "@pair/2\nTGCA\n+\nJJJJ\n");
    ArchiveEngine engine(factory);

    auto compressed = engine.compress({.inputPath = "r1.fastq",
                                       .matePath = "r2.fastq",
                                       .outputPath = "paired.fqc",
                                       .profile = format::DatasetProfile::kIllumina,
                                       .memoryLimitBytes = 64 * 1024 * 1024,
                                       .targetFrameBytes = 1,
                                       .forceOverwrite = true});
    ASSERT_TRUE(compressed);
    EXPECT_TRUE(compressed->paired);
    EXPECT_EQ(compressed->recordCount, 2U);

    auto decompressed = engine.decompress({.inputPath = "paired.fqc",
                                           .outputPath = "paired.fastq",
                                           .memoryLimitBytes = 64 * 1024 * 1024,
                                           .forceOverwrite = true});
    ASSERT_TRUE(decompressed);
    EXPECT_EQ(factory->getFileContent("paired.fastq"),
              "@pair/1\nACGT\n+\nIIII\n@pair/2\nTGCA\n+\nJJJJ\n");
}

TEST(ArchiveEngineTest, DetectsAllProfilesAndRejectsAmbiguousLongReads) {
    std::vector<ReadRecord> shortReads = {
        {"read", "", "ACGT", "IIII"},
    };
    auto shortProfile = detectProfile(shortReads);
    ASSERT_TRUE(shortProfile);
    EXPECT_EQ(*shortProfile, format::DatasetProfile::kIllumina);

    std::vector<ReadRecord> ontReads = {
        {"abc", "runid=123 ch=7", std::string(2'000, 'A'), std::string(2'000, 'I')},
    };
    ASSERT_EQ(detectProfile(ontReads).value(), format::DatasetProfile::kOnt);

    std::vector<ReadRecord> hifiReads = {
        {"m64011_220101_010101/42/ccs", "", std::string(2'000, 'A'), std::string(2'000, 'I')},
    };
    ASSERT_EQ(detectProfile(hifiReads).value(), format::DatasetProfile::kPacBioHiFi);

    std::vector<ReadRecord> clrReads = {
        {"m64011_220101_010101/42/0_2000", "", std::string(2'000, 'A'), std::string(2'000, 'I')},
    };
    ASSERT_EQ(detectProfile(clrReads).value(), format::DatasetProfile::kPacBioClr);

    std::vector<ReadRecord> ambiguous = {
        {"unknown", "", std::string(2'000, 'A'), std::string(2'000, 'I')},
    };
    auto ambiguousProfile = detectProfile(ambiguous);
    ASSERT_FALSE(ambiguousProfile);
    EXPECT_EQ(ambiguousProfile.error().code(), ErrorCode::kUsageError);
}

TEST(ArchiveEngineTest, RejectsPairedCountMismatchAndTinyMemoryLimit) {
    auto factory = makeFactory();
    factory->setFileContent("r1.fastq", "@a/1\nACGT\n+\nIIII\n@b/1\nACGT\n+\nIIII\n");
    factory->setFileContent("r2.fastq", "@a/2\nTGCA\n+\nJJJJ\n");
    ArchiveEngine engine(factory);

    auto mismatch = engine.compress({.inputPath = "r1.fastq",
                                     .matePath = "r2.fastq",
                                     .outputPath = "bad.fqc",
                                     .profile = format::DatasetProfile::kIllumina,
                                     .memoryLimitBytes = 64 * 1024 * 1024,
                                     .forceOverwrite = true});
    ASSERT_FALSE(mismatch);
    EXPECT_EQ(mismatch.error().code(), ErrorCode::kFormatError);

    auto tinyMemory = engine.compress({.inputPath = "r1.fastq",
                                       .matePath = {},
                                       .outputPath = "tiny.fqc",
                                       .profile = format::DatasetProfile::kIllumina,
                                       .memoryLimitBytes = 1024,
                                       .forceOverwrite = true});
    ASSERT_FALSE(tinyMemory);
    EXPECT_EQ(tinyMemory.error().code(), ErrorCode::kUsageError);
}

}  // namespace fqc::commands::test
