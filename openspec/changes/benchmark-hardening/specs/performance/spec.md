## ADDED Requirements

### Requirement: Public benchmark claims require reproducible inputs
The project SHALL tie public benchmark claims to dataset provenance, benchmark commands, and tracked result artifacts.

#### Scenario: User reviews a published benchmark claim
- **WHEN** a benchmark claim is published in repository-controlled docs
- **THEN** the dataset source, benchmark command path, and tracked result artifact are identified
- **AND** the claim can be re-run without private context

### Requirement: Tool availability is explicit
The benchmark surface SHALL distinguish requested tools, measured tools, unavailable tools, and deferred specialized-peer comparisons.

#### Scenario: Local benchmark run lacks one or more configured tools
- **WHEN** a benchmark run cannot execute every requested tool
- **THEN** the report records which tools were unavailable
- **AND** the result is not presented as a full-field comparison

### Requirement: Benchmark conclusions are dimension-specific
The project SHALL state benchmark conclusions separately for compression ratio, compression speed, and decompression speed.

#### Scenario: User asks whether fq-compressor is first-tier
- **WHEN** the repository summarizes the current benchmark result set
- **THEN** the answer is scoped to the measured dimensions and workloads
- **AND** unproven dimensions are called out explicitly
