# Release Engineering Specification

This document defines the release engineering capabilities for fq-compressor, including artifact publication, support matrix documentation, and release validation.

## Requirements

### Requirement: Explicit release artifacts
Release artifacts SHALL be explicit.

#### Scenario: User views published release
- **WHEN** a user views a published release
- **THEN** available artifacts are listed clearly
- **AND** installation/usage guidance is linked

### Requirement: Explicit support matrix
The support matrix SHALL be explicit.

#### Scenario: User checks release compatibility
- **WHEN** a user checks release compatibility
- **THEN** the support matrix is documented
- **AND** unsupported scenarios are not implied as supported

### Requirement: Repository script validation
Release validation SHALL use repository scripts.

#### Scenario: Maintainer prepares release
- **WHEN** the maintainer prepares a release
- **THEN** the documented validation flow uses `./scripts/lint.sh format-check` and `./scripts/test.sh clang-debug`
- **AND** additional checks are additive

### Requirement: Closeout-aligned release messaging
Release messaging SHALL match project closeout posture.

#### Scenario: User reads release notes
- **WHEN** a user reads release notes
- **THEN** they emphasize support boundaries, validation, and finished capability intent
- **AND** they do not suggest open-ended expansion
