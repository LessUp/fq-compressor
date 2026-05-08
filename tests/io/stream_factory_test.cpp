// =============================================================================
// fq-compressor - Stream Factory Tests
// =============================================================================

#include "fqc/io/stream_factory.h"

#include <gtest/gtest.h>

namespace fqc::io::test {

TEST(StreamFactoryTest, MemoryFactoryCapturesWrittenOutput) {
    MemoryStreamFactory factory;

    auto output = factory.createOutputStream("archive.fqc");
    ASSERT_NE(output, nullptr);

    *output << "fqc-data";
    output->flush();
    output.reset();

    EXPECT_EQ(factory.getFileContent("archive.fqc"), "fqc-data");
}

}  // namespace fqc::io::test
