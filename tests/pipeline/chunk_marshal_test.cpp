// =============================================================================
// fq-compressor - Chunk Marshal Tests
// =============================================================================

#include "fqc/common/types.h"
#include "fqc/pipeline/pipeline.h"

#include <gtest/gtest.h>

namespace fqc::pipeline::test {

TEST(ChunkMarshalTest, TracksUniformReads) {
    ReadChunk chunk;
    chunk.reads = {
        ReadRecord{"read1", "comment1", "ACGT", "IIII"},
        ReadRecord{"read2", "comment2", "TGCA", "JJJJ"},
    };

    const auto marshaled = marshalReadChunk(chunk);

    ASSERT_EQ(marshaled.readViews.size(), 2u);
    EXPECT_EQ(marshaled.uniformReadLength, 4u);
    EXPECT_TRUE(marshaled.lengths.empty());
    EXPECT_EQ(marshaled.ids[0], "read1");
    EXPECT_EQ(marshaled.comments[1], "comment2");
    EXPECT_EQ(marshaled.sequences[0], "ACGT");
    EXPECT_EQ(marshaled.qualities[1], "JJJJ");
}

TEST(ChunkMarshalTest, TracksVariableLengths) {
    ReadChunk chunk;
    chunk.reads = {
        ReadRecord{"read1", "comment1", "ACGT", "IIII"},
        ReadRecord{"read2", "comment2", "TGCATG", "JJJJJJ"},
    };

    const auto marshaled = marshalReadChunk(chunk);

    EXPECT_EQ(marshaled.uniformReadLength, 0u);
    ASSERT_EQ(marshaled.lengths.size(), 2u);
    EXPECT_EQ(marshaled.lengths[0], 4u);
    EXPECT_EQ(marshaled.lengths[1], 6u);
}

}  // namespace fqc::pipeline::test
