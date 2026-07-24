// =============================================================================
// fq-compressor - Error Handling Unit Tests
// =============================================================================

#include "fqc/common/error.h"

#include <string>

#include <gtest/gtest.h>

namespace fqc {
namespace {

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

TEST(ErrorTest, BasicConstruction) {
    Error err{ErrorCode::kIOError, "I/O failure"};
    EXPECT_EQ(err.code, ErrorCode::kIOError);
    EXPECT_EQ(err.message, "I/O failure");
}

TEST(ResultTest, SuccessValue) {
    Result<int> result = 42;
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(ResultTest, ErrorValue) {
    Result<int> result = makeError<int>(ErrorCode::kIOError, "Failed");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::kIOError);
    EXPECT_EQ(result.error().message, "Failed");
}

TEST(ResultTest, MakeErrorFromError) {
    Error err{ErrorCode::kFormatError, "bad format"};
    Result<int> result = makeError<int>(err);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::kFormatError);
}

TEST(VoidResultTest, SuccessValue) {
    VoidResult result{};
    EXPECT_TRUE(result.has_value());
}

TEST(VoidResultTest, ErrorValue) {
    VoidResult result = makeVoidError(ErrorCode::kIOError, "Failed");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::kIOError);
}

TEST(VoidResultTest, ResultPattern) {
    auto readFile = [](bool success) -> VoidResult {
        if (success) {
            return {};
        }
        return makeVoidError(ErrorCode::kIOError, "File does not exist");
    };

    auto result1 = readFile(true);
    EXPECT_TRUE(result1.has_value());

    auto result2 = readFile(false);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().code, ErrorCode::kIOError);
}

}  // namespace
}  // namespace fqc
