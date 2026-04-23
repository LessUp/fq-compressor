# Atomic Write Specification

This document defines the atomic write and recovery capabilities of fq-compressor.

## Requirements

### Requirement: Atomic file write
The system SHALL use atomic write operations to prevent file corruption on interruption.

#### Scenario: Atomic write process
- **WHEN** writing output file
- **THEN** data is first written to `.fqc.tmp` file
- **AND** file is renamed to final name only after successful completion

#### Scenario: Interrupted compression
- **WHEN** compression is interrupted (SIGINT, SIGTERM, crash)
- **THEN** temporary file is cleaned up
- **AND** no partial output file remains

### Requirement: Signal handling
The system SHALL handle termination signals gracefully.

#### Scenario: SIGINT handling
- **WHEN** user presses Ctrl+C during compression
- **THEN** cleanup is performed
- **AND** temporary file is removed
- **AND** exit is clean

#### Scenario: SIGTERM handling
- **WHEN** process receives SIGTERM
- **THEN** cleanup is performed
- **AND** temporary file is removed

### Requirement: Recovery from partial files
The system SHALL detect and reject incomplete files.

#### Scenario: Detect incomplete file
- **WHEN** user attempts to decompress a file without valid footer
- **THEN** error message indicates incomplete file
- **AND** exit code is 3 (FORMAT_ERROR)

#### Scenario: Detect temporary file
- **WHEN** `.fqc.tmp` file exists from interrupted compression
- **THEN** file is detected as incomplete
- **AND** user is advised to delete it

### Requirement: Disk space validation
The system SHALL validate available disk space before writing.

#### Scenario: Insufficient disk space
- **WHEN** output directory has insufficient space
- **THEN** error message indicates disk space issue
- **AND** no partial file is created

---

## Implementation Notes

### Atomic Write Pattern

```
1. Create output.fqc.tmp
2. Write all data to .tmp file
3. Sync to disk
4. Rename output.fqc.tmp → output.fqc
5. Sync parent directory
```

### Signal Handlers

```cpp
void setup_signal_handlers() {
    std::signal(SIGINT, cleanup_and_exit);
    std::signal(SIGTERM, cleanup_and_exit);
}

void cleanup_and_exit(int signal) {
    if (!temp_file.empty()) {
        std::filesystem::remove(temp_file);
    }
    std::_Exit(128 + signal);
}
```
