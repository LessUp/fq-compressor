# Documentation Site Specification

This document defines the documentation site capabilities for fq-compressor, including product explanation, benchmark presentation, and support boundary communication.

## Requirements

### Requirement: Product explanation ownership
The documentation site SHALL own the public product explanation.

#### Scenario: User lands on docs site
- **WHEN** a user lands on the docs site
- **THEN** it explains what `fq-compressor` is
- **AND** links to installation, quick start, benchmarks, and release surfaces

### Requirement: README mirroring avoidance
The documentation site SHALL avoid mirroring README content.

#### Scenario: User reads both entry surfaces
- **WHEN** a user reads both entry surfaces
- **THEN** README stays concise
- **AND** docs site provides the expanded explanation without copying the same long-form sections verbatim

### Requirement: Benchmark traceability
Benchmark claims on the documentation site SHALL remain traceable.

#### Scenario: User views benchmark result
- **WHEN** a benchmark result is presented on the docs site
- **THEN** the page identifies workload, dataset source, and reproduction path
- **AND** the claim is not unsupported marketing copy

### Requirement: Explicit support boundaries
The documentation site SHALL explicitly state support boundaries.

#### Scenario: User evaluates project fit
- **WHEN** a user evaluates project fit
- **THEN** the docs site states supported inputs, important limits, and any closeout-stage constraints
