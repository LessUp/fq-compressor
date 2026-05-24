// =============================================================================
// fq-compressor - Pipeline Node Header Tests
// =============================================================================

#include "fqc/pipeline/pipeline.h"
#include "fqc/pipeline/pipeline_node.h"
#include "fqc/pipeline/pipeline_nodes.h"

#include <gtest/gtest.h>

namespace fqc::pipeline::test {

TEST(PipelineNodeHeadersTest, CompatibilityHeadersCompileWithPipelineSurface) {
#ifndef FQC_PIPELINE_NODE_IS_LEGACY_COMPAT_HEADER
    GTEST_FAIL() << "pipeline_node.h should advertise legacy compatibility status";
#endif
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
