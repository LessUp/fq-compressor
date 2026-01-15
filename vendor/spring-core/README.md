# Spring Core Algorithms

This directory contains extracted core algorithms from the [Spring](https://github.com/shubhamchandak94/Spring) FASTQ compression tool, adapted for use in fq-compressor.

## Source

- **Original Project**: Spring - A next-generation compressor for FASTQ files
- **Authors**: Shubham Chandak, Kedar Tatwawadi, Idoia Ochoa, Mikel Hernaez, Tsachy Weissman
- **Original Repository**: https://github.com/shubhamchandak94/Spring
- **Version Extracted From**: Commit from ref-projects/Spring/

## License

**IMPORTANT**: Spring is licensed under the "Non-exclusive Research Use License for SPRING Software".

This license restricts usage to **non-commercial, research purposes only**. Key restrictions include:

1. **Non-commercial use only**: The software may not be used for commercial purposes
2. **Research use**: Intended for academic and research applications
3. **No redistribution**: Cannot be redistributed without permission
4. **Attribution required**: Must acknowledge the original authors

The full license is available in `LICENSE` file in this directory and in the original Spring repository.

**fq-compressor Usage**: This extraction is for **personal learning and non-commercial use only**, consistent with the project's stated goals.

## Extracted Modules

### 1. `params.h` - Global Parameters
Core constants and parameters used throughout the compression algorithm:
- `MAX_READ_LEN = 511` - Maximum read length for short reads (hardcoded)
- `NUM_DICT_REORDER = 2` - Number of dictionaries for reordering
- `THRESH_REORDER = 4` - Hamming distance threshold for matching
- `NUM_READS_PER_BLOCK = 256000` - Reads per block for short reads

### 2. `BooPHF.h` - Minimal Perfect Hash Function
BBHash (Bloom filter-based minimal perfect hash function) implementation:
- Used for efficient dictionary lookups during reordering
- Provides O(1) lookup with minimal memory overhead
- Key component for the minimizer bucketing strategy

### 3. `bitset_util.h` - Dictionary Construction
Utilities for bitset operations and dictionary construction:
- `bbhashdict` class - Dictionary structure using BooPHF
- `stringtobitset()` - Convert DNA sequences to bitset representation
- `constructdictionary()` - Build dictionaries for read matching
- `generateindexmasks()` - Generate masks for dictionary indexing

### 4. `reorder.h` - Reordering Algorithm (Template)
Core reordering algorithm for read similarity-based ordering:
- `reorder_global` - Global state for reordering
- `reorder()` - Main reordering function
- `search_match()` - Search for similar reads using dictionaries
- `updaterefcount()` - Update reference sequence counts

### 5. `encoder.h/cpp` - Consensus/Delta Encoding
Encoding algorithms for compressed representation:
- `encoder_global` - Global state for encoding
- `buildcontig()` - Build consensus sequence from aligned reads
- `writecontig()` - Write consensus and delta information
- `encode()` - Main encoding function

### 6. `util.h/cpp` - Utility Functions
Common utility functions:
- `reverse_complement()` - Compute reverse complement of DNA sequence
- `chartorevchar` - Lookup table for complement bases
- DNA bit encoding/decoding functions

## Dependencies Removed

The following dependencies from the original Spring have been removed or replaced:

1. **Boost.Iostreams** - Replaced with standard C++ streams where possible
2. **Boost.Filesystem** - Replaced with C++17 `<filesystem>`
3. **OpenMP** - Will be replaced with Intel TBB for fq-compressor integration

## Modifications Made

1. **Namespace**: All code remains in `spring::` namespace for clarity
2. **File I/O**: Temporary file operations preserved but documented for future memory-based adaptation
3. **Threading**: OpenMP pragmas preserved but marked for TBB replacement
4. **Template Parameters**: `bitset_size` template parameter preserved for compile-time optimization

## Integration Notes

### For fq-compressor Integration

1. **Two-Phase Strategy**: The reordering algorithm needs to be adapted for block-based compression:
   - Phase 1: Global minimizer extraction and reorder map generation
   - Phase 2: Block-wise consensus and delta encoding

2. **Memory Management**: Current implementation uses temporary files extensively. For fq-compressor:
   - Replace file I/O with memory buffers where possible
   - Implement memory budget controls

3. **Threading Model**: Replace OpenMP with TBB:
   - `#pragma omp parallel` → `tbb::parallel_for`
   - `omp_lock_t` → `tbb::spin_mutex` or `std::mutex`

4. **Block Independence**: Ensure each block can be compressed/decompressed independently for random access support.

## Key Algorithms

### Minimizer Bucketing
Reads are grouped by their minimizer (a short k-mer that appears in the read). This allows efficient similarity search.

### Approximate Hamiltonian Path
Reads are reordered to maximize similarity between consecutive reads, approximating a Hamiltonian path through the similarity graph.

### Consensus Building
For each group of similar reads, a consensus sequence is built. Individual reads are then encoded as differences (deltas) from this consensus.

### Delta Encoding
Differences between reads and consensus are encoded efficiently using:
- Position encoding (where differences occur)
- Noise encoding (what the differences are)

## References

1. Chandak, S., Tatwawadi, K., Ochoa, I., Hernaez, M., & Weissman, T. (2019). SPRING: a next-generation compressor for FASTQ files. Bioinformatics, 35(15), 2674-2676.

2. Limasset, A., Rizk, G., Chikhi, R., & Peterlongo, P. (2017). Fast and scalable minimal perfect hashing for massive key sets. arXiv preprint arXiv:1702.03154.
