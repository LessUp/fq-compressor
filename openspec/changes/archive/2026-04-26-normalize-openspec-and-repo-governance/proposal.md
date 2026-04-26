## Why

The repository currently mixes living OpenSpec material, archived specs, redundant documentation, and
tool-specific AI assets without a clear governance boundary. Closeout work needs a single source of
truth and an explicit keep/archive/remove policy.

## What Changes

- Make OpenSpec the unambiguous living requirements system.
- Define repository-governance rules for active specs, archived material, and structural cleanup.
- Reduce contributor confusion by clarifying what stays project-level and what should be archived or
  removed.

## Capabilities

### New Capabilities

- `repo-governance`: governs living specs, archived reference material, and cleanup boundaries for
  the repository.

### Modified Capabilities

- `build-system`: clarify which repository-level files are authoritative for contributor workflows.

## Impact

- `openspec/config.yaml`
- `openspec/specs/`
- `openspec/changes/`
- `specs/`
- `docs/`
- Root-level governance documents such as `AGENTS.md`, `CLAUDE.md`, and Copilot instructions
