// =============================================================================
// fq-compressor - Pipeline Nodes Header Tests
// =============================================================================

#include "fqc/pipeline/pipeline_nodes.h"

#include <gtest/gtest.h>

namespace fqc::pipeline::test {

TEST(PipelineNodesHeaderTest, PipelineNodesCompatibilityHeaderCompilesInIsolation) {
#ifndef FQC_PIPELINE_NODES_IS_LEGACY_COMPAT_HEADER
    GTEST_FAIL() << "pipeline_nodes.h should advertise legacy compatibility status";
#endif

    ReaderNodeConfig readerConfig;
    WriterNodeConfig writerConfig;
    FQCReaderNodeConfig readerNodeConfig;
    DecompressorNodeConfig decompressorConfig;
    FASTQWriterNodeConfig fastqWriterConfig;

    EXPECT_EQ(readerConfig.readLengthClass, ReadLengthClass::kShort);
    EXPECT_TRUE(writerConfig.atomicWrite);
    EXPECT_TRUE(readerNodeConfig.verifyChecksums);
    EXPECT_FALSE(decompressorConfig.skipCorrupted);
    EXPECT_EQ(fastqWriterConfig.lineWidth, 0u);
}

}  // namespace fqc::pipeline::test
