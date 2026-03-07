# FQC File Format

The `.fqc` format is fq-compressor's custom archive format, designed for **high compression**, **random access**, and **parallel processing**.

## Format Layout

```
┌─────────────────────────────────────────────┐
│  File Header (64 bytes)                      │
│  ├─ Magic Number (8 bytes): "FQC\x00\x01"   │
│  ├─ Version                                  │
│  ├─ Flags (PE, reordered, etc.)              │
│  └─ Global metadata                          │
├─────────────────────────────────────────────┤
│  Block 1 (e.g., 10 MB uncompressed)          │
│  ├─ Block Header (32 bytes)                  │
│  │   ├─ Block size / CRC32                   │
│  │   └─ Read count in block                  │
│  ├─ ID Stream (Delta + Tokenized)            │
│  ├─ Sequence Stream (ABC Encoded)            │
│  └─ Quality Stream (SCM Encoded)             │
├─────────────────────────────────────────────┤
│  Block 2                                     │
├─────────────────────────────────────────────┤
│  ...                                         │
├─────────────────────────────────────────────┤
│  Reorder Map (Optional)                      │
│  ├─ ZigZag varint encoded                    │
│  └─ Maps compressed order → original order   │
├─────────────────────────────────────────────┤
│  Block Index                                 │
│  ├─ Block ID → file offset mapping           │
│  └─ Enables O(1) random access               │
├─────────────────────────────────────────────┤
│  File Footer (32 bytes)                      │
│  ├─ Total read count                         │
│  ├─ Index offset                             │
│  └─ File checksum                            │
└─────────────────────────────────────────────┘
```

## Design Rationale

### Columnar Stream Separation

Within each block, data is physically separated into three independent streams:

| Stream | Content | Compression |
|--------|---------|-------------|
| **ID Stream** | Read identifiers | Tokenization + Delta |
| **Sequence Stream** | DNA bases (A/C/G/T/N) | ABC or Zstd |
| **Quality Stream** | Phred quality scores | SCM arithmetic coding |

This ensures each stream uses the optimal algorithm for its data type.

### Block Independence

Each block is fully self-contained:
- Compression models are **reset** at block boundaries
- Blocks can be **decompressed in parallel** without dependencies
- A corrupted block doesn't affect other blocks

### Random Access

The block index at the end of the file maps logical read ranges to file offsets:

```
Read 0-9999     → Block 0 at offset 64
Read 10000-19999 → Block 1 at offset 1048640
Read 20000-29999 → Block 2 at offset 2097216
...
```

To access reads 15000-16000, fq-compressor:
1. Looks up Block 1 in the index — O(1)
2. Seeks to the block offset
3. Decompresses only Block 1
4. Extracts the requested reads

### Reorder Map

When ABC reordering is enabled, the original read order is preserved in a **Reorder Map**:
- Encoded with ZigZag varint for compact storage
- Allows restoring the original FASTQ order after decompression
- Can be skipped if order preservation is not needed

## Why Not Use an Existing Format?

| Format | Limitation |
|--------|-----------|
| **BAM/CRAM** | Requires a reference genome; designed for aligned data |
| **BGZF** | Block-level compression only; no domain-specific modeling |
| **gzip/xz** | No random access; no columnar separation |

The `.fqc` format combines the best ideas from these formats while adding domain-specific compression for raw FASTQ data.
