// =============================================================================
// fq-compressor - V2 Sequential Archive Engine
// =============================================================================

#ifndef FQC_COMMANDS_V2_ARCHIVE_ENGINE_H
#define FQC_COMMANDS_V2_ARCHIVE_ENGINE_H

#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/format/v2_archive.h"
#include "fqc/io/stream_factory.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

namespace fqc::commands::v2 {

inline constexpr std::size_t kDefaultMemoryLimitBytes = format::v2::kDefaultMemoryLimitBytes;
inline constexpr std::size_t kProfileSampleMaxRecords = 50'000;
inline constexpr std::size_t kProfileSampleMaxBases = 512ULL * 1024ULL * 1024ULL;

struct CompressionRequest {
    std::filesystem::path inputPath;
    std::filesystem::path matePath;
    std::filesystem::path outputPath;
    std::optional<format::v2::DatasetProfile> profile;
    std::size_t memoryLimitBytes = kDefaultMemoryLimitBytes;
    std::size_t targetFrameBytes = format::v2::kDefaultTargetFrameBytes;
    bool forceOverwrite = false;

    [[nodiscard]] auto paired() const noexcept -> bool {
        return !matePath.empty();
    }
};

struct DecompressionRequest {
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    std::size_t memoryLimitBytes = kDefaultMemoryLimitBytes;
    bool forceOverwrite = false;
};

struct OperationStats {
    format::v2::DatasetProfile profile = format::v2::DatasetProfile::kIllumina;
    bool paired = false;
    std::uint64_t frameCount = 0;
    std::uint64_t recordCount = 0;
    std::uint64_t totalBases = 0;
    std::uint64_t inputBytes = 0;
    std::uint64_t outputBytes = 0;
};

[[nodiscard]] auto detectProfile(std::span<const ReadRecord> records)
    -> Result<format::v2::DatasetProfile>;

class ArchiveEngine {
public:
    explicit ArchiveEngine(std::shared_ptr<io::StreamFactory> streamFactory =
                               std::make_shared<io::FileStreamFactory>());

    [[nodiscard]] auto compress(const CompressionRequest& request) const -> Result<OperationStats>;
    [[nodiscard]] auto decompress(const DecompressionRequest& request) const
        -> Result<OperationStats>;
    [[nodiscard]] auto verify(const std::filesystem::path& inputPath,
                              std::size_t memoryLimitBytes = kDefaultMemoryLimitBytes) const
        -> Result<OperationStats>;

private:
    std::shared_ptr<io::StreamFactory> streamFactory_;
};

}  // namespace fqc::commands::v2

#endif  // FQC_COMMANDS_V2_ARCHIVE_ENGINE_H
