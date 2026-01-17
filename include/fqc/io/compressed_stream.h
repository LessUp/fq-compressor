// =============================================================================
// fq-compressor - Compressed Stream Support
// =============================================================================
// Transparent decompression support for compressed input files.
//
// This module provides:
// - CompressedStream: Wrapper for transparent decompression
// - Automatic format detection (gzip, bzip2, xz)
// - Streaming decompression for memory efficiency
//
// Phase 1: gzip support (libdeflate/zlib)
// Phase 2: bzip2, xz support (planned)
//
// Usage:
//   auto stream = CompressedStream::open("/path/to/reads.fastq.gz");
//   // Use stream like any std::istream
//
// Requirements: 1.1.1
// =============================================================================

#ifndef FQC_IO_COMPRESSED_STREAM_H
#define FQC_IO_COMPRESSED_STREAM_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <streambuf>
#include <string>
#include <vector>

#include "fqc/common/error.h"

namespace fqc::io {

// =============================================================================
// Compression Format Detection
// =============================================================================

/// @brief Supported compression formats.
enum class CompressionFormat : std::uint8_t {
    kNone = 0,   ///< Uncompressed (plain text)
    kGzip = 1,   ///< gzip (.gz)
    kBzip2 = 2,  ///< bzip2 (.bz2) - Phase 2
    kXz = 3,     ///< xz/lzma (.xz) - Phase 2
    kZstd = 4,   ///< zstd (.zst) - Phase 2
    kUnknown = 255
};

/// @brief Detect compression format from file magic bytes.
/// @param data First few bytes of the file.
/// @return Detected compression format.
[[nodiscard]] CompressionFormat detectCompressionFormat(std::span<const std::uint8_t> data);

/// @brief Detect compression format from file extension.
/// @param path File path.
/// @return Detected compression format.
[[nodiscard]] CompressionFormat detectCompressionFormatFromExtension(
    const std::filesystem::path& path);

/// @brief Get file extension for compression format.
/// @param format Compression format.
/// @return File extension (e.g., ".gz").
[[nodiscard]] std::string_view compressionFormatExtension(CompressionFormat format);

/// @brief Get human-readable name for compression format.
/// @param format Compression format.
/// @return Format name (e.g., "gzip").
[[nodiscard]] std::string_view compressionFormatName(CompressionFormat format);

// =============================================================================
// GzipStreamBuf
// =============================================================================

/// @brief Stream buffer for gzip decompression.
/// @note Uses zlib for streaming decompression.
class GzipStreamBuf : public std::streambuf {
public:
    /// @brief Construct a gzip stream buffer.
    /// @param source Source stream to decompress.
    /// @param bufferSize Internal buffer size.
    explicit GzipStreamBuf(std::istream& source, std::size_t bufferSize = 64 * 1024);

    /// @brief Destructor.
    ~GzipStreamBuf() override;

    // Non-copyable
    GzipStreamBuf(const GzipStreamBuf&) = delete;
    GzipStreamBuf& operator=(const GzipStreamBuf&) = delete;

    // Movable
    GzipStreamBuf(GzipStreamBuf&&) noexcept;
    GzipStreamBuf& operator=(GzipStreamBuf&&) noexcept;

protected:
    /// @brief Underflow handler - refill buffer.
    int_type underflow() override;

private:
    /// @brief Initialize zlib stream.
    void initZlib();

    /// @brief Cleanup zlib stream.
    void cleanupZlib();

    /// @brief Decompress more data into output buffer.
    /// @return Number of bytes decompressed.
    std::size_t decompress();

    /// @brief Source stream.
    std::istream* source_ = nullptr;

    /// @brief Compressed input buffer.
    std::vector<std::uint8_t> inputBuffer_;

    /// @brief Decompressed output buffer.
    std::vector<char> outputBuffer_;

    /// @brief zlib stream state (opaque pointer).
    void* zlibStream_ = nullptr;

    /// @brief Whether stream is initialized.
    bool initialized_ = false;

    /// @brief Whether end of compressed stream reached.
    bool streamEnd_ = false;
};

// =============================================================================
// Bzip2StreamBuf
// =============================================================================

/// @brief Stream buffer for bzip2 decompression.
/// @note Uses libbz2 for streaming decompression.
class Bzip2StreamBuf : public std::streambuf {
public:
    /// @brief Construct a bzip2 stream buffer.
    /// @param source Source stream to decompress.
    /// @param bufferSize Internal buffer size.
    explicit Bzip2StreamBuf(std::istream& source, std::size_t bufferSize = 64 * 1024);

    /// @brief Destructor.
    ~Bzip2StreamBuf() override;

    // Non-copyable
    Bzip2StreamBuf(const Bzip2StreamBuf&) = delete;
    Bzip2StreamBuf& operator=(const Bzip2StreamBuf&) = delete;

    // Movable
    Bzip2StreamBuf(Bzip2StreamBuf&&) noexcept;
    Bzip2StreamBuf& operator=(Bzip2StreamBuf&&) noexcept;

protected:
    /// @brief Underflow handler - refill buffer.
    int_type underflow() override;

private:
    /// @brief Initialize bzip2 stream.
    void initBzip2();

    /// @brief Cleanup bzip2 stream.
    void cleanupBzip2();

    /// @brief Decompress more data into output buffer.
    /// @return Number of bytes decompressed.
    std::size_t decompress();

    /// @brief Source stream.
    std::istream* source_ = nullptr;

    /// @brief Compressed input buffer.
    std::vector<char> inputBuffer_;

    /// @brief Decompressed output buffer.
    std::vector<char> outputBuffer_;

    /// @brief bzip2 stream state (opaque pointer).
    void* bzStream_ = nullptr;

    /// @brief Whether stream is initialized.
    bool initialized_ = false;

    /// @brief Whether end of compressed stream reached.
    bool streamEnd_ = false;
};

// =============================================================================
// XzStreamBuf
// =============================================================================

/// @brief Stream buffer for xz/lzma decompression.
/// @note Uses liblzma for streaming decompression.
class XzStreamBuf : public std::streambuf {
public:
    /// @brief Construct an xz stream buffer.
    /// @param source Source stream to decompress.
    /// @param bufferSize Internal buffer size.
    explicit XzStreamBuf(std::istream& source, std::size_t bufferSize = 64 * 1024);

    /// @brief Destructor.
    ~XzStreamBuf() override;

    // Non-copyable
    XzStreamBuf(const XzStreamBuf&) = delete;
    XzStreamBuf& operator=(const XzStreamBuf&) = delete;

    // Movable
    XzStreamBuf(XzStreamBuf&&) noexcept;
    XzStreamBuf& operator=(XzStreamBuf&&) noexcept;

protected:
    /// @brief Underflow handler - refill buffer.
    int_type underflow() override;

private:
    /// @brief Initialize lzma stream.
    void initLzma();

    /// @brief Cleanup lzma stream.
    void cleanupLzma();

    /// @brief Decompress more data into output buffer.
    /// @return Number of bytes decompressed.
    std::size_t decompress();

    /// @brief Source stream.
    std::istream* source_ = nullptr;

    /// @brief Compressed input buffer.
    std::vector<std::uint8_t> inputBuffer_;

    /// @brief Decompressed output buffer.
    std::vector<char> outputBuffer_;

    /// @brief lzma stream state (opaque pointer).
    void* lzmaStream_ = nullptr;

    /// @brief Whether stream is initialized.
    bool initialized_ = false;

    /// @brief Whether end of compressed stream reached.
    bool streamEnd_ = false;
};

// =============================================================================
// CompressedInputStream
// =============================================================================

/// @brief Input stream with transparent decompression.
class CompressedInputStream : public std::istream {
public:
    /// @brief Construct from a file path.
    /// @param path File path.
    /// @throws IOError if file cannot be opened.
    explicit CompressedInputStream(const std::filesystem::path& path);

    /// @brief Construct from an existing stream.
    /// @param source Source stream.
    /// @param format Compression format (auto-detect if kUnknown).
    explicit CompressedInputStream(std::unique_ptr<std::istream> source,
                                   CompressionFormat format = CompressionFormat::kUnknown);

    /// @brief Destructor.
    ~CompressedInputStream() override;

    // Non-copyable, non-movable (std::istream is not movable)
    CompressedInputStream(const CompressedInputStream&) = delete;
    CompressedInputStream& operator=(const CompressedInputStream&) = delete;
    CompressedInputStream(CompressedInputStream&&) = delete;
    CompressedInputStream& operator=(CompressedInputStream&&) = delete;

    /// @brief Get the detected compression format.
    [[nodiscard]] CompressionFormat format() const noexcept { return format_; }

    /// @brief Check if the stream is compressed.
    [[nodiscard]] bool isCompressed() const noexcept { return format_ != CompressionFormat::kNone; }

private:
    /// @brief Detect format and setup decompression.
    void setup();

    /// @brief Source file stream (if opened from path).
    std::unique_ptr<std::ifstream> fileStream_;

    /// @brief Source stream.
    std::unique_ptr<std::istream> sourceStream_;

    /// @brief Decompression stream buffer.
    std::unique_ptr<std::streambuf> decompressBuf_;

    /// @brief Detected compression format.
    CompressionFormat format_ = CompressionFormat::kUnknown;
};

// =============================================================================
// Factory Functions
// =============================================================================

/// @brief Open a file with automatic decompression.
/// @param path File path.
/// @return Input stream (decompressing if needed).
/// @throws IOError if file cannot be opened.
[[nodiscard]] std::unique_ptr<std::istream> openCompressedFile(const std::filesystem::path& path);

/// @brief Open a file or stdin with automatic decompression.
/// @param path File path (or "-" for stdin).
/// @return Input stream (decompressing if needed).
/// @throws IOError if file cannot be opened.
[[nodiscard]] std::unique_ptr<std::istream> openInputFile(const std::filesystem::path& path);

// =============================================================================
// Compression Support Query
// =============================================================================

/// @brief Check if a compression format is supported.
/// @param format Compression format.
/// @return true if format is supported for decompression.
[[nodiscard]] bool isCompressionSupported(CompressionFormat format) noexcept;

/// @brief Get list of supported compression formats.
/// @return Vector of supported formats.
[[nodiscard]] std::vector<CompressionFormat> supportedCompressionFormats();

}  // namespace fqc::io

#endif  // FQC_IO_COMPRESSED_STREAM_H
