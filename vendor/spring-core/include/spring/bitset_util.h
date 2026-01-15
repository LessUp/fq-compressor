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
 * This file contains utilities for:
 * - DNA sequence to bitset conversion
 * - Dictionary construction using BooPHF (minimal perfect hash)
 * - Index mask generation for dictionary lookups
 */

#ifndef SPRING_CORE_BITSET_UTIL_H_
#define SPRING_CORE_BITSET_UTIL_H_

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <fstream>
#include <string>

#include "BooPHF.h"
#include "params.h"

// Threading support - will be replaced with TBB in fq-compressor
#ifdef USE_OPENMP
#include <omp.h>
#else
// Stub for non-OpenMP builds
inline int omp_get_thread_num() { return 0; }
inline int omp_get_num_threads() { return 1; }
inline void omp_set_num_threads(int) {}
#endif

namespace spring {

// =============================================================================
// Dictionary Class using BBHash (Minimal Perfect Hash Function)
// =============================================================================

/**
 * @brief Dictionary structure for efficient read lookup during reordering
 *
 * Uses BooPHF (Bloom filter-based minimal perfect hash function) for O(1)
 * lookup of reads by their k-mer signatures.
 */
class bbhashdict {
public:
    boophf_t* bphf;           ///< Minimal perfect hash function
    int start;                ///< Start position of k-mer in read
    int end;                  ///< End position of k-mer in read
    uint32_t numkeys;         ///< Number of unique keys in dictionary
    uint32_t dict_numreads;   ///< Number of reads in this dictionary
    uint32_t* startpos;       ///< Start positions in read_id array for each key
    uint32_t* read_id;        ///< Array of read IDs
    bool* empty_bin;          ///< Flags for empty bins

    bbhashdict()
        : bphf(nullptr),
          start(0),
          end(0),
          numkeys(0),
          dict_numreads(0),
          startpos(nullptr),
          read_id(nullptr),
          empty_bin(nullptr) {}

    ~bbhashdict() {
        delete[] startpos;
        delete[] read_id;
        delete[] empty_bin;
        delete bphf;
    }

    // Non-copyable
    bbhashdict(const bbhashdict&) = delete;
    bbhashdict& operator=(const bbhashdict&) = delete;

    /**
     * @brief Find position range in read_id array for a given hash index
     * @param dictidx Output: [start, end) indices in read_id array
     * @param startposidx Hash index from MPHF lookup
     */
    void findpos(int64_t* dictidx, uint64_t startposidx) {
        dictidx[0] = startpos[startposidx];
        dictidx[1] = startpos[startposidx + 1];
    }

    /**
     * @brief Remove a read from the dictionary bin
     * @param dictidx Position range from findpos()
     * @param startposidx Hash index
     * @param current Read ID to remove
     */
    void remove(int64_t* dictidx, uint64_t startposidx, int64_t current) {
        // Find and remove the read from the bin
        for (int64_t i = dictidx[0]; i < dictidx[1]; i++) {
            if (read_id[i] == static_cast<uint32_t>(current)) {
                // Swap with last element and shrink
                read_id[i] = read_id[dictidx[1] - 1];
                startpos[startposidx + 1]--;
                if (startpos[startposidx] == startpos[startposidx + 1]) {
                    empty_bin[startposidx] = true;
                }
                break;
            }
        }
    }
};

// =============================================================================
// Bitset Conversion Functions
// =============================================================================

/**
 * @brief Convert DNA string to bitset representation
 * @tparam bitset_size Size of the bitset (typically 2 * MAX_READ_LEN)
 * @param s DNA sequence string
 * @param readlen Length of the sequence
 * @param b Output bitset
 * @param basemask Precomputed masks for each position and base
 *
 * Encoding: Each base uses 2 bits at positions [2*i, 2*i+1]
 * A=00, G=10, C=01, T=11
 */
template <size_t bitset_size>
void stringtobitset(const std::string& s, uint16_t readlen,
                    std::bitset<bitset_size>& b,
                    std::bitset<bitset_size>** basemask) {
    b.reset();
    for (int i = 0; i < readlen; i++) {
        b |= basemask[i][static_cast<uint8_t>(s[i])];
    }
}

/**
 * @brief Convert C-string to bitset representation
 * @tparam bitset_size Size of the bitset
 * @param s DNA sequence (null-terminated)
 * @param readlen Length of the sequence
 * @param b Output bitset (will be reset first)
 * @param basemask Precomputed masks for each position and base
 */
template <size_t bitset_size>
void chartobitset(char* s, int readlen, std::bitset<bitset_size>& b,
                  std::bitset<bitset_size>** basemask) {
    b.reset();
    for (int i = 0; i < readlen; i++) {
        b |= basemask[i][static_cast<uint8_t>(s[i])];
    }
}

// =============================================================================
// Mask Generation Functions
// =============================================================================

/**
 * @brief Generate index masks for dictionary lookups
 * @tparam bitset_size Size of the bitset
 * @param mask1 Output array of masks (one per dictionary)
 * @param dict Array of dictionaries
 * @param numdict Number of dictionaries
 * @param bpb Bits per base (2 for ACGT, 3 for ACGTN)
 *
 * Creates masks that select the k-mer region for each dictionary.
 */
template <size_t bitset_size>
void generateindexmasks(std::bitset<bitset_size>* mask1, bbhashdict* dict,
                        int numdict, int bpb) {
    for (int j = 0; j < numdict; j++) {
        mask1[j].reset();
    }
    for (int j = 0; j < numdict; j++) {
        for (int i = bpb * dict[j].start; i < bpb * (dict[j].end + 1); i++) {
            mask1[j][i] = 1;
        }
    }
}

/**
 * @brief Generate masks for Hamming distance computation
 * @tparam bitset_size Size of the bitset
 * @param mask 2D array of masks [shift][end_trim]
 * @param max_readlen Maximum read length
 * @param bpb Bits per base
 *
 * These masks zero out bits that shouldn't be compared when computing
 * Hamming distance between shifted reads of different lengths.
 */
template <size_t bitset_size>
void generatemasks(std::bitset<bitset_size>** mask, int max_readlen, int bpb) {
    for (int i = 0; i < max_readlen; i++) {
        for (int j = 0; j < max_readlen; j++) {
            mask[i][j].reset();
            for (int k = bpb * i; k < bpb * max_readlen - bpb * j; k++) {
                mask[i][j][k] = 1;
            }
        }
    }
}

// =============================================================================
// Dictionary Construction
// =============================================================================

/**
 * @brief Construct dictionaries for read matching
 * @tparam bitset_size Size of the bitset
 * @param read Array of read bitsets
 * @param dict Array of dictionaries to construct
 * @param read_lengths Array of read lengths
 * @param numdict Number of dictionaries
 * @param numreads Total number of reads
 * @param bpb Bits per base
 * @param basedir Base directory for temporary files
 * @param num_thr Number of threads
 *
 * This function:
 * 1. Extracts k-mers from each read at dictionary positions
 * 2. Builds a minimal perfect hash function for unique k-mers
 * 3. Creates lookup tables mapping k-mers to read IDs
 *
 * NOTE: This implementation uses temporary files for large datasets.
 * For fq-compressor, consider memory-based alternatives.
 */
template <size_t bitset_size>
void constructdictionary(std::bitset<bitset_size>* read, bbhashdict* dict,
                         uint16_t* read_lengths, int numdict,
                         uint32_t numreads, int bpb,
                         const std::string& basedir, int num_thr) {
    std::bitset<bitset_size>* mask = new std::bitset<bitset_size>[numdict];
    generateindexmasks<bitset_size>(mask, dict, numdict, bpb);

    for (int j = 0; j < numdict; j++) {
        uint64_t* ull = new uint64_t[numreads];

        // Compute keys in parallel
#ifdef USE_OPENMP
#pragma omp parallel
#endif
        {
            std::bitset<bitset_size> b;
            int tid = omp_get_thread_num();
            uint64_t i = static_cast<uint64_t>(tid) * numreads / omp_get_num_threads();
            uint64_t stop = static_cast<uint64_t>(tid + 1) * numreads / omp_get_num_threads();
            if (tid == omp_get_num_threads() - 1) stop = numreads;

            for (; i < stop; i++) {
                b = read[i] & mask[j];
                ull[i] = (b >> bpb * dict[j].start).to_ullong();
            }
        }

        // Remove keys for reads shorter than dict_end
        dict[j].dict_numreads = 0;
        for (uint32_t i = 0; i < numreads; i++) {
            if (read_lengths[i] > dict[j].end) {
                ull[dict[j].dict_numreads] = ull[i];
                dict[j].dict_numreads++;
            }
        }

        // Deduplicate keys
        std::sort(ull, ull + dict[j].dict_numreads);
        uint32_t k = 0;
        for (uint32_t i = 1; i < dict[j].dict_numreads; i++) {
            if (ull[i] != ull[k]) {
                ull[++k] = ull[i];
            }
        }
        dict[j].numkeys = k + 1;

        // Construct minimal perfect hash function
        auto data_iterator = boomphf::range(
            static_cast<const uint64_t*>(ull),
            static_cast<const uint64_t*>(ull + dict[j].numkeys));
        double gammaFactor = 5.0;
        dict[j].bphf = new boomphf::mphf<uint64_t, hasher_t>(
            dict[j].numkeys, data_iterator, num_thr, gammaFactor, true, false);

        // Build startpos and read_id arrays
        dict[j].startpos = new uint32_t[dict[j].numkeys + 1]();
        dict[j].empty_bin = new bool[dict[j].numkeys]();

        // Count reads per bin
        for (uint32_t i = 0; i < numreads; i++) {
            if (read_lengths[i] > dict[j].end) {
                std::bitset<bitset_size> b = read[i] & mask[j];
                uint64_t key = (b >> bpb * dict[j].start).to_ullong();
                uint64_t hash = dict[j].bphf->lookup(key);
                if (hash < dict[j].numkeys) {
                    dict[j].startpos[hash + 1]++;
                }
            }
        }

        // Cumulative sum
        for (uint32_t i = 1; i <= dict[j].numkeys; i++) {
            dict[j].startpos[i] += dict[j].startpos[i - 1];
        }

        // Fill read_id array
        dict[j].read_id = new uint32_t[dict[j].dict_numreads];
        uint32_t* current_pos = new uint32_t[dict[j].numkeys]();

        for (uint32_t i = 0; i < numreads; i++) {
            if (read_lengths[i] > dict[j].end) {
                std::bitset<bitset_size> b = read[i] & mask[j];
                uint64_t key = (b >> bpb * dict[j].start).to_ullong();
                uint64_t hash = dict[j].bphf->lookup(key);
                if (hash < dict[j].numkeys) {
                    uint32_t pos = dict[j].startpos[hash] + current_pos[hash];
                    dict[j].read_id[pos] = i;
                    current_pos[hash]++;
                }
            }
        }

        delete[] current_pos;
        delete[] ull;
    }

    delete[] mask;
}

}  // namespace spring

#endif  // SPRING_CORE_BITSET_UTIL_H_
