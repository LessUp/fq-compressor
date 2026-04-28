# Random Access Specification

This document defines the random access capabilities of fq-compressor, enabling O(1) read extraction without full decompression.

## Requirements

### Requirement: Block-based architecture
The system SHALL organize FQC files as independent blocks for random access.

#### Scenario: Block independence
- **WHEN** extracting any range of reads
- **THEN** only necessary blocks are read
- **AND** no sequential scan is needed

#### Scenario: O(1) seek time
- **WHEN** extracting reads at arbitrary positions
- **THEN** seek time is constant regardless of file size
- **AND** time complexity is O(1)

### Requirement: Block index
The system SHALL maintain a block index at the end of each FQC file.

#### Scenario: Index location
- **WHEN** FQC file is created
- **THEN** block index is written before file footer
- **AND** index can be read without decompressing blocks

#### Scenario: Index contents
- **WHEN** reading block index
- **THEN** each block entry contains:
  - Block ID
  - Starting read ID
  - Read count
  - Byte offset in file
  - Compressed size

### Requirement: Range query processing
The system SHALL efficiently process range queries using the block index.

#### Scenario: Single block range
- **WHEN** requested range fits within one block
- **THEN** only that block is read and decompressed

#### Scenario: Multi-block range
- **WHEN** requested range spans multiple blocks
- **THEN** only those blocks are read and decompressed
- **AND** reads are concatenated in correct order

### Requirement: Read count verification
The system SHALL verify read indices against total count.

#### Scenario: Out of range error
- **WHEN** user specifies `--range 1000000:1000100` but file has only 500,000 reads
- **THEN** error message is displayed
- **AND** exit code is 1 (ARGUMENT_ERROR)

### Requirement: Memory efficiency
The system SHALL use memory proportional to extracted range size.

#### Scenario: Small range memory usage
- **WHEN** extracting small range from large file
- **THEN** memory usage is proportional to range size
- **AND** not proportional to total file size

### Requirement: Paired-end random access
The system SHALL support pair-aware random access.

#### Scenario: Pair range extraction
- **WHEN** user runs `fqc decompress --range-pairs 100:200 -i paired.fqc`
- **THEN** 100 pairs (200 reads) are extracted
- **AND** both R1 and R2 of each pair are included

#### Scenario: Layout-independent pair access
- **WHEN** extracting pairs from consecutive layout file
- **THEN** pairs are correctly reconstructed
- **AND** R1 and R2 are properly matched

### Requirement: Random-access claims match the primary compression path
Random-access claims SHALL match the primary compression path.

#### Scenario: Project describes random-access support
- **WHEN** the project describes random-access support
- **THEN** the description matches the main supported compression path
- **AND** does not depend on an undocumented alternate path

### Requirement: Original-order restoration semantics are explicit
Original-order restoration semantics SHALL be explicit.

#### Scenario: User asks for original-order restoration
- **WHEN** a user asks for original-order restoration
- **THEN** requirements on reorder metadata are stated explicitly
- **AND** unsupported combinations are documented rather than implied to work

---

## Implementation Notes

### Block Index Structure

```
Block Index Entry (24 bytes each):
┌────────────────────────────────────┐
│ block_id      (uint32)             │
│ first_read_id (uint64)             │
│ read_count    (uint32)             │
│ byte_offset   (uint64)             │
│ compressed_sz (uint32)             │
└────────────────────────────────────┘
```

### Seek Algorithm

```
function find_blocks(range_start, range_end):
    1. Read block index from file end
    2. Binary search for first_read_id ≤ range_start
    3. Binary search for last block with first_read_id ≤ range_end
    4. Return list of relevant blocks
```

### Performance Characteristics

| Operation | Time Complexity | Notes |
|-----------|-----------------|-------|
| Seek to read N | O(log B) | B = number of blocks |
| Extract range | O(R + B) | R = reads in range, B = blocks touched |
| Full scan | O(N) | N = total reads |
