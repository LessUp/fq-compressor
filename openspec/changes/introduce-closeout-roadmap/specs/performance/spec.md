# Performance Specification

This document defines the performance capabilities for fq-compressor, including reproducibility of public benchmarks and prioritization of FASTQ-specific comparisons.

## Requirements

### Requirement: Public benchmarks are reproducible
Public benchmarks SHALL be reproducible.

#### Scenario: Benchmark result is published in repository-controlled docs
- **WHEN** a benchmark result is published in repository-controlled docs
- **THEN** workload, dataset source, command path, and comparison context are identified
- **AND** another engineer can reproduce the run without private context

### Requirement: FASTQ-specific comparisons are prioritized
FASTQ-specific comparisons SHALL be prioritized.

#### Scenario: Benchmark set is updated
- **WHEN** the benchmark set is updated
- **THEN** FASTQ-specialized peer tools are included where practical
- **AND** generic compressors are not the only comparison class
