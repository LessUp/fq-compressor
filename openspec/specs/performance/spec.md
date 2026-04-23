# Performance Specification

This document defines the performance capabilities of fq-compressor, including parallelism, memory management, and throughput targets.

## Requirements

### Requirement: Multi-thread parallelism
The system SHALL utilize multi-core CPUs for parallel processing.

#### Scenario: Thread scaling
- **WHEN** comparing 1-thread vs 8-thread compression
- **THEN** 8-thread speed is at least 5x faster
- **AND** output is identical

#### Scenario: Thread configuration
- **WHEN** user runs `fqc compress -t 16 -i input.fq -o output.fqc`
- **THEN** 16 threads are used for compression

#### Scenario: Auto thread detection
- **WHEN** user runs `fqc compress -t 0 -i input.fq -o output.fqc`
- **THEN** system automatically detects and uses optimal thread count

### Requirement: Memory control
The system SHALL provide memory limit controls.

#### Scenario: Memory limit enforcement
- **WHEN** user specifies `--memory-limit 2048` (2GB)
- **THEN** peak memory does not exceed 2GB
- **AND** compression completes (possibly in chunks)

#### Scenario: Large file chunking
- **WHEN** file size exceeds memory budget
- **THEN** file is processed in chunks
- **AND** each chunk is independently reordered

### Requirement: TBB pipeline architecture
The system SHALL use Intel oneTBB parallel pipeline for efficient processing.

#### Scenario: Pipeline stages
- **WHEN** compression runs
- **THEN** Reader stage reads chunks (serial)
- **AND** Compressor stage processes blocks (parallel)
- **AND** Writer stage outputs blocks (serial, in-order)

#### Scenario: Pipeline parallelism
- **WHEN** one block is being written
- **THEN** next block is being compressed
- **AND** following block is being read

### Requirement: Async I/O
The system SHALL use asynchronous I/O for improved throughput.

#### Scenario: Double buffering
- **WHEN** reading input file
- **THEN** next chunk is read while current chunk is processed
- **AND** I/O wait time is minimized

### Requirement: Block size adaptation
The system SHALL adapt block size based on read characteristics.

#### Scenario: Short read block size
- **WHEN** processing short reads
- **THEN** block size is 100,000 reads

#### Scenario: Long read block size
- **WHEN** processing long reads
- **THEN** block size is 10,000 reads
- **AND** block bases limit is applied

---

## Performance Targets

| Metric | Target | Stretch Goal |
|--------|--------|--------------|
| Compression speed | 20-50 MB/s/thread | 50-100 |
| Decompression speed | 100-200 MB/s/thread | 200-400 |
| Memory peak | < 4 GB | < 2 GB |

## Thread Scaling

| Threads | Compress Speedup | Decompress Speedup |
|---------|------------------|-------------------|
| 1 | 1.0x | 1.0x |
| 2 | 1.8x | 1.9x |
| 4 | 3.5x | 3.7x |
| 8 | 6.5x | 7.0x |
| 16 | 11x | 12x |

## Memory Usage

| Phase | Typical Usage | Peak |
|-------|---------------|------|
| Reading | 2× block_size | 3× block_size |
| Compressing | block_size × threads | block_size × (threads + 2) |
| Writing | 1 block | 2 blocks |

## Memory Budget Components

| Component | Memory per Read |
|-----------|-----------------|
| Minimizer index | ~16 bytes |
| Reorder map | ~8 bytes |
| Block buffer | ~50 bytes × block_size |
| Compression state | ~1 KB per thread |
