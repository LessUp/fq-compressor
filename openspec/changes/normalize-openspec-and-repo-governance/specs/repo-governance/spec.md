# Repo Governance Specification

## Requirements

### Requirement: OpenSpec is the living requirements source
The repository SHALL treat `openspec/specs/` as the only living requirements source for active
development.

#### Scenario: Reading active requirements
- **WHEN** a contributor or automation agent needs the current project requirements
- **THEN** it reads `openspec/specs/`
- **AND** it treats `openspec/changes/` as the place for pending work

### Requirement: Archived material is labeled and non-authoritative
The repository SHALL label archived specs and legacy documentation as reference-only material.

#### Scenario: Encountering archived specs
- **WHEN** a contributor opens `specs/` or another archived surface
- **THEN** the repository explains that the material is historical reference
- **AND** it points the contributor back to OpenSpec for active work

### Requirement: Cleanup uses an explicit retention policy
The repository SHALL classify major repository surfaces before aggressive cleanup proceeds.

#### Scenario: Starting a repository cleanup pass
- **WHEN** a contributor prepares to remove or consolidate repository assets
- **THEN** the assets are first classified as keep, archive, merge, or remove
- **AND** the resulting cleanup stays consistent with the published governance policy
