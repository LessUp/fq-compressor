# FQC File Format Specification

This document defines the FQC archive format structure and binary layout.

## Requirements

### Requirement: File structure
The system SHALL produce FQC files with a defined binary layout.

#### Scenario: File layout
- **WHEN** FQC file is created
- **THEN** layout follows this structure:
  - Magic Header (9 bytes)
  - Global Header (variable)
  - Block 0..N
  - Reorder Map (optional)
  - Block Index
  - File Footer (32 bytes)

### Requirement: Magic header
The system SHALL identify FQC files with a magic header.

#### Scenario: Magic header format
- **WHEN** reading FQC file
- **THEN** first 9 bytes are:
  - Magic: `0x89 'F' 'Q' 'C' 0x0D 0x0A 0x1A 0x0A`
  - Version: 1 byte (major:4bit, minor:4bit)

### Requirement: Global header
The system SHALL store global metadata in the global header.

#### Scenario: Global header fields
- **WHEN** reading global header
- **THEN** following fields are present:
  - header_size (uint32)
  - flags (uint64)
  - compression_algo (uint8)
  - checksum_type (uint8)
  - total_read_count (uint64)
  - original_filename (string)

### Requirement: Block structure
The system SHALL organize data into independent blocks.

#### Scenario: Block header format
- **WHEN** reading block header
- **THEN** header is 104 bytes containing:
  - header_size (uint32)
  - block_id (uint32)
  - codec_* (uint8[4]) - codecs for ID/SEQ/QUAL/AUX
  - block_xxhash64 (uint64)
  - uncompressed_count (uint32)
  - uniform_read_length (uint32)
  - compressed_size (uint64)
  - offset_* (uint64[4])
  - size_* (uint64[4])

#### Scenario: Block payload
- **WHEN** reading block payload
- **THEN** streams follow header:
  - ID Stream (compressed)
  - Sequence Stream (compressed)
  - Quality Stream (compressed)
  - Aux Stream (optional, for variable-length reads)

### Requirement: Reorder map
The system SHALL optionally store a reorder map for original order restoration.

#### Scenario: Reorder map storage
- **WHEN** reads were reordered during compression
- **THEN** reorder map is stored before block index
- **AND** map enables O(1) original position lookup

### Requirement: Block index
The system SHALL store a block index for O(1) random access.

#### Scenario: Block index structure
- **WHEN** reading block index
- **THEN** each entry contains:
  - block_id (uint32)
  - first_read_id (uint64)
  - read_count (uint32)
  - byte_offset (uint64)
  - compressed_size (uint32)

### Requirement: File footer
The system SHALL store a fixed-size footer at file end.

#### Scenario: Footer contents
- **WHEN** reading file footer
- **THEN** footer (32 bytes) contains:
  - global_checksum (uint64)
  - block_index_offset (uint64)
  - block_count (uint32)
  - magic_footer (8 bytes)

---

## Binary Format Reference

### Magic Header (9 bytes)

```
Offset | Size | Field
-------|------|------
0      | 8    | Magic: 0x89 'F' 'Q' 'C' 0x0D 0x0A 0x1A 0x0A
8      | 1    | Version (major:4bit, minor:4bit)
```

### Global Header (variable)

```
Offset | Size | Field
-------|------|------
0      | 4    | header_size
4      | 8    | flags
12     | 1    | compression_algo
13     | 1    | checksum_type
14     | 8    | total_read_count
22     | var  | original_filename (UTF-8, null-terminated)
```

### Block Header (104 bytes)

```
Offset | Size | Field
-------|------|------
0      | 4    | header_size
4      | 4    | block_id
8      | 4    | codec_id, codec_seq, codec_qual, codec_aux
12     | 8    | block_xxhash64
20     | 4    | uncompressed_count
24     | 4    | uniform_read_length (0 if variable)
28     | 8    | compressed_size
36     | 32   | offset_id, offset_seq, offset_qual, offset_aux (4×8)
68     | 32   | size_id, size_seq, size_qual, size_aux (4×8)
100    | 4    | reserved
```

### Codec Families

| Value | Name | Use |
|-------|------|-----|
| 0x0 | RAW | No compression |
| 0x1 | ABC_V1 | Spring ABC (Sequence) |
| 0x2 | SCM_V1 | Context Mixing (Quality) |
| 0x3 | DELTA_LZMA | IDs |
| 0x4 | DELTA_ZSTD | IDs |
| 0x7 | ZSTD_PLAIN | Fallback (Sequence) |

### Flag Bits

| Bit | Name | Description |
|-----|------|-------------|
| 0 | IS_PAIRED | Paired-end data |
| 1 | PRESERVE_ORDER | No reordering applied |
| 2-3 | QUALITY_MODE | 0=lossless, 1=illumina, 2=discard |
| 8-9 | PE_LAYOUT | 0=interleaved, 1=consecutive |
