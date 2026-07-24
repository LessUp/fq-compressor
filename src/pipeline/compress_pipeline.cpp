// =============================================================================
// fq-compressor - Concurrent 2-Stage Compression Pipeline
// =============================================================================

#include "fqc/pipeline/compress_pipeline.h"

#include "fqc/io/fastq_parser.h"
#include "fqc/pipeline/spsc_queue.h"

#include <cstddef>
#include <istream>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace fqc::pipeline {

namespace {

[[nodiscard]] auto retainedRecordBytes(const ReadRecord& record) noexcept -> std::size_t {
    return sizeof(ReadRecord) + record.id.capacity() + 1 + record.comment.capacity() + 1 +
        record.sequence.capacity() + 1 + record.quality.capacity() + 1;
}

}  // namespace

CompressPipeline::CompressPipeline(std::size_t targetFrameBytes, bool paired)
    : targetFrameBytes_(targetFrameBytes), paired_(paired) {}

auto CompressPipeline::run(std::istream& input, format::ArchiveWriter& writer)
    -> Result<PipelineStats> {
    SpscQueue<std::vector<ReadRecord>, kDefaultQueueDepth> queue;
    std::optional<Error> readerError;
    std::optional<Error> writerError;
    PipelineStats stats;

    std::thread reader([&] {
        io::FastqParser parser(input);
        std::vector<ReadRecord> frame;
        std::size_t retainedBytes = 0;

        auto pushFrame = [&] {
            if (!frame.empty()) {
                queue.push(std::move(frame));
                frame.clear();
                retainedBytes = 0;
            }
        };

        while (true) {
            auto result = parser.readRecord();
            if (!result) {
                readerError = result.error();
                break;
            }
            if (!result->has_value()) {
                break;
            }
            auto& record = **result;
            retainedBytes += retainedRecordBytes(record);
            frame.push_back(std::move(record));
            if (retainedBytes >= targetFrameBytes_ && (!paired_ || frame.size() % 2 == 0)) {
                pushFrame();
            }
        }
        pushFrame();
        queue.close();
    });

    std::thread writerThread([&] {
        while (true) {
            auto frame = queue.pop();
            if (!frame.has_value()) {
                break;
            }
            stats.recordCount += frame->size();
            stats.frameCount += 1;
            auto result = writer.writeFrame(*frame);
            if (!result) {
                writerError = result.error();
                break;
            }
        }
    });

    reader.join();
    writerThread.join();

    if (writerError.has_value()) {
        return makeError<PipelineStats>(std::move(*writerError));
    }
    if (readerError.has_value()) {
        return makeError<PipelineStats>(std::move(*readerError));
    }
    return stats;
}

}  // namespace fqc::pipeline
