// =============================================================================
// fq-compressor - Compression Profile
// =============================================================================
// Compression planning seam for FASTQ compression.
// =============================================================================

#ifndef FQC_COMMANDS_COMPRESSION_PROFILE_H
#define FQC_COMMANDS_COMPRESSION_PROFILE_H

#include "fqc/algo/block_compressor.h"
#include "fqc/algo/global_analyzer.h"
#include "fqc/commands/compress_command.h"
#include "fqc/commands/compression_request.h"
#include "fqc/common/error.h"
#include "fqc/io/stream_factory.h"
#include "fqc/pipeline/pipeline.h"

#include <cstdint>
#include <memory>

namespace fqc::commands {

struct CompressOptions;

enum class CompressionExecutionMode : std::uint8_t { kSingleThread = 0, kPipeline = 1 };

class CompressionProfile {
public:
    ~CompressionProfile();

    CompressionProfile(const CompressionProfile&);
    CompressionProfile(CompressionProfile&&) noexcept;
    auto operator=(const CompressionProfile&) -> CompressionProfile&;
    auto operator=(CompressionProfile&&) noexcept -> CompressionProfile&;

    [[nodiscard]] const CompressOptions& effectiveOptions() const noexcept;
    [[nodiscard]] ReadLengthClass readLengthClass() const noexcept;
    [[nodiscard]] bool streamingMode() const noexcept;
    [[nodiscard]] bool enableReordering() const noexcept;
    [[nodiscard]] bool saveReorderMap() const noexcept;
    [[nodiscard]] bool archivePreservesOrder() const noexcept;
    [[nodiscard]] bool archiveHasReorderMap() const noexcept;
    [[nodiscard]] CompressionExecutionMode executionMode() const noexcept;
    [[nodiscard]] const algo::GlobalAnalyzerConfig& globalAnalyzerConfig() const noexcept;
    [[nodiscard]] const algo::BlockCompressorConfig& blockCompressorConfig() const noexcept;
    [[nodiscard]] const pipeline::CompressionPipelineConfig& pipelineConfig() const noexcept;

private:
    struct Impl;

    explicit CompressionProfile(std::shared_ptr<const Impl> impl);

    std::shared_ptr<const Impl> impl_;

    friend auto buildCompressionProfile(CompressOptions options,
                                        std::shared_ptr<io::StreamFactory> streamFactory)
        -> Result<CompressionProfile>;
};

struct CompressionProfilePlan {
    CompressionRequest request;
    CompressOptions effectiveOptions;
    CompressionProfile profile;
};

[[nodiscard]] auto buildCompressionProfile(
    CompressOptions options,
    std::shared_ptr<io::StreamFactory> streamFactory = std::make_shared<io::FileStreamFactory>())
    -> Result<CompressionProfile>;

[[nodiscard]] auto buildCompressionProfilePlan(
    const CompressionRequest& request,
    std::shared_ptr<io::StreamFactory> streamFactory = std::make_shared<io::FileStreamFactory>())
    -> Result<CompressionProfilePlan>;

}  // namespace fqc::commands

#endif  // FQC_COMMANDS_COMPRESSION_PROFILE_H
