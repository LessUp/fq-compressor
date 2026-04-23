# Quality Handling Specification

This document defines the quality score handling capabilities of fq-compressor, including lossless, lossy, and discard modes.

## Requirements

### Requirement: Lossless quality compression
The system SHALL preserve all quality values in lossless mode.

#### Scenario: Lossless quality preservation
- **WHEN** FASTQ file is compressed and decompressed in lossless quality mode
- **THEN** all quality scores match original exactly
- **AND** no data loss occurs

### Requirement: Illumina binning
The system SHALL support Illumina 8-level quality binning for reduced file size.

#### Scenario: Apply Illumina binning
- **WHEN** user runs `fqc compress --quality-mode illumina -i input.fq -o output.fqc`
- **THEN** quality scores are mapped to 8 bins
- **AND** file size is reduced

#### Scenario: Illumina bin mapping
- **WHEN** Illumina binning is applied
- **THEN** quality scores are mapped as follows:
  - [0-9] → bin 0
  - [10-19] → bin 1
  - [20-24] → bin 2
  - [25-29] → bin 3
  - [30-34] → bin 4
  - [35-37] → bin 5
  - [38-40] → bin 6
  - [41] → bin 7

### Requirement: Quality discard
The system SHALL support dropping quality values entirely.

#### Scenario: Discard quality values
- **WHEN** user runs `fqc compress --quality-mode discard -i input.fq -o output.fqc`
- **THEN** quality values are not stored
- **AND** maximum compression is achieved for quality stream

#### Scenario: Quality reconstruction on decompress
- **WHEN** decompressing file compressed with quality discard
- **THEN** quality lines are `!` repeated (Phred 0)
- **AND** sequence and ID are preserved

### Requirement: Custom quality placeholder
The system SHALL allow custom placeholder quality values.

#### Scenario: Custom placeholder
- **WHEN** user runs `fqc decompress --placeholder-qual 'I' -i input.fqc -o output.fq`
- **THEN** quality lines use specified character (Phred 40)

### Requirement: SCM context model
The system SHALL use Statistical Context Mixing for quality compression.

#### Scenario: Order-2 context for short reads
- **WHEN** compressing short read quality scores
- **THEN** order-2 context model is used (2 previous QVs)
- **AND** higher compression ratio is achieved

#### Scenario: Order-1 context for long reads
- **WHEN** compressing long read quality scores
- **THEN** order-1 context model is used (1 previous QV)
- **AND** memory usage is reduced

---

## Algorithm Reference

### SCM Context Model

```
Context = {
    previous quality scores (order-1 or order-2),
    current base (A/C/G/T/N),
    position in read,
    run-length of identical scores
}
```

### SCM Pipeline

1. **Context Extraction** - Previous QVs, current base, position
2. **Probability Estimation** - Look up context in model
3. **Adaptive Update** - Update model with observed value
4. **Range Coding** - Encode using predicted distribution

### Performance Characteristics

| Algorithm | Compress Speed | Decompress Speed | Ratio |
|-----------|---------------|------------------|-------|
| SCM (Quality) | 50-100 MB/s | 100-200 MB/s | ~0.8 bits/QV |
