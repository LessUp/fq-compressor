// =============================================================================
// fq-compressor - Injectable Stream Factories
// =============================================================================

#include "fqc/io/stream_factory.h"

#include "fqc/common/error.h"
#include "fqc/common/logger.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include <fmt/format.h>

namespace fqc::io {

namespace detail {

class MemoryOutputStream final : public std::ostringstream {
public:
    MemoryOutputStream(MemoryStreamFactory& factory, std::filesystem::path path)
        : factory_(factory), path_(std::move(path)) {}

    ~MemoryOutputStream() override {
        factory_.persistOutput(path_, str());
    }

private:
    MemoryStreamFactory& factory_;
    std::filesystem::path path_;
};

}  // namespace detail

auto FileStreamFactory::createInputStream(const std::filesystem::path& path)
    -> std::unique_ptr<std::istream> {
    FQC_LOG_DEBUG("opening input stream '{}'", path.string());
    return openInputFile(path);
}

auto FileStreamFactory::createOutputStream(const std::filesystem::path& path,
                                           CompressionFormat format)
    -> std::unique_ptr<std::ostream> {
    FQC_LOG_DEBUG("opening output stream '{}'", path.string());
    if (format != CompressionFormat::kNone && format != CompressionFormat::kUnknown) {
        throw FQCException(ErrorCode::kUsageError,
                           fmt::format("compressed output streams are unsupported: {}",
                                       compressionFormatName(format)));
    }
    if (path == "-") {
        return std::make_unique<std::ostream>(std::cout.rdbuf());
    }

    auto stream = std::make_unique<std::ofstream>(path, std::ios::binary);
    if (!stream->is_open()) {
        throw FQCException(ErrorCode::kIOError,
                           fmt::format("cannot create output file: {}", path.string()));
    }
    return stream;
}

auto FileStreamFactory::outputExists(const std::filesystem::path& path) const -> bool {
    return path != "-" && std::filesystem::exists(path);
}

auto FileStreamFactory::detectCompression(const std::filesystem::path& path) const
    -> CompressionFormat {
    return detectCompressionFormatFromExtension(path);
}

auto MemoryStreamFactory::createInputStream(const std::filesystem::path& path)
    -> std::unique_ptr<std::istream> {
    std::lock_guard lock(mutex_);
    const auto entry = files_.find(path);
    if (entry == files_.end()) {
        throw FQCException(ErrorCode::kIOError,
                           fmt::format("memory file not found: {}", path.string()));
    }
    std::string content(reinterpret_cast<const char*>(entry->second.data()), entry->second.size());
    return std::make_unique<std::istringstream>(std::move(content));
}

auto MemoryStreamFactory::createOutputStream(const std::filesystem::path& path,
                                             CompressionFormat format)
    -> std::unique_ptr<std::ostream> {
    (void)format;
    {
        std::lock_guard lock(mutex_);
        files_[path].clear();
    }
    return std::make_unique<detail::MemoryOutputStream>(*this, path);
}

auto MemoryStreamFactory::outputExists(const std::filesystem::path& path) const -> bool {
    return hasFile(path);
}

auto MemoryStreamFactory::detectCompression(const std::filesystem::path& path) const
    -> CompressionFormat {
    return detectCompressionFormatFromExtension(path);
}

auto MemoryStreamFactory::setFileContent(const std::filesystem::path& path,
                                         std::string_view content) -> void {
    setFileContent(path,
                   std::span<const std::uint8_t>(
                       reinterpret_cast<const std::uint8_t*>(content.data()), content.size()));
}

auto MemoryStreamFactory::setFileContent(const std::filesystem::path& path,
                                         std::span<const std::uint8_t> data) -> void {
    std::lock_guard lock(mutex_);
    files_[path].assign(data.begin(), data.end());
}

auto MemoryStreamFactory::getFileContent(const std::filesystem::path& path) const -> std::string {
    std::lock_guard lock(mutex_);
    const auto entry = files_.find(path);
    if (entry == files_.end()) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(entry->second.data()), entry->second.size());
}

auto MemoryStreamFactory::hasFile(const std::filesystem::path& path) const -> bool {
    std::lock_guard lock(mutex_);
    return files_.contains(path);
}

auto MemoryStreamFactory::persistOutput(const std::filesystem::path& path, std::string_view content)
    -> void {
    std::lock_guard lock(mutex_);
    files_[path].assign(content.begin(), content.end());
}

}  // namespace fqc::io
