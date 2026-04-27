# Build System Specification

This document defines the build system capabilities for fq-compressor, including the default closeout validation gate and preflight workflow checks.

## Requirements

### Requirement: Default closeout validation gate remains small and explicit
The default closeout validation gate SHALL remain small and explicit.

#### Scenario: Contributor follows the default validation loop
- **WHEN** a contributor follows the default validation loop
- **THEN** `./scripts/lint.sh format-check` and `./scripts/test.sh clang-debug` are the documented baseline checks
- **AND** additional checks remain optional/situational unless intentionally promoted

### Requirement: Preflight remains the workflow entry check
Preflight SHALL remain the workflow entry check.

#### Scenario: Substantial work begins
- **WHEN** substantial work begins
- **THEN** `./scripts/dev/preflight.sh` is run first
- **AND** `openspec list --json` is used to confirm active-change state
