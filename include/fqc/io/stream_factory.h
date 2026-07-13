// =============================================================================
// fq-compressor - Injectable Stream Factories
// =============================================================================

#ifndef FQC_IO_STREAM_FACTORY_H
#define FQC_IO_STREAM_FACTORY_H

#include "fqc/io/compressed_stream.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fqc::io {

namespace detail {
class MemoryOutputStream;
}

class StreamFactory {
public:
    virtual ~StreamFactory() = default;

    [[nodiscard]] virtual auto createInputStream(const std::filesystem::path& path)
        -> std::unique_ptr<std::istream> = 0;

    [[nodiscard]] virtual auto createOutputStream(
        const std::filesystem::path& path, CompressionFormat format = CompressionFormat::kNone)
        -> std::unique_ptr<std::ostream> = 0;

    [[nodiscard]] virtual auto outputExists(const std::filesystem::path& path) const -> bool = 0;

    [[nodiscard]] virtual auto detectCompression(const std::filesystem::path& path) const
        -> CompressionFormat = 0;

    [[nodiscard]] virtual auto isFileStream() const noexcept -> bool {
        return false;
    }
};

class FileStreamFactory final : public StreamFactory {
public:
    [[nodiscard]] auto createInputStream(const std::filesystem::path& path)
        -> std::unique_ptr<std::istream> override;

    [[nodiscard]] auto createOutputStream(const std::filesystem::path& path,
                                          CompressionFormat format = CompressionFormat::kNone)
        -> std::unique_ptr<std::ostream> override;

    [[nodiscard]] auto outputExists(const std::filesystem::path& path) const -> bool override;

    [[nodiscard]] auto detectCompression(const std::filesystem::path& path) const
        -> CompressionFormat override;

    [[nodiscard]] auto isFileStream() const noexcept -> bool override {
        return true;
    }
};

class MemoryStreamFactory final : public StreamFactory {
public:
    MemoryStreamFactory() = default;
    MemoryStreamFactory(const MemoryStreamFactory&) = delete;
    auto operator=(const MemoryStreamFactory&) -> MemoryStreamFactory& = delete;

    [[nodiscard]] auto createInputStream(const std::filesystem::path& path)
        -> std::unique_ptr<std::istream> override;

    [[nodiscard]] auto createOutputStream(const std::filesystem::path& path,
                                          CompressionFormat format = CompressionFormat::kNone)
        -> std::unique_ptr<std::ostream> override;

    [[nodiscard]] auto outputExists(const std::filesystem::path& path) const -> bool override;

    [[nodiscard]] auto detectCompression(const std::filesystem::path& path) const
        -> CompressionFormat override;

    auto setFileContent(const std::filesystem::path& path, std::string_view content) -> void;
    auto setFileContent(const std::filesystem::path& path, std::span<const std::uint8_t> data)
        -> void;

    [[nodiscard]] auto getFileContent(const std::filesystem::path& path) const -> std::string;
    [[nodiscard]] auto hasFile(const std::filesystem::path& path) const -> bool;

private:
    friend class detail::MemoryOutputStream;

    auto persistOutput(const std::filesystem::path& path, std::string_view content) -> void;

    std::unordered_map<std::filesystem::path, std::vector<std::uint8_t>> files_;
    mutable std::mutex mutex_;
};

}  // namespace fqc::io

#endif  // FQC_IO_STREAM_FACTORY_H
