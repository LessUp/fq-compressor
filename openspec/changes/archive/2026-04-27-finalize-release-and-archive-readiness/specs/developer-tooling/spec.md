# Developer Tooling Specification

## Requirements

### Requirement: Repository preflight command
The repository SHALL provide a single preflight command that reports local repository state and
active OpenSpec changes.

#### Scenario: Starting a change
- **WHEN** a contributor or AI tool starts repository work
- **THEN** it can run one command to inspect local dirtiness, current branch, and active OpenSpec
  changes

### Requirement: Optional worktree bootstrap
The repository SHALL provide an optional command for creating a worktree and feature branch from an
OpenSpec change name.

#### Scenario: Creating a change worktree
- **WHEN** a contributor wants extra isolation for a change named `<change-name>`
- **THEN** the repository can create a dedicated worktree and branch for that change
- **AND** the command remains optional rather than mandatory for the default workflow

### Requirement: clangd is the repository LSP baseline
The repository SHALL publish clangd-oriented project-level configuration.

#### Scenario: Opening the repository in an editor
- **WHEN** an editor or AI tool loads the repository
- **THEN** it can use clangd with the generated compilation database
- **AND** it does not need a second project-level C++ language-server stack
