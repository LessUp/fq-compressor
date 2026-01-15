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
 *
 * This file contains the consensus/delta encoding algorithm for Assembly-based
 * Compression (ABC). After reads are reordered, this module:
 * 1. Builds consensus sequences from groups of similar reads
 * 2. Encodes each read as differences (deltas) from the consensus
 * 3. Compresses the delta information
 */

#ifndef SPRING_CORE_ENCODER_H_
#define SPRING_CORE_ENCODER_H_

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <string>

#include "bitset_util.h"
#include "params.h"
#include "util.h"

// Threading support
#ifdef USE_OPENMP
#include <omp.h>
#else
inline int omp_get_thread_num() { return 0; }
inline int omp_get_num_threads() { return 1; }
inline void omp_set_num_threads(int) {}
struct omp_lock_t {};
inline void omp_init_lock(omp_lock_t*) {}
inline void omp_destroy_lock(omp_lock_t*) {}
inline void omp_set_lock(omp_lock_t*) {}
inline void omp_unset_lock(omp_lock_t*) {}
inline int omp_test_lock(omp_lock_t*) { return 1; }
#endif

namespace spring {

// =============================================================================
// Encoder Global State (Bitset-dependent)
// =============================================================================

/**
 * @brief Bitset-dependent global state for encoder
 * @tparam bitset_size Size of bitsets (typically 3 * MAX_READ_LEN for N support)
 */
template <size_t bitset_size>
struct encoder_global_b {
    std::bitset<bitset_size>** basemask;  ///< Base masks for encoding
    int max_readlen;                       ///< Maximum read length

    /// Mask with 63 bits set (for conversion to ullong)
    std::bitset<bitset_size> mask63;

    explicit encoder_global_b(int max_readlen_param)
        : max_readlen(max_readlen_param) {
        basemask = new std::bitset<bitset_size>*[max_readlen_param];
        for (int i = 0; i < max_readlen_param; i++) {
            basemask[i] = new std::bitset<bitset_size>[128]();
        }
    }

    ~encoder_global_b() {
        for (int i = 0; i < max_readlen; i++) {
            delete[] basemask[i];
        }
        delete[] basemask;
    }

    // Non-copyable
    encoder_global_b(const encoder_global_b&) = delete;
    encoder_global_b& operator=(const encoder_global_b&) = delete;
};

// =============================================================================
// Encoder Global State (Non-template)
// =============================================================================

/**
 * @brief Non-template global state for encoder
 */
struct encoder_global {
    uint32_t numreads;      ///< Number of non-singleton reads
    uint32_t numreads_s;    ///< Number of singleton reads
    uint32_t numreads_N;    ///< Number of reads with N bases

    int numdict_s = NUM_DICT_ENCODER;  ///< Number of dictionaries for singletons
    int max_readlen;        ///< Maximum read length
    int num_thr;            ///< Number of threads

    // File paths
    std::string basedir;
    std::string infile;
    std::string infile_flag;
    std::string infile_pos;
    std::string infile_seq;
    std::string infile_RC;
    std::string infile_readlength;
    std::string infile_N;
    std::string outfile_unaligned;
    std::string outfile_seq;
    std::string outfile_pos;
    std::string outfile_noise;
    std::string outfile_noisepos;
    std::string infile_order;
    std::string infile_order_N;

    /**
     * @brief Noise encoding table
     *
     * enc_noise[ref_base][read_base] gives the encoded character for a
     * substitution. Based on substitution statistics from Minoche et al.
     */
    char enc_noise[128][128];
};

// =============================================================================
// Contig Read Structure
// =============================================================================

/**
 * @brief Structure representing a read within a contig
 */
struct contig_reads {
    std::string read;       ///< Read sequence
    int64_t pos;            ///< Position in contig
    char RC;                ///< 'd' for direct, 'r' for reverse complement
    uint32_t order;         ///< Original read order
    uint16_t read_length;   ///< Read length
};

// =============================================================================
// Consensus Building
// =============================================================================

/**
 * @brief Build consensus sequence from a list of aligned reads
 * @param current_contig List of reads in the contig
 * @param list_size Number of reads in the list
 * @return Consensus sequence string
 *
 * The consensus is computed as the majority base at each position.
 * This is the "reference" against which individual reads are encoded.
 */
std::string buildcontig(std::list<contig_reads>& current_contig,
                        uint32_t list_size);

/**
 * @brief Write contig and delta information to output files
 * @param ref Consensus sequence
 * @param current_contig List of reads in the contig
 * @param f_seq Output stream for consensus sequence
 * @param f_pos Output stream for positions
 * @param f_noise Output stream for noise (substitutions)
 * @param f_noisepos Output stream for noise positions
 * @param f_order Output stream for read order
 * @param f_RC Output stream for reverse complement flags
 * @param f_readlength Output stream for read lengths
 * @param eg Encoder global state
 * @param abs_pos Absolute position counter (updated)
 */
void writecontig(const std::string& ref,
                 std::list<contig_reads>& current_contig,
                 std::ofstream& f_seq, std::ofstream& f_pos,
                 std::ofstream& f_noise, std::ofstream& f_noisepos,
                 std::ofstream& f_order, std::ofstream& f_RC,
                 std::ofstream& f_readlength,
                 const encoder_global& eg, uint64_t& abs_pos);

// =============================================================================
// Bitset to String Conversion (Encoder version)
// =============================================================================

/**
 * @brief Convert bitset to string (encoder version with N support)
 * @tparam bitset_size Size of the bitset
 * @param b Bitset to convert (will be destroyed)
 * @param readlen Length of the sequence
 * @param egb Encoder global state with masks
 * @return DNA sequence string
 *
 * Uses 3 bits per base to support N:
 * A=000, C=001, G=010, T=011, N=100
 */
template <size_t bitset_size>
std::string bitsettostring(std::bitset<bitset_size> b, uint16_t readlen,
                           const encoder_global_b<bitset_size>& egb) {
    static const char revinttochar[8] = {'A', 'N', 'G', 0, 'C', 0, 'T', 0};
    std::string s;
    s.resize(readlen);
    unsigned long long ull;
    for (int i = 0; i < 3 * readlen / 63 + 1; i++) {
        ull = (b & egb.mask63).to_ullong();
        b >>= 63;
        for (int j = 21 * i; j < 21 * i + 21 && j < readlen; j++) {
            s[j] = revinttochar[ull % 8];
            ull /= 8;
        }
    }
    return s;
}

// =============================================================================
// Global Array Initialization (Encoder)
// =============================================================================

/**
 * @brief Initialize encoder global arrays
 * @tparam bitset_size Size of the bitset
 * @param eg Non-template global state
 * @param egb Template global state
 *
 * Sets up basemask for 3-bit encoding (supports N) and noise encoding table.
 */
template <size_t bitset_size>
void setglobalarrays(encoder_global& eg, encoder_global_b<bitset_size>& egb) {
    // Set mask63
    for (int i = 0; i < 63; i++) {
        egb.mask63[i] = 1;
    }

    // Set basemask for 3-bit encoding
    for (int i = 0; i < eg.max_readlen; i++) {
        egb.basemask[i][static_cast<uint8_t>('A')][3 * i] = 0;
        egb.basemask[i][static_cast<uint8_t>('A')][3 * i + 1] = 0;
        egb.basemask[i][static_cast<uint8_t>('A')][3 * i + 2] = 0;
        egb.basemask[i][static_cast<uint8_t>('C')][3 * i] = 0;
        egb.basemask[i][static_cast<uint8_t>('C')][3 * i + 1] = 0;
        egb.basemask[i][static_cast<uint8_t>('C')][3 * i + 2] = 1;
        egb.basemask[i][static_cast<uint8_t>('G')][3 * i] = 0;
        egb.basemask[i][static_cast<uint8_t>('G')][3 * i + 1] = 1;
        egb.basemask[i][static_cast<uint8_t>('G')][3 * i + 2] = 0;
        egb.basemask[i][static_cast<uint8_t>('T')][3 * i] = 0;
        egb.basemask[i][static_cast<uint8_t>('T')][3 * i + 1] = 1;
        egb.basemask[i][static_cast<uint8_t>('T')][3 * i + 2] = 1;
        egb.basemask[i][static_cast<uint8_t>('N')][3 * i] = 1;
        egb.basemask[i][static_cast<uint8_t>('N')][3 * i + 1] = 0;
        egb.basemask[i][static_cast<uint8_t>('N')][3 * i + 2] = 0;
    }

    // Set noise encoding table (based on Minoche et al. substitution statistics)
    eg.enc_noise[static_cast<uint8_t>('A')][static_cast<uint8_t>('C')] = '0';
    eg.enc_noise[static_cast<uint8_t>('A')][static_cast<uint8_t>('G')] = '1';
    eg.enc_noise[static_cast<uint8_t>('A')][static_cast<uint8_t>('T')] = '2';
    eg.enc_noise[static_cast<uint8_t>('A')][static_cast<uint8_t>('N')] = '3';
    eg.enc_noise[static_cast<uint8_t>('C')][static_cast<uint8_t>('A')] = '0';
    eg.enc_noise[static_cast<uint8_t>('C')][static_cast<uint8_t>('G')] = '1';
    eg.enc_noise[static_cast<uint8_t>('C')][static_cast<uint8_t>('T')] = '2';
    eg.enc_noise[static_cast<uint8_t>('C')][static_cast<uint8_t>('N')] = '3';
    eg.enc_noise[static_cast<uint8_t>('G')][static_cast<uint8_t>('T')] = '0';
    eg.enc_noise[static_cast<uint8_t>('G')][static_cast<uint8_t>('A')] = '1';
    eg.enc_noise[static_cast<uint8_t>('G')][static_cast<uint8_t>('C')] = '2';
    eg.enc_noise[static_cast<uint8_t>('G')][static_cast<uint8_t>('N')] = '3';
    eg.enc_noise[static_cast<uint8_t>('T')][static_cast<uint8_t>('G')] = '0';
    eg.enc_noise[static_cast<uint8_t>('T')][static_cast<uint8_t>('C')] = '1';
    eg.enc_noise[static_cast<uint8_t>('T')][static_cast<uint8_t>('A')] = '2';
    eg.enc_noise[static_cast<uint8_t>('T')][static_cast<uint8_t>('N')] = '3';
    eg.enc_noise[static_cast<uint8_t>('N')][static_cast<uint8_t>('A')] = '0';
    eg.enc_noise[static_cast<uint8_t>('N')][static_cast<uint8_t>('G')] = '1';
    eg.enc_noise[static_cast<uint8_t>('N')][static_cast<uint8_t>('C')] = '2';
    eg.enc_noise[static_cast<uint8_t>('N')][static_cast<uint8_t>('T')] = '3';
}

// =============================================================================
// Main Encoder Entry Point
// =============================================================================

/**
 * @brief Main entry point for encoding algorithm
 * @tparam bitset_size Size of the bitset
 * @param temp_dir Temporary directory for intermediate files
 * @param cp Compression parameters
 *
 * This function orchestrates the encoding process:
 * 1. Read reordered reads
 * 2. Build contigs (consensus sequences)
 * 3. Encode reads as deltas from consensus
 * 4. Compress output streams
 *
 * NOTE: For fq-compressor integration, this should be refactored to:
 * - Accept reads in memory
 * - Return encoded data rather than writing to files
 * - Support block-based processing
 */
template <size_t bitset_size>
void encoder_main(const std::string& temp_dir, const compression_params& cp);

// =============================================================================
// Encode Function (Core Algorithm)
// =============================================================================

/**
 * @brief Core encoding function
 * @tparam bitset_size Size of the bitset
 * @param read Array of read bitsets
 * @param dict Array of dictionaries
 * @param order_s Order array for singleton reads
 * @param read_lengths_s Length array for singleton reads
 * @param eg Non-template global state
 * @param egb Template global state
 *
 * This function:
 * 1. Processes reordered reads to build contigs
 * 2. Tries to align singleton reads to contigs
 * 3. Encodes all reads as deltas from consensus
 */
template <size_t bitset_size>
void encode(std::bitset<bitset_size>* read, bbhashdict* dict,
            uint32_t* order_s, uint16_t* read_lengths_s,
            const encoder_global& eg,
            const encoder_global_b<bitset_size>& egb);

}  // namespace spring

#endif  // SPRING_CORE_ENCODER_H_
