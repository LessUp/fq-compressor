## Why

`fq-compressor` is in closeout mode, but the repository state still reflects several partially
overlapping cleanup efforts running in parallel. The result is drift across OpenSpec changes,
documentation surfaces, GitHub presentation, AI instruction entrypoints, and engineering workflows.

The owner wants a single closeout track that can bring the project to a stable, archive-ready
standard without publicly repositioning it as abandoned. That requires consolidating the current
cleanup work into one authoritative change and using it to drive the remaining repository hardening.

## What Changes

- Consolidate the current closeout-oriented OpenSpec changes into one umbrella change.
- Define the audit, governance, and closeout criteria for repository surfaces.
- Reposition documentation and GitHub Pages around a lean landing-story model.
- Revalidate historical bug reports, preserve the green default baseline, and fix any real remaining
  closeout-critical defects.
- Simplify CI/CD, dependency/version anchors, AI tooling entrypoints, and local developer tooling.
- Produce the final readiness checklist, GitHub metadata alignment, and handoff material for the last
  maintenance phase.

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
- `scripts/dev/preflight.sh`
- `AGENTS.md`, `CLAUDE.md`, `.github/copilot-instructions.md`
- `.claude/`, `.github/prompts/`, `.github/skills/`, `.clangd`, `.vscode/`
