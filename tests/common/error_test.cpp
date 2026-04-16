// =============================================================================
// fq-compressor - Error Handling Unit Tests
// =============================================================================
// Unit tests for ErrorCode, FQCException hierarchy, and Result types.
// =============================================================================

#include "fqc/common/error.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace fqc {
namespace {

// =============================================================================
// ErrorCode Tests
// =============================================================================

TEST(ErrorCodeTest, ToExitCodeSuccess) {
    EXPECT_EQ(toExitCode(ErrorCode::kSuccess), 0);
}

TEST(ErrorCodeTest, ToExitCodeUsageError) {
    EXPECT_EQ(toExitCode(ErrorCode::kUsageError), 1);
    EXPECT_EQ(toExitCode(ErrorCode::kInvalidArgument), 1);
    EXPECT_EQ(toExitCode(ErrorCode::kCancelled), 1);
}

TEST(ErrorCodeTest, ToExitCodeIOError) {
    EXPECT_EQ(toExitCode(ErrorCode::kIOError), 2);
    EXPECT_EQ(toExitCode(ErrorCode::kFileNotFound), 2);
    EXPECT_EQ(toExitCode(ErrorCode::kFileExists), 2);
    EXPECT_EQ(toExitCode(ErrorCode::kFileOpenFailed), 2);
    EXPECT_EQ(toExitCode(ErrorCode::kSeekFailed), 2);
}

TEST(ErrorCodeTest, ToExitCodeFormatError) {
    EXPECT_EQ(toExitCode(ErrorCode::kFormatError), 3);
    EXPECT_EQ(toExitCode(ErrorCode::kInvalidFormat), 3);
    EXPECT_EQ(toExitCode(ErrorCode::kInvalidState), 3);
    EXPECT_EQ(toExitCode(ErrorCode::kUnsupportedFormat), 3);
    EXPECT_EQ(toExitCode(ErrorCode::kCorruptedData), 3);
    EXPECT_EQ(toExitCode(ErrorCode::kInternalError), 3);
}

TEST(ErrorCodeTest, ToExitCodeChecksumError) {
    EXPECT_EQ(toExitCode(ErrorCode::kChecksumError), 4);
    EXPECT_EQ(toExitCode(ErrorCode::kChecksumMismatch), 4);
}

TEST(ErrorCodeTest, ToExitCodeCodecError) {
    EXPECT_EQ(toExitCode(ErrorCode::kUnsupportedCodec), 5);
    EXPECT_EQ(toExitCode(ErrorCode::kCompressionFailed), 5);
    EXPECT_EQ(toExitCode(ErrorCode::kDecompressionFailed), 5);
    EXPECT_EQ(toExitCode(ErrorCode::kDecompressionError), 5);
}

TEST(ErrorCodeTest, ErrorCodeToString) {
    EXPECT_EQ(errorCodeToString(ErrorCode::kSuccess), "success");
    EXPECT_EQ(errorCodeToString(ErrorCode::kUsageError), "usage error");
    EXPECT_EQ(errorCodeToString(ErrorCode::kIOError), "I/O error");
    EXPECT_EQ(errorCodeToString(ErrorCode::kFormatError), "format error");
    EXPECT_EQ(errorCodeToString(ErrorCode::kChecksumError), "checksum error");
    EXPECT_EQ(errorCodeToString(ErrorCode::kUnsupportedCodec), "unsupported codec");
    EXPECT_EQ(errorCodeToString(ErrorCode::kFileNotFound), "file not found");
    EXPECT_EQ(errorCodeToString(ErrorCode::kInvalidFormat), "invalid format");
    EXPECT_EQ(errorCodeToString(ErrorCode::kCorruptedData), "corrupted data");
}

TEST(ErrorCodeTest, IsSuccessAndIsError) {
    EXPECT_TRUE(isSuccess(ErrorCode::kSuccess));
    EXPECT_FALSE(isError(ErrorCode::kSuccess));

    EXPECT_FALSE(isSuccess(ErrorCode::kIOError));
    EXPECT_TRUE(isError(ErrorCode::kIOError));
}

// =============================================================================
// ErrorContext Tests
// =============================================================================

TEST(ErrorContextTest, DefaultConstruction) {
    ErrorContext ctx;
    EXPECT_TRUE(ctx.filePath.empty());
    EXPECT_FALSE(ctx.blockId.has_value());
    EXPECT_FALSE(ctx.readId.has_value());
    EXPECT_FALSE(ctx.byteOffset.has_value());
}

TEST(ErrorContextTest, WithFile) {
    ErrorContext ctx;
    ctx.withFile("/path/to/file.fqc");
    EXPECT_EQ(ctx.filePath, "/path/to/file.fqc");
}

TEST(ErrorContextTest, WithBlock) {
    ErrorContext ctx;
    ctx.withBlock(42);
    EXPECT_TRUE(ctx.blockId.has_value());
    EXPECT_EQ(ctx.blockId.value(), 42);
}

TEST(ErrorContextTest, WithRead) {
    ErrorContext ctx;
    ctx.withRead(12345ULL);
    EXPECT_TRUE(ctx.readId.has_value());
    EXPECT_EQ(ctx.readId.value(), 12345ULL);
}

TEST(ErrorContextTest, WithOffset) {
    ErrorContext ctx;
    ctx.withOffset(4096ULL);
    EXPECT_TRUE(ctx.byteOffset.has_value());
    EXPECT_EQ(ctx.byteOffset.value(), 4096ULL);
}

TEST(ErrorContextTest, ChainedBuilder) {
    ErrorContext ctx;
    ctx.withFile("test.fqc").withBlock(1).withRead(100).withOffset(2048);

    EXPECT_EQ(ctx.filePath, "test.fqc");
    EXPECT_EQ(ctx.blockId.value(), 1);
    EXPECT_EQ(ctx.readId.value(), 100);
    EXPECT_EQ(ctx.byteOffset.value(), 2048);
}

TEST(ErrorContextTest, StringConstruction) {
    ErrorContext ctx("/path/to/file.fqc");
    EXPECT_EQ(ctx.filePath, "/path/to/file.fqc");
}

TEST(ErrorContextTest, Format) {
    ErrorContext ctx;
    ctx.withFile("test.fqc").withBlock(5);

    std::string formatted = ctx.format();
    EXPECT_TRUE(formatted.find("test.fqc") != std::string::npos);
    EXPECT_TRUE(formatted.find("5") != std::string::npos);
}

// =============================================================================
// FQCException Base Tests
// =============================================================================

TEST(FQCExceptionTest, BasicConstruction) {
    FQCException ex(ErrorCode::kIOError, "Test error message");

    EXPECT_EQ(ex.code(), ErrorCode::kIOError);
    EXPECT_EQ(ex.message(), "Test error message");
    EXPECT_EQ(ex.exitCode(), 2);
    EXPECT_FALSE(ex.hasContext());

    std::string what = ex.what();
    EXPECT_TRUE(what.find("Test error message") != std::string::npos);
}

TEST(FQCExceptionTest, WithContext) {
    ErrorContext ctx;
    ctx.withFile("test.fqc").withBlock(10);

    FQCException ex(ErrorCode::kFormatError, "Format error", ctx);

    EXPECT_EQ(ex.code(), ErrorCode::kFormatError);
    EXPECT_TRUE(ex.hasContext());
    EXPECT_TRUE(ex.context().has_value());
    EXPECT_EQ(ex.context()->filePath, "test.fqc");
    EXPECT_EQ(ex.context()->blockId.value(), 10);
}

TEST(FQCExceptionTest, CopyAndMove) {
    FQCException ex1(ErrorCode::kIOError, "Test error");

    // Copy construct
    FQCException ex2(ex1);
    EXPECT_EQ(ex2.code(), ErrorCode::kIOError);
    EXPECT_EQ(ex2.message(), "Test error");

    // Move construct
    FQCException ex3(std::move(ex2));
    EXPECT_EQ(ex3.code(), ErrorCode::kIOError);

    // Copy assign
    FQCException ex4 = ex3;
    EXPECT_EQ(ex4.code(), ErrorCode::kIOError);

    // Move assign
    FQCException ex5 = std::move(ex4);
    EXPECT_EQ(ex5.code(), ErrorCode::kIOError);
}

TEST(FQCExceptionTest, CanBeCaughtAsStdException) {
    try {
        throw FQCException(ErrorCode::kIOError, "Test exception");
    } catch (const std::exception& e) {
        EXPECT_TRUE(std::string(e.what()).find("Test exception") != std::string::npos);
    }
}

// =============================================================================
// UsageError Tests
// =============================================================================

TEST(UsageErrorTest, BasicConstruction) {
    UsageError ex("Invalid argument");

    EXPECT_EQ(ex.code(), ErrorCode::kUsageError);
    EXPECT_EQ(ex.exitCode(), 1);
    EXPECT_EQ(ex.message(), "Invalid argument");
}

TEST(UsageErrorTest, WithContext) {
    ErrorContext ctx;
    ctx.withFile("config.txt");

    UsageError ex("Invalid option value", ctx);
    EXPECT_TRUE(ex.hasContext());
    EXPECT_EQ(ex.context()->filePath, "config.txt");
}

TEST(ArgumentErrorAliasTest, IsUsageError) {
    ArgumentError ex("Missing required argument");

    EXPECT_EQ(ex.code(), ErrorCode::kUsageError);
    EXPECT_EQ(ex.exitCode(), 1);
}

// =============================================================================
// IOError Tests
// =============================================================================

TEST(IOErrorTest, BasicConstruction) {
    IOError ex("Failed to open file");

    EXPECT_EQ(ex.code(), ErrorCode::kIOError);
    EXPECT_EQ(ex.exitCode(), 2);
    EXPECT_EQ(ex.message(), "Failed to open file");
    EXPECT_FALSE(ex.systemError().has_value());
}

TEST(IOErrorTest, WithSystemError) {
    std::error_code ec = std::make_error_code(std::errc::no_such_file_or_directory);
    IOError ex("Failed to open file", ec);

    EXPECT_EQ(ex.code(), ErrorCode::kIOError);
    EXPECT_TRUE(ex.systemError().has_value());
    EXPECT_EQ(ex.systemError()->value(), static_cast<int>(std::errc::no_such_file_or_directory));

    std::string what = ex.what();
    EXPECT_TRUE(what.find("Failed to open file") != std::string::npos);
}

TEST(IOErrorTest, WithContextAndSystemError) {
    std::error_code ec = std::make_error_code(std::errc::permission_denied);
    ErrorContext ctx;
    ctx.withFile("/root/secret.txt");

    IOError ex("Permission denied", ec, ctx);

    EXPECT_TRUE(ex.hasContext());
    EXPECT_TRUE(ex.systemError().has_value());
    EXPECT_EQ(ex.context()->filePath, "/root/secret.txt");
}

// =============================================================================
// FormatError Tests
// =============================================================================

TEST(FormatErrorTest, BasicConstruction) {
    FormatError ex("Invalid file format");

    EXPECT_EQ(ex.code(), ErrorCode::kFormatError);
    EXPECT_EQ(ex.exitCode(), 3);
    EXPECT_EQ(ex.message(), "Invalid file format");
}

TEST(FormatErrorTest, WithContext) {
    ErrorContext ctx;
    ctx.withFile("corrupt.fqc").withOffset(1024);

    FormatError ex("Corrupted header", ctx);
    EXPECT_TRUE(ex.hasContext());
    EXPECT_EQ(ex.context()->filePath, "corrupt.fqc");
    EXPECT_EQ(ex.context()->byteOffset.value(), 1024);
}

// =============================================================================
// ChecksumError Tests
// =============================================================================

TEST(ChecksumErrorTest, BasicConstruction) {
    ChecksumError ex("Checksum mismatch");

    EXPECT_EQ(ex.code(), ErrorCode::kChecksumError);
    EXPECT_EQ(ex.exitCode(), 4);
    EXPECT_EQ(ex.message(), "Checksum mismatch");
    EXPECT_FALSE(ex.expected().has_value());
    EXPECT_FALSE(ex.actual().has_value());
}

TEST(ChecksumErrorTest, WithValues) {
    ErrorContext ctx;
    ctx.withBlock(5);

    ChecksumError ex(0xDEADBEEF, 0xCAFEBABE, ctx);

    EXPECT_EQ(ex.code(), ErrorCode::kChecksumError);
    EXPECT_TRUE(ex.expected().has_value());
    EXPECT_TRUE(ex.actual().has_value());
    EXPECT_EQ(ex.expected().value(), 0xDEADBEEF);
    EXPECT_EQ(ex.actual().value(), 0xCAFEBABE);

    std::string what = ex.what();
    // Message should contain the values
    EXPECT_TRUE(what.find("deadbeef") != std::string::npos ||
                what.find("DEADBEEF") != std::string::npos ||
                what.find("3735928559") != std::string::npos);
}

// =============================================================================
// UnsupportedCodecError Tests
// =============================================================================

TEST(UnsupportedCodecErrorTest, BasicConstruction) {
    UnsupportedCodecError ex("Unknown codec family");

    EXPECT_EQ(ex.code(), ErrorCode::kUnsupportedCodec);
    EXPECT_EQ(ex.exitCode(), 5);
    EXPECT_EQ(ex.message(), "Unknown codec family");
    EXPECT_FALSE(ex.codecFamily().has_value());
}

TEST(UnsupportedCodecErrorTest, WithCodecFamily) {
    ErrorContext ctx;
    ctx.withBlock(10);

    UnsupportedCodecError ex(0x42, ctx);

    EXPECT_EQ(ex.code(), ErrorCode::kUnsupportedCodec);
    EXPECT_TRUE(ex.codecFamily().has_value());
    EXPECT_EQ(ex.codecFamily().value(), 0x42);
}

// =============================================================================
// Error Class Tests
// =============================================================================

TEST(ErrorTest, BasicConstruction) {
    Error err(ErrorCode::kIOError, "I/O failure");

    EXPECT_EQ(err.code(), ErrorCode::kIOError);
    EXPECT_EQ(err.message(), "I/O failure");
    EXPECT_EQ(err.exitCode(), 2);
}

TEST(ErrorTest, FromException) {
    IOError ex("File not found");
    Error err(ex);

    EXPECT_EQ(err.code(), ErrorCode::kIOError);
    EXPECT_EQ(err.message(), "File not found");
}

TEST(ErrorTest, ToException) {
    Error err(ErrorCode::kFormatError, "Bad format");
    FQCException ex = err.toException();

    EXPECT_EQ(ex.code(), ErrorCode::kFormatError);
    EXPECT_EQ(ex.message(), "Bad format");
}

TEST(ErrorTest, ThrowException) {
    Error err(ErrorCode::kIOError, "Test throw");

    try {
        err.throwException();
        FAIL() << "Expected exception to be thrown";
    } catch (const IOError& e) {
        EXPECT_EQ(e.message(), "Test throw");
    } catch (...) {
        FAIL() << "Expected IOError";
    }
}

// =============================================================================
// Result Type Tests (VoidResult)
// =============================================================================

TEST(VoidResultTest, SuccessValue) {
    VoidResult result = VoidResult{};
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result);
}

TEST(VoidResultTest, ErrorValue) {
    VoidResult result = std::unexpected(Error(ErrorCode::kIOError, "Failed"));
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::kIOError);
}

TEST(VoidResultTest, ThrowOnError) {
    VoidResult result = std::unexpected(Error(ErrorCode::kFileNotFound, "Not found"));

    try {
        result.error().throwException();
        FAIL() << "Expected exception";
    } catch (const IOError& e) {
        EXPECT_TRUE(std::string(e.what()).find("Not found") != std::string::npos);
    }
}

// =============================================================================
// Exception Hierarchy Tests
// =============================================================================

TEST(ExceptionHierarchyTest, AllDeriveFromFQCException) {
    EXPECT_TRUE((std::is_base_of_v<FQCException, UsageError>));
    EXPECT_TRUE((std::is_base_of_v<FQCException, IOError>));
    EXPECT_TRUE((std::is_base_of_v<FQCException, FormatError>));
    EXPECT_TRUE((std::is_base_of_v<FQCException, ChecksumError>));
    EXPECT_TRUE((std::is_base_of_v<FQCException, UnsupportedCodecError>));
}

TEST(ExceptionHierarchyTest, CanCatchBaseClass) {
    try {
        throw FormatError("Test error");
    } catch (const FQCException& e) {
        EXPECT_EQ(e.code(), ErrorCode::kFormatError);
    }
}

TEST(ExceptionHierarchyTest, CanCatchDerivedClass) {
    try {
        throw IOError("File error");
    } catch (const IOError& e) {
        EXPECT_EQ(e.code(), ErrorCode::kIOError);
    } catch (...) {
        FAIL() << "Should have caught IOError";
    }
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST(ErrorIntegrationTest, FullErrorFlow) {
    // Simulate a typical error flow

    // Create context
    ErrorContext ctx;
    ctx.withFile("test.fqc").withBlock(42).withRead(1234);

    // Throw exception
    try {
        throw FormatError("Invalid block header", ctx);
    } catch (const FQCException& e) {
        // Verify all properties
        EXPECT_EQ(e.code(), ErrorCode::kFormatError);
        EXPECT_EQ(e.exitCode(), 3);
        EXPECT_TRUE(e.hasContext());
        EXPECT_EQ(e.context()->filePath, "test.fqc");
        EXPECT_EQ(e.context()->blockId.value(), 42);
        EXPECT_EQ(e.context()->readId.value(), 1234);

        // Create Error from exception
        Error err(e);
        EXPECT_EQ(err.code(), ErrorCode::kFormatError);
        EXPECT_EQ(err.exitCode(), 3);
    }
}

TEST(ErrorIntegrationTest, ResultPattern) {
    // Function that returns a result
    auto readFile = [](bool success) -> VoidResult {
        if (success) {
            return {};
        }
        return std::unexpected(Error(ErrorCode::kFileNotFound, "File does not exist"));
    };

    // Success case
    auto result1 = readFile(true);
    EXPECT_TRUE(result1.has_value());

    // Failure case
    auto result2 = readFile(false);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().code(), ErrorCode::kFileNotFound);
    EXPECT_EQ(result2.error().exitCode(), 2);
}

}  // namespace
}  // namespace fqc
