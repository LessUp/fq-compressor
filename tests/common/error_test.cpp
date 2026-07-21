// =============================================================================
// fq-compressor - Error Handling Unit Tests
// =============================================================================
// Unit tests for ErrorCode, FQCException, and Result types.
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
}

TEST(ErrorCodeTest, ToExitCodeIOError) {
    EXPECT_EQ(toExitCode(ErrorCode::kIOError), 2);
}

TEST(ErrorCodeTest, ToExitCodeFormatError) {
    EXPECT_EQ(toExitCode(ErrorCode::kFormatError), 3);
    EXPECT_EQ(toExitCode(ErrorCode::kInternalError), 3);
}

TEST(ErrorCodeTest, ToExitCodeChecksumError) {
    EXPECT_EQ(toExitCode(ErrorCode::kChecksumError), 4);
}

TEST(ErrorCodeTest, ToExitCodeCodecError) {
    EXPECT_EQ(toExitCode(ErrorCode::kUnsupportedCodec), 5);
}

TEST(ErrorCodeTest, ErrorCodeToString) {
    EXPECT_EQ(errorCodeToString(ErrorCode::kSuccess), "success");
    EXPECT_EQ(errorCodeToString(ErrorCode::kUsageError), "usage error");
    EXPECT_EQ(errorCodeToString(ErrorCode::kIOError), "I/O error");
    EXPECT_EQ(errorCodeToString(ErrorCode::kFormatError), "format error");
    EXPECT_EQ(errorCodeToString(ErrorCode::kChecksumError), "checksum error");
    EXPECT_EQ(errorCodeToString(ErrorCode::kUnsupportedCodec), "unsupported codec");
    EXPECT_EQ(errorCodeToString(ErrorCode::kInternalError), "internal error");
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
// FQCException Tests
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

    FQCException ex2(ex1);
    EXPECT_EQ(ex2.code(), ErrorCode::kIOError);
    EXPECT_EQ(ex2.message(), "Test error");

    FQCException ex3(std::move(ex2));
    EXPECT_EQ(ex3.code(), ErrorCode::kIOError);

    FQCException ex4 = ex3;
    EXPECT_EQ(ex4.code(), ErrorCode::kIOError);

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

TEST(FQCExceptionTest, AllExitCodes) {
    EXPECT_EQ(FQCException(ErrorCode::kUsageError, "").exitCode(), 1);
    EXPECT_EQ(FQCException(ErrorCode::kIOError, "").exitCode(), 2);
    EXPECT_EQ(FQCException(ErrorCode::kFormatError, "").exitCode(), 3);
    EXPECT_EQ(FQCException(ErrorCode::kInternalError, "").exitCode(), 3);
    EXPECT_EQ(FQCException(ErrorCode::kChecksumError, "").exitCode(), 4);
    EXPECT_EQ(FQCException(ErrorCode::kUnsupportedCodec, "").exitCode(), 5);
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
    FQCException ex(ErrorCode::kIOError, "File not found");
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
    } catch (const FQCException& e) {
        EXPECT_EQ(e.code(), ErrorCode::kIOError);
        EXPECT_EQ(e.message(), "Test throw");
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
    VoidResult result = std::unexpected(Error(ErrorCode::kIOError, "Not found"));

    try {
        result.error().throwException();
        FAIL() << "Expected exception";
    } catch (const FQCException& e) {
        EXPECT_TRUE(std::string(e.what()).find("Not found") != std::string::npos);
    }
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST(ErrorIntegrationTest, FullErrorFlow) {
    ErrorContext ctx;
    ctx.withFile("test.fqc").withBlock(42).withRead(1234);

    try {
        throw FQCException(ErrorCode::kFormatError, "Invalid block header", ctx);
    } catch (const FQCException& e) {
        EXPECT_EQ(e.code(), ErrorCode::kFormatError);
        EXPECT_EQ(e.exitCode(), 3);
        EXPECT_TRUE(e.hasContext());
        EXPECT_EQ(e.context()->filePath, "test.fqc");
        EXPECT_EQ(e.context()->blockId.value(), 42);
        EXPECT_EQ(e.context()->readId.value(), 1234);

        Error err(e);
        EXPECT_EQ(err.code(), ErrorCode::kFormatError);
        EXPECT_EQ(err.exitCode(), 3);
    }
}

TEST(ErrorIntegrationTest, ResultPattern) {
    auto readFile = [](bool success) -> VoidResult {
        if (success) {
            return {};
        }
        return std::unexpected(Error(ErrorCode::kIOError, "File does not exist"));
    };

    auto result1 = readFile(true);
    EXPECT_TRUE(result1.has_value());

    auto result2 = readFile(false);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().code(), ErrorCode::kIOError);
    EXPECT_EQ(result2.error().exitCode(), 2);
}

}  // namespace
}  // namespace fqc
