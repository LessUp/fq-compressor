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
 * This file contains the core reordering algorithm for Assembly-based
 * Compression (ABC). The algorithm reorders reads to maximize similarity
 * between consecutive reads, enabling efficient delta encoding.
 *
 * Key concepts:
 * - Minimizer bucketing: Group reads by shared k-mers
 * - Approximate Hamiltonian path: Order reads to minimize total edit distance
 * - Reference sequence tracking: Maintain consensus for delta encoding
 */

#ifndef SPRING_CORE_REORDER_H_
#define SPRING_CORE_REORDER_H_

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <numeric>
#include <string>
#include <utility>

#include "bitset_util.h"
#include "params.h"
#include "util.h"

// Threading support - will be replaced with TBB in fq-compressor
#ifdef USE_OPENMP
#include <omp.h>
#else
// Stubs for non-OpenMP builds
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
// Global State Structure for Reordering
// =============================================================================

/**
 * @brief Global state for the reordering algorithm
 * @tparam bitset_size Size of bitsets (typically 2 * MAX_READ_LEN)
 *
 * This structure holds all state needed for the reordering process.
 * For fq-compressor integration, this should be encapsulated in a
 * ReorderContext class with proper memory management.
 */
template <size_t bitset_size>
struct reorder_global {
    uint32_t numreads;           ///< Total number of reads
    uint32_t numreads_array[2];  ///< Reads per file (for PE)

    int maxshift;                ///< Maximum shift for matching
    int num_thr;                 ///< Number of threads
    int max_readlen;             ///< Maximum read length
    const int numdict = NUM_DICT_REORDER;  ///< Number of dictionaries

    std::string basedir;         ///< Base directory for temp files
    std::string infile[2];       ///< Input file paths
    std::string outfile;         ///< Output file path
    std::string outfileRC;       ///< Reverse complement flags
    std::string outfileflag;     ///< Match flags
    std::string outfilepos;      ///< Position information
    std::string outfileorder;    ///< Read order
    std::string outfilereadlength; ///< Read lengths

    bool paired_end;             ///< Paired-end mode

    /// Basemask: precomputed masks for each position and base
    /// basemask[position][ascii_char] gives the bitset with that base at that position
    std::bitset<bitset_size>** basemask;

    /// Mask with 64 bits set to 1 (for conversion to ullong)
    std::bitset<bitset_size> mask64;

    explicit reorder_global(int max_readlen_param) : max_readlen(max_readlen_param) {
        basemask = new std::bitset<bitset_size>*[max_readlen_param];
        for (int i = 0; i < max_readlen_param; i++) {
            basemask[i] = new std::bitset<bitset_size>[128]();
        }
    }

    ~reorder_global() {
        for (int i = 0; i < max_readlen; i++) {
            delete[] basemask[i];
        }
        delete[] basemask;
    }

    // Non-copyable
    reorder_global(const reorder_global&) = delete;
    reorder_global& operator=(const reorder_global&) = delete;
};

// =============================================================================
// Bitset to String Conversion
// =============================================================================

/**
 * @brief Convert bitset back to DNA string
 * @tparam bitset_size Size of the bitset
 * @param b Bitset to convert (will be destroyed)
 * @param s Output buffer (must be at least readlen+1)
 * @param readlen Length of the sequence
 * @param rg Global state with mask64
 */
template <size_t bitset_size>
void bitsettostring(std::bitset<bitset_size> b, char* s, uint16_t readlen,
                    const reorder_global<bitset_size>& rg) {
    static const char revinttochar[4] = {'A', 'G', 'C', 'T'};
    unsigned long long ull;
    for (int i = 0; i < 2 * readlen / 64 + 1; i++) {
        ull = (b & rg.mask64).to_ullong();
        b >>= 64;
        for (int j = 32 * i; j < 32 * i + 32 && j < readlen; j++) {
            s[j] = revinttochar[ull % 4];
            ull /= 4;
        }
    }
    s[readlen] = '\0';
}

// =============================================================================
// Global Array Initialization
// =============================================================================

/**
 * @brief Initialize global arrays (basemask and mask64)
 * @tparam bitset_size Size of the bitset
 * @param rg Global state to initialize
 *
 * Sets up the basemask array for DNA encoding:
 * - A: 00 at position [2*i, 2*i+1]
 * - C: 01
 * - G: 10
 * - T: 11
 */
template <size_t bitset_size>
void setglobalarrays(reorder_global<bitset_size>& rg) {
    // Set mask64 to have first 64 bits set
    for (int i = 0; i < 64; i++) {
        rg.mask64[i] = 1;
    }

    // Set basemask for each position and base
    for (int i = 0; i < rg.max_readlen; i++) {
        rg.basemask[i][static_cast<uint8_t>('A')][2 * i] = 0;
        rg.basemask[i][static_cast<uint8_t>('A')][2 * i + 1] = 0;
        rg.basemask[i][static_cast<uint8_t>('C')][2 * i] = 0;
        rg.basemask[i][static_cast<uint8_t>('C')][2 * i + 1] = 1;
        rg.basemask[i][static_cast<uint8_t>('G')][2 * i] = 1;
        rg.basemask[i][static_cast<uint8_t>('G')][2 * i + 1] = 0;
        rg.basemask[i][static_cast<uint8_t>('T')][2 * i] = 1;
        rg.basemask[i][static_cast<uint8_t>('T')][2 * i + 1] = 1;
    }
}

// =============================================================================
// Reference Count Update
// =============================================================================

/**
 * @brief Update reference sequence based on matched read
 * @tparam bitset_size Size of the bitset
 * @param cur Current read bitset
 * @param ref Output reference bitset
 * @param revref Output reverse complement reference bitset
 * @param count Base counts at each position [4][max_readlen]
 * @param resetcount Whether to reset counts (new contig)
 * @param rev Whether current read is reverse complemented
 * @param shift Shift amount between reads
 * @param cur_readlen Length of current read
 * @param ref_len Reference length (updated)
 * @param rg Global state
 *
 * This function maintains a consensus reference sequence by tracking
 * base counts at each position. When a new read is added, counts are
 * updated and the reference is recomputed as the majority base at each
 * position.
 */
template <size_t bitset_size>
void updaterefcount(std::bitset<bitset_size>& cur,
                    std::bitset<bitset_size>& ref,
                    std::bitset<bitset_size>& revref,
                    int** count, bool resetcount, bool rev, int shift,
                    uint16_t cur_readlen, int& ref_len,
                    const reorder_global<bitset_size>& rg) {
    static const char inttochar[4] = {'A', 'C', 'T', 'G'};
    auto chartoint = [](uint8_t a) { return (a & 0x06) >> 1; };

    char s[MAX_READ_LEN + 1], s1[MAX_READ_LEN + 1], *current;
    bitsettostring<bitset_size>(cur, s, cur_readlen, rg);

    if (!rev) {
        current = s;
    } else {
        reverse_complement(s, s1, cur_readlen);
        current = s1;
    }

    if (resetcount) {
        // Start new contig
        std::fill(count[0], count[0] + rg.max_readlen, 0);
        std::fill(count[1], count[1] + rg.max_readlen, 0);
        std::fill(count[2], count[2] + rg.max_readlen, 0);
        std::fill(count[3], count[3] + rg.max_readlen, 0);
        for (int i = 0; i < cur_readlen; i++) {
            count[chartoint(static_cast<uint8_t>(current[i]))][i] = 1;
        }
        ref_len = cur_readlen;
    } else {
        // Update existing contig
        if (!rev) {
            // Forward match: shift counts left
            for (int i = 0; i < ref_len - shift; i++) {
                for (int j = 0; j < 4; j++) {
                    count[j][i] = count[j][i + shift];
                }
                if (i < cur_readlen) {
                    count[chartoint(static_cast<uint8_t>(current[i]))][i] += 1;
                }
            }
            for (int i = ref_len - shift; i < cur_readlen; i++) {
                for (int j = 0; j < 4; j++) {
                    count[j][i] = 0;
                }
                count[chartoint(static_cast<uint8_t>(current[i]))][i] = 1;
            }
            ref_len = std::max<int>(ref_len - shift, cur_readlen);
        } else {
            // Reverse match: more complex shifting
            if (cur_readlen - shift >= ref_len) {
                for (int i = cur_readlen - shift - ref_len; i < cur_readlen - shift; i++) {
                    for (int j = 0; j < 4; j++) {
                        count[j][i] = count[j][i - (cur_readlen - shift - ref_len)];
                    }
                    count[chartoint(static_cast<uint8_t>(current[i]))][i] += 1;
                }
                for (int i = 0; i < cur_readlen - shift - ref_len; i++) {
                    for (int j = 0; j < 4; j++) {
                        count[j][i] = 0;
                    }
                    count[chartoint(static_cast<uint8_t>(current[i]))][i] = 1;
                }
                for (int i = cur_readlen - shift; i < cur_readlen; i++) {
                    for (int j = 0; j < 4; j++) {
                        count[j][i] = 0;
                    }
                    count[chartoint(static_cast<uint8_t>(current[i]))][i] = 1;
                }
                ref_len = cur_readlen;
            } else if (ref_len + shift <= rg.max_readlen) {
                for (int i = ref_len - cur_readlen + shift; i < ref_len; i++) {
                    count[chartoint(static_cast<uint8_t>(
                        current[i - (ref_len - cur_readlen + shift)]))][i] += 1;
                }
                for (int i = ref_len; i < ref_len + shift; i++) {
                    for (int j = 0; j < 4; j++) {
                        count[j][i] = 0;
                    }
                    count[chartoint(static_cast<uint8_t>(
                        current[i - (ref_len - cur_readlen + shift)]))][i] = 1;
                }
                ref_len = ref_len + shift;
            } else {
                for (int i = 0; i < rg.max_readlen - shift; i++) {
                    for (int j = 0; j < 4; j++) {
                        count[j][i] = count[j][i + (ref_len + shift - rg.max_readlen)];
                    }
                }
                for (int i = rg.max_readlen - cur_readlen; i < rg.max_readlen - shift; i++) {
                    count[chartoint(static_cast<uint8_t>(
                        current[i - (rg.max_readlen - cur_readlen)]))][i] += 1;
                }
                for (int i = rg.max_readlen - shift; i < rg.max_readlen; i++) {
                    for (int j = 0; j < 4; j++) {
                        count[j][i] = 0;
                    }
                    count[chartoint(static_cast<uint8_t>(
                        current[i - (rg.max_readlen - cur_readlen)]))][i] = 1;
                }
                ref_len = rg.max_readlen;
            }
        }

        // Compute reference as majority base at each position
        for (int i = 0; i < ref_len; i++) {
            int max = 0, indmax = 0;
            for (int j = 0; j < 4; j++) {
                if (count[j][i] > max) {
                    max = count[j][i];
                    indmax = j;
                }
            }
            current[i] = inttochar[indmax];
        }
    }

    // Convert reference to bitset
    chartobitset<bitset_size>(current, ref_len, ref, rg.basemask);

    // Compute reverse complement
    char revcurrent[MAX_READ_LEN + 1];
    reverse_complement(current, revcurrent, ref_len);
    chartobitset<bitset_size>(revcurrent, ref_len, revref, rg.basemask);
}

// =============================================================================
// Match Search Function
// =============================================================================

/**
 * @brief Search for a matching read in the dictionaries
 * @tparam bitset_size Size of the bitset
 * @param ref Reference bitset to match against
 * @param mask1 Dictionary index masks
 * @param dict_lock Locks for dictionary access
 * @param read_lock Locks for read access
 * @param mask Hamming distance masks
 * @param read_lengths Array of read lengths
 * @param remainingreads Flags for unprocessed reads
 * @param read Array of read bitsets
 * @param dict Array of dictionaries
 * @param k Output: matched read ID
 * @param rev Whether searching reverse complement
 * @param shift Current shift amount
 * @param ref_len Reference length
 * @param rg Global state
 * @return true if match found, false otherwise
 *
 * This function searches the dictionaries for a read that matches the
 * reference within the Hamming distance threshold (THRESH_REORDER).
 */
template <size_t bitset_size>
bool search_match(const std::bitset<bitset_size>& ref,
                  std::bitset<bitset_size>* mask1,
                  omp_lock_t* dict_lock, omp_lock_t* read_lock,
                  std::bitset<bitset_size>** mask,
                  uint16_t* read_lengths, bool* remainingreads,
                  std::bitset<bitset_size>* read, bbhashdict* dict,
                  uint32_t& k, bool rev, int shift, int ref_len,
                  const reorder_global<bitset_size>& rg) {
    static const unsigned int thresh = THRESH_REORDER;
    const int maxsearch = MAX_SEARCH_REORDER;

    std::bitset<bitset_size> b;
    uint64_t ull;
    int64_t dictidx[2];
    uint64_t startposidx;
    bool flag = false;

    for (int l = 0; l < rg.numdict; l++) {
        // Check if dictionary index is within bounds
        if (!rev) {
            if (dict[l].end + shift >= ref_len) continue;
        } else {
            if (dict[l].end >= ref_len + shift || dict[l].start <= shift) continue;
        }

        b = ref & mask1[l];
        ull = (b >> 2 * dict[l].start).to_ullong();
        startposidx = dict[l].bphf->lookup(ull);

        if (startposidx >= dict[l].numkeys) continue;

        // Try to acquire dictionary lock
        if (!omp_test_lock(&dict_lock[startposidx & 0xFFFFFF])) continue;

        dict[l].findpos(dictidx, startposidx);

        if (dict[l].empty_bin[startposidx]) {
            omp_unset_lock(&dict_lock[startposidx & 0xFFFFFF]);
            continue;
        }

        // Verify key matches
        uint64_t ull1 = ((read[dict[l].read_id[dictidx[0]]] & mask1[l]) >>
                         2 * dict[l].start).to_ullong();

        if (ull == ull1) {
            // Search for matching read
            for (int64_t i = dictidx[1] - 1;
                 i >= dictidx[0] && i >= dictidx[1] - maxsearch; i--) {
                auto rid = dict[l].read_id[i];
                size_t hamming;

                if (!rev) {
                    hamming = ((ref ^ read[rid]) &
                               mask[0][rg.max_readlen -
                                       std::min<int>(ref_len - shift, read_lengths[rid])])
                                  .count();
                } else {
                    hamming = ((ref ^ read[rid]) &
                               mask[shift][rg.max_readlen -
                                           std::min<int>(ref_len + shift, read_lengths[rid])])
                                  .count();
                }

                if (hamming <= thresh) {
                    if (!omp_test_lock(&read_lock[rid & 0xFFFFFF])) continue;

                    if (remainingreads[rid]) {
                        remainingreads[rid] = false;
                        k = rid;
                        flag = true;
                    }

                    omp_unset_lock(&read_lock[rid & 0xFFFFFF]);
                    if (flag) break;
                }
            }
        }

        omp_unset_lock(&dict_lock[startposidx & 0xFFFFFF]);
        if (flag) break;
    }

    return flag;
}

// =============================================================================
// Main Reorder Entry Point
// =============================================================================

/**
 * @brief Main entry point for reordering algorithm
 * @tparam bitset_size Size of the bitset
 * @param temp_dir Temporary directory for intermediate files
 * @param cp Compression parameters
 *
 * This function orchestrates the entire reordering process:
 * 1. Initialize global state
 * 2. Read input files
 * 3. Construct dictionaries
 * 4. Perform reordering
 * 5. Write output files
 *
 * NOTE: For fq-compressor integration, this should be refactored to:
 * - Accept reads in memory rather than from files
 * - Return reorder map rather than writing to files
 * - Support block-based processing
 */
template <size_t bitset_size>
void reorder_main(const std::string& temp_dir, const compression_params& cp);

}  // namespace spring

#endif  // SPRING_CORE_REORDER_H_
