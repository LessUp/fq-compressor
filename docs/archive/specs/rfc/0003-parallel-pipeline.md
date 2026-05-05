# RFC-0003: Parallel Pipeline

> **Status**: Adopted  
> **Author**: Project Team  
> **Date**: 2026-01-20

---

## Summary

This RFC describes the Intel oneTBB parallel pipeline architecture that enables efficient multi-core scaling for both compression and decompression.

---

## Pipeline Architecture

### Design Pattern: Producer-Consumer

Based on the pigz model: Read → Process → Write

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   Reader     │ ──→ │  Compressor  │ ──→ │   Writer     │
│   (Serial)   │     │  (Parallel)  │     │   (Serial)   │
└──────────────┘     └──────────────┘     └──────────────┘
      │                     │                    │
      ▼                     ▼                    ▼
  Read chunks        Process blocks        Write blocks
  from input         in parallel           in order
```

### TBB parallel_pipeline

```cpp
tbb::parallel_pipeline(
    max_threads,
    // Stage 1: Reader (Serial, InOrder)
    tbb::make_filter<void, Chunk>(
        tbb::filter_mode::serial_in_order,
        [&](tbb::flow_control& fc) -> Chunk {
            if (eof) { fc.stop(); return {}; }
            return read_chunk(input_stream, chunk_size);
        }
    )
    // Stage 2: Compressor (Parallel)
    & tbb::make_filter<Chunk, Block>(
        tbb::filter_mode::parallel,
        [&](Chunk chunk) -> Block {
            return compress_chunk(chunk);
        }
    )
    // Stage 3: Writer (Serial, InOrder)
    & tbb::make_filter<Block, void>(
        tbb::filter_mode::serial_in_order,
        [&](Block block) {
            write_block(output_stream, block);
        }
    )
);
```

---

## Pipeline Stages

### Stage 1: Reader (Serial)

**Responsibilities**:
- Read FASTQ records from input
- Parse and validate format
- Create chunks of `block_size` reads

**Implementation**:
```cpp
class ReaderFilter {
public:
    Chunk operator()(tbb::flow_control& fc) {
        Chunk chunk;
        while (chunk.size() < block_size) {
            auto record = parser.parse_next();
            if (!record) {
                fc.stop();
                break;
            }
            chunk.push_back(*record);
        }
        return chunk;
    }

private:
    FastqParser parser;
    size_t block_size;
};
```

**Block Size by Read Class**:
| Class | Block Size | Rationale |
|-------|------------|-----------|
| Short | 100K reads | Balance memory and compression |
| Medium | 50K reads | Larger reads need more memory |
| Long | 10K reads | Prevent memory exhaustion |
| Ultra-long | 10K + max_bases | Cap total bases per block |

### Stage 2: Compressor (Parallel)

**Responsibilities**:
- Compress ID stream (Delta + Zstd)
- Compress Sequence stream (ABC or Zstd)
- Compress Quality stream (SCM)
- Calculate block checksum

**Implementation**:
```cpp
class CompressFilter {
public:
    Block operator()(Chunk chunk) {
        Block block;
        block.header.uncompressed_count = chunk.size();
        
        // Separate streams
        auto ids = extract_ids(chunk);
        auto seqs = extract_sequences(chunk);
        auto quals = extract_qualities(chunk);
        
        // Compress in parallel within block
        block.id_stream = id_compressor.compress(ids);
        block.seq_stream = seq_compressor.compress(seqs);
        block.qual_stream = qual_compressor.compress(quals);
        
        // Checksum
        block.header.block_xxhash64 = compute_checksum(ids, seqs, quals);
        
        return block;
    }
    
private:
    IDCompressor id_compressor;
    SequenceCompressor seq_compressor;
    QualityCompressor qual_compressor;
};
```

### Stage 3: Writer (Serial, InOrder)

**Responsibilities**:
- Write blocks in original order
- Update block index
- Calculate global checksum

**Implementation**:
```cpp
class WriterFilter {
public:
    void operator()(Block block) {
        // Update index
        index.entries.push_back({
            .offset = current_offset,
            .compressed_size = block.compressed_size(),
            .archive_id_start = current_archive_id,
            .read_count = block.header.uncompressed_count
        });
        
        // Write block
        writer.write_block(block);
        
        // Update state
        current_offset += block.compressed_size();
        current_archive_id += block.header.uncompressed_count;
        
        // Update global checksum
        global_hash.update(block.data());
    }

private:
    FQCWriter writer;
    BlockIndex index;
    uint64_t current_offset = 0;
    uint64_t current_archive_id = 1;
    xxhash64 global_hash;
};
```

---

## Memory Management

### Memory Budget

```cpp
struct MemoryBudget {
    size_t max_total_mb = 8192;      // Total limit
    size_t phase1_reserve_mb = 2048; // Global analysis
    size_t block_buffer_mb = 512;    // Block pool
    size_t per_worker_mb = 64;       // Stack/scratch
};
```

### Estimation

| Component | Memory per Read |
|-----------|-----------------|
| Minimizer index | ~16 bytes |
| Reorder map | ~8 bytes |
| Block buffer | ~50 bytes × block_size |
| Compression state | ~1 KB per thread |

### Chunked Processing

For files exceeding memory budget:

```
File Size: 100 GB
Memory Budget: 8 GB
Chunk Size: 4 GB (with overhead)

Processing:
├── Chunk 0: Reads 0-50M → Blocks 0-500
├── Chunk 1: Reads 50M-100M → Blocks 501-1000
├── ...
└── Chunk N: Final blocks

Note: Each chunk has independent reordering
      (slight compression ratio reduction)
```

---

## Async I/O

### Double Buffering

```
┌─────────────────────────────────────────────────────┐
│                 Buffer Pool                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐            │
│  │ Buffer 0 │ │ Buffer 1 │ │ Buffer 2 │            │
│  │ (Filled) │ │ (Filling)│ │ (Empty)  │            │
│  └──────────┘ └──────────┘ └──────────┘            │
│       ↓             ↓             ↓                 │
│    Reader        Reader        Reader               │
│    writing       waiting       ready                │
└─────────────────────────────────────────────────────┘
```

### AsyncReader

```cpp
class AsyncReader {
public:
    Chunk read_next() {
        // Return already-filled buffer
        auto chunk = filled_queue.pop();
        
        // Trigger async fill of next
        async_fill_next();
        
        return chunk;
    }

private:
    void async_fill_next() {
        std::async(std::launch::async, [this]() {
            auto chunk = read_from_disk();
            filled_queue.push(chunk);
        });
    }
    
    ThreadSafeQueue<Chunk> filled_queue;
};
```

---

## Parallel Decompression

### Strategy

For range extraction, decompress only necessary blocks:

```
Request: --range 50000:51000

1. Binary search in Block Index
   → Block 500 contains reads 50000-50200
   → Block 501 contains reads 50201-50400
   → ...

2. Parallel decompress needed blocks:
   TBB::parallel_for(block_range, [&](BlockID id) {
       decompress_block(id);
   });

3. Filter to requested range
4. Output in order
```

### Thread-Local State

Each decompression thread maintains:
- Codec state (reset per block)
- Decode buffer
- Checksum accumulator

---

## Performance Characteristics

### Scaling

| Threads | Compress Speedup | Decompress Speedup |
|---------|------------------|-------------------|
| 1 | 1.0x | 1.0x |
| 2 | 1.8x | 1.9x |
| 4 | 3.5x | 3.7x |
| 8 | 6.5x | 7.0x |
| 16 | 11x | 12x |

*Diminishing returns due to serial I/O stages*

### Memory Usage

| Phase | Typical Usage | Peak |
|-------|---------------|------|
| Reading | 2× block_size | 3× block_size |
| Compressing | block_size × threads | block_size × (threads + 2) |
| Writing | 1 block | 2 blocks |

---

## References

- Intel oneTBB: https://oneapi-src.github.io/oneTBB/
- pigz: https://zlib.net/pigz/
- TBB Pipeline Tutorial: Intel Documentation
