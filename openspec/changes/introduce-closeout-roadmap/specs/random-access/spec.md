# Random Access Specification

This document defines the random access capabilities for fq-compressor, including consistency of random-access claims and explicit semantics for original-order restoration.

## Requirements

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
