// =============================================================================
// fq-compressor - Compressed Stream Implementation
// =============================================================================
// Transparent decompression support implementation.
//
// Supported formats: gzip (zlib), bzip2 (libbz2), xz (liblzma)
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
        throw IOError( "Failed to initialize zlib: " +
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
        throw IOError(
                      "Gzip decompression failed: " + std::string(zError(ret)));
    }

    return outputBuffer_.size() - stream->avail_out;
}

// =============================================================================
// Bzip2StreamBuf Implementation
// =============================================================================

Bzip2StreamBuf::Bzip2StreamBuf(std::istream& source, std::size_t bufferSize)
    : source_(&source), inputBuffer_(bufferSize), outputBuffer_(bufferSize) {
    initBzip2();
}

Bzip2StreamBuf::~Bzip2StreamBuf() { cleanupBzip2(); }

Bzip2StreamBuf::Bzip2StreamBuf(Bzip2StreamBuf&& other) noexcept
    : std::streambuf(std::move(other)),
      source_(other.source_),
      inputBuffer_(std::move(other.inputBuffer_)),
      outputBuffer_(std::move(other.outputBuffer_)),
      bzStream_(other.bzStream_),
      initialized_(other.initialized_),
      streamEnd_(other.streamEnd_) {
    other.source_ = nullptr;
    other.bzStream_ = nullptr;
    other.initialized_ = false;
}

Bzip2StreamBuf& Bzip2StreamBuf::operator=(Bzip2StreamBuf&& other) noexcept {
    if (this != &other) {
        cleanupBzip2();

        source_ = other.source_;
        inputBuffer_ = std::move(other.inputBuffer_);
        outputBuffer_ = std::move(other.outputBuffer_);
        bzStream_ = other.bzStream_;
        initialized_ = other.initialized_;
        streamEnd_ = other.streamEnd_;

        other.source_ = nullptr;
        other.bzStream_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

void Bzip2StreamBuf::initBzip2() {
    auto* stream = new bz_stream;
    std::memset(stream, 0, sizeof(bz_stream));

    int ret = BZ2_bzDecompressInit(stream, 0 /* verbosity */, 0 /* small */);  
    if (ret != BZ_OK) {
        delete stream;
        throw IOError("Failed to initialize bzip2 decompressor (error: " +
                      std::to_string(ret) + ")");
    }

    bzStream_ = stream;
    initialized_ = true;
}

void Bzip2StreamBuf::cleanupBzip2() {
    if (bzStream_) {
        auto* stream = static_cast<bz_stream*>(bzStream_);
        BZ2_bzDecompressEnd(stream);
        delete stream;
        bzStream_ = nullptr;
    }
    initialized_ = false;
}

Bzip2StreamBuf::int_type Bzip2StreamBuf::underflow() {
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

std::size_t Bzip2StreamBuf::decompress() {
    if (!initialized_ || !source_) {
        return 0;
    }

    auto* stream = static_cast<bz_stream*>(bzStream_);

    // Read more compressed data if needed
    if (stream->avail_in == 0 && !source_->eof()) {
        source_->read(inputBuffer_.data(),
                      static_cast<std::streamsize>(inputBuffer_.size()));
        auto bytesRead = static_cast<std::size_t>(source_->gcount());
        if (bytesRead > 0) {
            stream->avail_in = static_cast<unsigned int>(bytesRead);
            stream->next_in = inputBuffer_.data();
        }
    }

    // Setup output buffer
    stream->avail_out = static_cast<unsigned int>(outputBuffer_.size());
    stream->next_out = outputBuffer_.data();

    // Decompress
    int ret = BZ2_bzDecompress(stream);

    if (ret == BZ_STREAM_END) {
        streamEnd_ = true;
    } else if (ret != BZ_OK) {
        throw IOError("Bzip2 decompression failed (error: " +
                      std::to_string(ret) + ")");
    }

    return outputBuffer_.size() - stream->avail_out;
}

// =============================================================================
// XzStreamBuf Implementation
// =============================================================================

XzStreamBuf::XzStreamBuf(std::istream& source, std::size_t bufferSize)
    : source_(&source), inputBuffer_(bufferSize), outputBuffer_(bufferSize) {
    initLzma();
}

XzStreamBuf::~XzStreamBuf() { cleanupLzma(); }

XzStreamBuf::XzStreamBuf(XzStreamBuf&& other) noexcept
    : std::streambuf(std::move(other)),
      source_(other.source_),
      inputBuffer_(std::move(other.inputBuffer_)),
      outputBuffer_(std::move(other.outputBuffer_)),
      lzmaStream_(other.lzmaStream_),
      initialized_(other.initialized_),
      streamEnd_(other.streamEnd_) {
    other.source_ = nullptr;
    other.lzmaStream_ = nullptr;
    other.initialized_ = false;
}

XzStreamBuf& XzStreamBuf::operator=(XzStreamBuf&& other) noexcept {
    if (this != &other) {
        cleanupLzma();

        source_ = other.source_;
        inputBuffer_ = std::move(other.inputBuffer_);
        outputBuffer_ = std::move(other.outputBuffer_);
        lzmaStream_ = other.lzmaStream_;
        initialized_ = other.initialized_;
        streamEnd_ = other.streamEnd_;

        other.source_ = nullptr;
        other.lzmaStream_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

void XzStreamBuf::initLzma() {
    auto* stream = new lzma_stream;
    *stream = LZMA_STREAM_INIT;

    // Use auto decoder which handles both xz and lzma formats
    // Memory limit: 256 MB
    constexpr std::uint64_t kMemLimit = 256ULL * 1024 * 1024;
    lzma_ret ret = lzma_auto_decoder(stream, kMemLimit, 0);
    if (ret != LZMA_OK) {
        delete stream;
        throw IOError("Failed to initialize lzma decompressor (error: " +
                      std::to_string(static_cast<int>(ret)) + ")");
    }

    lzmaStream_ = stream;
    initialized_ = true;
}

void XzStreamBuf::cleanupLzma() {
    if (lzmaStream_) {
        auto* stream = static_cast<lzma_stream*>(lzmaStream_);
        lzma_end(stream);
        delete stream;
        lzmaStream_ = nullptr;
    }
    initialized_ = false;
}

XzStreamBuf::int_type XzStreamBuf::underflow() {
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

std::size_t XzStreamBuf::decompress() {
    if (!initialized_ || !source_) {
        return 0;
    }

    auto* stream = static_cast<lzma_stream*>(lzmaStream_);

    // Read more compressed data if needed
    if (stream->avail_in == 0 && !source_->eof()) {
        source_->read(reinterpret_cast<char*>(inputBuffer_.data()),
                      static_cast<std::streamsize>(inputBuffer_.size()));
        auto bytesRead = static_cast<std::size_t>(source_->gcount());
        if (bytesRead > 0) {
            stream->avail_in = bytesRead;
            stream->next_in = inputBuffer_.data();
        }
    }

    // Setup output buffer
    stream->avail_out = outputBuffer_.size();
    stream->next_out = reinterpret_cast<std::uint8_t*>(outputBuffer_.data());

    // Decompress
    lzma_action action = source_->eof() ? LZMA_FINISH : LZMA_RUN;
    lzma_ret ret = lzma_code(stream, action);

    if (ret == LZMA_STREAM_END) {
        streamEnd_ = true;
    } else if (ret != LZMA_OK) {
        throw IOError("XZ decompression failed (error: " +
                      std::to_string(static_cast<int>(ret)) + ")");
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
        throw IOError( "Failed to open file: " + path.string());
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

void CompressedInputStream::setup() {
    std::istream* source = fileStream_ ? fileStream_.get() : sourceStream_.get();
    if (!source) {
        throw IOError( "No source stream available");
    }

    switch (format_) {
        case CompressionFormat::kNone:
            // Use source directly
            rdbuf(source->rdbuf());
            break;

        case CompressionFormat::kGzip:
            decompressBuf_ = std::make_unique<GzipStreamBuf>(*source);
            rdbuf(decompressBuf_.get());
            FQC_LOG_DEBUG("Opened gzip compressed stream");
            break;

        case CompressionFormat::kBzip2:
            decompressBuf_ = std::make_unique<Bzip2StreamBuf>(*source);
            rdbuf(decompressBuf_.get());
            FQC_LOG_DEBUG("Opened bzip2 compressed stream");
            break;

        case CompressionFormat::kXz:
            decompressBuf_ = std::make_unique<XzStreamBuf>(*source);
            rdbuf(decompressBuf_.get());
            FQC_LOG_DEBUG("Opened xz compressed stream");
            break;

        case CompressionFormat::kZstd:
            throw IOError(
                          "Compression format not yet supported: " +
                              std::string(compressionFormatName(format_)));

        default:
            throw IOError( "Unknown compression format");
    }
}

// =============================================================================
// Factory Functions
// =============================================================================

std::unique_ptr<std::istream> openCompressedFile(const std::filesystem::path& path) {
    return std::make_unique<CompressedInputStream>(path);
}

/// @brief A streambuf that prepends buffered bytes before delegating to another streambuf.
/// @note Enables streaming stdin without loading the entire input into memory.
class PrependStreamBuf : public std::streambuf {
public:
    PrependStreamBuf(const std::uint8_t* prefix, std::size_t prefixLen,
                     std::streambuf* underlying)
        : prefix_(reinterpret_cast<const char*>(prefix),
                  reinterpret_cast<const char*>(prefix) + prefixLen)
        , underlying_(underlying)
        , phase_(prefixLen > 0 ? Phase::kPrefix : Phase::kUnderlying) {
        if (phase_ == Phase::kPrefix) {
            setg(prefix_.data(), prefix_.data(),
                 prefix_.data() + static_cast<std::ptrdiff_t>(prefix_.size()));
        }
    }

protected:
    int_type underflow() override {
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }
        if (phase_ == Phase::kPrefix) {
            phase_ = Phase::kUnderlying;
        }
        if (phase_ == Phase::kUnderlying && underlying_) {
            return underlying_->sgetc();
        }
        return traits_type::eof();
    }

    int_type uflow() override {
        if (gptr() < egptr()) {
            char_type ch = *gptr();
            gbump(1);
            return traits_type::to_int_type(ch);
        }
        if (phase_ == Phase::kPrefix) {
            phase_ = Phase::kUnderlying;
        }
        if (phase_ == Phase::kUnderlying && underlying_) {
            return underlying_->sbumpc();
        }
        return traits_type::eof();
    }

    std::streamsize xsgetn(char_type* s, std::streamsize count) override {
        std::streamsize totalRead = 0;
        // Drain prefix first
        if (phase_ == Phase::kPrefix) {
            std::streamsize avail = egptr() - gptr();
            std::streamsize toCopy = std::min(count, avail);
            std::memcpy(s, gptr(), static_cast<std::size_t>(toCopy));
            gbump(static_cast<int>(toCopy));
            totalRead += toCopy;
            s += toCopy;
            count -= toCopy;
            if (gptr() >= egptr()) {
                phase_ = Phase::kUnderlying;
            }
        }
        // Read from underlying
        if (count > 0 && phase_ == Phase::kUnderlying && underlying_) {
            totalRead += underlying_->sgetn(s, count);
        }
        return totalRead;
    }

private:
    enum class Phase { kPrefix, kUnderlying };
    std::vector<char> prefix_;
    std::streambuf* underlying_;
    Phase phase_;
};

/// @brief An istream that owns a PrependStreamBuf for streaming stdin.
class PrependInputStream : public std::istream {
public:
    PrependInputStream(const std::uint8_t* prefix, std::size_t prefixLen,
                       std::streambuf* underlying)
        : std::istream(nullptr)
        , buf_(prefix, prefixLen, underlying) {
        rdbuf(&buf_);
    }
private:
    PrependStreamBuf buf_;
};

std::unique_ptr<std::istream> openInputFile(const std::filesystem::path& path) {
    if (path == "-") {
        // stdin - check if compressed
        FQC_LOG_DEBUG("Opening stdin for input");

        // Read magic bytes
        std::uint8_t magic[8];
        std::cin.read(reinterpret_cast<char*>(magic), sizeof(magic));
        auto bytesRead = static_cast<std::size_t>(std::cin.gcount());

        auto format = detectCompressionFormat({magic, bytesRead});

        if (format == CompressionFormat::kNone) {
            // Stream stdin with prepended magic bytes (no full-copy into memory)
            return std::make_unique<PrependInputStream>(
                magic, bytesRead, std::cin.rdbuf());
        }

        // Compressed stdin - need to handle specially
        if (!isCompressionSupported(format)) {
            throw IOError(
                          "Compressed stdin not supported for format: " +
                              std::string(compressionFormatName(format)));
        }

        // Compressed stdin: must buffer fully for decompression seek support
        auto ss = std::make_unique<std::stringstream>();
        ss->write(reinterpret_cast<const char*>(magic), static_cast<std::streamsize>(bytesRead));
        *ss << std::cin.rdbuf();

        return std::make_unique<CompressedInputStream>(std::move(ss), format);
    }

    return openCompressedFile(path);
}

}  // namespace fqc::io
