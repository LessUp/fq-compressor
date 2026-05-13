// =============================================================================
// fq-compressor - Pipeline Node Header Tests
// =============================================================================

#include "fqc/algo/block_compressor.h"
#include "fqc/pipeline/decompressor_node.h"
#include "fqc/pipeline/fastq_writer_node.h"
#include "fqc/pipeline/fqc_reader_node.h"
#include "fqc/pipeline/pipeline_nodes.h"
#include "fqc/pipeline/reader_node.h"
#include "fqc/pipeline/writer_node.h"

#include <gtest/gtest.h>

namespace fqc::pipeline::test {

TEST(PipelineNodeHeadersTest, SplitHeadersProvideNodeTypes) {
    ReaderNodeConfig readerConfig;
    algo::BlockCompressorConfig compressorConfig;
    WriterNodeConfig writerConfig;
    FQCReaderNodeConfig readerNodeConfig;
    DecompressorNodeConfig decompressorConfig;
    FASTQWriterNodeConfig fastqWriterConfig;

    EXPECT_EQ(readerConfig.readLengthClass, ReadLengthClass::kShort);
    EXPECT_EQ(compressorConfig.idMode, IDMode::kExact);
    EXPECT_TRUE(writerConfig.atomicWrite);
    EXPECT_TRUE(readerNodeConfig.verifyChecksums);
    EXPECT_FALSE(decompressorConfig.skipCorrupted);
    EXPECT_EQ(fastqWriterConfig.lineWidth, 0u);
}

}  // namespace fqc::pipeline::test
