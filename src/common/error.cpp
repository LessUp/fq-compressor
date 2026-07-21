// =============================================================================
// fq-compressor - Error Handling Framework Implementation
// =============================================================================
// Implementation of error handling utilities and exception classes.
//
// Requirements: 7.3 (Code style)
// =============================================================================

#include "fqc/common/error.h"

#include <sstream>

#include <fmt/format.h>

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
// Error Implementation
// =============================================================================

FQCException Error::toException() const {
    return FQCException(code_, message_);
}

[[noreturn]] void Error::throwException() const {
    throw toException();
}

}  // namespace fqc
