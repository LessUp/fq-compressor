## ADDED Requirements

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
