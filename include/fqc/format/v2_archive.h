// =============================================================================
// fq-compressor - FQC v2 Sequential Archive
// =============================================================================

#ifndef FQC_FORMAT_V2_ARCHIVE_H
#define FQC_FORMAT_V2_ARCHIVE_H

#include "fqc/common/error.h"
#include "fqc/common/types.h"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace fqc::format::v2 {

inline constexpr std::uint16_t kArchiveVersion = 2;
inline constexpr std::size_t kDefaultTargetFrameBytes = std::size_t{64} * 1024 * 1024;
inline constexpr std::size_t kDefaultMaxFrameBytes = std::size_t{512} * 1024 * 1024;
inline constexpr std::size_t kDefaultMemoryLimitBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;

enum class DatasetProfile : std::uint8_t {
    kIllumina = 1,
    kOnt = 2,
    kPacBioHiFi = 3,
    kPacBioClr = 4,
};

[[nodiscard]] constexpr auto profileToString(DatasetProfile profile) noexcept -> std::string_view {
    switch (profile) {
        case DatasetProfile::kIllumina:
            return "illumina";
        case DatasetProfile::kOnt:
            return "ont";
        case DatasetProfile::kPacBioHiFi:
            return "pacbio-hifi";
        case DatasetProfile::kPacBioClr:
            return "pacbio-clr";
    }
    return "unknown";
}

[[nodiscard]] auto parseProfile(std::string_view value) -> Result<DatasetProfile>;

struct ArchiveOptions {
    DatasetProfile profile = DatasetProfile::kIllumina;
    bool paired = false;
    std::size_t maxFrameBytes = kDefaultMaxFrameBytes;
    std::size_t memoryLimitBytes = kDefaultMemoryLimitBytes;
};

struct ArchiveMetadata {
    std::uint16_t version = kArchiveVersion;
    DatasetProfile profile = DatasetProfile::kIllumina;
    bool paired = false;
};

struct ArchiveStats {
    std::uint64_t frameCount = 0;
    std::uint64_t recordCount = 0;
    std::uint64_t totalBases = 0;
    std::uint64_t encodedBytes = 0;
};

class ArchiveWriter {
public:
    ArchiveWriter(std::ostream& output, ArchiveOptions options);
    ~ArchiveWriter();

    ArchiveWriter(const ArchiveWriter&) = delete;
    ArchiveWriter& operator=(const ArchiveWriter&) = delete;
    ArchiveWriter(ArchiveWriter&&) noexcept;
    ArchiveWriter& operator=(ArchiveWriter&&) noexcept;

    [[nodiscard]] auto writeFrame(std::span<const ReadRecord> records) -> VoidResult;
    [[nodiscard]] auto finish() -> VoidResult;

    [[nodiscard]] auto metadata() const noexcept -> const ArchiveMetadata&;
    [[nodiscard]] auto stats() const noexcept -> const ArchiveStats&;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class ArchiveReader {
public:
    explicit ArchiveReader(std::istream& input,
                           std::size_t maxFrameBytes = kDefaultMaxFrameBytes,
                           std::size_t memoryLimitBytes = kDefaultMemoryLimitBytes);
    ~ArchiveReader();

    ArchiveReader(const ArchiveReader&) = delete;
    ArchiveReader& operator=(const ArchiveReader&) = delete;
    ArchiveReader(ArchiveReader&&) noexcept;
    ArchiveReader& operator=(ArchiveReader&&) noexcept;

    [[nodiscard]] auto open() -> Result<ArchiveMetadata>;
    [[nodiscard]] auto readFrame() -> Result<std::optional<std::vector<ReadRecord>>>;

    [[nodiscard]] auto metadata() const noexcept -> const ArchiveMetadata&;
    [[nodiscard]] auto stats() const noexcept -> const ArchiveStats&;
    [[nodiscard]] auto finished() const noexcept -> bool;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace fqc::format::v2

#endif  // FQC_FORMAT_V2_ARCHIVE_H
