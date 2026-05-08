// =============================================================================
// fq-compressor - Stream Factory Implementation
// =============================================================================
// Implementation of StreamFactory, FileStreamFactory, and MemoryStreamFactory.
//
// Requirements: Architecture improvement - I/O layer dependency injection
// =============================================================================

#include "fqc/io/stream_factory.h"

#include "fqc/common/error.h"
#include "fqc/common/logger.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <fmt/format.h>

namespace fqc::io {

namespace detail {

class MemoryOutputStream : public std::ostringstream {
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

// =============================================================================
// FileStreamFactory Implementation
// =============================================================================

FileStreamFactory::FileStreamFactory(std::size_t bufferSize) : bufferSize_(bufferSize) {}

auto FileStreamFactory::createInputStream(const std::filesystem::path& path)
    -> std::unique_ptr<std::istream> {
    FQC_LOG_DEBUG("FileStreamFactory: Creating input stream for '{}'", path.string());

    // Use existing openInputFile which handles stdin and compression
    return openInputFile(path);
}

auto FileStreamFactory::createOutputStream(
    const std::filesystem::path& path, CompressionFormat format) -> std::unique_ptr<std::ostream> {
    FQC_LOG_DEBUG("FileStreamFactory: Creating output stream for '{}'", path.string());

    // For now, compression for output is not supported
    // Future work: add CompressedOutputStream
    if (format != CompressionFormat::kNone && format != CompressionFormat::kUnknown) {
        throw ArgumentError(fmt::format("Compression for output streams not yet supported: {}",
                                        compressionFormatName(format)));
    }

    auto stream = std::make_unique<std::ofstream>(path, std::ios::binary);
    if (!stream->is_open()) {
        throw IOError(fmt::format("Cannot create output file: {}", path.string()));
    }

    return stream;
}

auto FileStreamFactory::detectCompression(const std::filesystem::path& path) const
    -> CompressionFormat {
    return detectCompressionFormatFromExtension(path);
}

// =============================================================================
// MemoryStreamFactory Implementation
// =============================================================================

auto MemoryStreamFactory::createInputStream(const std::filesystem::path& path)
    -> std::unique_ptr<std::istream> {
    std::lock_guard<std::mutex> lock(mutex_);

    FQC_LOG_DEBUG("MemoryStreamFactory: Creating input stream for '{}'", path.string());

    auto it = files_.find(path);
    if (it == files_.end()) {
        throw IOError(fmt::format("Memory file not found: {}", path.string()));
    }

    // Check for error injection
    if (it->second.hasError) {
        const auto& config = it->second.errorConfig;
        throw IOError(fmt::format("Injected error for: {}", path.string()));
    }

    // Create string stream from content
    // Note: This copies the data, which is fine for testing
    std::string content(reinterpret_cast<const char*>(it->second.content.data()),
                        it->second.content.size());

    return std::make_unique<std::istringstream>(std::move(content));
}

auto MemoryStreamFactory::createOutputStream(
    const std::filesystem::path& path, CompressionFormat format) -> std::unique_ptr<std::ostream> {
    std::lock_guard<std::mutex> lock(mutex_);

    FQC_LOG_DEBUG("MemoryStreamFactory: Creating output stream for '{}'", path.string());

    // For memory streams, compression is ignored
    (void)format;

    // Create an output string stream
    // We'll capture its content when it's destroyed
    // Note: This is a simplified implementation
    // A more robust version would use a custom streambuf

    // Initialize empty entry if it doesn't exist
    files_[path] = FileEntry{};

    return std::make_unique<detail::MemoryOutputStream>(*this, path);
}

auto MemoryStreamFactory::detectCompression(const std::filesystem::path& path) const
    -> CompressionFormat {
    return detectCompressionFormatFromExtension(path);
}

auto MemoryStreamFactory::injectError(const std::filesystem::path& path,
                                      ErrorConfig config) -> void {
    std::lock_guard<std::mutex> lock(mutex_);

    FQC_LOG_DEBUG("MemoryStreamFactory: Injecting error for '{}'", path.string());

    auto& entry = files_[path];
    entry.errorConfig = config;
    entry.hasError = config.injectError;
}

auto MemoryStreamFactory::setFileContent(const std::filesystem::path& path,
                                         std::string_view content) -> void {
    setFileContent(path,
                   std::span<const std::uint8_t>(
                       reinterpret_cast<const std::uint8_t*>(content.data()), content.size()));
}

auto MemoryStreamFactory::setFileContent(const std::filesystem::path& path,
                                         std::span<const std::uint8_t> data) -> void {
    std::lock_guard<std::mutex> lock(mutex_);

    FQC_LOG_DEBUG(
        "MemoryStreamFactory: Setting content for '{}' ({} bytes)", path.string(), data.size());

    FileEntry entry;
    entry.content.assign(data.begin(), data.end());
    files_[path] = std::move(entry);
}

auto MemoryStreamFactory::getFileContent(const std::filesystem::path& path) const -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = files_.find(path);
    if (it == files_.end()) {
        return {};
    }

    return std::string(reinterpret_cast<const char*>(it->second.content.data()),
                       it->second.content.size());
}

auto MemoryStreamFactory::getFileContentBinary(const std::filesystem::path& path) const
    -> std::vector<std::uint8_t> {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = files_.find(path);
    if (it == files_.end()) {
        return {};
    }

    return it->second.content;
}

auto MemoryStreamFactory::hasFile(const std::filesystem::path& path) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return files_.find(path) != files_.end();
}

auto MemoryStreamFactory::clear() -> void {
    std::lock_guard<std::mutex> lock(mutex_);
    files_.clear();
}

auto MemoryStreamFactory::persistOutput(const std::filesystem::path& path,
                                        std::string_view content) -> void {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& entry = files_[path];
    entry.content.assign(content.begin(), content.end());
}

auto MemoryStreamFactory::listFiles() const -> std::vector<std::filesystem::path> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::filesystem::path> paths;
    paths.reserve(files_.size());

    for (const auto& [path, entry] : files_) {
        paths.push_back(path);
    }

    return paths;
}

// =============================================================================
// Global Factory Functions
// =============================================================================

namespace {

// Thread-local default factory
thread_local std::shared_ptr<StreamFactory> tlsDefaultFactory;

}  // anonymous namespace

auto getDefaultStreamFactory() -> std::shared_ptr<StreamFactory> {
    if (!tlsDefaultFactory) {
        tlsDefaultFactory = std::make_shared<FileStreamFactory>();
    }
    return tlsDefaultFactory;
}

auto setDefaultStreamFactory(std::shared_ptr<StreamFactory> factory) -> void {
    tlsDefaultFactory = std::move(factory);
}

}  // namespace fqc::io
