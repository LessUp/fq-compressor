// =============================================================================
// fq-compressor - Async IO Unit Tests
// =============================================================================
// Unit tests for AsyncReader, AsyncWriter, and BufferPool components.
// =============================================================================

#include "fqc/io/async_io.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <random>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace fqc::io {
namespace {

// =============================================================================
// BufferPool Tests
// =============================================================================

class BufferPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use small buffers for testing
        testBufferSize_ = 1024;
        testBufferCount_ = 4;
    }

    std::size_t testBufferSize_;
    std::size_t testBufferCount_;
};

TEST_F(BufferPoolTest, CreateAndDestroy) {
    BufferPool pool(testBufferSize_, testBufferCount_);
    EXPECT_EQ(pool.bufferSize(), testBufferSize_);
    EXPECT_EQ(pool.bufferCount(), testBufferCount_);
    EXPECT_EQ(pool.availableCount(), testBufferCount_);
    EXPECT_FALSE(pool.empty());
}

TEST_F(BufferPoolTest, AcquireSingleBuffer) {
    BufferPool pool(testBufferSize_, testBufferCount_);

    auto buffer = pool.acquire();
    ASSERT_TRUE(buffer.valid());
    EXPECT_EQ(buffer.capacity(), testBufferSize_);
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(pool.availableCount(), testBufferCount_ - 1);
}

TEST_F(BufferPoolTest, AcquireAllBuffers) {
    BufferPool pool(testBufferSize_, testBufferCount_);

    std::vector<ManagedBuffer> buffers;
    for (std::size_t i = 0; i < testBufferCount_; ++i) {
        auto buffer = pool.acquire();
        ASSERT_TRUE(buffer.valid());
        buffers.push_back(std::move(buffer));
    }

    EXPECT_TRUE(pool.empty());
    EXPECT_EQ(pool.availableCount(), 0);
}

TEST_F(BufferPoolTest, AcquireReleaseCycle) {
    BufferPool pool(testBufferSize_, testBufferCount_);

    // Acquire all buffers
    std::vector<ManagedBuffer> buffers;
    for (std::size_t i = 0; i < testBufferCount_; ++i) {
        buffers.push_back(pool.acquire());
    }

    EXPECT_TRUE(pool.empty());

    // Release buffers
    buffers.clear();

    EXPECT_EQ(pool.availableCount(), testBufferCount_);
    EXPECT_FALSE(pool.empty());
}

TEST_F(BufferPoolTest, TryAcquireNonBlocking) {
    BufferPool pool(testBufferSize_, testBufferCount_);

    // Acquire all buffers
    std::vector<ManagedBuffer> buffers;
    for (std::size_t i = 0; i < testBufferCount_; ++i) {
        buffers.push_back(pool.acquire());
    }

    // TryAcquire should return nullopt
    auto buffer = pool.tryAcquire();
    EXPECT_FALSE(buffer.has_value());

    // Release one buffer
    buffers.pop_back();

    // TryAcquire should now succeed
    buffer = pool.tryAcquire();
    EXPECT_TRUE(buffer.has_value());
}

TEST_F(BufferPoolTest, AcquireWithTimeout) {
    BufferPool pool(testBufferSize_, testBufferCount_);

    // Acquire all buffers
    std::vector<ManagedBuffer> buffers;
    for (std::size_t i = 0; i < testBufferCount_; ++i) {
        buffers.push_back(pool.acquire());
    }

    // AcquireWithTimeout should timeout
    auto start = std::chrono::steady_clock::now();
    auto buffer = pool.acquireWithTimeout(100);  // 100ms
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(buffer.has_value());
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 90);
}

TEST_F(BufferPoolTest, BufferMoveSemantics) {
    BufferPool pool(testBufferSize_, testBufferCount_);

    auto buffer1 = pool.acquire();
    ASSERT_TRUE(buffer1.valid());

    // Write some data
    std::memset(buffer1.data(), 0xAB, testBufferSize_);
    buffer1.setSize(testBufferSize_);

    // Move construct
    ManagedBuffer buffer2(std::move(buffer1));
    EXPECT_FALSE(buffer1.valid());
    EXPECT_TRUE(buffer2.valid());
    EXPECT_EQ(buffer2.size(), testBufferSize_);
    EXPECT_EQ(buffer2.data()[0], 0xAB);

    // Move assign
    ManagedBuffer buffer3;
    buffer3 = std::move(buffer2);
    EXPECT_FALSE(buffer2.valid());
    EXPECT_TRUE(buffer3.valid());
    EXPECT_EQ(buffer3.size(), testBufferSize_);
}

TEST_F(BufferPoolTest, BufferSpanAccess) {
    BufferPool pool(testBufferSize_, testBufferCount_);

    auto buffer = pool.acquire();
    ASSERT_TRUE(buffer.valid());

    // Fill buffer with test data
    std::vector<std::uint8_t> testData(testBufferSize_, 0x42);
    std::memcpy(buffer.data(), testData.data(), testBufferSize_);
    buffer.setSize(testBufferSize_);

    // Access via span
    auto span = buffer.span();
    EXPECT_EQ(span.size(), testBufferSize_);
    EXPECT_EQ(span[0], 0x42);
    EXPECT_EQ(span[testBufferSize_ - 1], 0x42);
}

TEST_F(BufferPoolTest, Reset) {
    BufferPool pool(testBufferSize_, testBufferCount_);

    // Acquire some buffers
    std::vector<ManagedBuffer> buffers;
    for (std::size_t i = 0; i < testBufferCount_ / 2; ++i) {
        buffers.push_back(pool.acquire());
    }

    EXPECT_EQ(pool.availableCount(), testBufferCount_ / 2);

    // Reset should return all buffers
    pool.reset();

    EXPECT_EQ(pool.availableCount(), testBufferCount_);
}

// =============================================================================
// AsyncReader Tests
// =============================================================================

class AsyncReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test file
        testFilePath_ = std::filesystem::temp_directory_path() / "fqc_async_reader_test.bin";

        // Generate test data
        testData_.resize(64 * 1024);  // 64KB
        std::mt19937 rng(42);
        std::uniform_int_distribution<std::uint8_t> dist(0, 255);
        for (auto& byte : testData_) {
            byte = dist(rng);
        }

        // Write test data to file
        std::ofstream file(testFilePath_, std::ios::binary);
        file.write(reinterpret_cast<const char*>(testData_.data()), testData_.size());
    }

    void TearDown() override {
        std::filesystem::remove(testFilePath_);
    }

    std::filesystem::path testFilePath_;
    std::vector<std::uint8_t> testData_;
};

TEST_F(AsyncReaderTest, OpenAndClose) {
    AsyncReaderConfig config;
    config.bufferSize = 1024;
    config.bufferCount = 2;

    AsyncReader reader(config);
    auto result = reader.open(testFilePath_);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(reader.isOpen());
    EXPECT_FALSE(reader.eof());

    reader.close();
    EXPECT_FALSE(reader.isOpen());
}

TEST_F(AsyncReaderTest, ReadEntireFile) {
    AsyncReaderConfig config;
    config.bufferSize = 16 * 1024;  // 16KB buffers
    config.bufferCount = 4;

    AsyncReader reader(config);
    ASSERT_TRUE(reader.open(testFilePath_).has_value());

    std::vector<std::uint8_t> allData;
    std::size_t totalRead = 0;

    while (auto buffer = reader.read()) {
        allData.insert(allData.end(), buffer->data(), buffer->data() + buffer->size());
        totalRead += buffer->size();
    }

    EXPECT_EQ(totalRead, testData_.size());
    EXPECT_EQ(allData, testData_);
    EXPECT_TRUE(reader.eof());
}

TEST_F(AsyncReaderTest, HasMoreAndEof) {
    AsyncReaderConfig config;
    config.bufferSize = testData_.size() / 2;  // Need 2 reads

    AsyncReader reader(config);
    ASSERT_TRUE(reader.open(testFilePath_).has_value());

    EXPECT_TRUE(reader.hasMore());
    EXPECT_FALSE(reader.eof());

    // First read
    auto buffer1 = reader.read();
    ASSERT_TRUE(buffer1.has_value());
    EXPECT_TRUE(reader.hasMore());

    // Second read (EOF)
    auto buffer2 = reader.read();
    ASSERT_TRUE(buffer2.has_value());

    // Third read should be nullopt
    auto buffer3 = reader.read();
    EXPECT_FALSE(buffer3.has_value());
    EXPECT_TRUE(reader.eof());
}

TEST_F(AsyncReaderTest, TotalBytesRead) {
    AsyncReaderConfig config;
    config.bufferSize = 16 * 1024;

    AsyncReader reader(config);
    ASSERT_TRUE(reader.open(testFilePath_).has_value());

    EXPECT_EQ(reader.totalBytesRead(), 0);
    EXPECT_EQ(reader.fileSize(), testData_.size());

    while (auto buffer = reader.read()) {
        // Just consume
    }

    EXPECT_EQ(reader.totalBytesRead(), testData_.size());
}

TEST_F(AsyncReaderTest, Position) {
    AsyncReaderConfig config;
    config.bufferSize = 16 * 1024;

    AsyncReader reader(config);
    ASSERT_TRUE(reader.open(testFilePath_).has_value());

    std::uint64_t expectedPos = 0;
    while (auto buffer = reader.read()) {
        expectedPos += buffer->size();
        EXPECT_EQ(reader.position(), expectedPos);
    }
}

TEST_F(AsyncReaderTest, ReadWithTimeout) {
    AsyncReaderConfig config;
    config.bufferSize = 16 * 1024;

    AsyncReader reader(config);
    ASSERT_TRUE(reader.open(testFilePath_).has_value());

    // Normal read with timeout should succeed
    auto buffer = reader.readWithTimeout(1000);
    ASSERT_TRUE(buffer.has_value());
    EXPECT_GT(buffer->size(), 0);

    // Read until EOF
    while (reader.readWithTimeout(100)) {
    }

    // At EOF, should return nullopt quickly
    buffer = reader.readWithTimeout(100);
    EXPECT_FALSE(buffer.has_value());
}

TEST_F(AsyncReaderTest, InvalidFile) {
    AsyncReader reader;
    auto result = reader.open("/nonexistent/path/to/file.bin");
    EXPECT_FALSE(result.has_value());
}

TEST_F(AsyncReaderTest, ConfigValidation) {
    AsyncReaderConfig config;
    config.bufferSize = 0;  // Invalid

    auto result = config.validate();
    EXPECT_FALSE(result.has_value());

    config.bufferSize = 1024;
    config.bufferCount = 0;  // Invalid

    result = config.validate();
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// AsyncWriter Tests
// =============================================================================

class AsyncWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        testFilePath_ = std::filesystem::temp_directory_path() / "fqc_async_writer_test.bin";

        // Generate test data
        testData_.resize(64 * 1024);  // 64KB
        std::mt19937 rng(42);
        std::uniform_int_distribution<std::uint8_t> dist(0, 255);
        for (auto& byte : testData_) {
            byte = dist(rng);
        }
    }

    void TearDown() override {
        std::filesystem::remove(testFilePath_);
    }

    void verifyWrittenData() {
        std::ifstream file(testFilePath_, std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> written(size);
        file.read(reinterpret_cast<char*>(written.data()), size);

        EXPECT_EQ(written.size(), testData_.size());
        EXPECT_EQ(written, testData_);
    }

    std::filesystem::path testFilePath_;
    std::vector<std::uint8_t> testData_;
};

TEST_F(AsyncWriterTest, OpenAndClose) {
    AsyncWriterConfig config;
    config.bufferSize = 1024;
    config.bufferCount = 2;

    AsyncWriter writer(config);
    auto result = writer.open(testFilePath_);
    ASSERT_TRUE(result.has_value());

    auto finalizeResult = writer.finalize();
    ASSERT_TRUE(finalizeResult.has_value());
}

TEST_F(AsyncWriterTest, WriteSingleBuffer) {
    AsyncWriterConfig config;
    config.bufferSize = testData_.size();

    AsyncWriter writer(config);
    ASSERT_TRUE(writer.open(testFilePath_).has_value());

    auto buffer = writer.acquireBuffer();
    std::memcpy(buffer.data(), testData_.data(), testData_.size());
    buffer.setSize(testData_.size());

    auto writeResult = writer.write(std::move(buffer));
    ASSERT_TRUE(writeResult.has_value());

    auto finalizeResult = writer.finalize();
    ASSERT_TRUE(finalizeResult.has_value());

    verifyWrittenData();
}

TEST_F(AsyncWriterTest, WriteMultipleBuffers) {
    AsyncWriterConfig config;
    config.bufferSize = 16 * 1024;  // 16KB buffers

    AsyncWriter writer(config);
    ASSERT_TRUE(writer.open(testFilePath_).has_value());

    std::size_t offset = 0;
    while (offset < testData_.size()) {
        auto buffer = writer.acquireBuffer();
        std::size_t toWrite = std::min(buffer.capacity(), testData_.size() - offset);
        std::memcpy(buffer.data(), testData_.data() + offset, toWrite);
        buffer.setSize(toWrite);
        offset += toWrite;

        auto writeResult = writer.write(std::move(buffer));
        ASSERT_TRUE(writeResult.has_value());
    }

    auto finalizeResult = writer.finalize();
    ASSERT_TRUE(finalizeResult.has_value());

    verifyWrittenData();
}

TEST_F(AsyncWriterTest, WriteDirectSpan) {
    AsyncWriterConfig config;
    config.bufferSize = 16 * 1024;

    AsyncWriter writer(config);
    ASSERT_TRUE(writer.open(testFilePath_).has_value());

    // Write directly via span
    auto writeResult = writer.write(std::span<const std::uint8_t>(testData_));
    ASSERT_TRUE(writeResult.has_value());

    auto finalizeResult = writer.finalize();
    ASSERT_TRUE(finalizeResult.has_value());

    verifyWrittenData();
}

TEST_F(AsyncWriterTest, TotalBytesWritten) {
    AsyncWriterConfig config;
    config.bufferSize = 16 * 1024;

    AsyncWriter writer(config);
    ASSERT_TRUE(writer.open(testFilePath_).has_value());

    EXPECT_EQ(writer.totalBytesWritten(), 0);

    // Write data in chunks
    std::size_t offset = 0;
    while (offset < testData_.size()) {
        std::size_t chunkSize = std::min(static_cast<std::size_t>(16 * 1024), testData_.size() - offset);
        auto writeResult = writer.write(std::span<const std::uint8_t>(testData_.data() + offset, chunkSize));
        ASSERT_TRUE(writeResult.has_value());
        offset += chunkSize;
    }

    auto finalizeResult = writer.finalize();
    ASSERT_TRUE(finalizeResult.has_value());

    EXPECT_EQ(writer.totalBytesWritten(), testData_.size());
}

TEST_F(AsyncWriterTest, Flush) {
    AsyncWriterConfig config;
    config.bufferSize = testData_.size();

    AsyncWriter writer(config);
    ASSERT_TRUE(writer.open(testFilePath_).has_value());

    auto buffer = writer.acquireBuffer();
    std::memcpy(buffer.data(), testData_.data(), testData_.size());
    buffer.setSize(testData_.size());
    writer.write(std::move(buffer));

    auto flushResult = writer.flush();
    ASSERT_TRUE(flushResult.has_value());

    auto finalizeResult = writer.finalize();
    ASSERT_TRUE(finalizeResult.has_value());

    verifyWrittenData();
}

TEST_F(AsyncWriterTest, Abort) {
    AsyncWriterConfig config;

    AsyncWriter writer(config);
    ASSERT_TRUE(writer.open(testFilePath_).has_value());

    // Write some data
    auto buffer = writer.acquireBuffer();
    std::memset(buffer.data(), 0, buffer.capacity());
    buffer.setSize(buffer.capacity());
    writer.write(std::move(buffer));

    // Abort should clean up
    writer.abort();

    // File should not exist or be empty
    EXPECT_FALSE(std::filesystem::exists(testFilePath_) ||
                 std::filesystem::file_size(testFilePath_) == 0);
}

TEST_F(AsyncWriterTest, TryAcquireBuffer) {
    AsyncWriterConfig config;
    config.bufferCount = 2;

    AsyncWriter writer(config);
    ASSERT_TRUE(writer.open(testFilePath_).has_value());

    // Should be able to acquire all configured buffers
    auto buffer1 = writer.tryAcquireBuffer();
    EXPECT_TRUE(buffer1.has_value());

    auto buffer2 = writer.tryAcquireBuffer();
    EXPECT_TRUE(buffer2.has_value());

    // No more buffers available
    auto buffer3 = writer.tryAcquireBuffer();
    EXPECT_FALSE(buffer3.has_value());

    // Return one buffer
    buffer1->setSize(0);
    writer.write(std::move(*buffer1));

    // Now should be able to acquire again
    buffer3 = writer.tryAcquireBuffer();
    EXPECT_TRUE(buffer3.has_value());

    writer.abort();
}

TEST_F(AsyncWriterTest, ConfigValidation) {
    AsyncWriterConfig config;
    config.bufferSize = 0;  // Invalid

    auto result = config.validate();
    EXPECT_FALSE(result.has_value());

    config.bufferSize = 1024;
    config.bufferCount = 0;  // Invalid

    result = config.validate();
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Integration Tests (Reader + Writer)
// =============================================================================

TEST(AsyncIOIntegrationTest, RoundTrip) {
    // Create test data
    std::vector<std::uint8_t> originalData(256 * 1024);  // 256KB
    std::mt19937 rng(12345);
    std::uniform_int_distribution<std::uint8_t> dist(0, 255);
    for (auto& byte : originalData) {
        byte = dist(rng);
    }

    auto tempPath = std::filesystem::temp_directory_path() / "fqc_roundtrip_test.bin";

    // Write
    {
        AsyncWriterConfig config;
        config.bufferSize = 32 * 1024;

        AsyncWriter writer(config);
        ASSERT_TRUE(writer.open(tempPath).has_value());
        ASSERT_TRUE(writer.write(std::span<const std::uint8_t>(originalData)).has_value());
        ASSERT_TRUE(writer.finalize().has_value());
    }

    // Read
    {
        AsyncReaderConfig config;
        config.bufferSize = 32 * 1024;

        AsyncReader reader(config);
        ASSERT_TRUE(reader.open(tempPath).has_value());

        std::vector<std::uint8_t> readData;
        while (auto buffer = reader.read()) {
            readData.insert(readData.end(), buffer->data(), buffer->data() + buffer->size());
        }

        EXPECT_EQ(readData, originalData);
    }

    std::filesystem::remove(tempPath);
}

TEST(AsyncIOIntegrationTest, ConcurrentReadWrite) {
    // Test that we can read and write concurrently using separate threads
    std::vector<std::uint8_t> originalData(128 * 1024);
    std::mt19937 rng(54321);
    std::uniform_int_distribution<std::uint8_t> dist(0, 255);
    for (auto& byte : originalData) {
        byte = dist(rng);
    }

    auto tempPath = std::filesystem::temp_directory_path() / "fqc_concurrent_test.bin";

    // Write in background thread
    std::thread writeThread([&]() {
        AsyncWriterConfig config;
        config.bufferSize = 16 * 1024;

        AsyncWriter writer(config);
        ASSERT_TRUE(writer.open(tempPath).has_value());

        std::size_t offset = 0;
        while (offset < originalData.size()) {
            std::size_t chunkSize = std::min(static_cast<std::size_t>(16 * 1024),
                                             originalData.size() - offset);
            auto buffer = writer.acquireBuffer();
            std::memcpy(buffer.data(), originalData.data() + offset, chunkSize);
            buffer.setSize(chunkSize);
            writer.write(std::move(buffer));
            offset += chunkSize;

            // Simulate processing delay
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        writer.finalize();
    });

    writeThread.join();

    // Verify data
    AsyncReaderConfig config;
    config.bufferSize = 16 * 1024;

    AsyncReader reader(config);
    ASSERT_TRUE(reader.open(tempPath).has_value());

    std::vector<std::uint8_t> readData;
    while (auto buffer = reader.read()) {
        readData.insert(readData.end(), buffer->data(), buffer->data() + buffer->size());
    }

    EXPECT_EQ(readData, originalData);
    std::filesystem::remove(tempPath);
}

}  // namespace
}  // namespace fqc::io
