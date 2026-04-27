## Why

`fq-compressor` is already down to one active OpenSpec change:
`finalize-release-and-archive-readiness`. The consolidation work happened, but the umbrella
artifacts still read partly like a summary of what was merged instead of the execution surface for
everything that remains.

The repository also has a more stable baseline than the umbrella docs currently acknowledge:

- `openspec list --json` shows exactly one active change
- `./scripts/lint.sh format-check` and `./scripts/test.sh clang-debug` are back to green as the
  default closeout gate
- closeout is now about final convergence, evidence-driven fixes, simplification, and handoff
  readiness rather than feature expansion

This umbrella change therefore needs to become the single closeout program for the rest of the
repository. It must explicitly absorb the remaining work across four phases so a later maintainer,
reviewer, or low-context GLM pass can continue from one place without reopening parallel changes or
guessing which historical artifact is still authoritative.

## What Changes

- Refresh the umbrella proposal, design, and tasks so they document the one-change closeout model
  and explicitly absorb the remaining scope from the previously superseded closeout changes.
- Keep the superseded closeout changes archived as historical inputs rather than reviving any new
  parallel workstream.
- Preserve the current default gate (`./scripts/lint.sh format-check` +
  `./scripts/test.sh clang-debug`) as the baseline for all remaining work and make bug cleanup
  evidence-driven.
- Organize the rest of closeout into four explicit phases:
  - **Phase 1:** aggressive review, evidence-driven bug revalidation, and OpenSpec-guided
    normalization
  - **Phase 2:** workflow, release, Pages, metadata, and GitHub integration simplification
  - **Phase 3:** AI workflow governance, instruction consolidation, and tooling boundary cleanup
  - **Phase 4:** final checklist, handoff, archive-readiness review, and closeout completion
- Clarify how docs, Pages, GitHub metadata, AI instructions, local tooling, and archive-readiness
  work together as one closeout execution surface.

## Capabilities

### New Capabilities

- `release-readiness`: defines the repository state required before entering low-maintenance mode.
- `repo-governance`: governs living specs, archived material, and structural cleanup boundaries.
- `project-presentation`: defines landing-page, documentation, and repository-metadata requirements.
- `ai-collaboration`: defines the shared AI-assisted workflow for this repository.
- `developer-tooling`: defines local workflow helpers and the clangd-centered editor baseline.

### Modified Capabilities

- `build-system`: clarify the default closeout validation gate and workflow-entry expectations.
- `compatibility`: clarify that final closeout includes repository-state and metadata readiness checks.

## Impact

- `openspec/changes/finalize-release-and-archive-readiness/`
- `openspec/changes/archive/`
- `openspec/specs/`
- `specs/`
- `docs/`, `docs/website/`, README and GitHub Pages copy
- `.github/workflows/`
- `.github/` repository metadata and release-facing configuration
- `scripts/dev/preflight.sh`
- `AGENTS.md`, `CLAUDE.md`, `.github/copilot-instructions.md`
- `.claude/`, `.github/prompts/`, `.github/skills/`, `.clangd`, `.vscode/`
- optional helper surfaces related to MCP, CLI skills, and editor integration
