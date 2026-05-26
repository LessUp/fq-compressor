// =============================================================================
// fq-compressor - FASTQ Parser Tests
// =============================================================================

#include "fqc/io/fastq_parser.h"

#include "fqc/common/types.h"
#include "fqc/io/stream_factory.h"

#include <type_traits>

#include <gtest/gtest.h>

namespace fqc::io::test {

TEST(FastqParserTest, FastqRecordUsesSharedReadRecordType) {
    EXPECT_TRUE((std::is_same_v<FastqRecord, ReadRecord>));
}

TEST(FastqParserTest, LastErrorUsesOneBasedRecordNumberForFailingRecord) {
    auto factory = std::make_shared<MemoryStreamFactory>();
    factory->setFileContent("broken.fastq", "@read1\nACGT\n-\nIIII\n");

    FastqParser parser("broken.fastq", factory);
    parser.open();

    EXPECT_THROW(parser.readRecord(), FormatError);
    ASSERT_TRUE(parser.lastError().has_value());
    EXPECT_EQ(parser.lastError()->lineNumber, 3u);
    EXPECT_EQ(parser.lastError()->recordNumber, 1u);
}

TEST(FastqParserTest, SampleRecordsPreservesCurrentReadPosition) {
    auto factory = std::make_shared<MemoryStreamFactory>();
    factory->setFileContent("reads.fastq",
                            "@read1\nACGT\n+\nFFFF\n"
                            "@read2\nTGCA\n+\nHHHH\n");

    FastqParser parser("reads.fastq", factory);
    parser.open();

    auto first = parser.readRecord();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->id, "read1");

    const auto sample = parser.sampleRecords(1);
    EXPECT_EQ(sample.totalRecords, 1u);

    auto next = parser.readRecord();
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->id, "read2");
}

TEST(FastqParserTest, TrailingSpacesInSequenceAndQualityRemainFormatErrors) {
    auto factory = std::make_shared<MemoryStreamFactory>();
    factory->setFileContent("broken.fastq", "@read1\nACGT \n+\nFFFF \n");

    FastqParser parser("broken.fastq", factory);
    parser.open();

    EXPECT_THROW(parser.readRecord(), FormatError);
}

}  // namespace fqc::io::test
