// =============================================================================
// fq-compressor - FASTQ Parser Tests
// =============================================================================

#include "fqc/io/fastq_parser.h"

#include "fqc/common/types.h"

#include <type_traits>

#include <gtest/gtest.h>

namespace fqc::io::test {

TEST(FastqParserTest, FastqRecordUsesSharedReadRecordType) {
    EXPECT_TRUE((std::is_same_v<FastqRecord, ReadRecord>));
}

}  // namespace fqc::io::test
