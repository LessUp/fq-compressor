// =============================================================================
// fq-compressor - Error Handling Framework
// =============================================================================
// Comprehensive error handling for the fq-compressor library.
//
// This module provides:
// - ErrorCode enum matching CLI exit codes
// - FQCException hierarchy for structured error handling
// - Result<T, E> type for functional error handling (using std::expected)
// - Error context and message support
//
// Exit Code Convention (per requirements):
// - 0: Success
// - 1: Usage/argument error
// - 2: I/O error (file not found, read/write failure)
// - 3: Format error or version incompatibility
// - 4: Checksum verification failure
// - 5: Unsupported algorithm/codec
//
// Naming Conventions (per project style guide):
// - Enums: PascalCase with kConstant values
// - Classes: PascalCase
// - Functions: camelCase
// - Constants: kConstant
//
// Requirements: 7.3 (Code style)
// =============================================================================

#ifndef FQC_COMMON_ERROR_H
#define FQC_COMMON_ERROR_H

#include <cstdint>
#include <exception>
#include <expected>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

namespace fqc {

// =============================================================================
// Error Code Enumeration
// =============================================================================

/// @brief Error codes matching CLI exit codes.
/// @note These values are used as process exit codes.
enum class ErrorCode : std::uint8_t {
    /// @brief Operation completed successfully.
    kSuccess = 0,

    /// @brief Usage or argument error.
    /// @note Invalid command-line arguments, missing required options, etc.
    kUsageError = 1,

    /// @brief I/O error.
    /// @note File not found, read/write failure, permission denied, etc.
    kIOError = 2,

    /// @brief Format error or version incompatibility.
    /// @note Invalid file format, unsupported version, corrupted header, etc.
    kFormatError = 3,

    /// @brief Checksum verification failure.
    /// @note Block or global checksum mismatch, data corruption detected.
    kChecksumError = 4,

    /// @brief Unsupported algorithm or codec.
    /// @note Unknown codec family, incompatible compression algorithm.
    kUnsupportedCodec = 5,

    /// @brief Invalid argument value.
    kInvalidArgument = 6,

    /// @brief File not found.
    kFileNotFound = 7,

    /// @brief File already exists.
    kFileExists = 8,

    /// @brief Failed to open file.
    kFileOpenFailed = 9,

    /// @brief Seek operation failed.
    kSeekFailed = 10,

    /// @brief Invalid file format.
    kInvalidFormat = 11,

    /// @brief Invalid state for operation.
    kInvalidState = 12,

    /// @brief Operation was cancelled.
    kCancelled = 13,

    /// @brief Decompression failed.
    kDecompressionFailed = 14,

    /// @brief Unsupported format.
    kUnsupportedFormat = 15,

    /// @brief Corrupted data detected.
    kCorruptedData = 16
};

/// @brief Convert ErrorCode to its integer exit code value.
/// @param code The error code.
/// @return Integer exit code suitable for process exit.
[[nodiscard]] constexpr int toExitCode(ErrorCode code) noexcept {
    return static_cast<int>(code);
}

/// @brief Convert ErrorCode to string representation.
/// @param code The error code.
/// @return Human-readable string describing the error category.
[[nodiscard]] constexpr std::string_view errorCodeToString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::kSuccess:
            return "success";
        case ErrorCode::kUsageError:
            return "usage error";
        case ErrorCode::kIOError:
            return "I/O error";
        case ErrorCode::kFormatError:
            return "format error";
        case ErrorCode::kChecksumError:
            return "checksum error";
        case ErrorCode::kUnsupportedCodec:
            return "unsupported codec";
        case ErrorCode::kInvalidArgument:
            return "invalid argument";
        case ErrorCode::kFileNotFound:
            return "file not found";
        case ErrorCode::kFileExists:
            return "file exists";
        case ErrorCode::kFileOpenFailed:
            return "file open failed";
        case ErrorCode::kSeekFailed:
            return "seek failed";
        case ErrorCode::kInvalidFormat:
            return "invalid format";
        case ErrorCode::kInvalidState:
            return "invalid state";
        case ErrorCode::kCancelled:
            return "cancelled";
        case ErrorCode::kDecompressionFailed:
            return "decompression failed";
        case ErrorCode::kUnsupportedFormat:
            return "unsupported format";
        case ErrorCode::kCorruptedData:
            return "corrupted data";
    }
    return "unknown error";
}

/// @brief Check if an error code represents success.
/// @param code The error code.
/// @return true if the code represents success.
[[nodiscard]] constexpr bool isSuccess(ErrorCode code) noexcept {
    return code == ErrorCode::kSuccess;
}

/// @brief Check if an error code represents an error.
/// @param code The error code.
/// @return true if the code represents an error.
[[nodiscard]] constexpr bool isError(ErrorCode code) noexcept {
    return code != ErrorCode::kSuccess;
}

// =============================================================================
// Error Context Structure
// =============================================================================

/// @brief Additional context information for errors.
/// @note Provides detailed information about where and why an error occurred.
struct ErrorContext {
    /// @brief File path associated with the error (if applicable).
    std::string filePath;

    /// @brief Block ID where the error occurred (if applicable).
    std::optional<std::uint32_t> blockId;

    /// @brief Read ID where the error occurred (if applicable).
    std::optional<std::uint64_t> readId;

    /// @brief Byte offset in file where error occurred (if applicable).
    std::optional<std::uint64_t> byteOffset;

    /// @brief Source location where the error was created.
    std::source_location location;

    /// @brief Default constructor with current source location.
    ErrorContext(std::source_location loc = std::source_location::current()) : location(loc) {}

    /// @brief Construct with file path.
    explicit ErrorContext(std::string path,
                          std::source_location loc = std::source_location::current())
        : filePath(std::move(path)), location(loc) {}

    /// @brief Set the file path.
    /// @return Reference to this for method chaining.
    ErrorContext& withFile(std::string path) {
        filePath = std::move(path);
        return *this;
    }

    /// @brief Set the block ID.
    /// @return Reference to this for method chaining.
    ErrorContext& withBlock(std::uint32_t id) {
        blockId = id;
        return *this;
    }

    /// @brief Set the read ID.
    /// @return Reference to this for method chaining.
    ErrorContext& withRead(std::uint64_t id) {
        readId = id;
        return *this;
    }

    /// @brief Set the byte offset.
    /// @return Reference to this for method chaining.
    ErrorContext& withOffset(std::uint64_t offset) {
        byteOffset = offset;
        return *this;
    }

    /// @brief Format context as a string for error messages.
    /// @return Formatted context string.
    [[nodiscard]] std::string format() const;
};

// =============================================================================
// Base Exception Class
// =============================================================================

/// @brief Base exception class for all fq-compressor errors.
/// @note All fqc exceptions derive from this class.
/// @note Provides error code, message, and optional context.
class FQCException : public std::exception {
public:
    /// @brief Construct with error code and message.
    /// @param code The error code.
    /// @param message Descriptive error message.
    FQCException(ErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {
        formatWhat();
    }

    /// @brief Construct with error code, message, and context.
    /// @param code The error code.
    /// @param message Descriptive error message.
    /// @param context Additional error context.
    FQCException(ErrorCode code, std::string message, ErrorContext context)
        : code_(code), message_(std::move(message)), context_(std::move(context)) {
        formatWhat();
    }

    /// @brief Virtual destructor.
    ~FQCException() override = default;

    /// @brief Copy constructor.
    FQCException(const FQCException&) = default;

    /// @brief Move constructor.
    FQCException(FQCException&&) noexcept = default;

    /// @brief Copy assignment.
    FQCException& operator=(const FQCException&) = default;

    /// @brief Move assignment.
    FQCException& operator=(FQCException&&) noexcept = default;

    /// @brief Get the error message.
    /// @return Null-terminated error message string.
    [[nodiscard]] const char* what() const noexcept override { return what_.c_str(); }

    /// @brief Get the error code.
    /// @return The error code associated with this exception.
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }

    /// @brief Get the exit code for this error.
    /// @return Integer exit code suitable for process exit.
    [[nodiscard]] int exitCode() const noexcept { return toExitCode(code_); }

    /// @brief Get the error message (without context).
    /// @return The error message.
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

    /// @brief Get the error context.
    /// @return Optional error context.
    [[nodiscard]] const std::optional<ErrorContext>& context() const noexcept { return context_; }

    /// @brief Check if this exception has context information.
    /// @return true if context is available.
    [[nodiscard]] bool hasContext() const noexcept { return context_.has_value(); }

protected:
    /// @brief Format the what() string from message and context.
    void formatWhat();

    ErrorCode code_;
    std::string message_;
    std::optional<ErrorContext> context_;
    std::string what_;
};

// =============================================================================
// Specific Exception Classes
// =============================================================================

/// @brief Exception for usage and argument errors (exit code 1).
/// @note Thrown for invalid command-line arguments, missing required options,
///       invalid parameter values, etc.
class UsageError : public FQCException {
public:
    /// @brief Construct with message.
    /// @param message Descriptive error message.
    explicit UsageError(std::string message)
        : FQCException(ErrorCode::kUsageError, std::move(message)) {}

    /// @brief Construct with message and context.
    /// @param message Descriptive error message.
    /// @param context Additional error context.
    UsageError(std::string message, ErrorContext context)
        : FQCException(ErrorCode::kUsageError, std::move(message), std::move(context)) {}
};

/// @brief Exception for I/O errors (exit code 2).
/// @note Thrown for file not found, read/write failures, permission denied,
///       disk full, etc.
class IOError : public FQCException {
public:
    /// @brief Construct with message.
    /// @param message Descriptive error message.
    explicit IOError(std::string message)
        : FQCException(ErrorCode::kIOError, std::move(message)) {}

    /// @brief Construct with message and context.
    /// @param message Descriptive error message.
    /// @param context Additional error context.
    IOError(std::string message, ErrorContext context)
        : FQCException(ErrorCode::kIOError, std::move(message), std::move(context)) {}

    /// @brief Construct from system error code.
    /// @param message Descriptive error message.
    /// @param ec System error code.
    IOError(std::string message, std::error_code ec)
        : FQCException(ErrorCode::kIOError, formatWithSystemError(message, ec)),
          systemError_(ec) {}

    /// @brief Construct from system error code with context.
    /// @param message Descriptive error message.
    /// @param ec System error code.
    /// @param context Additional error context.
    IOError(std::string message, std::error_code ec, ErrorContext context)
        : FQCException(ErrorCode::kIOError, formatWithSystemError(message, ec),
                       std::move(context)),
          systemError_(ec) {}

    /// @brief Get the system error code (if available).
    /// @return Optional system error code.
    [[nodiscard]] const std::optional<std::error_code>& systemError() const noexcept {
        return systemError_;
    }

private:
    static std::string formatWithSystemError(const std::string& message, std::error_code ec);

    std::optional<std::error_code> systemError_;
};

/// @brief Exception for format errors (exit code 3).
/// @note Thrown for invalid file format, unsupported version, corrupted header,
///       invalid magic number, etc.
class FormatError : public FQCException {
public:
    /// @brief Construct with message.
    /// @param message Descriptive error message.
    explicit FormatError(std::string message)
        : FQCException(ErrorCode::kFormatError, std::move(message)) {}

    /// @brief Construct with message and context.
    /// @param message Descriptive error message.
    /// @param context Additional error context.
    FormatError(std::string message, ErrorContext context)
        : FQCException(ErrorCode::kFormatError, std::move(message), std::move(context)) {}
};

/// @brief Exception for checksum verification failures (exit code 4).
/// @note Thrown when block or global checksum doesn't match, indicating
///       data corruption.
class ChecksumError : public FQCException {
public:
    /// @brief Construct with message.
    /// @param message Descriptive error message.
    explicit ChecksumError(std::string message)
        : FQCException(ErrorCode::kChecksumError, std::move(message)) {}

    /// @brief Construct with message and context.
    /// @param message Descriptive error message.
    /// @param context Additional error context.
    ChecksumError(std::string message, ErrorContext context)
        : FQCException(ErrorCode::kChecksumError, std::move(message), std::move(context)) {}

    /// @brief Construct with expected and actual checksum values.
    /// @param expected Expected checksum value.
    /// @param actual Actual checksum value.
    /// @param context Additional error context.
    ChecksumError(std::uint64_t expected, std::uint64_t actual, ErrorContext context)
        : FQCException(ErrorCode::kChecksumError,
                       formatChecksumMismatch(expected, actual),
                       std::move(context)),
          expected_(expected),
          actual_(actual) {}

    /// @brief Get the expected checksum value (if available).
    /// @return Optional expected checksum.
    [[nodiscard]] std::optional<std::uint64_t> expected() const noexcept { return expected_; }

    /// @brief Get the actual checksum value (if available).
    /// @return Optional actual checksum.
    [[nodiscard]] std::optional<std::uint64_t> actual() const noexcept { return actual_; }

private:
    static std::string formatChecksumMismatch(std::uint64_t expected, std::uint64_t actual);

    std::optional<std::uint64_t> expected_;
    std::optional<std::uint64_t> actual_;
};

/// @brief Exception for unsupported codec errors (exit code 5).
/// @note Thrown when encountering an unknown codec family or incompatible
///       compression algorithm.
class UnsupportedCodecError : public FQCException {
public:
    /// @brief Construct with message.
    /// @param message Descriptive error message.
    explicit UnsupportedCodecError(std::string message)
        : FQCException(ErrorCode::kUnsupportedCodec, std::move(message)) {}

    /// @brief Construct with message and context.
    /// @param message Descriptive error message.
    /// @param context Additional error context.
    UnsupportedCodecError(std::string message, ErrorContext context)
        : FQCException(ErrorCode::kUnsupportedCodec, std::move(message), std::move(context)) {}

    /// @brief Construct with codec family ID.
    /// @param codecFamily The unsupported codec family ID.
    /// @param context Additional error context.
    UnsupportedCodecError(std::uint8_t codecFamily, ErrorContext context)
        : FQCException(ErrorCode::kUnsupportedCodec,
                       formatUnsupportedCodec(codecFamily),
                       std::move(context)),
          codecFamily_(codecFamily) {}

    /// @brief Get the unsupported codec family ID (if available).
    /// @return Optional codec family ID.
    [[nodiscard]] std::optional<std::uint8_t> codecFamily() const noexcept {
        return codecFamily_;
    }

private:
    static std::string formatUnsupportedCodec(std::uint8_t codecFamily);

    std::optional<std::uint8_t> codecFamily_;
};

// =============================================================================
// Result Type (using std::expected)
// =============================================================================

/// @brief Error type for Result, wrapping ErrorCode and message.
/// @note Lightweight error type for use with std::expected.
class Error {
public:
    /// @brief Construct with error code and message.
    /// @param code The error code.
    /// @param message Descriptive error message.
    Error(ErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {}

    /// @brief Construct from an FQCException.
    /// @param ex The exception to convert.
    explicit Error(const FQCException& ex) : code_(ex.code()), message_(ex.message()) {}

    /// @brief Get the error code.
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }

    /// @brief Get the error message.
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

    /// @brief Get the exit code.
    [[nodiscard]] int exitCode() const noexcept { return toExitCode(code_); }

    /// @brief Convert to the appropriate exception type.
    /// @return FQCException (or derived) matching the error code.
    [[nodiscard]] FQCException toException() const;

    /// @brief Throw the appropriate exception.
    /// @note This function does not return.
    [[noreturn]] void throwException() const;

private:
    ErrorCode code_;
    std::string message_;
};

/// @brief Result type for operations that can fail.
/// @tparam T The success value type.
/// @tparam E The error type (defaults to Error).
/// @note Uses std::expected from C++23.
template <typename T, typename E = Error>
using Result = std::expected<T, E>;

/// @brief Create a success result.
/// @tparam T The value type.
/// @param value The success value.
/// @return Result containing the value.
template <typename T>
[[nodiscard]] Result<T> makeSuccess(T value) {
    return Result<T>{std::move(value)};
}

/// @brief Create an error result.
/// @tparam T The expected value type.
/// @param code The error code.
/// @param message The error message.
/// @return Result containing the error.
template <typename T>
[[nodiscard]] Result<T> makeError(ErrorCode code, std::string message) {
    return std::unexpected(Error{code, std::move(message)});
}

/// @brief Create an error result from an Error object.
/// @tparam T The expected value type.
/// @param error The error object.
/// @return Result containing the error.
template <typename T>
[[nodiscard]] Result<T> makeError(Error error) {
    return std::unexpected(std::move(error));
}

/// @brief Create an error result from an exception.
/// @tparam T The expected value type.
/// @param ex The exception.
/// @return Result containing the error.
template <typename T>
[[nodiscard]] Result<T> makeError(const FQCException& ex) {
    return std::unexpected(Error{ex});
}

// =============================================================================
// Void Result Type
// =============================================================================

/// @brief Result type for operations that return nothing on success.
/// @note Uses std::monostate as the success type.
using VoidResult = Result<std::monostate>;

/// @brief Create a success void result.
/// @return VoidResult indicating success.
[[nodiscard]] inline VoidResult makeVoidSuccess() {
    return VoidResult{std::monostate{}};
}

/// @brief Create an error void result.
/// @param code The error code.
/// @param message The error message.
/// @return VoidResult containing the error.
[[nodiscard]] inline VoidResult makeVoidError(ErrorCode code, std::string message) {
    return std::unexpected(Error{code, std::move(message)});
}

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Convert a Result to an exception if it contains an error.
/// @tparam T The value type.
/// @param result The result to check.
/// @return The value if successful.
/// @throws FQCException (or derived) if the result contains an error.
template <typename T>
[[nodiscard]] T unwrapOrThrow(Result<T> result) {
    if (result.has_value()) {
        return std::move(result.value());
    }
    result.error().throwException();
}

/// @brief Convert a Result to an exception if it contains an error (void version).
/// @param result The result to check.
/// @throws FQCException (or derived) if the result contains an error.
inline void unwrapOrThrow(VoidResult result) {
    if (!result.has_value()) {
        result.error().throwException();
    }
}

/// @brief Try to execute a function and convert exceptions to Result.
/// @tparam F The function type.
/// @param func The function to execute.
/// @return Result containing the return value or error.
template <typename F>
[[nodiscard]] auto tryExecute(F&& func) -> Result<decltype(func())> {
    using ReturnType = decltype(func());
    try {
        if constexpr (std::is_void_v<ReturnType>) {
            func();
            return std::monostate{};
        } else {
            return func();
        }
    } catch (const FQCException& ex) {
        return std::unexpected(Error{ex});
    } catch (const std::exception& ex) {
        return std::unexpected(Error{ErrorCode::kIOError, ex.what()});
    }
}

}  // namespace fqc

#endif  // FQC_COMMON_ERROR_H
