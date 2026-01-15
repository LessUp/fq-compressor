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

#include "spring/util.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>

namespace spring {

// =============================================================================
// DNA Bit Encoding/Decoding Implementation
// =============================================================================

void write_dna_in_bits(const std::string& read, std::ofstream& fout) {
    uint8_t dna2int[128];
    dna2int[static_cast<uint8_t>('A')] = 0;
    dna2int[static_cast<uint8_t>('C')] = 2;  // Aligned with bitset representation
    dna2int[static_cast<uint8_t>('G')] = 1;
    dna2int[static_cast<uint8_t>('T')] = 3;

    uint8_t bitarray[128];
    uint8_t pos_in_bitarray = 0;
    uint16_t readlen = static_cast<uint16_t>(read.size());

    fout.write(reinterpret_cast<const char*>(&readlen), sizeof(uint16_t));

    for (int i = 0; i < readlen / 4; i++) {
        bitarray[pos_in_bitarray] = 0;
        for (int j = 0; j < 4; j++) {
            bitarray[pos_in_bitarray] |=
                (dna2int[static_cast<uint8_t>(read[4 * i + j])] << (2 * j));
        }
        pos_in_bitarray++;
    }

    if (readlen % 4 != 0) {
        int i = readlen / 4;
        bitarray[pos_in_bitarray] = 0;
        for (int j = 0; j < readlen % 4; j++) {
            bitarray[pos_in_bitarray] |=
                (dna2int[static_cast<uint8_t>(read[4 * i + j])] << (2 * j));
        }
        pos_in_bitarray++;
    }

    fout.write(reinterpret_cast<const char*>(&bitarray[0]), pos_in_bitarray);
}

void read_dna_from_bits(std::string& read, std::ifstream& fin) {
    uint16_t readlen;
    uint8_t bitarray[128];
    const char int2dna[4] = {'A', 'G', 'C', 'T'};

    fin.read(reinterpret_cast<char*>(&readlen), sizeof(uint16_t));
    read.resize(readlen);

    uint16_t num_bytes_to_read = (static_cast<uint32_t>(readlen) + 4 - 1) / 4;
    fin.read(reinterpret_cast<char*>(&bitarray[0]), num_bytes_to_read);

    uint8_t pos_in_bitarray = 0;
    for (int i = 0; i < readlen / 4; i++) {
        for (int j = 0; j < 4; j++) {
            read[4 * i + j] = int2dna[bitarray[pos_in_bitarray] & 3];
            bitarray[pos_in_bitarray] >>= 2;
        }
        pos_in_bitarray++;
    }

    if (readlen % 4 != 0) {
        int i = readlen / 4;
        for (int j = 0; j < readlen % 4; j++) {
            read[4 * i + j] = int2dna[bitarray[pos_in_bitarray] & 3];
            bitarray[pos_in_bitarray] >>= 2;
        }
    }
}

void write_dnaN_in_bits(const std::string& read, std::ofstream& fout) {
    uint8_t dna2int[128];
    dna2int[static_cast<uint8_t>('A')] = 0;
    dna2int[static_cast<uint8_t>('C')] = 2;
    dna2int[static_cast<uint8_t>('G')] = 1;
    dna2int[static_cast<uint8_t>('T')] = 3;
    dna2int[static_cast<uint8_t>('N')] = 4;

    uint8_t bitarray[256];
    uint8_t pos_in_bitarray = 0;
    uint16_t readlen = static_cast<uint16_t>(read.size());

    fout.write(reinterpret_cast<const char*>(&readlen), sizeof(uint16_t));

    for (int i = 0; i < readlen / 2; i++) {
        bitarray[pos_in_bitarray] = 0;
        for (int j = 0; j < 2; j++) {
            bitarray[pos_in_bitarray] |=
                (dna2int[static_cast<uint8_t>(read[2 * i + j])] << (4 * j));
        }
        pos_in_bitarray++;
    }

    if (readlen % 2 != 0) {
        int i = readlen / 2;
        bitarray[pos_in_bitarray] = 0;
        for (int j = 0; j < readlen % 2; j++) {
            bitarray[pos_in_bitarray] |=
                (dna2int[static_cast<uint8_t>(read[2 * i + j])] << (4 * j));
        }
        pos_in_bitarray++;
    }

    fout.write(reinterpret_cast<const char*>(&bitarray[0]), pos_in_bitarray);
}

void read_dnaN_from_bits(std::string& read, std::ifstream& fin) {
    uint16_t readlen;
    uint8_t bitarray[256];
    const char int2dna[5] = {'A', 'G', 'C', 'T', 'N'};

    fin.read(reinterpret_cast<char*>(&readlen), sizeof(uint16_t));
    read.resize(readlen);

    uint16_t num_bytes_to_read = (static_cast<uint32_t>(readlen) + 2 - 1) / 2;
    fin.read(reinterpret_cast<char*>(&bitarray[0]), num_bytes_to_read);

    uint8_t pos_in_bitarray = 0;
    for (int i = 0; i < readlen / 2; i++) {
        for (int j = 0; j < 2; j++) {
            read[2 * i + j] = int2dna[bitarray[pos_in_bitarray] & 15];
            bitarray[pos_in_bitarray] >>= 4;
        }
        pos_in_bitarray++;
    }

    if (readlen % 2 != 0) {
        int i = readlen / 2;
        for (int j = 0; j < readlen % 2; j++) {
            read[2 * i + j] = int2dna[bitarray[pos_in_bitarray] & 15];
            bitarray[pos_in_bitarray] >>= 4;
        }
    }
}

// =============================================================================
// Variable-length Integer Encoding (Varint)
// =============================================================================

// Based on code from:
// https://github.com/sean-/postgresql-varint/blob/trunk/src/varint.c
// https://github.com/shubhamchandak94/CDTC/blob/master/src/util.cpp

namespace {

uint64_t zigzag_encode64(int64_t n) {
    return static_cast<uint64_t>((n << 1) ^ (n >> 63));
}

int64_t zigzag_decode64(uint64_t n) {
    return static_cast<int64_t>((n >> 1) ^ -(static_cast<int64_t>(n & 1)));
}

}  // namespace

void write_var_int64(int64_t val, std::ofstream& fout) {
    uint64_t uval = zigzag_encode64(val);
    uint8_t byte;

    while (uval > 127) {
        byte = static_cast<uint8_t>(uval & 0x7f) | 0x80;
        fout.write(reinterpret_cast<const char*>(&byte), sizeof(uint8_t));
        uval >>= 7;
    }

    byte = static_cast<uint8_t>(uval & 0x7f);
    fout.write(reinterpret_cast<const char*>(&byte), sizeof(uint8_t));
}

int64_t read_var_int64(std::ifstream& fin) {
    uint64_t uval = 0;
    uint8_t byte;
    uint8_t shift = 0;

    do {
        fin.read(reinterpret_cast<char*>(&byte), sizeof(uint8_t));
        uval |= (static_cast<uint64_t>(byte & 0x7f) << shift);
        shift += 7;
    } while (byte & 0x80);

    return zigzag_decode64(uval);
}

}  // namespace spring
