# AI Collaboration Specification

## Requirements

### Requirement: AI-assisted work follows a shared solo-maintainer loop
The repository SHALL require AI-assisted work to follow the same sequence: local preflight,
OpenSpec inspection, implementation, local validation, and direct push.

#### Scenario: Starting AI-assisted work
- **WHEN** an AI tool starts non-trivial repository work
- **THEN** it runs the preflight checks
- **AND** it inspects the active OpenSpec changes before editing code

#### Scenario: Finishing AI-assisted work
- **WHEN** an AI tool prepares a non-trivial change for merge
- **THEN** it completes the local validation commands
- **AND** it may run `/review` when the diff is risky or unusually broad

### Requirement: Project-level AI instructions stay focused
The repository SHALL provide short, project-specific AI instruction files instead of generic prompt
libraries.

#### Scenario: Reading project instructions
- **WHEN** Claude or Copilot reads project instructions
- **THEN** it learns the OpenSpec-first workflow, closeout posture, and validation commands
- **AND** it does not need to infer the workflow from unrelated generic guidance
