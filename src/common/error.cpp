// =============================================================================
// fq-compressor - Error Handling Framework Implementation
// =============================================================================
// Implementation of error handling utilities and exception classes.
//
// Requirements: 7.3 (Code style)
// =============================================================================

#include "fqc/common/error.h"

#include <fmt/format.h>
#include <sstream>

namespace fqc {

// =============================================================================
// ErrorContext Implementation
// =============================================================================

std::string ErrorContext::format() const {
    std::ostringstream oss;
    bool hasContent = false;

    if (!filePath.empty()) {
        oss << "file: " << filePath;
        hasContent = true;
    }

    if (blockId.has_value()) {
        if (hasContent) {
            oss << ", ";
        }
        oss << "block: " << *blockId;
        hasContent = true;
    }

    if (readId.has_value()) {
        if (hasContent) {
            oss << ", ";
        }
        oss << "read: " << *readId;
        hasContent = true;
    }

    if (byteOffset.has_value()) {
        if (hasContent) {
            oss << ", ";
        }
        oss << "offset: 0x" << std::hex << *byteOffset;
        hasContent = true;
    }

    // Add source location in debug builds
#ifndef NDEBUG
    if (hasContent) {
        oss << " (at " << location.file_name() << ":" << location.line() << ")";
    }
#endif

    return oss.str();
}

// =============================================================================
// FQCException Implementation
// =============================================================================

void FQCException::formatWhat() {
    std::ostringstream oss;
    oss << "[" << errorCodeToString(code_) << "] " << message_;

    if (context_.has_value()) {
        std::string contextStr = context_->format();
        if (!contextStr.empty()) {
            oss << " (" << contextStr << ")";
        }
    }

    what_ = oss.str();
}

// =============================================================================
// IOError Implementation
// =============================================================================

std::string IOError::formatWithSystemError(const std::string& message, std::error_code ec) {
    return fmt::format("{}: {} (error code: {})", message, ec.message(), ec.value());
}

// =============================================================================
// ChecksumError Implementation
// =============================================================================

std::string ChecksumError::formatChecksumMismatch(std::uint64_t expected, std::uint64_t actual) {
    return fmt::format("checksum mismatch: expected 0x{:016x}, got 0x{:016x}", expected, actual);
}

// =============================================================================
// UnsupportedCodecError Implementation
// =============================================================================

std::string UnsupportedCodecError::formatUnsupportedCodec(std::uint8_t codecFamily) {
    return fmt::format("unsupported codec family: 0x{:02x}", codecFamily);
}

// =============================================================================
// Error Implementation
// =============================================================================

FQCException Error::toException() const {
    switch (code_) {
        case ErrorCode::kUsageError:
            return UsageError(message_);
        case ErrorCode::kIOError:
        case ErrorCode::kFileNotFound:
        case ErrorCode::kFileExists:
        case ErrorCode::kFileOpenFailed:
        case ErrorCode::kSeekFailed:
            return IOError(message_);
        case ErrorCode::kFormatError:
        case ErrorCode::kInvalidFormat:
        case ErrorCode::kInvalidArgument:
        case ErrorCode::kInvalidState:
        case ErrorCode::kCorruptedData:
            return FormatError(message_);
        case ErrorCode::kChecksumError:
        case ErrorCode::kChecksumMismatch:
            return ChecksumError(message_);
        case ErrorCode::kUnsupportedCodec:
        case ErrorCode::kUnsupportedFormat:
            return UnsupportedCodecError(message_);
        case ErrorCode::kSuccess:
        case ErrorCode::kCancelled:
        case ErrorCode::kDecompressionFailed:
        case ErrorCode::kInternalError:
        case ErrorCode::kCompressionFailed:
        case ErrorCode::kDecompressionError:
            // Handle gracefully with base exception
            return FQCException(code_, message_);
    }
    // Fallback for unknown error codes
    return FQCException(code_, message_);
}

[[noreturn]] void Error::throwException() const {
    switch (code_) {
        case ErrorCode::kUsageError:
            throw UsageError(message_);
        case ErrorCode::kIOError:
        case ErrorCode::kFileNotFound:
        case ErrorCode::kFileExists:
        case ErrorCode::kFileOpenFailed:
        case ErrorCode::kSeekFailed:
            throw IOError(message_);
        case ErrorCode::kFormatError:
        case ErrorCode::kInvalidFormat:
        case ErrorCode::kInvalidArgument:
        case ErrorCode::kInvalidState:
        case ErrorCode::kCorruptedData:
            throw FormatError(message_);
        case ErrorCode::kChecksumError:
        case ErrorCode::kChecksumMismatch:
            throw ChecksumError(message_);
        case ErrorCode::kUnsupportedCodec:
        case ErrorCode::kUnsupportedFormat:
            throw UnsupportedCodecError(message_);
        case ErrorCode::kSuccess:
        case ErrorCode::kCancelled:
        case ErrorCode::kDecompressionFailed:
        case ErrorCode::kInternalError:
        case ErrorCode::kCompressionFailed:
        case ErrorCode::kDecompressionError:
            // Handle gracefully with base exception
            throw FQCException(code_, message_);
    }
    // Fallback for unknown error codes
    throw FQCException(code_, message_);
}

}  // namespace fqc
