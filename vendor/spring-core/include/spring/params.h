/*
 * Copyright 2018 University of Illinois Board of Trustees and Stanford
 * University. All Rights Reserved.
 * Licensed under the "Non-exclusive Research Use License for SPRING Software"
 * license (the "License");
 * You may not use this file except in compliance with the License.
 * The License is included in the distribution as license.pdf file.
 *
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Extracted for fq-compressor project - Non-commercial/Research use only
 */

#ifndef SPRING_CORE_PARAMS_H_
#define SPRING_CORE_PARAMS_H_

#include <cstdint>
#include <string>

namespace spring {

// =============================================================================
// Core Algorithm Parameters
// =============================================================================

/// Maximum read length for short reads (HARDCODED - deeply embedded in bitset sizes)
/// WARNING: Changing this requires recompilation and affects memory layout
constexpr uint16_t MAX_READ_LEN = 511;

/// Maximum read length for long read mode (bypasses ABC algorithm)
constexpr uint32_t MAX_READ_LEN_LONG = 4294967290;

/// Maximum number of reads supported
constexpr uint32_t MAX_NUM_READS = 4294967290;

// =============================================================================
// Reordering Parameters
// =============================================================================

/// Number of dictionaries used for reordering (covers different positions)
constexpr int NUM_DICT_REORDER = 2;

/// Maximum number of reads to search in each dictionary bin
constexpr int MAX_SEARCH_REORDER = 1000;

/// Hamming distance threshold for considering two reads as "matching"
/// Lower values = stricter matching = fewer matches but higher quality
constexpr int THRESH_REORDER = 4;

/// Number of locks for concurrent dictionary access (power of 2 for fast mod)
/// 0x1000000 = 16 million locks
constexpr int NUM_LOCKS_REORDER = 0x1000000;

/// Fraction of unmatched reads in last 1M for thread to give up searching
/// When this fraction is exceeded, the thread stops trying to find matches
constexpr float STOP_CRITERIA_REORDER = 0.5f;

// =============================================================================
// Encoder Parameters
// =============================================================================

/// Number of dictionaries used for encoding singleton reads
constexpr int NUM_DICT_ENCODER = 2;

/// Maximum number of reads to search during encoding
constexpr int MAX_SEARCH_ENCODER = 1000;

/// Hamming distance threshold for encoding (higher than reorder threshold)
constexpr int THRESH_ENCODER = 24;

// =============================================================================
// Block Size Parameters
// =============================================================================

/// Number of reads per block for short reads
constexpr int NUM_READS_PER_BLOCK = 256000;

/// Number of reads per block for long reads
constexpr int NUM_READS_PER_BLOCK_LONG = 10000;

/// BSC (Block Sorting Compressor) block size in MB
constexpr int BSC_BLOCK_SIZE = 64;

// =============================================================================
// Compression Parameters Structure
// =============================================================================

/// Parameters passed through the compression pipeline
struct compression_params {
    bool paired_end;           ///< Whether input is paired-end data
    bool preserve_order;       ///< Whether to preserve original read order
    bool preserve_quality;     ///< Whether to preserve quality scores
    bool preserve_id;          ///< Whether to preserve read IDs
    bool long_flag;            ///< Long read mode (bypasses ABC)
    bool qvz_flag;             ///< Use QVZ quality compression
    bool ill_bin_flag;         ///< Use Illumina 8-bin quality binning
    bool bin_thr_flag;         ///< Use binary threshold quality binning
    double qvz_ratio;          ///< QVZ compression ratio
    unsigned int bin_thr_thr;  ///< Binary binning threshold
    unsigned int bin_thr_high; ///< Binary binning high value
    unsigned int bin_thr_low;  ///< Binary binning low value
    uint32_t num_reads;        ///< Total number of reads
    uint32_t num_reads_clean[2]; ///< Number of clean reads (no N) per file
    uint32_t max_readlen;      ///< Maximum read length in dataset
    uint8_t paired_id_code;    ///< Code for paired-end ID pattern
    bool paired_id_match;      ///< Whether paired IDs match pattern
    int num_reads_per_block;   ///< Reads per block (short reads)
    int num_reads_per_block_long; ///< Reads per block (long reads)
    int num_thr;               ///< Number of threads
};

}  // namespace spring

#endif  // SPRING_CORE_PARAMS_H_
