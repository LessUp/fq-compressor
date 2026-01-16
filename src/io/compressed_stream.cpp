// =============================================================================
// fq-compressor - Compressed Stream Implementation
// =============================================================================
// Transparent decompression support implementation.
//
// Phase 1: gzip support using zlib
// Phase 2: bzip2, xz support (planned)
//
// Requirements: 1.1.1
// =============================================================================

#include "fqc/io/compressed_stream.h"

#include <zlib.h>
#include <bzlib.h>
#include <lzma.h>

#include <algorithm>
#include <cstring>
#include <iostream>

#include "fqc/common/logger.h"

namespace fqc::io {

// =============================================================================
// Magic Bytes for Format Detection
// =============================================================================

namespace {

// Gzip magic: 0x1f 0x8b
constexpr std::uint8_t kGzipMagic[] = {0x1f, 0x8b};

// Bzip2 magic: 'B' 'Z' 'h'
constexpr std::uint8_t kBzip2Magic[] = {0x42, 0x5a, 0x68};

// XZ magic: 0xfd '7' 'z' 'X' 'Z' 0x00
constexpr std::uint8_t kXzMagic[] = {0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00};

// Zstd magic: 0x28 0xb5 0x2f 0xfd
constexpr std::uint8_t kZstdMagic[] = {0x28, 0xb5, 0x2f, 0xfd};

}  // namespace

// =============================================================================
// Format Detection
// =============================================================================

CompressionFormat detectCompressionFormat(std::span<const std::uint8_t> data) {
    if (data.size() < 2) {
        return CompressionFormat::kNone;
    }

    // Check gzip (most common for FASTQ)
    if (data.size() >= sizeof(kGzipMagic) &&
        std::memcmp(data.data(), kGzipMagic, sizeof(kGzipMagic)) == 0) {
        return CompressionFormat::kGzip;
    }

    // Check bzip2
    if (data.size() >= sizeof(kBzip2Magic) &&
        std::memcmp(data.data(), kBzip2Magic, sizeof(kBzip2Magic)) == 0) {
        return CompressionFormat::kBzip2;
    }

    // Check xz
    if (data.size() >= sizeof(kXzMagic) &&
        std::memcmp(data.data(), kXzMagic, sizeof(kXzMagic)) == 0) {
        return CompressionFormat::kXz;
    }

    // Check zstd
    if (data.size() >= sizeof(kZstdMagic) &&
        std::memcmp(data.data(), kZstdMagic, sizeof(kZstdMagic)) == 0) {
        return CompressionFormat::kZstd;
    }

    // Assume uncompressed
    return CompressionFormat::kNone;
}

CompressionFormat detectCompressionFormatFromExtension(const std::filesystem::path& path) {
    auto ext = path.extension().string();

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (ext == ".gz" || ext == ".gzip") {
        return CompressionFormat::kGzip;
    }
    if (ext == ".bz2" || ext == ".bzip2") {
        return CompressionFormat::kBzip2;
    }
    if (ext == ".xz" || ext == ".lzma") {
        return CompressionFormat::kXz;
    }
    if (ext == ".zst" || ext == ".zstd") {
        return CompressionFormat::kZstd;
    }

    // Check for double extensions like .fastq.gz
    auto stem = path.stem();
    if (stem.has_extension()) {
        auto innerExt = stem.extension().string();
        std::transform(innerExt.begin(), innerExt.end(), innerExt.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (innerExt == ".fastq" || innerExt == ".fq") {
            // Already checked outer extension above
            return CompressionFormat::kNone;
        }
    }

    return CompressionFormat::kNone;
}

std::string_view compressionFormatExtension(CompressionFormat format) {
    switch (format) {
        case CompressionFormat::kGzip:
            return ".gz";
        case CompressionFormat::kBzip2:
            return ".bz2";
        case CompressionFormat::kXz:
            return ".xz";
        case CompressionFormat::kZstd:
            return ".zst";
        case CompressionFormat::kNone:
        case CompressionFormat::kUnknown:
        default:
            return "";
    }
}

std::string_view compressionFormatName(CompressionFormat format) {
    switch (format) {
        case CompressionFormat::kGzip:
            return "gzip";
        case CompressionFormat::kBzip2:
            return "bzip2";
        case CompressionFormat::kXz:
            return "xz";
        case CompressionFormat::kZstd:
            return "zstd";
        case CompressionFormat::kNone:
            return "none";
        case CompressionFormat::kUnknown:
        default:
            return "unknown";
    }
}

bool isCompressionSupported(CompressionFormat format) noexcept {
    switch (format) {
        case CompressionFormat::kNone:
        case CompressionFormat::kGzip:
        case CompressionFormat::kBzip2:
        case CompressionFormat::kXz:
            return true;
        case CompressionFormat::kZstd:
            return false;  // Not yet implemented
        default:
            return false;
    }
}

std::vector<CompressionFormat> supportedCompressionFormats() {
    return {CompressionFormat::kNone, CompressionFormat::kGzip,
            CompressionFormat::kBzip2, CompressionFormat::kXz};
}

// =============================================================================
// GzipStreamBuf Implementation
// =============================================================================

GzipStreamBuf::GzipStreamBuf(std::istream& source, std::size_t bufferSize)
    : source_(&source), inputBuffer_(bufferSize), outputBuffer_(bufferSize) {
    initZlib();
}

GzipStreamBuf::~GzipStreamBuf() { cleanupZlib(); }

GzipStreamBuf::GzipStreamBuf(GzipStreamBuf&& other) noexcept
    : std::streambuf(std::move(other)),
      source_(other.source_),
      inputBuffer_(std::move(other.inputBuffer_)),
      outputBuffer_(std::move(other.outputBuffer_)),
      zlibStream_(other.zlibStream_),
      initialized_(other.initialized_),
      streamEnd_(other.streamEnd_) {
    other.source_ = nullptr;
    other.zlibStream_ = nullptr;
    other.initialized_ = false;
}

GzipStreamBuf& GzipStreamBuf::operator=(GzipStreamBuf&& other) noexcept {
    if (this != &other) {
        cleanupZlib();

        source_ = other.source_;
        inputBuffer_ = std::move(other.inputBuffer_);
        outputBuffer_ = std::move(other.outputBuffer_);
        zlibStream_ = other.zlibStream_;
        initialized_ = other.initialized_;
        streamEnd_ = other.streamEnd_;

        other.source_ = nullptr;
        other.zlibStream_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

void GzipStreamBuf::initZlib() {
    auto* stream = new z_stream;
    std::memset(stream, 0, sizeof(z_stream));

    // Use inflateInit2 with 16+MAX_WBITS for gzip format
    int ret = inflateInit2(stream, 16 + MAX_WBITS);
    if (ret != Z_OK) {
        delete stream;
        throw IOError(ErrorCode::kDecompressionFailed, "Failed to initialize zlib: " +
                                                           std::string(zError(ret)));
    }

    zlibStream_ = stream;
    initialized_ = true;
}

void GzipStreamBuf::cleanupZlib() {
    if (zlibStream_) {
        auto* stream = static_cast<z_stream*>(zlibStream_);
        inflateEnd(stream);
        delete stream;
        zlibStream_ = nullptr;
    }
    initialized_ = false;
}

GzipStreamBuf::int_type GzipStreamBuf::underflow() {
    if (gptr() < egptr()) {
        return traits_type::to_int_type(*gptr());
    }

    if (streamEnd_) {
        return traits_type::eof();
    }

    std::size_t decompressed = decompress();
    if (decompressed == 0) {
        return traits_type::eof();
    }

    setg(outputBuffer_.data(), outputBuffer_.data(), outputBuffer_.data() + decompressed);
    return traits_type::to_int_type(*gptr());
}

std::size_t GzipStreamBuf::decompress() {
    if (!initialized_ || !source_) {
        return 0;
    }

    auto* stream = static_cast<z_stream*>(zlibStream_);

    // Read more compressed data if needed
    if (stream->avail_in == 0 && !source_->eof()) {
        source_->read(reinterpret_cast<char*>(inputBuffer_.data()),
                      static_cast<std::streamsize>(inputBuffer_.size()));
        auto bytesRead = static_cast<std::size_t>(source_->gcount());
        if (bytesRead > 0) {
            stream->avail_in = static_cast<uInt>(bytesRead);
            stream->next_in = inputBuffer_.data();
        }
    }

    // Setup output buffer
    stream->avail_out = static_cast<uInt>(outputBuffer_.size());
    stream->next_out = reinterpret_cast<Bytef*>(outputBuffer_.data());

    // Decompress
    int ret = inflate(stream, Z_NO_FLUSH);

    if (ret == Z_STREAM_END) {
        streamEnd_ = true;
    } else if (ret != Z_OK && ret != Z_BUF_ERROR) {
        throw IOError(ErrorCode::kDecompressionFailed,
                      "Gzip decompression failed: " + std::string(zError(ret)));
    }

    return outputBuffer_.size() - stream->avail_out;
}

// =============================================================================
// CompressedInputStream Implementation
// =============================================================================

CompressedInputStream::CompressedInputStream(const std::filesystem::path& path)
    : std::istream(nullptr) {
    // Open file
    fileStream_ = std::make_unique<std::ifstream>(path, std::ios::binary);
    if (!fileStream_->is_open()) {
        throw IOError(ErrorCode::kFileOpenFailed, "Failed to open file: " + path.string());
    }

    // Detect format from magic bytes
    std::uint8_t magic[8];
    fileStream_->read(reinterpret_cast<char*>(magic), sizeof(magic));
    auto bytesRead = static_cast<std::size_t>(fileStream_->gcount());

    // Seek back to beginning
    fileStream_->clear();
    fileStream_->seekg(0, std::ios::beg);

    format_ = detectCompressionFormat({magic, bytesRead});

    // Fallback to extension-based detection
    if (format_ == CompressionFormat::kNone && bytesRead < 2) {
        format_ = detectCompressionFormatFromExtension(path);
    }

    setup();
}

CompressedInputStream::CompressedInputStream(std::unique_ptr<std::istream> source,
                                             CompressionFormat format)
    : std::istream(nullptr), sourceStream_(std::move(source)), format_(format) {
    if (format_ == CompressionFormat::kUnknown && sourceStream_) {
        // Try to detect from magic bytes
        std::uint8_t magic[8];
        sourceStream_->read(reinterpret_cast<char*>(magic), sizeof(magic));
        auto bytesRead = static_cast<std::size_t>(sourceStream_->gcount());

        // Put bytes back (if possible)
        for (std::size_t i = bytesRead; i > 0; --i) {
            sourceStream_->putback(static_cast<char>(magic[i - 1]));
        }

        format_ = detectCompressionFormat({magic, bytesRead});
    }

    setup();
}

CompressedInputStream::~CompressedInputStream() = default;

CompressedInputStream::CompressedInputStream(CompressedInputStream&&) noexcept = default;
CompressedInputStream& CompressedInputStream::operator=(CompressedInputStream&&) noexcept = default;

void CompressedInputStream::setup() {
    std::istream* source = fileStream_ ? fileStream_.get() : sourceStream_.get();
    if (!source) {
        throw IOError(ErrorCode::kFileOpenFailed, "No source stream available");
    }

    switch (format_) {
        case CompressionFormat::kNone:
            // Use source directly
            rdbuf(source->rdbuf());
            break;

        case CompressionFormat::kGzip:
            decompressBuf_ = std::make_unique<GzipStreamBuf>(*source);
            rdbuf(decompressBuf_.get());
            LOG_DEBUG("Opened gzip compressed stream");
            break;

        case CompressionFormat::kBzip2:
        case CompressionFormat::kXz:
        case CompressionFormat::kZstd:
            throw IOError(ErrorCode::kUnsupportedFormat,
                          "Compression format not yet supported: " +
                              std::string(compressionFormatName(format_)));

        default:
            throw IOError(ErrorCode::kUnsupportedFormat, "Unknown compression format");
    }
}

// =============================================================================
// Factory Functions
// =============================================================================

std::unique_ptr<std::istream> openCompressedFile(const std::filesystem::path& path) {
    return std::make_unique<CompressedInputStream>(path);
}

std::unique_ptr<std::istream> openInputFile(const std::filesystem::path& path) {
    if (path == "-") {
        // stdin - check if compressed
        // Note: For stdin, we can't seek back, so we need to buffer
        LOG_DEBUG("Opening stdin for input");

        // Read magic bytes
        std::uint8_t magic[8];
        std::cin.read(reinterpret_cast<char*>(magic), sizeof(magic));
        auto bytesRead = static_cast<std::size_t>(std::cin.gcount());

        auto format = detectCompressionFormat({magic, bytesRead});

        if (format == CompressionFormat::kNone) {
            // Create a stream that prepends the magic bytes
            auto ss = std::make_unique<std::stringstream>();
            ss->write(reinterpret_cast<const char*>(magic), static_cast<std::streamsize>(bytesRead));

            // Copy rest of stdin
            *ss << std::cin.rdbuf();

            return ss;
        }

        // Compressed stdin - need to handle specially
        if (!isCompressionSupported(format)) {
            throw IOError(ErrorCode::kUnsupportedFormat,
                          "Compressed stdin not supported for format: " +
                              std::string(compressionFormatName(format)));
        }

        // Create a stream with the magic bytes prepended
        auto ss = std::make_unique<std::stringstream>();
        ss->write(reinterpret_cast<const char*>(magic), static_cast<std::streamsize>(bytesRead));
        *ss << std::cin.rdbuf();

        return std::make_unique<CompressedInputStream>(std::move(ss), format);
    }

    return openCompressedFile(path);
}

}  // namespace fqc::io
