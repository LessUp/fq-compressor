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

#ifndef SPRING_CORE_UTIL_H_
#define SPRING_CORE_UTIL_H_

#include <cstdint>
#include <fstream>
#include <string>

namespace spring {

// =============================================================================
// DNA Complement Lookup Table
// =============================================================================

/// Lookup table for reverse complement: chartorevchar['A'] = 'T', etc.
/// Index by ASCII value of base character
static const char chartorevchar[128] = {
    0, 0,   0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0,   0, 0, 0,
    0, 0,   0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0,   0, 0, 0,
    0, 0,   0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0,   0, 0, 'T',
    0, 'G', 0, 0, 0, 'C', 0, 0, 0, 0, 0, 0, 'N', 0, 0, 0, 0, 0, 'A', 0, 0, 0,
    0, 0,   0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0,   0, 0, 0,
    0, 0,   0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0};

// =============================================================================
// DNA Sequence Operations
// =============================================================================

/**
 * @brief Compute reverse complement of a DNA sequence (C-string version)
 * @param s Input sequence (null-terminated)
 * @param s1 Output buffer for reverse complement (must be at least readlen+1)
 * @param readlen Length of the sequence
 */
inline void reverse_complement(const char* s, char* s1, int readlen) {
    for (int j = 0; j < readlen; j++) {
        s1[j] = chartorevchar[static_cast<uint8_t>(s[readlen - j - 1])];
    }
    s1[readlen] = '\0';
}

/**
 * @brief Compute reverse complement of a DNA sequence (std::string version)
 * @param s Input sequence
 * @param readlen Length of the sequence
 * @return Reverse complement string
 */
inline std::string reverse_complement(const std::string& s, int readlen) {
    std::string s1;
    s1.resize(readlen);
    for (int j = 0; j < readlen; j++) {
        s1[j] = chartorevchar[static_cast<uint8_t>(s[readlen - j - 1])];
    }
    return s1;
}

// =============================================================================
// DNA Bit Encoding/Decoding
// =============================================================================

/**
 * @brief Write DNA sequence in compact bit format (2 bits per base)
 * @param read DNA sequence string
 * @param fout Output file stream
 * 
 * Encoding: A=0, G=1, C=2, T=3 (chosen to align with bitset representation)
 * Format: [uint16_t length][packed bytes]
 */
void write_dna_in_bits(const std::string& read, std::ofstream& fout);

/**
 * @brief Read DNA sequence from compact bit format
 * @param read Output string for DNA sequence
 * @param fin Input file stream
 */
void read_dna_from_bits(std::string& read, std::ifstream& fin);

/**
 * @brief Write DNA sequence with N bases in compact bit format (4 bits per base)
 * @param read DNA sequence string (may contain N)
 * @param fout Output file stream
 * 
 * Encoding: A=0, G=1, C=2, T=3, N=4
 */
void write_dnaN_in_bits(const std::string& read, std::ofstream& fout);

/**
 * @brief Read DNA sequence with N bases from compact bit format
 * @param read Output string for DNA sequence
 * @param fin Input file stream
 */
void read_dnaN_from_bits(std::string& read, std::ifstream& fin);

// =============================================================================
// Variable-length Integer Encoding (Varint)
// =============================================================================

/**
 * @brief Write a signed 64-bit integer using variable-length encoding
 * @param val Value to write
 * @param fout Output file stream
 * 
 * Uses zigzag encoding for efficient storage of signed values
 */
void write_var_int64(int64_t val, std::ofstream& fout);

/**
 * @brief Read a signed 64-bit integer from variable-length encoding
 * @param fin Input file stream
 * @return Decoded value
 */
int64_t read_var_int64(std::ifstream& fin);

// =============================================================================
// String Utilities
// =============================================================================

/**
 * @brief Remove carriage return from end of string (Windows line ending fix)
 * @param str String to modify in place
 */
inline void remove_CR_from_end(std::string& str) {
    if (!str.empty() && str.back() == '\r') {
        str.pop_back();
    }
}

}  // namespace spring

#endif  // SPRING_CORE_UTIL_H_
