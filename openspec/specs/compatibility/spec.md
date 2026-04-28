# Version Compatibility Specification

This document defines the version compatibility capabilities of fq-compressor.

## Requirements

### Requirement: Backward compatibility
The system SHALL ensure new readers can read all old file versions.

#### Scenario: Read older version file
- **WHEN** FQC file created with v1.0 is read by v1.5
- **THEN** file reads successfully
- **AND** all data is preserved

#### Scenario: Graceful version handling
- **WHEN** reader encounters unknown version
- **THEN** error message indicates unsupported version
- **AND** exit code is 3 (FORMAT_ERROR)

### Requirement: Forward compatibility
The system SHALL allow old readers to skip unknown fields in newer files.

#### Scenario: Read newer minor version
- **WHEN** FQC file created with v1.5 is read by v1.0
- **THEN** unknown fields are skipped
- **AND** core data is readable
- **AND** warning is displayed about newer format

#### Scenario: Reject newer major version
- **WHEN** FQC file created with v2.0 is read by v1.0
- **THEN** error message indicates incompatible major version
- **AND** exit code is 3 (FORMAT_ERROR)

### Requirement: Version encoding
The system SHALL encode version information in the file header.

#### Scenario: Version format
- **WHEN** FQC file is created
- **THEN** version is encoded as (major:4bit, minor:4bit)
- **AND** version is stored in magic header

### Requirement: Header extensibility
The system SHALL support extensible headers for future features.

#### Scenario: Header size field
- **WHEN** reading global header
- **THEN** header_size field indicates total size
- **AND** unknown fields can be skipped

#### Scenario: Block header extensibility
- **WHEN** reading block header
- **THEN** header_size field allows skipping unknown fields
- **AND** older readers can read newer blocks

### Requirement: Format migration
The system SHALL provide guidance for format migrations.

#### Scenario: Migration warning
- **WHEN** reading file with deprecated format features
- **THEN** warning suggests migration options
- **AND** operation completes successfully

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

---

## Version Encoding

```
Version byte: [MMMM|mmmm]
              ^^^^^ major version (4 bits)
                    ^^^^^ minor version (4 bits)

Example: v1.5 = 0x15
         v2.0 = 0x20
```

## Compatibility Matrix

| File Version | Reader v1.0 | Reader v1.5 | Reader v2.0 |
|--------------|-------------|-------------|-------------|
| v1.0 | ✅ Full | ✅ Full | ✅ Full |
| v1.5 | ⚠️ Partial | ✅ Full | ✅ Full |
| v2.0 | ❌ Reject | ⚠️ Partial | ✅ Full |

Legend:
- ✅ Full: All features supported
- ⚠️ Partial: Core features work, new features skipped
- ❌ Reject: Incompatible major version
