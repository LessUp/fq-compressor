#pragma once

#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/format/archive.h"

#include <cstddef>
#include <istream>
#include <vector>

namespace fqc::pipeline {

struct PipelineStats {
    std::size_t frameCount = 0;
    std::size_t recordCount = 0;
};

class CompressPipeline {
public:
    static constexpr std::size_t kDefaultQueueDepth = 4;

    explicit CompressPipeline(std::size_t targetFrameBytes, bool paired = false);

    [[nodiscard]] auto run(std::istream& input, format::ArchiveWriter& writer)
        -> Result<PipelineStats>;

private:
    std::size_t targetFrameBytes_;
    bool paired_;
};

}  // namespace fqc::pipeline
