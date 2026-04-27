# Compatibility Specification

This document defines the compatibility capabilities for fq-compressor, including documentation of compatibility boundaries and consistency of compatibility statements across public surfaces.

## Requirements

### Requirement: Compatibility boundaries are documented
Compatibility boundaries SHALL be documented.

#### Scenario: User consults compatibility guidance
- **WHEN** a user consults compatibility guidance
- **THEN** supported and unsupported version boundaries are described explicitly
- **AND** partial support is not presented as full support

### Requirement: Compatibility statements match release messaging
Compatibility statements SHALL match release messaging.

#### Scenario: Compatibility is described in more than one public surface
- **WHEN** compatibility is described in more than one public surface
- **THEN** the stated support boundaries agree
- **AND** no surface promises a broader guarantee than the others
