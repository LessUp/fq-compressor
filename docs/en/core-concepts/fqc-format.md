---
title: FQC Archive Format
description: Technical specification of the .fqc archive format
version: 0.2.0
last_updated: 2026-04-22
language: en
---

# FQC Archive Format Specification

> Technical specification for the `.fqc` file format

## Overview

The FQC (FASTQ Compressed) format is a domain-specific archive format optimized for genomic FASTQ data. It features:

- **Block-based storage**: Independent compressed blocks for parallel access
- **Columnar streams**: Separate storage for IDs, sequences, and qualities
- **Random access**: O(1) access to any read range
- **Integrity verification**: Checksums at multiple levels
- **Reorder support**: Read ordering map for original-order restoration

## File Structure

```
┌─────────────────────────────────────────────────────────────────┐
│                        FQC Archive                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Fixed Header (256 bytes)                                │   │
│  │   - Magic: "FQCC" (4 bytes)                            │   │
│  │   - Version: uint16_t                                   │   │
│  │   - Format flags: uint16_t                              │   │
│  │   - Block count: uint32_t                               │   │
│  │   - Total reads: uint64_t                               │   │
│  │   - Total bases: uint64_t                               │   │
│  │   - Reserved: padding to 256 bytes                      │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Compression Parameters Block (variable)                 │   │
│  │   - Algorithm parameters                               │   │
│  │   - Quality compression settings                       │   │
│  │   - Block size configuration                           │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Block 0                                                 │   │
│  │   ┌─────────────────────────────────────────────────┐   │   │
│  │   │ Block Header (64 bytes)                         │   │   │
│  │   │   - Block ID, original size, compressed size    │   │   │
│  │   │   - Read count, base count                      │   │   │
│  │   │   - Checksum (CRC32C)                           │   │   │
│  │   └─────────────────────────────────────────────────┘   │   │
│  │   ┌─────────────────────────────────────────────────┐   │   │
│  │   │ Compressed Data                                 │   │   │
│  │   │   ┌─────────────┐ ┌─────────────┐ ┌─────────┐  │   │   │
│  │   │   │ ID Stream   │ │ Seq Stream  │ │ Qual    │  │   │   │
│  │   │   │ (zstd)      │ │ (zstd)      │ │ Stream  │  │   │   │
│  │   │   │             │ │             │ │ (zstd)  │  │   │   │
│  │   │   └─────────────┘ └─────────────┘ └─────────┘  │   │   │
│  │   └─────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Block 1 ...                                            │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Block N-1                                              │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Footer (variable)                                       │   │
│  │   - Block Index Table                                  │   │
│  │   - Reorder Map (if enabled)                           │   │
│  │   - Global Metadata (optional)                         │   │
│  │   - Footer checksum & size                             │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Fixed Header Format

```c
struct FQCHeader {
    // Magic number
    char magic[4] = {'F', 'Q', 'C', 'C'};  // "FQCC"
    
    // Version information
    uint16_t version_major = 0;
    uint16_t version_minor = 1;
    uint16_t version_patch = 0;
    
    // Format flags (bitmask)
    uint16_t format_flags;
    // Bit 0: Paired-end data
    // Bit 1: Reordered (has reorder map)
    // Bit 2: Original order preserved
    // Bit 3-15: Reserved
    
    // File statistics
    uint32_t block_count;
    uint64_t total_reads;
    uint64_t total_bases;
    
    // Reserved for future use
    uint8_t reserved[224];  // Pad to 256 bytes
};
```

## Block Format

### Block Header

```c
struct BlockHeader {
    uint32_t block_id;           // Sequential block number
    uint32_t flags;              // Block-specific flags
    
    // Size information
    uint64_t original_size;      // Uncompressed data size
    uint64_t compressed_size;    // Compressed data size
    
    // Content statistics
    uint32_t read_count;         // Number of reads in this block
    uint64_t base_count;         // Number of bases in this block
    
    // Checksums
    uint32_t crc32c_data;        // CRC32C of compressed data
    uint32_t crc32c_uncompressed; // CRC32C of uncompressed data
    
    // Reserved
    uint8_t reserved[16];
};
```

### Block Data Layout

Each block contains three separate compressed streams:

```c
struct BlockData {
    // ID Stream (tokenized + delta encoded)
    uint64_t id_stream_offset;
    uint64_t id_stream_size;
    
    // Sequence Stream (ABC encoded)
    uint64_t seq_stream_offset;
    uint64_t seq_stream_size;
    
    // Quality Stream (SCM encoded)
    uint64_t qual_stream_offset;
    uint64_t qual_stream_size;
    
    // Compressed data follows
    uint8_t data[];
};
```

#### ID Stream Format

```c
struct IDStream {
    // Header
    uint32_t compression_type;   // 0 = zstd, 1 = lz4
    uint32_t token_count;        // Number of tokens
    
    // Token dictionary (if used)
    uint32_t dict_entries;
    // Followed by dictionary entries...
    
    // Compressed token data
    uint8_t compressed_data[];
};
```

#### Sequence Stream Format

```c
struct SequenceStream {
    // Header
    uint32_t compression_type;
    uint32_t consensus_count;    // Number of consensus sequences
    
    // Consensus sequences (variable length)
    uint32_t consensus_lengths[];  // Length of each consensus
    char consensus_data[];         // Concatenated consensus sequences
    
    // Delta-encoded reads
    uint8_t compressed_deltas[];
};

struct DeltaEntry {
    uint32_t consensus_id;       // Which consensus this read maps to
    uint32_t edit_count;         // Number of edits
    Edit edits[];                // Array of edits
};

struct Edit {
    uint8_t type;                // 0=match, 1=sub, 2=ins, 3=del
    uint32_t position;           // Position in consensus
    char base;                   // For substitution/insertion
};
```

#### Quality Stream Format

```c
struct QualityStream {
    // Header
    uint32_t compression_type;   // 0 = SCM + arithmetic
    uint32_t context_order;      // Order of context model
    
    // Model parameters (for SCM)
    uint32_t model_weights[];    // Context model weights
    
    // Compressed quality scores
    uint8_t compressed_data[];
};
```

## Footer Format

### Block Index Table

```c
struct BlockIndex {
    uint32_t entry_count;        // = block_count
    
    struct BlockIndexEntry {
        uint64_t offset;         // File offset of block
        uint64_t size;           // Total block size (header + data)
        uint32_t read_count;     // Number of reads in block
        uint64_t first_read_id;  // Global read ID of first read
    } entries[];
};
```

### Reorder Map

Used to restore original read order after compression reordering.

```c
struct ReorderMap {
    uint32_t format_version = 1;
    uint64_t read_count;
    uint32_t compression_type;   // 0 = zstd, 1 = none
    
    // Mapping: compressed_order -> original_order
    // Stored as delta-encoded varints for compression
    uint8_t compressed_mapping[];
};
```

**Delta Encoding**:
```
Original: [1000, 5, 999, 4, 998, 3, ...]
Delta:    [1000, -995, 994, -995, 994, -995, ...]
Varint:   Compact integer encoding
```

### Footer Trailer

```c
struct FooterTrailer {
    uint64_t footer_size;        // Size of footer in bytes
    uint64_t footer_offset;      // File offset where footer begins
    uint64_t index_offset;       // Offset to block index
    uint64_t reorder_map_offset; // Offset to reorder map (0 if none)
    uint32_t crc32c_footer;      // Checksum of footer data
};
```

## Format Flags

| Bit | Name | Description |
|-----|------|-------------|
| 0 | `PAIRED_END` | Archive contains paired-end data |
| 1 | `REORDERED` | Reads were reordered during compression |
| 2 | `ORIG_ORDER_PRESERVED` | Original order can be restored |
| 3 | `HAS_METADATA` | Contains global metadata block |
| 4-15 | Reserved | Must be 0 |

## Version History

| Version | Date | Changes |
|---------|------|--------- |
| 0.1.0 | 2026-04-16 | Initial release |

## Implementation Notes

### Block Size Selection

Default block size: ~10MB uncompressed per block

- **Smaller blocks**: Better random access granularity, more overhead
- **Larger blocks**: Better compression, worse random access

Recommendation:
- For random access workloads: 5-10MB blocks
- For sequential workloads: 10-20MB blocks

### Compression Levels

| Mode | ZSTD Level | Use Case |
|------|------------|----------|
| fast | 1 | Speed critical |
| balanced | 3 | Default |
| best | 19 | Storage critical |

### Memory Mapping

The format supports memory-mapped reading:

```cpp
// Block alignment ensures block headers are at predictable offsets
constexpr size_t kBlockAlignment = 4096;  // Page size

// Total block size is padded to alignment
padded_size = ((actual_size + kBlockAlignment - 1) / kBlockAlignment) * kBlockAlignment;
```

### Checksums

- **CRC32C**: Used for data integrity (hardware accelerated on x86)
- **Coverage**: Block data, footer structure
- **Verification**: On-read and explicit via `fqc verify`

## Compatibility

### Forward Compatibility

Newer readers should:
- Ignore unknown format flags
- Skip unknown footer sections
- Handle larger than expected headers

### Backward Compatibility

Older readers encountering newer formats should:
- Reject files with higher major version
- Warn on higher minor version
- Work with compatible feature subsets

---

**🌐 Language**: [English (current)](./fqc-format.md) | [简体中文](../../zh/core-concepts/fqc-format.md)
