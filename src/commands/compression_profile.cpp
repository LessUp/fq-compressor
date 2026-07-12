// =============================================================================
// fq-compressor - Compression Profile
// =============================================================================

#include "fqc/commands/compression_profile.h"

#include "fqc/commands/compress_command.h"
#include "fqc/io/fastq_parser.h"

#include <filesystem>
#include <limits>
#include <thread>
#include <utility>

namespace fqc::commands {

namespace {

struct CompressionProfileDefaults {
    std::size_t blockSize;
    bool enableReordering;
    std::size_t defaultMaxBlockBases;
};

struct ArchiveFacts {
    bool preservesOrder;
    bool hasReorderMap;
};

[[nodiscard]] constexpr auto profileDefaultsFor(ReadLengthClass lengthClass)
    -> CompressionProfileDefaults {
    switch (lengthClass) {
        case ReadLengthClass::kShort:
            return {kDefaultBlockSizeShort, true, 0};
        case ReadLengthClass::kMedium:
            return {kDefaultBlockSizeMedium, false, kDefaultMaxBlockBasesLong};
        case ReadLengthClass::kLong:
            return {kDefaultBlockSizeLong, false, kDefaultMaxBlockBasesUltraLong};
    }

    return {kDefaultBlockSizeShort, true, 0};
}

auto validateOptions(CompressOptions& options,
                     const std::shared_ptr<io::StreamFactory>& streamFactory) -> VoidResult {
    if (!streamFactory) {
        return makeVoidError(ErrorCode::kInvalidArgument, "StreamFactory cannot be null");
    }

    if (options.inputPath != "-") {
        if (streamFactory->isFileStream()) {
            if (!std::filesystem::exists(options.inputPath)) {
                throw IOError("Input file not found: " + options.inputPath.string());
            }
        } else {
            auto inputStream = streamFactory->createInputStream(options.inputPath);
            if (!inputStream || !*inputStream) {
                throw IOError("Input file not found: " + options.inputPath.string());
            }
        }
    }

    if (!options.forceOverwrite && streamFactory->outputExists(options.outputPath)) {
        throw IOError("Output file already exists: " + options.outputPath.string() +
                      " (use -f to overwrite)");
    }

    if (options.compressionLevel < 1 || options.compressionLevel > 9) {
        throw ArgumentError("Compression level must be 1-9");
    }

    if (options.streamingMode) {
        options.enableReordering = false;
        options.saveReorderMap = false;
    }

    if (options.inputPath == "-" && !options.streamingMode) {
        options.streamingMode = true;
        options.enableReordering = false;
        options.saveReorderMap = false;
    }

    if (!options.enableReordering) {
        options.saveReorderMap = false;
    }

    return {};
}

[[nodiscard]] auto resolveReadLengthClass(const CompressOptions& options,
                                          const std::shared_ptr<io::StreamFactory>& streamFactory)
    -> ReadLengthClass {
    if (options.inputPath == "-") {
        return options.autoDetectLongRead ? ReadLengthClass::kMedium : options.longReadMode;
    }

    if (!options.autoDetectLongRead || options.streamingMode) {
        return options.longReadMode;
    }

    try {
        io::FastqParser parser(options.inputPath, streamFactory);
        parser.open();

        io::ParserStats sampleStats;
        if (options.scanAllLengths) {
            while (auto record = parser.readRecord()) {
                sampleStats.update(*record);
            }
        } else {
            sampleStats = parser.sampleRecords(1000);
        }

        return io::detectReadLengthClass(sampleStats);
    } catch (const std::exception&) {
        return ReadLengthClass::kMedium;
    }
}

auto applyResolvedDefaults(CompressOptions& options, ReadLengthClass lengthClass) -> void {
    const auto defaults = profileDefaultsFor(lengthClass);

    if (!options.blockSizeExplicit) {
        options.blockSize = defaults.blockSize;
    }

    options.enableReordering = defaults.enableReordering && options.enableReordering;
    if (options.maxBlockBases == 0) {
        options.maxBlockBases = defaults.defaultMaxBlockBases;
    }

    if (!options.enableReordering) {
        options.saveReorderMap = false;
    }

    options.longReadMode = lengthClass;
}

[[nodiscard]] auto toCompressOptions(const CompressionRequest& request) -> CompressOptions {
    CompressOptions options;
    options.inputPath = request.input.primaryPath;
    options.input2Path = request.input.secondaryPath;
    options.outputPath = request.outputPath;
    options.compressionLevel = request.compressionLevel;
    options.threads = request.threads;
    options.memoryLimitMb = request.memoryLimitMb;
    options.enableReordering = request.enableReordering;
    options.streamingMode = request.mode == CompressionMode::kStreaming;
    options.qualityMode = request.qualityMode;
    options.idMode = request.idMode;
    options.longReadMode = request.requestedLengthClass;
    options.autoDetectLongRead = request.autoDetectLongRead;
    options.scanAllLengths = request.scanAllLengths;
    options.maxBlockBases = request.maxBlockBases;
    options.paired = request.paired;
    options.interleaved = request.input.kind == CompressionInputKind::kInterleavedFile ||
        (request.input.kind == CompressionInputKind::kStdin && request.paired);
    options.peLayout = request.input.archiveLayout;
    options.blockSize = request.blockSize;
    options.blockSizeExplicit = request.blockSizeExplicit;
    options.saveReorderMap = request.saveReorderMap;
    options.checksumType = request.checksumType;
    options.forceOverwrite = request.forceOverwrite;
    options.showProgress = request.showProgress;
    options.inputBytesHint = request.inputBytesHint;
    return options;
}

[[nodiscard]] auto finalizePlannedRequest(const CompressionProfile& profile) -> CompressionRequest {
    auto request = toCompressionRequest(profile.effectiveOptions());
    if (profile.executionMode() == CompressionExecutionMode::kPipeline &&
        request.mode != CompressionMode::kStreaming) {
        request.mode = CompressionMode::kPipeline;
    }
    return request;
}

[[nodiscard]] auto shouldForcePipelineForScale(const CompressOptions& options,
                                               ReadLengthClass lengthClass) -> bool {
    return options.inputBytesHint >= 64ULL * 1024ULL * 1024ULL && !options.blockSizeExplicit &&
        !options.streamingMode && !options.paired && !options.interleaved &&
        options.input2Path.empty() && lengthClass == ReadLengthClass::kShort;
}

[[nodiscard]] auto resolveExecutionMode(const CompressOptions& options,
                                        bool forcePipelineForScale) -> CompressionExecutionMode {
    if (forcePipelineForScale) {
        return CompressionExecutionMode::kPipeline;
    }

    const bool requiresPipelinePath =
        options.paired || options.interleaved || !options.input2Path.empty();
    if (requiresPipelinePath) {
        return CompressionExecutionMode::kPipeline;
    }

    if (options.threads == 1) {
        return CompressionExecutionMode::kSingleThread;
    }

    const auto hardwareThreads = std::thread::hardware_concurrency();
    if (options.threads == 0 && hardwareThreads <= 1) {
        return CompressionExecutionMode::kSingleThread;
    }

    return CompressionExecutionMode::kPipeline;
}

[[nodiscard]] auto resolveArchiveFacts(const CompressOptions& options,
                                       CompressionExecutionMode executionMode) -> ArchiveFacts {
    const bool reorderingPerformed = executionMode == CompressionExecutionMode::kSingleThread &&
        options.enableReordering && !options.streamingMode;
    return {
        .preservesOrder = !reorderingPerformed,
        .hasReorderMap = reorderingPerformed && options.saveReorderMap,
    };
}

[[nodiscard]] auto makeAnalyzerConfig(const CompressOptions& options,
                                      ReadLengthClass lengthClass,
                                      CompressionExecutionMode executionMode)
    -> algo::GlobalAnalyzerConfig {
    algo::GlobalAnalyzerConfig config;
    config.readsPerBlock = options.blockSize;
    if (options.memoryLimitMb > 0) {
        constexpr std::size_t kBytesPerMb = 1024 * 1024;
        // Guard against size_t overflow on MB->bytes conversion (32-bit builds).
        config.memoryLimit =
            (options.memoryLimitMb > std::numeric_limits<std::size_t>::max() / kBytesPerMb)
            ? std::numeric_limits<std::size_t>::max()
            : options.memoryLimitMb * kBytesPerMb;
    } else {
        config.memoryLimit = 0;
    }
    config.numThreads = options.threads > 0 ? static_cast<std::size_t>(options.threads) : 0;
    config.enableReorder = executionMode == CompressionExecutionMode::kSingleThread &&
        options.enableReordering && !options.streamingMode;
    config.readLengthClass = lengthClass;
    return config;
}

[[nodiscard]] auto makeBlockConfig(const CompressOptions& options) -> algo::BlockCompressorConfig {
    auto config = options.toCompressorConfig();
    config.readLengthClass = options.longReadMode;
    return config;
}

[[nodiscard]] auto makePipelineConfig(const CompressOptions& options,
                                      const algo::BlockCompressorConfig& blockConfig,
                                      const ArchiveFacts& archiveFacts)
    -> pipeline::CompressionPipelineConfig {
    pipeline::CompressionPipelineConfig config;
    config.numThreads = options.threads > 0 ? static_cast<std::size_t>(options.threads) : 0;
    config.blockSize = options.blockSize;
    config.maxBlockBases = options.maxBlockBases;
    config.enableReorder = options.enableReordering && !options.streamingMode;
    config.saveReorderMap = options.saveReorderMap;
    config.streamingMode = options.streamingMode;
    config.archivePreservesOrder = archiveFacts.preservesOrder;
    config.archiveHasReorderMap = archiveFacts.hasReorderMap;
    config.memoryLimitMB = options.memoryLimitMb;
    config.compressorConfig = blockConfig;
    return config;
}

}  // namespace

struct CompressionProfile::Impl {
    CompressOptions effectiveOptions;
    ReadLengthClass readLengthClass = ReadLengthClass::kShort;
    bool archivePreservesOrder = true;
    bool archiveHasReorderMap = false;
    CompressionExecutionMode executionMode = CompressionExecutionMode::kSingleThread;
    algo::GlobalAnalyzerConfig globalAnalyzerConfig;
    algo::BlockCompressorConfig blockCompressorConfig;
    pipeline::CompressionPipelineConfig pipelineConfig;
};

CompressionProfile::CompressionProfile(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}

CompressionProfile::~CompressionProfile() = default;
CompressionProfile::CompressionProfile(const CompressionProfile&) = default;
CompressionProfile::CompressionProfile(CompressionProfile&&) noexcept = default;
auto CompressionProfile::operator=(const CompressionProfile&) -> CompressionProfile& = default;
auto CompressionProfile::operator=(CompressionProfile&&) noexcept -> CompressionProfile& = default;

const CompressOptions& CompressionProfile::effectiveOptions() const noexcept {
    return impl_->effectiveOptions;
}

ReadLengthClass CompressionProfile::readLengthClass() const noexcept {
    return impl_->readLengthClass;
}

bool CompressionProfile::streamingMode() const noexcept {
    return impl_->effectiveOptions.streamingMode;
}

bool CompressionProfile::enableReordering() const noexcept {
    return impl_->effectiveOptions.enableReordering;
}

bool CompressionProfile::saveReorderMap() const noexcept {
    return impl_->effectiveOptions.saveReorderMap;
}

bool CompressionProfile::archivePreservesOrder() const noexcept {
    return impl_->archivePreservesOrder;
}

bool CompressionProfile::archiveHasReorderMap() const noexcept {
    return impl_->archiveHasReorderMap;
}

CompressionExecutionMode CompressionProfile::executionMode() const noexcept {
    return impl_->executionMode;
}

const algo::GlobalAnalyzerConfig& CompressionProfile::globalAnalyzerConfig() const noexcept {
    return impl_->globalAnalyzerConfig;
}

const algo::BlockCompressorConfig& CompressionProfile::blockCompressorConfig() const noexcept {
    return impl_->blockCompressorConfig;
}

const pipeline::CompressionPipelineConfig& CompressionProfile::pipelineConfig() const noexcept {
    return impl_->pipelineConfig;
}

auto buildCompressionProfile(CompressOptions options,
                             std::shared_ptr<io::StreamFactory> streamFactory)
    -> Result<CompressionProfile> {
    try {
        if (auto result = validateOptions(options, streamFactory); !result) {
            return makeError<CompressionProfile>(result.error());
        }

        const auto lengthClass = resolveReadLengthClass(options, streamFactory);
        applyResolvedDefaults(options, lengthClass);

        const bool forcePipelineForScale = shouldForcePipelineForScale(options, lengthClass);
        if (forcePipelineForScale) {
            options.enableReordering = false;
            options.saveReorderMap = false;
        }

        const auto executionMode = resolveExecutionMode(options, forcePipelineForScale);
        const auto archiveFacts = resolveArchiveFacts(options, executionMode);
        const auto analyzerConfig = makeAnalyzerConfig(options, lengthClass, executionMode);
        const auto blockConfig = makeBlockConfig(options);
        const auto pipelineConfig = makePipelineConfig(options, blockConfig, archiveFacts);

        if (auto result = analyzerConfig.validate(); !result) {
            return makeError<CompressionProfile>(result.error());
        }
        if (auto result = blockConfig.validate(); !result) {
            return makeError<CompressionProfile>(result.error());
        }
        if (executionMode == CompressionExecutionMode::kPipeline) {
            if (auto result = pipelineConfig.validate(); !result) {
                return makeError<CompressionProfile>(result.error());
            }
        }

        auto impl = std::make_shared<CompressionProfile::Impl>();
        impl->effectiveOptions = std::move(options);
        impl->readLengthClass = lengthClass;
        impl->archivePreservesOrder = archiveFacts.preservesOrder;
        impl->archiveHasReorderMap = archiveFacts.hasReorderMap;
        impl->executionMode = executionMode;
        impl->globalAnalyzerConfig = analyzerConfig;
        impl->blockCompressorConfig = blockConfig;
        impl->pipelineConfig = pipelineConfig;

        return CompressionProfile(std::move(impl));
    } catch (const FQCException& ex) {
        return makeError<CompressionProfile>(ex);
    }
}

auto buildCompressionProfilePlan(const CompressionRequest& request,
                                 std::shared_ptr<io::StreamFactory> streamFactory)
    -> Result<CompressionProfilePlan> {
    auto profileResult =
        buildCompressionProfile(toCompressOptions(request), std::move(streamFactory));
    if (!profileResult) {
        return makeError<CompressionProfilePlan>(profileResult.error());
    }

    auto profile = std::move(profileResult.value());
    auto effectiveOptions = profile.effectiveOptions();
    auto effectiveRequest = finalizePlannedRequest(profile);

    return CompressionProfilePlan{
        std::move(effectiveRequest),
        std::move(effectiveOptions),
        std::move(profile),
    };
}

}  // namespace fqc::commands
