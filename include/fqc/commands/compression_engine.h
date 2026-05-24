// =============================================================================
// fq-compressor - Compression Engine
// =============================================================================
// Transitional command-layer boundary for compression planning and execution.
// =============================================================================

#ifndef FQC_COMMANDS_COMPRESSION_ENGINE_H
#define FQC_COMMANDS_COMPRESSION_ENGINE_H

#include "fqc/commands/compression_profile.h"

namespace fqc::commands {

using CompressionPlan = CompressionProfilePlan;

class CompressionEngine {
public:
    explicit CompressionEngine(std::shared_ptr<io::StreamFactory> streamFactory =
                                   std::make_shared<io::FileStreamFactory>());

    [[nodiscard]] auto plan(const CompressionRequest& request) const -> Result<CompressionPlan>;
    [[nodiscard]] auto execute(const CompressionRequest& request) const -> Result<CompressionStats>;
    [[nodiscard]] auto execute(const CompressionPlan& plan) const -> Result<CompressionStats>;

private:
    std::shared_ptr<io::StreamFactory> streamFactory_;
};

}  // namespace fqc::commands

#endif  // FQC_COMMANDS_COMPRESSION_ENGINE_H
