// =============================================================================
// fq-compressor - Paired-End FASTQ Parser Implementation
// =============================================================================
// Implementation of paired-end FASTQ parsing.
//
// Requirements: 1.1.3
// =============================================================================

#include "fqc/io/paired_end_parser.h"

#include <algorithm>

#include "fqc/common/logger.h"
#include "fqc/io/compressed_stream.h"

namespace fqc::io {

// =============================================================================
// PairedEndParser Implementation
// =============================================================================

PairedEndParser::PairedEndParser(std::filesystem::path r1Path,
                                 std::filesystem::path r2Path,
                                 PairedEndParserOptions options)
    : options_(std::move(options)),
      r1Path_(std::move(r1Path)),
      r2Path_(std::move(r2Path)) {
    options_.inputMode = PEInputMode::kDualFile;
}

PairedEndParser::PairedEndParser(std::filesystem::path interleavedPath,
                                 PairedEndParserOptions options)
    : options_(std::move(options)), r1Path_(std::move(interleavedPath)) {
    options_.inputMode = PEInputMode::kInterleaved;
}

PairedEndParser::PairedEndParser(std::unique_ptr<std::istream> r1Stream,
                                 std::unique_ptr<std::istream> r2Stream,
                                 PairedEndParserOptions options)
    : options_(std::move(options)),
      r1Parser_(std::make_unique<FastqParser>(std::move(r1Stream), options_.baseOptions)),
      r2Parser_(std::make_unique<FastqParser>(std::move(r2Stream), options_.baseOptions)) {
    options_.inputMode = PEInputMode::kDualFile;
    isOpen_ = true;
}

PairedEndParser::PairedEndParser(std::unique_ptr<std::istream> interleavedStream,
                                 PairedEndParserOptions options)
    : options_(std::move(options)),
      r1Parser_(std::make_unique<FastqParser>(std::move(interleavedStream), options_.baseOptions)) {
    options_.inputMode = PEInputMode::kInterleaved;
    isOpen_ = true;
}

PairedEndParser::~PairedEndParser() { close(); }

PairedEndParser::PairedEndParser(PairedEndParser&&) noexcept = default;
PairedEndParser& PairedEndParser::operator=(PairedEndParser&&) noexcept = default;

void PairedEndParser::open() {
    if (isOpen_) {
        return;
    }

    if (options_.inputMode == PEInputMode::kDualFile) {
        // Open both R1 and R2 files
        auto r1Stream = openCompressedFile(r1Path_);
        auto r2Stream = openCompressedFile(r2Path_);

        r1Parser_ = std::make_unique<FastqParser>(std::move(r1Stream), options_.baseOptions);
        r2Parser_ = std::make_unique<FastqParser>(std::move(r2Stream), options_.baseOptions);

        r1Parser_->open();
        r2Parser_->open();

        LOG_DEBUG("Opened paired-end files: R1={}, R2={}", r1Path_.string(), r2Path_.string());
    } else {
        // Open single interleaved file
        auto stream = openCompressedFile(r1Path_);
        r1Parser_ = std::make_unique<FastqParser>(std::move(stream), options_.baseOptions);
        r1Parser_->open();

        LOG_DEBUG("Opened interleaved paired-end file: {}", r1Path_.string());
    }

    isOpen_ = true;
    eof_ = false;
    stats_.reset();
}

void PairedEndParser::close() noexcept {
    if (!isOpen_) {
        return;
    }

    if (r1Parser_) {
        r1Parser_->close();
        r1Parser_.reset();
    }
    if (r2Parser_) {
        r2Parser_->close();
        r2Parser_.reset();
    }

    isOpen_ = false;
    eof_ = true;
}

std::optional<PairedEndRecord> PairedEndParser::readPair() {
    if (!isOpen_ || eof_) {
        return std::nullopt;
    }

    PairedEndRecord pair;

    if (options_.inputMode == PEInputMode::kDualFile) {
        // Read from both files
        auto r1 = r1Parser_->readRecord();
        auto r2 = r2Parser_->readRecord();

        if (!r1 || !r2) {
            eof_ = true;
            if (r1 && !r2) {
                LOG_WARNING("R1 has more reads than R2");
            } else if (!r1 && r2) {
                LOG_WARNING("R2 has more reads than R1");
            }
            return std::nullopt;
        }

        pair.read1 = std::move(*r1);
        pair.read2 = std::move(*r2);
    } else {
        // Read two consecutive records from interleaved file
        auto r1 = r1Parser_->readRecord();
        if (!r1) {
            eof_ = true;
            return std::nullopt;
        }

        auto r2 = r1Parser_->readRecord();
        if (!r2) {
            eof_ = true;
            LOG_WARNING("Interleaved file has odd number of reads");
            return std::nullopt;
        }

        pair.read1 = std::move(*r1);
        pair.read2 = std::move(*r2);
    }

    // Validate pairing if enabled
    if (options_.validatePairing && !validatePairIds(pair.read1, pair.read2)) {
        ++stats_.mismatchedPairs;
        LOG_WARNING("Mismatched pair IDs: '{}' vs '{}'", pair.read1.id, pair.read2.id);
    }

    // Update statistics
    ++stats_.totalPairs;
    stats_.r1Stats.update(pair.read1);
    stats_.r2Stats.update(pair.read2);

    return pair;
}

std::optional<PairedEndParser::Chunk> PairedEndParser::readChunk(std::size_t maxPairs) {
    if (!isOpen_ || eof_) {
        return std::nullopt;
    }

    Chunk chunk;
    chunk.reserve(maxPairs);

    for (std::size_t i = 0; i < maxPairs && !eof_; ++i) {
        auto pair = readPair();
        if (!pair) {
            break;
        }
        chunk.push_back(std::move(*pair));
    }

    if (chunk.empty()) {
        return std::nullopt;
    }

    return chunk;
}

std::vector<PairedEndRecord> PairedEndParser::readAll() {
    std::vector<PairedEndRecord> pairs;

    while (!eof_) {
        auto pair = readPair();
        if (!pair) {
            break;
        }
        pairs.push_back(std::move(*pair));
    }

    return pairs;
}

bool PairedEndParser::isStdin() const noexcept {
    if (r1Parser_) {
        return r1Parser_->isStdin();
    }
    return false;
}

void PairedEndParser::reset() {
    if (r1Parser_) {
        r1Parser_->reset();
    }
    if (r2Parser_) {
        r2Parser_->reset();
    }
    eof_ = false;
    stats_.reset();
}

bool PairedEndParser::canSeek() const noexcept {
    if (options_.inputMode == PEInputMode::kDualFile) {
        return r1Parser_ && r1Parser_->canSeek() && r2Parser_ && r2Parser_->canSeek();
    }
    return r1Parser_ && r1Parser_->canSeek();
}

bool PairedEndParser::validatePairIds(const FastqRecord& r1, const FastqRecord& r2) const {
    return arePairedIds(r1.id, r2.id);
}

std::string_view PairedEndParser::stripPairSuffix(std::string_view id) {
    return extractBaseReadId(id);
}

// =============================================================================
// Utility Functions
// =============================================================================

std::string_view extractBaseReadId(std::string_view id) {
    if (id.empty()) {
        return id;
    }

    // Check for common pair suffixes: /1, /2, .1, .2, _1, _2
    if (id.length() >= 2) {
        char lastChar = id.back();
        char secondLast = id[id.length() - 2];

        if ((lastChar == '1' || lastChar == '2') &&
            (secondLast == '/' || secondLast == '.' || secondLast == '_')) {
            return id.substr(0, id.length() - 2);
        }
    }

    // Check for space-separated comments (Illumina 1.8+ format)
    // @INSTRUMENT:RUN:FLOWCELL:LANE:TILE:X:Y 1:N:0:INDEX
    auto spacePos = id.find(' ');
    if (spacePos != std::string_view::npos) {
        return id.substr(0, spacePos);
    }

    return id;
}

bool arePairedIds(std::string_view id1, std::string_view id2) {
    auto base1 = extractBaseReadId(id1);
    auto base2 = extractBaseReadId(id2);
    return base1 == base2;
}

bool detectInterleavedFormat(const std::filesystem::path& filePath, std::size_t sampleCount) {
    try {
        auto stream = openCompressedFile(filePath);
        FastqParser parser(std::move(stream));
        parser.open();

        std::size_t matchedPairs = 0;
        std::size_t totalPairs = 0;

        for (std::size_t i = 0; i < sampleCount; ++i) {
            auto r1 = parser.readRecord();
            if (!r1) break;

            auto r2 = parser.readRecord();
            if (!r2) break;

            ++totalPairs;
            if (arePairedIds(r1->id, r2->id)) {
                ++matchedPairs;
            }
        }

        // If most pairs have matching IDs, it's likely interleaved
        return totalPairs > 0 && (matchedPairs * 2 >= totalPairs);

    } catch (const std::exception& e) {
        LOG_DEBUG("Failed to detect interleaved format: {}", e.what());
        return false;
    }
}

}  // namespace fqc::io
