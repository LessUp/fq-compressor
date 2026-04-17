# RFC-0005: Paired-End Support

> **Status**: Adopted  
> **Author**: Project Team  
> **Date**: 2026-02-05

---

## Summary

This RFC describes the paired-end (PE) support design, including input formats, storage layouts, compression optimizations, and output options.

---

## Input Formats

### Dual File Input

```
fqc compress -1 reads_R1.fastq -2 reads_R2.fastq -o reads.fqc
```

**Validation**:
- R1 and R2 must have equal read counts
- Order must correspond (R1[i] pairs with R2[i])

### Interleaved Input

```
fqc compress --interleaved -i reads.fastq -o reads.fqc
```

**Format**: R1 and R2 alternate
```
@read1/1
SEQUENCE1
+
QUALITY1
@read1/2
SEQUENCE2
+
QUALITY2
@read2/1
...
```

---

## Storage Layouts

### Interleaved (Default)

```
Archive order: R1_0, R2_0, R1_1, R2_1, R1_2, R2_2, ...

Benefits:
- Paired reads stored adjacently
- Better compression (R1/R2 similarity exploited)
- Natural for most workflows
```

### Consecutive

```
Archive order: R1_0, R1_1, ..., R1_N, R2_0, R2_1, ..., R2_N

Benefits:
- Single-end extraction is contiguous
- No skipping needed for R1-only access

Trade-off:
- R1/R2 not adjacent
- Cannot exploit pair similarity for compression
- Slightly lower compression ratio
```

### Layout Selection

```
--pe-layout <interleaved|consecutive>
    Default: interleaved
```

---

## Compression Optimizations

### R1/R2 Complementarity

For paired-end data, R2 is often the reverse complement of R1:
- If R2 matches RC(R1), store only the difference
- Exploits biological complementarity

```cpp
// Pseudocode
if (is_reverse_complement(r1, r2)) {
    // Store R1 normally
    // Store only position/type of mismatches for R2
    pe_optimization = COMPLEMENTARY;
} else {
    // Store both independently
    pe_optimization = INDEPENDENT;
}
```

### Reordering Preservation

During global reordering, paired reads **must move together**:

```
Original: [(R1_0, R2_0), (R1_1, R2_1), (R1_2, R2_2)]
Reorder to: [(R1_1, R2_1), (R1_0, R2_0), (R1_2, R2_2)]

NOT allowed: [(R1_0, R2_1), ...]  // Breaking pairs
```

### Quality Compression

Same SCM model applies, but can use pair context:
- R2 quality often correlates with R1
- Additional context signal for model

---

## Archive ID Semantics

### Read Counting

**PE reads are counted separately**:
```
Total reads = 2 × Number of pairs
archive_id 1 = R1 of pair 1
archive_id 2 = R2 of pair 1
archive_id 3 = R1 of pair 2
archive_id 4 = R2 of pair 2
...
```

### Range Semantics

`--range` always uses archive_id:

| Layout | `--range 1:4` returns |
|--------|----------------------|
| Interleaved | R1_p1, R2_p1, R1_p2, R2_p2 (2 pairs) |
| Consecutive | R1_p1, R1_p2, R1_p3, R1_p4 (4 R1 reads) |

### Pair-Based Range

```
--range-pairs <start:end>

--range-pairs 1:50
    Returns first 50 pairs (100 reads)
    Independent of storage layout
```

---

## Output Options

### Standard Output

```bash
# Interleaved FASTQ output
fqc decompress -i reads.fqc -o output.fastq
```

### Split Output

```bash
# Separate R1 and R2 files
fqc decompress -i reads.fqc --split-pe -o output_prefix
# Creates: output_prefix_R1.fastq, output_prefix_R2.fastq
```

### Header-Only

```bash
# Just the IDs
fqc decompress -i reads.fqc --header-only -o ids.txt
```

---

## ID Reconstruction (Discard Mode)

When `--id-mode discard` is used:

### Interleaved Layout

```
@1/1
@1/2
@2/1
@2/2
@3/1
@3/2
...
```

### Consecutive Layout

```
@1      (R1 of pair 1)
@2      (R1 of pair 2)
...
@N      (R1 of pair N)
@N+1    (R2 of pair 1)
@N+2    (R2 of pair 2)
...
```

### Custom Prefix

```
--id-prefix "SAMPLE_A_"
→ @SAMPLE_A_1/1, @SAMPLE_A_1/2, ...
```

---

## Validation

### Input Validation

```cpp
void validate_pe_files(R1_file, R2_file) {
    size_t r1_count = count_reads(R1_file);
    size_t r2_count = count_reads(R2_file);
    
    if (r1_count != r2_count) {
        throw FormatError(
            "PE read count mismatch: R1=" + r1_count + 
            ", R2=" + r2_count
        );
    }
}
```

### Format Flags

Global Header `flags` field:
- Bit 0 (`IS_PAIRED`): Set for PE data
- Bits 8-9 (`PE_LAYOUT`): 00=interleaved, 01=consecutive

---

## Error Handling

### Mismatched Counts

```
Error: PE read count mismatch
R1 file: 100,000 reads
R2 file: 99,999 reads
Suggestion: Ensure both files are complete and from the same sequencing run
Exit code: 3 (FORMAT_ERROR)
```

### Corrupted Pair

```
Warning: Block X has odd read count in PE mode
This indicates corrupted or truncated data
```

---

## Performance Impact

| Metric | SE | PE (Interleaved) | PE (Consecutive) |
|--------|-----|------------------|------------------|
| Compression ratio | 1.0x | 1.05x | 0.98x |
| Compress speed | 1.0x | 0.95x | 0.95x |
| Decompress speed | 1.0x | 1.0x | 1.0x |

*Interleaved has slight compression advantage due to pair similarity exploitation*
