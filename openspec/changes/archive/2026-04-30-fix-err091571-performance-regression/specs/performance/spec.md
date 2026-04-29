## ADDED Requirements

### Requirement: Regression-sensitive benchmark workloads remain actionable
The project SHALL maintain at least one repository-controlled benchmark workload that can expose the measured `ERR091571` short-read regression during normal engineering work.

#### Scenario: Maintainer investigates the tracked short-read slowdown
- **WHEN** a maintainer runs the repository benchmark flow for the tracked `ERR091571` workload
- **THEN** the workload reproduces the regression-sensitive short-read path with repository-controlled inputs
- **AND** the result can be compared before and after an optimization change

### Requirement: Performance fixes are validated in fast and large-data tiers
Performance-sensitive changes SHALL be validated with both a rapid functional tier and a larger performance-validation tier.

#### Scenario: Maintainer implements a performance fix
- **WHEN** a maintainer changes code to improve benchmark behavior
- **THEN** the change is checked on small local data for quick functional verification
- **AND** the change is checked on larger local data for runtime, stability, memory, and I/O behavior

### Requirement: Tracked benchmark evidence is refreshed after measured improvements
The project SHALL refresh tracked benchmark artifacts after a change materially improves or regresses a tracked benchmark workload.

#### Scenario: Maintainer lands a short-read performance fix
- **WHEN** a performance fix changes the measured outcome for the tracked `ERR091571` artifact
- **THEN** the corresponding tracked JSON and Markdown benchmark artifacts are regenerated
- **AND** repository-facing benchmark summaries continue to match the refreshed evidence
