// =============================================================================
// fq-compressor - Async IO Module
// =============================================================================
// Implements asynchronous I/O operations for improved pipeline throughput.
//
// Key features:
// - Double buffering for overlapped read/write operations
// - Async file reading with prefetch
// - Async file writing with write-behind
// - Thread-safe buffer management
// - Backpressure support
//
// Design inspired by Pigz's parallel I/O model.
//
// Requirements: 4.1 (Parallel processing), 16.1 (Async IO)
// =============================================================================

#ifndef FQC_IO_ASYNC_IO_H
#define FQC_IO_ASYNC_IO_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc::io {

// =============================================================================
// Forward Declarations
// =============================================================================

class AsyncReaderImpl;
class AsyncWriterImpl;
class BufferPool;

// =============================================================================
// Constants
// =============================================================================

/// @brief Default buffer size for async operations (4MB)
inline constexpr std::size_t kDefaultAsyncBufferSize = 4 * 1024 * 1024;

/// @brief Default number of buffers in pool
inline constexpr std::size_t kDefaultBufferCount = 4;

/// @brief Default prefetch depth (number of buffers to read ahead)
inline constexpr std::size_t kDefaultPrefetchDepth = 2;

/// @brief Default write-behind depth (number of buffers to queue for writing)
inline constexpr std::size_t kDefaultWriteBehindDepth = 2;

// =============================================================================
// Buffer Types
// =============================================================================

/// @brief A managed buffer from the buffer pool
class ManagedBuffer {
public:
    /// @brief Default constructor (empty buffer)
    ManagedBuffer() = default;

    /// @brief Construct with data and pool reference
    /// @param data Buffer data
    /// @param size Buffer size
    /// @param pool Pool to return buffer to
    ManagedBuffer(std::uint8_t* data, std::size_t size, BufferPool* pool);

    /// @brief Destructor - returns buffer to pool
    ~ManagedBuffer();

    // Non-copyable, movable
    ManagedBuffer(const ManagedBuffer&) = delete;
    ManagedBuffer& operator=(const ManagedBuffer&) = delete;
    ManagedBuffer(ManagedBuffer&& other) noexcept;
    ManagedBuffer& operator=(ManagedBuffer&& other) noexcept;

    /// @brief Get pointer to buffer data
    [[nodiscard]] std::uint8_t* data() noexcept { return data_; }
    [[nodiscard]] const std::uint8_t* data() const noexcept { return data_; }

    /// @brief Get buffer capacity
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    /// @brief Get current data size
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /// @brief Set current data size
    void setSize(std::size_t size) noexcept { size_ = std::min(size, capacity_); }

    /// @brief Check if buffer is valid
    [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }

    /// @brief Check if buffer is empty
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /// @brief Get span view of data
    [[nodiscard]] std::span<std::uint8_t> span() noexcept {
        return {data_, size_};
    }
    [[nodiscard]] std::span<const std::uint8_t> span() const noexcept {
        return {data_, size_};
    }

    /// @brief Release buffer without returning to pool
    std::uint8_t* release() noexcept;

private:
    std::uint8_t* data_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t size_ = 0;
    BufferPool* pool_ = nullptr;
};

// =============================================================================
// Buffer Pool
// =============================================================================

/// @brief Thread-safe pool of reusable buffers
///
/// Manages a fixed set of buffers to avoid repeated allocations
/// during I/O operations. Supports blocking and non-blocking acquisition.
class BufferPool {
public:
    /// @brief Construct with buffer parameters
    /// @param bufferSize Size of each buffer
    /// @param bufferCount Number of buffers in pool
    explicit BufferPool(
        std::size_t bufferSize = kDefaultAsyncBufferSize,
        std::size_t bufferCount = kDefaultBufferCount);

    /// @brief Destructor
    ~BufferPool();

    // Non-copyable, non-movable
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    BufferPool(BufferPool&&) = delete;
    BufferPool& operator=(BufferPool&&) = delete;

    /// @brief Acquire a buffer (blocks if none available)
    /// @return Managed buffer
    [[nodiscard]] ManagedBuffer acquire();

    /// @brief Try to acquire a buffer (non-blocking)
    /// @return Managed buffer or nullopt if none available
    [[nodiscard]] std::optional<ManagedBuffer> tryAcquire();

    /// @brief Acquire a buffer with timeout
    /// @param timeoutMs Timeout in milliseconds
    /// @return Managed buffer or nullopt if timeout
    [[nodiscard]] std::optional<ManagedBuffer> acquireWithTimeout(
        std::uint32_t timeoutMs);

    /// @brief Return a buffer to the pool
    /// @param data Buffer data pointer
    void release(std::uint8_t* data);

    /// @brief Get buffer size
    [[nodiscard]] std::size_t bufferSize() const noexcept { return bufferSize_; }

    /// @brief Get total buffer count
    [[nodiscard]] std::size_t bufferCount() const noexcept { return bufferCount_; }

    /// @brief Get number of available buffers
    [[nodiscard]] std::size_t availableCount() const noexcept;

    /// @brief Check if pool is empty (no buffers available)
    [[nodiscard]] bool empty() const noexcept;

    /// @brief Reset pool (returns all buffers)
    void reset();

private:
    /// @brief Custom deleter for aligned memory
    struct AlignedDeleter {
        void operator()(std::uint8_t* ptr) const noexcept {
            if (ptr) { std::free(ptr); }
        }
    };
    
    /// @brief Aligned buffer type for cache-line alignment
    using AlignedBuffer = std::unique_ptr<std::uint8_t[], AlignedDeleter>;
    
    std::size_t bufferSize_;
    std::size_t bufferCount_;
    std::vector<AlignedBuffer> buffers_;
    std::queue<std::uint8_t*> available_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

// =============================================================================
// Async Reader Configuration
// =============================================================================

/// @brief Configuration for async reader
struct AsyncReaderConfig {
    /// @brief Buffer size for each read operation
    std::size_t bufferSize = kDefaultAsyncBufferSize;

    /// @brief Number of buffers in pool
    std::size_t bufferCount = kDefaultBufferCount;

    /// @brief Prefetch depth (buffers to read ahead)
    std::size_t prefetchDepth = kDefaultPrefetchDepth;

    /// @brief Enable memory mapping for large files
    bool enableMmap = false;

    /// @brief Minimum file size for mmap (bytes)
    std::size_t mmapThreshold = 100 * 1024 * 1024;  // 100MB

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

// =============================================================================
// Async Reader
// =============================================================================

/// @brief Asynchronous file reader with prefetch
///
/// Implements double-buffering and read-ahead to overlap I/O with processing.
/// Uses a background thread to prefetch data while the main thread processes.
///
/// Usage:
/// @code
/// AsyncReaderConfig config;
/// config.prefetchDepth = 2;
///
/// AsyncReader reader(config);
/// reader.open("input.fastq");
///
/// while (auto buffer = reader.read()) {
///     // Process buffer->data(), buffer->size()
/// }
/// @endcode
class AsyncReader {
public:
    /// @brief Construct with configuration
    /// @param config Reader configuration
    explicit AsyncReader(AsyncReaderConfig config = {});

    /// @brief Destructor
    ~AsyncReader();

    // Non-copyable, movable
    AsyncReader(const AsyncReader&) = delete;
    AsyncReader& operator=(const AsyncReader&) = delete;
    AsyncReader(AsyncReader&&) noexcept;
    AsyncReader& operator=(AsyncReader&&) noexcept;

    /// @brief Open file for reading
    /// @param path File path
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult open(const std::filesystem::path& path);

    /// @brief Open stdin for reading
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult openStdin();

    /// @brief Read next buffer
    /// @return Buffer with data, or nullopt if EOF
    [[nodiscard]] std::optional<ManagedBuffer> read();

    /// @brief Read with timeout
    /// @param timeoutMs Timeout in milliseconds
    /// @return Buffer with data, nullopt if EOF or timeout
    [[nodiscard]] std::optional<ManagedBuffer> readWithTimeout(std::uint32_t timeoutMs);

    /// @brief Check if more data is available
    [[nodiscard]] bool hasMore() const noexcept;

    /// @brief Check if at end of file
    [[nodiscard]] bool eof() const noexcept;

    /// @brief Get total bytes read
    [[nodiscard]] std::uint64_t totalBytesRead() const noexcept;

    /// @brief Get file size (0 if unknown, e.g., stdin)
    [[nodiscard]] std::uint64_t fileSize() const noexcept;

    /// @brief Get current position
    [[nodiscard]] std::uint64_t position() const noexcept;

    /// @brief Close the reader
    void close() noexcept;

    /// @brief Check if reader is open
    [[nodiscard]] bool isOpen() const noexcept;

    /// @brief Get configuration
    [[nodiscard]] const AsyncReaderConfig& config() const noexcept;

private:
    std::unique_ptr<AsyncReaderImpl> impl_;
};

// =============================================================================
// Async Writer Configuration
// =============================================================================

/// @brief Configuration for async writer
struct AsyncWriterConfig {
    /// @brief Buffer size for each write operation
    std::size_t bufferSize = kDefaultAsyncBufferSize;

    /// @brief Number of buffers in pool
    std::size_t bufferCount = kDefaultBufferCount;

    /// @brief Write-behind depth (buffers to queue)
    std::size_t writeBehindDepth = kDefaultWriteBehindDepth;

    /// @brief Use atomic write (temp file + rename)
    bool atomicWrite = true;

    /// @brief Sync to disk after each write
    bool syncOnWrite = false;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

// =============================================================================
// Async Writer
// =============================================================================

/// @brief Asynchronous file writer with write-behind
///
/// Implements double-buffering and write-behind to overlap I/O with processing.
/// Uses a background thread to write data while the main thread prepares more.
///
/// Usage:
/// @code
/// AsyncWriterConfig config;
/// config.atomicWrite = true;
///
/// AsyncWriter writer(config);
/// writer.open("output.fqc");
///
/// auto buffer = writer.acquireBuffer();
/// // Fill buffer...
/// buffer.setSize(dataSize);
/// writer.write(std::move(buffer));
///
/// writer.finalize();
/// @endcode
class AsyncWriter {
public:
    /// @brief Construct with configuration
    /// @param config Writer configuration
    explicit AsyncWriter(AsyncWriterConfig config = {});

    /// @brief Destructor
    ~AsyncWriter();

    // Non-copyable, movable
    AsyncWriter(const AsyncWriter&) = delete;
    AsyncWriter& operator=(const AsyncWriter&) = delete;
    AsyncWriter(AsyncWriter&&) noexcept;
    AsyncWriter& operator=(AsyncWriter&&) noexcept;

    /// @brief Open file for writing
    /// @param path File path
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult open(const std::filesystem::path& path);

    /// @brief Open stdout for writing
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult openStdout();

    /// @brief Acquire a buffer for writing
    /// @return Managed buffer
    [[nodiscard]] ManagedBuffer acquireBuffer();

    /// @brief Try to acquire a buffer (non-blocking)
    /// @return Managed buffer or nullopt if none available
    [[nodiscard]] std::optional<ManagedBuffer> tryAcquireBuffer();

    /// @brief Write a buffer (async)
    /// @param buffer Buffer to write
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult write(ManagedBuffer buffer);

    /// @brief Write data directly (copies to internal buffer)
    /// @param data Data to write
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult write(std::span<const std::uint8_t> data);

    /// @brief Flush pending writes
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult flush();

    /// @brief Finalize and close (commits atomic write)
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult finalize();

    /// @brief Abort and cleanup (removes temp file)
    void abort() noexcept;

    /// @brief Get total bytes written
    [[nodiscard]] std::uint64_t totalBytesWritten() const noexcept;

    /// @brief Get current position
    [[nodiscard]] std::uint64_t position() const noexcept;

    /// @brief Check if writer is open
    [[nodiscard]] bool isOpen() const noexcept;

    /// @brief Check if writer is finalized
    [[nodiscard]] bool isFinalized() const noexcept;

    /// @brief Get configuration
    [[nodiscard]] const AsyncWriterConfig& config() const noexcept;

private:
    std::unique_ptr<AsyncWriterImpl> impl_;
};

// =============================================================================
// Double Buffer Helper
// =============================================================================

/// @brief Double buffer for ping-pong style I/O
///
/// Manages two buffers that alternate between filling and draining,
/// allowing overlap of I/O and processing.
template <typename T>
class DoubleBuffer {
public:
    /// @brief Construct with initial values
    /// @param a First buffer
    /// @param b Second buffer
    DoubleBuffer(T a, T b)
        : buffers_{std::move(a), std::move(b)}
        , fillIndex_(0)
        , drainIndex_(1) {}

    /// @brief Get buffer for filling
    [[nodiscard]] T& fillBuffer() noexcept { return buffers_[fillIndex_]; }
    [[nodiscard]] const T& fillBuffer() const noexcept { return buffers_[fillIndex_]; }

    /// @brief Get buffer for draining
    [[nodiscard]] T& drainBuffer() noexcept { return buffers_[drainIndex_]; }
    [[nodiscard]] const T& drainBuffer() const noexcept { return buffers_[drainIndex_]; }

    /// @brief Swap fill and drain buffers
    void swap() noexcept {
        std::swap(fillIndex_, drainIndex_);
    }

private:
    T buffers_[2];
    std::size_t fillIndex_;
    std::size_t drainIndex_;
};

// =============================================================================
// Async I/O Statistics
// =============================================================================

/// @brief Statistics for async I/O operations
struct AsyncIOStats {
    /// @brief Total bytes transferred
    std::uint64_t totalBytes = 0;

    /// @brief Number of I/O operations
    std::uint64_t operationCount = 0;

    /// @brief Total time spent in I/O (microseconds)
    std::uint64_t ioTimeUs = 0;

    /// @brief Total time spent waiting for buffers (microseconds)
    std::uint64_t waitTimeUs = 0;

    /// @brief Number of buffer stalls (had to wait for buffer)
    std::uint64_t stallCount = 0;

    /// @brief Get average operation size
    [[nodiscard]] double avgOperationSize() const noexcept {
        if (operationCount == 0) return 0.0;
        return static_cast<double>(totalBytes) / static_cast<double>(operationCount);
    }

    /// @brief Get throughput (MB/s)
    [[nodiscard]] double throughputMBps() const noexcept {
        if (ioTimeUs == 0) return 0.0;
        return (static_cast<double>(totalBytes) / (1024.0 * 1024.0)) /
               (static_cast<double>(ioTimeUs) / 1'000'000.0);
    }

    /// @brief Get I/O efficiency (time in I/O vs total time)
    [[nodiscard]] double efficiency() const noexcept {
        std::uint64_t totalTime = ioTimeUs + waitTimeUs;
        if (totalTime == 0) return 1.0;
        return static_cast<double>(ioTimeUs) / static_cast<double>(totalTime);
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Check if file is seekable
/// @param path File path
/// @return true if file supports seeking
[[nodiscard]] bool isSeekable(const std::filesystem::path& path);

/// @brief Get optimal buffer size for file
/// @param path File path
/// @return Recommended buffer size
[[nodiscard]] std::size_t optimalBufferSize(const std::filesystem::path& path);

/// @brief Get optimal prefetch depth based on system
/// @return Recommended prefetch depth
[[nodiscard]] std::size_t optimalPrefetchDepth() noexcept;

}  // namespace fqc::io

#endif  // FQC_IO_ASYNC_IO_H
