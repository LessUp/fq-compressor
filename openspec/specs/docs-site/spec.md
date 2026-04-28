# Documentation Site Specification

This document defines the documentation site capabilities for fq-compressor, including product explanation, benchmark presentation, and support boundary communication.

## Purpose

Define the public documentation-site requirements for fq-compressor, including product explanation, benchmark traceability, and explicit scope boundaries.
## Requirements
### Requirement: Product explanation ownership
The documentation site SHALL own the public product explanation.

#### Scenario: User lands on docs site
- **WHEN** a user lands on docs site
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

### Requirement: Benchmark pages link to tracked result artifacts
The documentation site SHALL point benchmark readers to tracked result artifacts rather than unsupported static benchmark copy.

#### Scenario: User opens a benchmark page
- **WHEN** a benchmark summary is shown on the docs site
- **THEN** the page links to the tracked JSON or Markdown result artifact that supports it
- **AND** the page does not rely only on hand-maintained comparison tables

### Requirement: Benchmark scope limits are explicit
The documentation site SHALL state when a benchmark summary covers only the supported local toolset or a partial competitor field.

#### Scenario: Specialized peers are unavailable locally
- **WHEN** a benchmark page summarizes the current result set
- **THEN** the page states that unavailable or deferred specialized peers are outside the verified comparison scope
- **AND** the page does not imply a universal ranking

