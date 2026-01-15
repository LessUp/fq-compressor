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

#include "spring/encoder.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <list>
#include <string>
#include <vector>

namespace spring {

// =============================================================================
// Consensus Building Implementation
// =============================================================================

std::string buildcontig(std::list<contig_reads>& current_contig,
                        uint32_t list_size) {
    static const char longtochar[5] = {'A', 'C', 'G', 'T', 'N'};
    static const long chartolong[128] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 3, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    // Single read case
    if (list_size == 1) {
        return current_contig.front().read;
    }

    // Build count array for each position
    auto current_contig_it = current_contig.begin();
    int64_t currentpos = 0;
    int64_t currentsize = 0;
    int64_t to_insert;
    std::vector<std::array<long, 4>> count;

    for (; current_contig_it != current_contig.end(); ++current_contig_it) {
        if (current_contig_it == current_contig.begin()) {
            // First read
            to_insert = current_contig_it->read_length;
        } else {
            currentpos = current_contig_it->pos;
            if (currentpos + current_contig_it->read_length > currentsize) {
                to_insert = currentpos + current_contig_it->read_length - currentsize;
            } else {
                to_insert = 0;
            }
        }

        count.insert(count.end(), to_insert, {0, 0, 0, 0});
        currentsize = currentsize + to_insert;

        for (long i = 0; i < current_contig_it->read_length; i++) {
            count[currentpos + i][chartolong[static_cast<uint8_t>(
                current_contig_it->read[i])]] += 1;
        }
    }

    // Build consensus as majority base at each position
    std::string ref(count.size(), 'A');
    for (size_t i = 0; i < count.size(); i++) {
        long max = 0;
        long indmax = 0;
        for (long j = 0; j < 4; j++) {
            if (count[i][j] > max) {
                max = count[i][j];
                indmax = j;
            }
        }
        ref[i] = longtochar[indmax];
    }

    return ref;
}

// =============================================================================
// Contig Writing Implementation
// =============================================================================

void writecontig(const std::string& ref,
                 std::list<contig_reads>& current_contig,
                 std::ofstream& f_seq, std::ofstream& f_pos,
                 std::ofstream& f_noise, std::ofstream& f_noisepos,
                 std::ofstream& f_order, std::ofstream& f_RC,
                 std::ofstream& f_readlength,
                 const encoder_global& eg, uint64_t& abs_pos) {
    // Write consensus sequence
    f_seq << ref;

    uint16_t pos_var;
    long prevj = 0;

    for (auto& cr : current_contig) {
        long currentpos = cr.pos;
        prevj = 0;

        // Find and encode differences (noise)
        for (long j = 0; j < cr.read_length; j++) {
            if (cr.read[j] != ref[currentpos + j]) {
                f_noise << eg.enc_noise[static_cast<uint8_t>(ref[currentpos + j])]
                                       [static_cast<uint8_t>(cr.read[j])];
                pos_var = static_cast<uint16_t>(j - prevj);
                f_noisepos.write(reinterpret_cast<const char*>(&pos_var),
                                 sizeof(uint16_t));
                prevj = j;
            }
        }
        f_noise << "\n";

        // Write position
        uint64_t abs_current_pos = abs_pos + currentpos;
        f_pos.write(reinterpret_cast<const char*>(&abs_current_pos),
                    sizeof(uint64_t));

        // Write order
        f_order.write(reinterpret_cast<const char*>(&cr.order), sizeof(uint32_t));

        // Write read length
        f_readlength.write(reinterpret_cast<const char*>(&cr.read_length),
                           sizeof(uint16_t));

        // Write RC flag
        f_RC << cr.RC;
    }

    abs_pos += ref.size();
}

}  // namespace spring
