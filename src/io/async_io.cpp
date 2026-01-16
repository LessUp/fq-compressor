// =============================================================================
// fq-compressor - Async IO Implementation
// =============================================================================
// Implements asynchronous I/O operations for improved pipeline throughput.
//
// Requirements: 4.1 (Parallel processing), 16.1 (Async IO)
// =============================================================================

#include "fqc/io/async_io.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

#include "fqc/common/logger.h"

namespace fqc::io {

// =============================================================================
// ManagedBuffer Implementation
// =============================================================================

ManagedBuffer::ManagedBuffer(std::uint8_t* data, std::size_t size, BufferPool* pool)
    : data_(data)
    , capacity_(size)
    , size_(0)
    , pool_(pool) {}

ManagedBuffer::~ManagedBuffer() {
    if (data_ && pool_) {
        pool_->release(data_);
    }
}

ManagedBuffer::ManagedBuffer(ManagedBuffer&& other) noexcept
    : data_(other.data_)
    , capacity_(other.capacity_)
    , size_(other.size_)
    , pool_(other.pool_) {
    other.data_ = nullptr;
    other.capacity_ = 0;
    other.size_ = 0;
    other.pool_ = nullptr;
}

ManagedBuffer& ManagedBuffer::operator=(ManagedBuffer&& other) noexcept {
    if (this != &other) {
        // Release current buffer
        if (data_ && pool_) {
            pool_->release(data_);
        }
        
        // Take ownership
        data_ = other.data_;
        capacity_ = other.capacity_;
        size_ = other.size_;
        pool_ = other.pool_;
        
        // Clear other
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
        other.pool_ = nullptr;
    }
    return *this;
}

std::uint8_t* ManagedBuffer::release() noexcept {
    auto* ptr = data_;
    data_ = nullptr;
    capacity_ = 0;
    size_ = 0;
    pool_ = nullptr;
    return ptr;
}

// =============================================================================
// BufferPool Implementation
// =============================================================================

BufferPool::BufferPool(std::size_t bufferSize, std::size_t bufferCount)
    : bufferSize_(bufferSize)
    , bufferCount_(bufferCount) {
    // Allocate all buffers upfront
    buffers_.reserve(bufferCount);
    for (std::size_t i = 0; i < bufferCount; ++i) {
        auto buffer = std::make_unique<std::uint8_t[]>(bufferSize);
        available_.push(buffer.get());
        buffers_.push_back(std::move(buffer));
    }
    
    FQC_LOG_DEBUG("BufferPool created: size={}, count={}", bufferSize, bufferCount);
}

BufferPool::~BufferPool() {
    // Buffers are automatically freed by unique_ptr
    FQC_LOG_DEBUG("BufferPool destroyed");
}

ManagedBuffer BufferPool::acquire() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return !available_.empty(); });
    
    auto* data = available_.front();
    available_.pop();
    
    return ManagedBuffer(data, bufferSize_, this);
}

std::optional<ManagedBuffer> BufferPool::tryAcquire() {
    std::lock_guard lock(mutex_);
    
    if (available_.empty()) {
        return std::nullopt;
    }
    
    auto* data = available_.front();
    available_.pop();
    
    return ManagedBuffer(data, bufferSize_, this);
}

std::optional<ManagedBuffer> BufferPool::acquireWithTimeout(std::uint32_t timeoutMs) {
    std::unique_lock lock(mutex_);
    
    if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                      [this] { return !available_.empty(); })) {
        return std::nullopt;
    }
    
    auto* data = available_.front();
    available_.pop();
    
    return ManagedBuffer(data, bufferSize_, this);
}

void BufferPool::release(std::uint8_t* data) {
    if (!data) return;
    
    {
        std::lock_guard lock(mutex_);
        available_.push(data);
    }
    cv_.notify_one();
}

std::size_t BufferPool::availableCount() const noexcept {
    std::lock_guard lock(mutex_);
    return available_.size();
}

bool BufferPool::empty() const noexcept {
    std::lock_guard lock(mutex_);
    return available_.empty();
}

void BufferPool::reset() {
    std::lock_guard lock(mutex_);
    
    // Clear and refill available queue
    while (!available_.empty()) {
        available_.pop();
    }
    
    for (const auto& buffer : buffers_) {
        available_.push(buffer.get());
    }
}

// =============================================================================
// Configuration Validation
// =============================================================================

VoidResult AsyncReaderConfig::validate() const {
    if (bufferSize == 0) {
        return makeError(ErrorCode::kInvalidArgument, std::string("Buffer size must be > 0"));
    }
    if (bufferCount == 0) {
        return makeError(ErrorCode::kInvalidArgument, std::string("Buffer count must be > 0"));
    }
    if (prefetchDepth > bufferCount) {
        return makeError(ErrorCode::kInvalidArgument,
                         "Prefetch depth ({}) cannot exceed buffer count ({})",
                         prefetchDepth, bufferCount);
    }
    return {};
}

VoidResult AsyncWriterConfig::validate() const {
    if (bufferSize == 0) {
        return makeError(ErrorCode::kInvalidArgument, std::string("Buffer size must be > 0"));
    }
    if (bufferCount == 0) {
        return makeError(ErrorCode::kInvalidArgument, std::string("Buffer count must be > 0"));
    }
    if (writeBehindDepth > bufferCount) {
        return makeError(ErrorCode::kInvalidArgument,
                         "Write-behind depth ({}) cannot exceed buffer count ({})",
                         writeBehindDepth, bufferCount);
    }
    return {};
}

// =============================================================================
// AsyncReaderImpl
// =============================================================================

class AsyncReaderImpl {
public:
    explicit AsyncReaderImpl(AsyncReaderConfig config)
        : config_(std::move(config))
        , bufferPool_(config_.bufferSize, config_.bufferCount) {}

    ~AsyncReaderImpl() {
        close();
    }

    VoidResult open(const std::filesystem::path& path) {
        if (isOpen_) {
            return makeError(ErrorCode::kInvalidState, std::string("Reader already open"));
        }

        try {
            inputPath_ = path;
            isStdin_ = false;
            
            // Get file size
            fileSize_ = std::filesystem::file_size(path);
            
            // Open file
            stream_.open(path, std::ios::binary);
            if (!stream_.is_open()) {
                return makeError(ErrorCode::kIOError,
                                 "Failed to open file: {}", path.string());
            }
            
            isOpen_ = true;
            eof_ = false;
            position_ = 0;
            totalBytesRead_ = 0;
            
            // Start prefetch thread
            startPrefetchThread();
            
            FQC_LOG_DEBUG("AsyncReader opened: path={}, size={}", path.string(), fileSize_);
            
            return {};
        } catch (const std::exception& e) {
            return makeError(ErrorCode::kIOError, std::string("Failed to open file: {}"), e.what());
        }
    }

    VoidResult openStdin() {
        if (isOpen_) {
            return makeError(ErrorCode::kInvalidState, std::string("Reader already open"));
        }

        isStdin_ = true;
        isOpen_ = true;
        eof_ = false;
        position_ = 0;
        totalBytesRead_ = 0;
        fileSize_ = 0;  // Unknown for stdin
        
        // Start prefetch thread
        startPrefetchThread();
        
        FQC_LOG_DEBUG("AsyncReader opened: stdin");
        
        return {};
    }

    std::optional<ManagedBuffer> read() {
        if (!isOpen_ || (eof_ && readyQueue_.empty())) {
            return std::nullopt;
        }

        // Wait for a buffer from the prefetch queue
        std::unique_lock lock(queueMutex_);
        queueCv_.wait(lock, [this] {
            return !readyQueue_.empty() || (eof_ && prefetchDone_);
        });
        
        if (readyQueue_.empty()) {
            return std::nullopt;
        }
        
        auto buffer = std::move(readyQueue_.front());
        readyQueue_.pop();
        lock.unlock();
        
        // Signal prefetch thread that a buffer was consumed
        prefetchCv_.notify_one();
        
        return buffer;
    }

    std::optional<ManagedBuffer> readWithTimeout(std::uint32_t timeoutMs) {
        if (!isOpen_ || (eof_ && readyQueue_.empty())) {
            return std::nullopt;
        }

        std::unique_lock lock(queueMutex_);
        if (!queueCv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                               [this] { return !readyQueue_.empty() || (eof_ && prefetchDone_); })) {
            return std::nullopt;  // Timeout
        }
        
        if (readyQueue_.empty()) {
            return std::nullopt;
        }
        
        auto buffer = std::move(readyQueue_.front());
        readyQueue_.pop();
        lock.unlock();
        
        prefetchCv_.notify_one();
        
        return buffer;
    }

    bool hasMore() const noexcept {
        std::lock_guard lock(queueMutex_);
        return !readyQueue_.empty() || (!eof_ && isOpen_);
    }

    bool eof() const noexcept { return eof_ && readyQueue_.empty(); }
    std::uint64_t totalBytesRead() const noexcept { return totalBytesRead_; }
    std::uint64_t fileSize() const noexcept { return fileSize_; }
    std::uint64_t position() const noexcept { return position_; }
    bool isOpen() const noexcept { return isOpen_; }
    const AsyncReaderConfig& config() const noexcept { return config_; }

    void close() noexcept {
        if (!isOpen_) return;
        
        // Stop prefetch thread
        stopPrefetch_ = true;
        prefetchCv_.notify_all();
        
        if (prefetchThread_.joinable()) {
            prefetchThread_.join();
        }
        
        // Close file
        if (stream_.is_open()) {
            stream_.close();
        }
        
        // Clear queues
        {
            std::lock_guard lock(queueMutex_);
            while (!readyQueue_.empty()) {
                readyQueue_.pop();
            }
        }
        
        isOpen_ = false;
        eof_ = true;
        
        FQC_LOG_DEBUG("AsyncReader closed: total_bytes={}", totalBytesRead_.load());
    }

private:
    void startPrefetchThread() {
        stopPrefetch_ = false;
        prefetchDone_ = false;
        
        prefetchThread_ = std::thread([this] {
            prefetchLoop();
        });
    }

    void prefetchLoop() {
        while (!stopPrefetch_) {
            // Check if we should prefetch more
            {
                std::unique_lock lock(queueMutex_);
                prefetchCv_.wait(lock, [this] {
                    return stopPrefetch_ || 
                           readyQueue_.size() < config_.prefetchDepth;
                });
            }
            
            if (stopPrefetch_) break;
            
            // Acquire a buffer
            auto bufferOpt = bufferPool_.tryAcquire();
            if (!bufferOpt) {
                // Wait for a buffer to become available
                bufferOpt = bufferPool_.acquireWithTimeout(100);
                if (!bufferOpt) continue;
            }
            
            auto& buffer = *bufferOpt;
            
            // Read data
            std::size_t bytesRead = 0;
            
            if (isStdin_) {
                std::cin.read(reinterpret_cast<char*>(buffer.data()), 
                              static_cast<std::streamsize>(buffer.capacity()));
                bytesRead = static_cast<std::size_t>(std::cin.gcount());
                
                if (std::cin.eof() || bytesRead == 0) {
                    eof_ = true;
                }
            } else {
                std::lock_guard fileLock(fileMutex_);
                stream_.read(reinterpret_cast<char*>(buffer.data()),
                             static_cast<std::streamsize>(buffer.capacity()));
                bytesRead = static_cast<std::size_t>(stream_.gcount());
                
                if (stream_.eof() || bytesRead == 0) {
                    eof_ = true;
                }
            }
            
            if (bytesRead > 0) {
                buffer.setSize(bytesRead);
                totalBytesRead_ += bytesRead;
                position_ += bytesRead;
                
                // Add to ready queue
                {
                    std::lock_guard lock(queueMutex_);
                    readyQueue_.push(std::move(buffer));
                }
                queueCv_.notify_one();
            }
            
            if (eof_) {
                prefetchDone_ = true;
                queueCv_.notify_all();
                break;
            }
        }
        
        prefetchDone_ = true;
        queueCv_.notify_all();
    }

    AsyncReaderConfig config_;
    BufferPool bufferPool_;
    
    std::filesystem::path inputPath_;
    std::ifstream stream_;
    bool isStdin_ = false;
    bool isOpen_ = false;
    std::atomic<bool> eof_{false};
    std::atomic<std::uint64_t> totalBytesRead_{0};
    std::uint64_t fileSize_ = 0;
    std::atomic<std::uint64_t> position_{0};
    
    // Prefetch thread
    std::thread prefetchThread_;
    std::atomic<bool> stopPrefetch_{false};
    std::atomic<bool> prefetchDone_{false};
    
    // Ready queue
    std::queue<ManagedBuffer> readyQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::condition_variable prefetchCv_;
    
    // File access mutex (for thread-safe reads)
    std::mutex fileMutex_;
};

// =============================================================================
// AsyncWriterImpl
// =============================================================================

class AsyncWriterImpl {
public:
    explicit AsyncWriterImpl(AsyncWriterConfig config)
        : config_(std::move(config))
        , bufferPool_(config_.bufferSize, config_.bufferCount) {}

    ~AsyncWriterImpl() {
        if (isOpen_ && !isFinalized_) {
            abort();
        }
    }

    VoidResult open(const std::filesystem::path& path) {
        if (isOpen_) {
            return makeError(ErrorCode::kInvalidState, std::string("Writer already open"));
        }

        try {
            targetPath_ = path;
            isStdout_ = false;
            
            // Use temp file for atomic write
            if (config_.atomicWrite) {
                tempPath_ = path;
                tempPath_ += ".tmp";
                stream_.open(tempPath_, std::ios::binary | std::ios::trunc);
            } else {
                stream_.open(path, std::ios::binary | std::ios::trunc);
            }
            
            if (!stream_.is_open()) {
                return makeError(ErrorCode::kIOError,
                                 "Failed to open file for writing: {}", path.string());
            }
            
            isOpen_ = true;
            isFinalized_ = false;
            position_ = 0;
            totalBytesWritten_ = 0;
            
            // Start write-behind thread
            startWriteThread();
            
            FQC_LOG_DEBUG("AsyncWriter opened: path={}", path.string());
            
            return {};
        } catch (const std::exception& e) {
            return makeError(ErrorCode::kIOError, std::string("Failed to open file: {}"), e.what());
        }
    }

    VoidResult openStdout() {
        if (isOpen_) {
            return makeError(ErrorCode::kInvalidState, std::string("Writer already open"));
        }

        isStdout_ = true;
        isOpen_ = true;
        isFinalized_ = false;
        position_ = 0;
        totalBytesWritten_ = 0;
        
        // Start write-behind thread
        startWriteThread();
        
        FQC_LOG_DEBUG("AsyncWriter opened: stdout");
        
        return {};
    }

    ManagedBuffer acquireBuffer() {
        return bufferPool_.acquire();
    }

    std::optional<ManagedBuffer> tryAcquireBuffer() {
        return bufferPool_.tryAcquire();
    }

    VoidResult write(ManagedBuffer buffer) {
        if (!isOpen_ || isFinalized_) {
            return makeError(ErrorCode::kInvalidState, std::string("Writer not open"));
        }

        if (buffer.empty()) {
            return {};  // Nothing to write
        }

        // Add to write queue
        {
            std::unique_lock lock(queueMutex_);
            
            // Wait if queue is full (backpressure)
            queueCv_.wait(lock, [this] {
                return writeQueue_.size() < config_.writeBehindDepth || stopWrite_;
            });
            
            if (stopWrite_) {
                return makeError(ErrorCode::kCancelled, std::string("Writer stopped"));
            }
            
            writeQueue_.push(std::move(buffer));
        }
        writeCv_.notify_one();
        
        return {};
    }

    VoidResult write(std::span<const std::uint8_t> data) {
        if (data.empty()) {
            return {};
        }

        // Acquire buffer and copy data
        auto buffer = acquireBuffer();
        
        if (data.size() > buffer.capacity()) {
            // Data too large for single buffer - write in chunks
            std::size_t offset = 0;
            while (offset < data.size()) {
                std::size_t chunkSize = std::min(buffer.capacity(), data.size() - offset);
                std::memcpy(buffer.data(), data.data() + offset, chunkSize);
                buffer.setSize(chunkSize);
                
                auto result = write(std::move(buffer));
                if (!result) {
                    return result;
                }
                
                offset += chunkSize;
                
                if (offset < data.size()) {
                    buffer = acquireBuffer();
                }
            }
        } else {
            std::memcpy(buffer.data(), data.data(), data.size());
            buffer.setSize(data.size());
            return write(std::move(buffer));
        }
        
        return {};
    }

    VoidResult flush() {
        if (!isOpen_) {
            return makeError(ErrorCode::kInvalidState, std::string("Writer not open"));
        }

        // Wait for write queue to drain
        {
            std::unique_lock lock(queueMutex_);
            flushCv_.wait(lock, [this] {
                return writeQueue_.empty() || stopWrite_;
            });
        }
        
        // Sync to disk if configured
        if (config_.syncOnWrite && !isStdout_) {
            std::lock_guard fileLock(fileMutex_);
            stream_.flush();
        }
        
        return {};
    }

    VoidResult finalize() {
        if (!isOpen_ || isFinalized_) {
            return makeError(ErrorCode::kInvalidState, std::string("Writer not open or already finalized"));
        }

        // Flush remaining writes
        auto flushResult = flush();
        if (!flushResult) {
            return flushResult;
        }
        
        // Stop write thread
        stopWrite_ = true;
        writeCv_.notify_all();
        
        if (writeThread_.joinable()) {
            writeThread_.join();
        }
        
        // Close file
        if (!isStdout_) {
            std::lock_guard fileLock(fileMutex_);
            stream_.close();
            
            // Atomic rename
            if (config_.atomicWrite) {
                try {
                    std::filesystem::rename(tempPath_, targetPath_);
                } catch (const std::exception& e) {
                    return makeError(ErrorCode::kIOError,
                                     std::string("Failed to rename temp file: ") + e.what());
                }
            }
        }
        
        isFinalized_ = true;
        
        FQC_LOG_DEBUG("AsyncWriter finalized: total_bytes={}", totalBytesWritten_.load());
        
        return {};
    }

    void abort() noexcept {
        if (!isOpen_) return;
        
        // Stop write thread
        stopWrite_ = true;
        writeCv_.notify_all();
        queueCv_.notify_all();
        
        if (writeThread_.joinable()) {
            writeThread_.join();
        }
        
        // Close and remove temp file
        if (!isStdout_) {
            stream_.close();
            
            if (config_.atomicWrite) {
                try {
                    std::filesystem::remove(tempPath_);
                } catch (...) {
                    // Ignore errors during abort
                }
            }
        }
        
        // Clear queue
        {
            std::lock_guard lock(queueMutex_);
            while (!writeQueue_.empty()) {
                writeQueue_.pop();
            }
        }
        
        isOpen_ = false;
        isFinalized_ = false;
        
        FQC_LOG_DEBUG("AsyncWriter aborted");
    }

    std::uint64_t totalBytesWritten() const noexcept { return totalBytesWritten_; }
    std::uint64_t position() const noexcept { return position_; }
    bool isOpen() const noexcept { return isOpen_; }
    bool isFinalized() const noexcept { return isFinalized_; }
    const AsyncWriterConfig& config() const noexcept { return config_; }

private:
    void startWriteThread() {
        stopWrite_ = false;
        
        writeThread_ = std::thread([this] {
            writeLoop();
        });
    }

    void writeLoop() {
        while (!stopWrite_) {
            ManagedBuffer buffer;
            
            // Get buffer from queue
            {
                std::unique_lock lock(queueMutex_);
                writeCv_.wait(lock, [this] {
                    return !writeQueue_.empty() || stopWrite_;
                });
                
                if (stopWrite_ && writeQueue_.empty()) {
                    break;
                }
                
                if (writeQueue_.empty()) {
                    continue;
                }
                
                buffer = std::move(writeQueue_.front());
                writeQueue_.pop();
            }
            
            // Signal that queue has space
            queueCv_.notify_one();
            
            // Write data
            if (buffer.valid() && buffer.size() > 0) {
                if (isStdout_) {
                    std::cout.write(reinterpret_cast<const char*>(buffer.data()),
                                    static_cast<std::streamsize>(buffer.size()));
                } else {
                    std::lock_guard fileLock(fileMutex_);
                    stream_.write(reinterpret_cast<const char*>(buffer.data()),
                                  static_cast<std::streamsize>(buffer.size()));
                }
                
                totalBytesWritten_ += buffer.size();
                position_ += buffer.size();
            }
            
            // Check if queue is empty for flush
            {
                std::lock_guard lock(queueMutex_);
                if (writeQueue_.empty()) {
                    flushCv_.notify_all();
                }
            }
        }
        
        // Final flush notification
        flushCv_.notify_all();
    }

    AsyncWriterConfig config_;
    BufferPool bufferPool_;
    
    std::filesystem::path targetPath_;
    std::filesystem::path tempPath_;
    std::ofstream stream_;
    bool isStdout_ = false;
    bool isOpen_ = false;
    bool isFinalized_ = false;
    std::atomic<std::uint64_t> totalBytesWritten_{0};
    std::atomic<std::uint64_t> position_{0};
    
    // Write thread
    std::thread writeThread_;
    std::atomic<bool> stopWrite_{false};
    
    // Write queue
    std::queue<ManagedBuffer> writeQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable writeCv_;
    std::condition_variable queueCv_;
    std::condition_variable flushCv_;
    
    // File access mutex
    std::mutex fileMutex_;
};

// =============================================================================
// AsyncReader Public Interface
// =============================================================================

AsyncReader::AsyncReader(AsyncReaderConfig config)
    : impl_(std::make_unique<AsyncReaderImpl>(std::move(config))) {}

AsyncReader::~AsyncReader() = default;

AsyncReader::AsyncReader(AsyncReader&&) noexcept = default;
AsyncReader& AsyncReader::operator=(AsyncReader&&) noexcept = default;

VoidResult AsyncReader::open(const std::filesystem::path& path) {
    return impl_->open(path);
}

VoidResult AsyncReader::openStdin() {
    return impl_->openStdin();
}

std::optional<ManagedBuffer> AsyncReader::read() {
    return impl_->read();
}

std::optional<ManagedBuffer> AsyncReader::readWithTimeout(std::uint32_t timeoutMs) {
    return impl_->readWithTimeout(timeoutMs);
}

bool AsyncReader::hasMore() const noexcept {
    return impl_->hasMore();
}

bool AsyncReader::eof() const noexcept {
    return impl_->eof();
}

std::uint64_t AsyncReader::totalBytesRead() const noexcept {
    return impl_->totalBytesRead();
}

std::uint64_t AsyncReader::fileSize() const noexcept {
    return impl_->fileSize();
}

std::uint64_t AsyncReader::position() const noexcept {
    return impl_->position();
}

void AsyncReader::close() noexcept {
    impl_->close();
}

bool AsyncReader::isOpen() const noexcept {
    return impl_->isOpen();
}

const AsyncReaderConfig& AsyncReader::config() const noexcept {
    return impl_->config();
}

// =============================================================================
// AsyncWriter Public Interface
// =============================================================================

AsyncWriter::AsyncWriter(AsyncWriterConfig config)
    : impl_(std::make_unique<AsyncWriterImpl>(std::move(config))) {}

AsyncWriter::~AsyncWriter() = default;

AsyncWriter::AsyncWriter(AsyncWriter&&) noexcept = default;
AsyncWriter& AsyncWriter::operator=(AsyncWriter&&) noexcept = default;

VoidResult AsyncWriter::open(const std::filesystem::path& path) {
    return impl_->open(path);
}

VoidResult AsyncWriter::openStdout() {
    return impl_->openStdout();
}

ManagedBuffer AsyncWriter::acquireBuffer() {
    return impl_->acquireBuffer();
}

std::optional<ManagedBuffer> AsyncWriter::tryAcquireBuffer() {
    return impl_->tryAcquireBuffer();
}

VoidResult AsyncWriter::write(ManagedBuffer buffer) {
    return impl_->write(std::move(buffer));
}

VoidResult AsyncWriter::write(std::span<const std::uint8_t> data) {
    return impl_->write(data);
}

VoidResult AsyncWriter::flush() {
    return impl_->flush();
}

VoidResult AsyncWriter::finalize() {
    return impl_->finalize();
}

void AsyncWriter::abort() noexcept {
    impl_->abort();
}

std::uint64_t AsyncWriter::totalBytesWritten() const noexcept {
    return impl_->totalBytesWritten();
}

std::uint64_t AsyncWriter::position() const noexcept {
    return impl_->position();
}

bool AsyncWriter::isOpen() const noexcept {
    return impl_->isOpen();
}

bool AsyncWriter::isFinalized() const noexcept {
    return impl_->isFinalized();
}

const AsyncWriterConfig& AsyncWriter::config() const noexcept {
    return impl_->config();
}

// =============================================================================
// Utility Functions
// =============================================================================

bool isSeekable(const std::filesystem::path& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        // Try to seek
        file.seekg(0, std::ios::end);
        return file.good();
    } catch (...) {
        return false;
    }
}

std::size_t optimalBufferSize(const std::filesystem::path& path) {
    try {
        auto fileSize = std::filesystem::file_size(path);
        
        // For small files, use smaller buffers
        if (fileSize < 1024 * 1024) {  // < 1MB
            return 64 * 1024;  // 64KB
        }
        if (fileSize < 100 * 1024 * 1024) {  // < 100MB
            return 1024 * 1024;  // 1MB
        }
        if (fileSize < 1024 * 1024 * 1024) {  // < 1GB
            return 4 * 1024 * 1024;  // 4MB
        }
        
        // For large files, use larger buffers
        return 8 * 1024 * 1024;  // 8MB
    } catch (...) {
        return kDefaultAsyncBufferSize;
    }
}

std::size_t optimalPrefetchDepth() noexcept {
    // Base on available memory and CPU cores
    auto hwThreads = std::thread::hardware_concurrency();
    if (hwThreads == 0) {
        return kDefaultPrefetchDepth;
    }
    
    // More cores = more prefetch to keep them busy
    if (hwThreads >= 16) {
        return 4;
    }
    if (hwThreads >= 8) {
        return 3;
    }
    
    return kDefaultPrefetchDepth;
}

}  // namespace fqc::io
