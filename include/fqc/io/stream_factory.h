// =============================================================================
// fq-compressor - Stream Factory Interface
// =============================================================================
// Abstract factory for creating input/output streams with dependency injection.
//
// This module provides:
// - StreamFactory: Interface for creating streams
// - FileStreamFactory: Production adapter using real filesystem
// - MemoryStreamFactory: Testing adapter using in-memory buffers
//
// Design Goals:
// - Enable dependency injection for I/O operations
// - Allow memory-in-place testing without temporary files
// - Support error injection for testing error handling
//
// Usage (Production):
//   auto factory = std::make_shared<FileStreamFactory>();
//   auto stream = factory->createInputStream("/path/to/file.gz");
//
// Usage (Testing):
//   auto factory = std::make_shared<MemoryStreamFactory>();
//   factory->setFileContent("test.gz", compressedData);
//   auto stream = factory->createInputStream("test.gz");
//
// Requirements: Architecture improvement - I/O layer dependency injection
// =============================================================================

#ifndef FQC_IO_STREAM_FACTORY_H
#define FQC_IO_STREAM_FACTORY_H

#include "fqc/common/error.h"
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

// =============================================================================
// StreamFactory Interface
// =============================================================================

/// @brief Abstract factory for creating input/output streams.
///
/// This interface defines a seam where I/O behavior can be altered
/// without editing call sites. Production code uses FileStreamFactory,
/// while tests can use MemoryStreamFactory for in-memory testing.
///
/// Thread Safety:
/// - Implementations must be thread-safe for concurrent create* calls
/// - Each created stream is independent and not thread-safe
class StreamFactory {
public:
    // =========================================================================
    // Types
    // =========================================================================

    /// @brief Error injection configuration for testing.
    struct ErrorConfig {
        /// @brief Whether to inject errors.
        bool injectError = false;

        /// @brief Error code to inject.
        ErrorCode errorCode = ErrorCode::kIOError;

        /// @brief Position in stream to inject error (0 = immediately).
        std::size_t errorPosition = 0;
    };

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /// @brief Virtual destructor.
    virtual ~StreamFactory() = default;

    // =========================================================================
    // Stream Creation
    // =========================================================================

    /// @brief Create an input stream with automatic compression detection.
    /// @param path File path (or "-" for stdin in FileStreamFactory).
    /// @return Input stream (may be decompressing).
    /// @throws IOError if file cannot be opened or error is injected.
    [[nodiscard]] virtual auto createInputStream(const std::filesystem::path& path)
        -> std::unique_ptr<std::istream> = 0;

    /// @brief Create an output stream.
    /// @param path File path.
    /// @param format Compression format (default: None).
    /// @return Output stream (may be compressing).
    /// @throws IOError if file cannot be created or error is injected.
    [[nodiscard]] virtual auto createOutputStream(
        const std::filesystem::path& path, CompressionFormat format = CompressionFormat::kNone)
        -> std::unique_ptr<std::ostream> = 0;

    // =========================================================================
    // Path Queries
    // =========================================================================

    /// @brief Check whether an output path already exists.
    /// @param path Output path to inspect.
    /// @return true if writing would overwrite existing content.
    [[nodiscard]] virtual auto outputExists(const std::filesystem::path& path) const -> bool = 0;

    // =========================================================================
    // Compression Detection
    // =========================================================================

    /// @brief Detect compression format from file path.
    /// @param path File path.
    /// @return Detected compression format.
    [[nodiscard]] virtual auto detectCompression(const std::filesystem::path& path) const
        -> CompressionFormat = 0;

    /// @brief Whether this factory is backed by the real filesystem.
    /// @return true for FileStreamFactory, false otherwise.
    /// @note Used to avoid dynamic_cast when choosing between filesystem
    ///       queries (e.g. std::filesystem::exists) and stream probing.
    [[nodiscard]] virtual auto isFileStream() const -> bool {
        return false;
    }

    // =========================================================================
    // Error Injection (Testing Support)
    // =========================================================================

    /// @brief Configure error injection for a path (testing only).
    /// @param path File path to inject error for.
    /// @param config Error configuration.
    /// @note Default implementation does nothing (production behavior).
    virtual auto injectError(const std::filesystem::path& path, ErrorConfig config) -> void {
        // Default: no-op for production factories
        (void)path;
        (void)config;
    }
};

// =============================================================================
// FileStreamFactory - Production Adapter
// =============================================================================

/// @brief Production stream factory using real filesystem.
///
/// This adapter creates real file streams with transparent compression
/// support. It uses CompressedInputStream for automatic decompression.
class FileStreamFactory : public StreamFactory {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /// @brief Default constructor.
    FileStreamFactory() = default;

    /// @brief Constructor with custom buffer size.
    /// @param bufferSize Buffer size for file I/O.
    explicit FileStreamFactory(std::size_t bufferSize);

    /// @brief Destructor.
    ~FileStreamFactory() override = default;

    // Non-copyable, movable
    FileStreamFactory(const FileStreamFactory&) = delete;
    FileStreamFactory& operator=(const FileStreamFactory&) = delete;
    FileStreamFactory(FileStreamFactory&&) noexcept = default;
    FileStreamFactory& operator=(FileStreamFactory&&) noexcept = default;

    // =========================================================================
    // StreamFactory Interface
    // =========================================================================

    /// @brief Create an input stream with automatic decompression.
    /// @param path File path (or "-" for stdin).
    /// @return Input stream (decompressing if needed).
    /// @throws IOError if file cannot be opened.
    [[nodiscard]] auto createInputStream(const std::filesystem::path& path)
        -> std::unique_ptr<std::istream> override;

    /// @brief Create an output stream.
    /// @param path File path.
    /// @param format Compression format (not yet supported for output).
    /// @return Output stream.
    /// @throws IOError if file cannot be created.
    /// @note Compression for output streams will be added in future.
    [[nodiscard]] auto createOutputStream(const std::filesystem::path& path,
                                          CompressionFormat format = CompressionFormat::kNone)
        -> std::unique_ptr<std::ostream> override;

    /// @brief Check whether an output path already exists.
    /// @param path Output path to inspect.
    /// @return true if the filesystem path exists.
    [[nodiscard]] auto outputExists(const std::filesystem::path& path) const -> bool override;

    /// @brief Detect compression from file extension.
    /// @param path File path.
    /// @return Detected compression format.
    [[nodiscard]] auto detectCompression(const std::filesystem::path& path) const
        -> CompressionFormat override;

    /// @brief FileStreamFactory is backed by the real filesystem.
    [[nodiscard]] auto isFileStream() const -> bool override {
        return true;
    }

private:
    /// @brief Buffer size for file I/O.
    std::size_t bufferSize_ = 64 * 1024;
};

// =============================================================================
// MemoryStreamFactory - Testing Adapter
// =============================================================================

/// @brief In-memory stream factory for testing.
///
/// This adapter stores file contents in memory, enabling tests without
/// temporary files. It supports:
/// - Pre-setting file contents for read tests
/// - Capturing written data for verification
/// - Error injection for testing error handling
///
/// Example:
/// @code
/// auto factory = std::make_shared<MemoryStreamFactory>();
///
/// // Setup input
/// factory->setFileContent("input.fastq", "@read1\nACGT\n+\nIIII\n");
///
/// // Create parser
/// FastqParser parser("input.fastq", factory);
///
/// // Test output
/// auto output = factory->createOutputStream("output.fqc");
/// output->write(data.data(), data.size());
///
/// // Verify output
/// auto written = factory->getFileContent("output.fqc");
/// EXPECT_EQ(written, expected);
/// @endcode
class MemoryStreamFactory : public StreamFactory {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /// @brief Default constructor.
    MemoryStreamFactory() = default;

    /// @brief Destructor.
    ~MemoryStreamFactory() override = default;

    // Non-copyable, non-movable (due to mutex)
    MemoryStreamFactory(const MemoryStreamFactory&) = delete;
    MemoryStreamFactory& operator=(const MemoryStreamFactory&) = delete;
    MemoryStreamFactory(MemoryStreamFactory&&) = delete;
    MemoryStreamFactory& operator=(MemoryStreamFactory&&) = delete;

    // =========================================================================
    // StreamFactory Interface
    // =========================================================================

    /// @brief Create an input stream from in-memory content.
    /// @param path File path (lookup key in memory store).
    /// @return Input stream reading from memory buffer.
    /// @throws IOError if path not found or error is injected.
    [[nodiscard]] auto createInputStream(const std::filesystem::path& path)
        -> std::unique_ptr<std::istream> override;

    /// @brief Create an output stream that writes to memory.
    /// @param path File path (key for later retrieval).
    /// @param format Compression format (ignored for memory streams).
    /// @return Output stream writing to memory buffer.
    [[nodiscard]] auto createOutputStream(const std::filesystem::path& path,
                                          CompressionFormat format = CompressionFormat::kNone)
        -> std::unique_ptr<std::ostream> override;

    /// @brief Check whether an output path already exists in memory.
    /// @param path Output path to inspect.
    /// @return true if the memory-backed file exists.
    [[nodiscard]] auto outputExists(const std::filesystem::path& path) const -> bool override;

    /// @brief Detect compression from extension.
    /// @param path File path.
    /// @return Detected compression format.
    [[nodiscard]] auto detectCompression(const std::filesystem::path& path) const
        -> CompressionFormat override;

    /// @brief Inject error for testing.
    /// @param path File path to inject error for.
    /// @param config Error configuration.
    auto injectError(const std::filesystem::path& path, ErrorConfig config) -> void override;

    // =========================================================================
    // Test Support Methods
    // =========================================================================

    /// @brief Set content for a file path.
    /// @param path File path to set content for.
    /// @param content File content (will be copied).
    auto setFileContent(const std::filesystem::path& path, std::string_view content) -> void;

    /// @brief Set content for a file path from binary data.
    /// @param path File path to set content for.
    /// @param data Binary data (will be copied).
    auto setFileContent(const std::filesystem::path& path, std::span<const std::uint8_t> data)
        -> void;

    /// @brief Get content written to a file path.
    /// @param path File path to get content from.
    /// @return File content (empty if not written to).
    [[nodiscard]] auto getFileContent(const std::filesystem::path& path) const -> std::string;

    /// @brief Get content as binary data.
    /// @param path File path to get content from.
    /// @return Binary data (empty if not written to).
    [[nodiscard]] auto getFileContentBinary(const std::filesystem::path& path) const
        -> std::vector<std::uint8_t>;

    /// @brief Check if a file exists in memory.
    /// @param path File path to check.
    /// @return true if file content exists.
    [[nodiscard]] auto hasFile(const std::filesystem::path& path) const -> bool;

    /// @brief Clear all stored file contents.
    auto clear() -> void;

    /// @brief Get list of all stored file paths.
    /// @return Vector of file paths.
    [[nodiscard]] auto listFiles() const -> std::vector<std::filesystem::path>;

private:
    friend class detail::MemoryOutputStream;

    auto persistOutput(const std::filesystem::path& path, std::string_view content) -> void;

    // =========================================================================
    // Internal Types
    // =========================================================================

    /// @brief In-memory file entry.
    struct FileEntry {
        /// @brief File content.
        std::vector<std::uint8_t> content;

        /// @brief Error configuration (if any).
        ErrorConfig errorConfig;

        /// @brief Whether error should be injected.
        bool hasError = false;
    };

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// @brief In-memory file store (path -> content).
    std::unordered_map<std::filesystem::path, FileEntry> files_;

    /// @brief Mutex for thread safety.
    mutable std::mutex mutex_;
};

// =============================================================================
// Global Factory Functions
// =============================================================================

/// @brief Get the default stream factory (thread-local).
/// @return Shared pointer to default factory.
/// @note Returns a FileStreamFactory by default.
[[nodiscard]] auto getDefaultStreamFactory() -> std::shared_ptr<StreamFactory>;

/// @brief Set the default stream factory (thread-local).
/// @param factory Factory to set as default.
auto setDefaultStreamFactory(std::shared_ptr<StreamFactory> factory) -> void;

}  // namespace fqc::io

#endif  // FQC_IO_STREAM_FACTORY_H
