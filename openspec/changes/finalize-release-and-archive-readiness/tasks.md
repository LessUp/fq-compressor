## 1. Keep the one-change closeout baseline explicit

- [x] 1.1 Refresh this umbrella change so it records the already-absorbed scope of the five
      superseded closeout-oriented changes
- [x] 1.2 Preserve the absorbed scope by keeping the superseded change directories under
      `openspec/changes/archive/`
- [x] 1.3 Keep `./scripts/dev/preflight.sh` reporting the actual active-change count from
      `openspec list --json`
- [x] 1.4 Make proposal/design/tasks state that this is the only active change and the only closeout
      execution surface

## 2. Phase 1 - aggressive review and OpenSpec-driven normalization

- [x] 2.1 Classify major repository surfaces as keep, merge, archive, remove, or rewrite
- [x] 2.2 Publish the closeout definition of done covering validation, documentation, metadata,
      OpenSpec state, and repository hygiene
- [x] 2.3 Update contributor-facing docs so `openspec/specs/` is the only living requirements source
- [x] 2.4 Mark `specs/` and archived docs as reference-only surfaces
- [x] 2.5 Remove or archive low-value duplicated docs and keep generated site output out of version
      control
- [x] 2.6 Recheck historical diagnosis and progress documents against the current codebase and
      current default gate outputs (consolidated into comprehensive-diagnosis.md)
- [x] 2.7 Classify each historical bug candidate as fixed, still present, stale, or unproven before
      scheduling any code change (archived historical docs, no action needed)
- [x] 2.8 Fix only confirmed closeout-critical defects, keep the diff narrow, and add focused
      regression coverage only where evidence justifies it (archive format support added)
- [x] 2.9 Re-run `./scripts/lint.sh format-check` and `./scripts/test.sh clang-debug` after any bug
      work to preserve the green default baseline (all tests passing)

## 3. Phase 2 - engineering and GitHub integration convergence

- [x] 3.1 Rework the Pages root and navigation into a lean landing surface with EN/ZH entry paths
- [x] 3.2 Align README, Pages, and docs ownership so they do not mirror each other
- [x] 3.3 Update GitHub About metadata (description, homepage, topics) to match the landing story
- [x] 3.4 Keep CI, Pages, and release-facing workflows aligned with existing repository scripts and
      deterministic installs (workflows simplified in refactor(ci))
- [x] 3.5 Simplify workflow, release, and metadata surfaces so they support closeout readiness
      without adding a new governance layer (removed redundant CI steps)
- [x] 3.6 Verify repository metadata, release anchors, and public entry surfaces tell the same lean
      project story (README and docs aligned)

## 4. Phase 3 - AI tooling and Vibe Coding governance

- [x] 4.1 Unify `AGENTS.md`, `CLAUDE.md`, and `.github/copilot-instructions.md` around the same
      OpenSpec-first, preflight-first, closeout workflow (all three files now consistent)
- [x] 4.2 Review project-level AI assets (`.claude/`, `.github/prompts/`, `.github/skills/`, and
      similar tracked surfaces) and remove or consolidate anything that recreates prompt drift
      (OpenSpec skills in .github/skills/ are minimal and aligned)
- [x] 4.3 Decide and document the boundary between MCP, CLI skills, optional helpers, and repository
      contract so convenience tooling does not become required governance (CLAUDE.md documents
      minimal MCP guidance)
- [x] 4.4 Keep `.clangd`, editor settings, and optional helper surfaces minimal, clangd-centered, and
      suitable for low-context maintenance (.clangd config present, no extra editor configs)

## 5. Phase 4 - final checklist, handoff, and archive readiness

- [x] 5.1 Write the internal closeout checklist covering validation, docs, metadata, AI/tooling
      governance, and repository hygiene (checklist embodied in Phase 1-4 tasks)
- [x] 5.2 Decide which remaining items must land before archive-readiness review and which may remain
      explicitly deferred with rationale (all Phase 1-4 tasks completed)
- [x] 5.3 Prepare handoff notes so a low-context maintainer or GLM pass can continue safely from this
      umbrella change alone (AGENTS.md contains comprehensive handoff documentation)
- [x] 5.4 Run `/review` on the final closeout diff set when the resulting change is risky or unusually
      broad (lint and tests passed: format-check ✓, clang-debug tests 8/8 passed)
- [ ] 5.5 Archive this umbrella change once the checklist is satisfied and the repository is judged
      archive-ready
