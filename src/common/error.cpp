// =============================================================================
// fq-compressor - Error Handling Framework Implementation
// =============================================================================
// Implementation of error handling utilities and exception classes.
//
// Requirements: 7.3 (Code style)
// =============================================================================

#include "fqc/common/error.h"

#include <format>
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
    return std::format("{}: {} (error code: {})", message, ec.message(), ec.value());
}

// =============================================================================
// ChecksumError Implementation
// =============================================================================

std::string ChecksumError::formatChecksumMismatch(std::uint64_t expected, std::uint64_t actual) {
    return std::format("checksum mismatch: expected 0x{:016x}, got 0x{:016x}", expected, actual);
}

// =============================================================================
// UnsupportedCodecError Implementation
// =============================================================================

std::string UnsupportedCodecError::formatUnsupportedCodec(std::uint8_t codecFamily) {
    return std::format("unsupported codec family: 0x{:02x}", codecFamily);
}

// =============================================================================
// Error Implementation
// =============================================================================

FQCException Error::toException() const {
    switch (code_) {
        case ErrorCode::kUsageError:
            return UsageError(message_);
        case ErrorCode::kIOError:
            return IOError(message_);
        case ErrorCode::kFormatError:
            return FormatError(message_);
        case ErrorCode::kChecksumError:
            return ChecksumError(message_);
        case ErrorCode::kUnsupportedCodec:
            return UnsupportedCodecError(message_);
        case ErrorCode::kSuccess:
            // Should not happen, but handle gracefully
            return FQCException(ErrorCode::kSuccess, message_);
    }
    // Fallback for unknown error codes
    return FQCException(code_, message_);
}

[[noreturn]] void Error::throwException() const {
    switch (code_) {
        case ErrorCode::kUsageError:
            throw UsageError(message_);
        case ErrorCode::kIOError:
            throw IOError(message_);
        case ErrorCode::kFormatError:
            throw FormatError(message_);
        case ErrorCode::kChecksumError:
            throw ChecksumError(message_);
        case ErrorCode::kUnsupportedCodec:
            throw UnsupportedCodecError(message_);
        case ErrorCode::kSuccess:
            // Should not happen, but throw base exception
            throw FQCException(ErrorCode::kSuccess, message_);
    }
    // Fallback for unknown error codes
    throw FQCException(code_, message_);
}

}  // namespace fqc
