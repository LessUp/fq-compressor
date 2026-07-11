// =============================================================================
// fq-compressor - Pipeline Node Shared Implementation
// =============================================================================
// Contains configuration validation and BackpressureController.
// Individual node implementations are in separate files:
//   reader_node.cpp, writer_node.cpp,
//   fqc_reader_node.cpp, decompressor_node.cpp, fastq_writer_node.cpp
// Note: Compression logic is now inline in pipeline.cpp (BlockCompressionState)
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/decompressor_node.h"
#include "fqc/pipeline/fastq_writer_node.h"
#include "fqc/pipeline/fqc_reader_node.h"
#include "fqc/pipeline/node_common.h"
#include "fqc/pipeline/reader_node.h"
#include "fqc/pipeline/writer_node.h"

#include <fmt/format.h>

namespace fqc::pipeline {

// =============================================================================
// Configuration Validation
// =============================================================================

VoidResult ReaderNodeConfig::validate() const {
    if (blockSize < kMinBlockSize || blockSize > kMaxBlockSize) {
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            fmt::format("Block size must be between {} and {}", kMinBlockSize, kMaxBlockSize));
    }
    if (bufferSize == 0) {
        return makeVoidError(ErrorCode::kInvalidArgument, "Buffer size must be > 0");
    }
    return {};
}

VoidResult WriterNodeConfig::validate() const {
    if (bufferSize == 0) {
        return makeVoidError(ErrorCode::kInvalidArgument, "Buffer size must be > 0");
    }
    return {};
}

VoidResult FQCReaderNodeConfig::validate() const {
    if (rangeStart > 0 && rangeEnd > 0 && rangeStart > rangeEnd) {
        return makeVoidError(ErrorCode::kInvalidArgument, "Range start must be <= range end");
    }
    return {};
}

VoidResult DecompressorNodeConfig::validate() const {
    // Placeholder quality must be valid ASCII
    if (placeholderQual < 33 || placeholderQual > 126) {
        return makeVoidError(ErrorCode::kInvalidArgument,
                             "Placeholder quality must be ASCII 33-126");
    }
    return {};
}

VoidResult FASTQWriterNodeConfig::validate() const {
    if (bufferSize == 0) {
        return makeVoidError(ErrorCode::kInvalidArgument, "Buffer size must be > 0");
    }
    return {};
}

// =============================================================================
// BackpressureController Implementation
// =============================================================================

BackpressureController::BackpressureController(std::size_t maxInFlight)
    : maxInFlight_(maxInFlight) {}

void BackpressureController::acquire() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return inFlight_.load() < maxInFlight_; });
    ++inFlight_;
}

bool BackpressureController::tryAcquire() {
    std::size_t current = inFlight_.load();
    while (current < maxInFlight_) {
        if (inFlight_.compare_exchange_weak(current, current + 1)) {
            return true;
        }
    }
    return false;
}

void BackpressureController::release() {
    --inFlight_;
    cv_.notify_one();
}

std::size_t BackpressureController::inFlight() const noexcept {
    return inFlight_.load();
}

std::size_t BackpressureController::maxInFlight() const noexcept {
    return maxInFlight_;
}

void BackpressureController::reset() noexcept {
    {
        std::lock_guard lock(mutex_);
        inFlight_.store(0);
    }
    cv_.notify_all();
}

}  // namespace fqc::pipeline
