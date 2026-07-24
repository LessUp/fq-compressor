// =============================================================================
// fq-compressor - Concurrent Compression Pipeline
// =============================================================================

#ifndef FQC_PIPELINE_COMPRESS_PIPELINE_H
#define FQC_PIPELINE_COMPRESS_PIPELINE_H

#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/format/archive.h"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>

namespace fqc::pipeline {

struct PipelineStats {
    std::size_t frameCount = 0;
    std::size_t recordCount = 0;
    std::uint64_t logicalBytes = 0;
};

/// Two-stage concurrent pipeline: a reader thread parses FASTQ and accumulates
/// bounded frames, a writer thread encodes and writes each frame. The stages
/// are decoupled by a bounded SPSC queue, so parsing overlaps CPU-bound
/// encoding.
class CompressPipeline {
public:
    static constexpr std::size_t kDefaultQueueDepth = 4;

    explicit CompressPipeline(std::size_t targetFrameBytes, bool paired = false);

    /// Run the pipeline. `initialRecords` are emitted first (e.g. a profile
    /// sample already consumed by the caller), then records are streamed from
    /// `primary` (and `mate` when paired). If the writer fails, the queue is
    /// aborted so the reader does not deadlock on a full push.
    [[nodiscard]] auto run(std::istream& primary,
                           std::istream* mate,
                           std::span<const ReadRecord> initialRecords,
                           format::ArchiveWriter& writer) -> Result<PipelineStats>;

private:
    std::size_t targetFrameBytes_;
    bool paired_;
};

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_COMPRESS_PIPELINE_H
