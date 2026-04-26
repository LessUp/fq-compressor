## Context

The repository already contains the right broad ideas for closeout, but they are currently spread
across six active OpenSpec changes:

- `finalize-release-and-archive-readiness`
- `normalize-openspec-and-repo-governance`
- `reposition-docs-and-pages`
- `stabilize-baseline-health`
- `standardize-ai-collaboration`
- `streamline-engineering-tooling`

That fragmentation conflicts with the repository's own rule of working one change at a time and makes
future execution harder for lower-cost models or sporadic maintenance passes. Closeout work therefore
starts by collapsing those parallel efforts into one umbrella change.

The repository is also in a mixed state:

- default local validation is green again, so bug work must be evidence-driven rather than speculative
- documentation, Pages, and GitHub About still compete as entry surfaces
- AI entrypoints exist, but their authority boundaries are still easy to misread
- workflow/tooling complexity is higher than a closeout-stage project needs

## Goals / Non-Goals

**Goals:**
- Use one umbrella change to drive the remaining closeout work.
- Define what "archive ready" means internally without public deprecation messaging.
- Re-establish clear ownership boundaries across OpenSpec, docs, Pages, GitHub metadata, AI guidance,
  and engineering tooling.
- Revalidate historical defect reports before spending effort on code fixes.
- Keep the repository understandable and operational for sporadic future maintenance or archival.

**Non-Goals:**
- Announcing public end-of-life or maintenance freeze messaging.
- Rewriting every historical artifact in one pass.
- Expanding CI, docs, or AI integration surface area beyond what closeout needs.
- Promising that no future defect can ever exist.

## Decisions

### Use one umbrella change instead of parallel cleanup changes

`finalize-release-and-archive-readiness` becomes the single active closeout change. The remaining
five closeout-oriented changes are treated as superseded inputs: their scope is absorbed here, then
their directories move to `openspec/changes/archive/`.

**Alternatives considered**
- Keep all six changes active with a documented order: rejected because it preserves governance drift.
- Start a brand-new umbrella change: rejected because it would create another layer of change history.

### Revalidate historical bug reports before fixing code

Historical diagnosis documents are useful input, but they are not trusted ground truth. Any reported
bug must be rechecked against the current codebase and current validation state before it becomes a
real closeout task.

**Alternatives considered**
- Treat archived diagnosis material as automatically current: rejected because drift is already visible.
- Ignore historical reports entirely: rejected because closeout should still clear confirmed old debt.

### Keep the public story lean and layered

README remains the repository entry summary, GitHub Pages becomes the landing surface, `docs/en/` and
`docs/zh/` provide maintained reference paths, and archived material is clearly marked as reference
only.

**Alternatives considered**
- Make Pages a README mirror: rejected because it wastes the landing surface.
- Collapse everything into one giant docs tree: rejected because it increases drift and maintenance.

### Keep the default gate small and explicit

Closeout continues to treat `./scripts/lint.sh format-check` and `./scripts/test.sh clang-debug` as
the default health gate. Additional checks may exist, but they do not become the default loop unless
they prove necessary and proportionate.

**Alternatives considered**
- Promote sanitizer/release workflows into the default gate: rejected because they are too expensive
  for every closeout iteration.
- Add a new bespoke health framework: rejected because the repository already has working entrypoints.

### Keep AI guidance unified and minimal

Project-level AI instruction files remain tracked only when they reinforce the same OpenSpec-first,
preflight-first, closeout-oriented workflow. Tool-specific duplication is reduced instead of expanded.

**Alternatives considered**
- Maintain separate workflow definitions for each AI client: rejected because it recreates prompt drift.
- Remove project-level AI guidance entirely: rejected because low-cost models benefit from explicit
  repository-local instructions.

## Risks / Trade-offs

- **[Risk]** The umbrella change becomes too broad.  
  **Mitigation:** organize it into sequential phases with explicit audit boundaries and keep/delete
  classifications before broad cleanup.
- **[Risk]** Archiving superseded changes hides useful design context.  
  **Mitigation:** absorb their requirements and decisions into this design before moving them.
- **[Risk]** Historical bug review expands indefinitely.  
  **Mitigation:** require each candidate bug to be classified as fixed, still present, stale, or
  unproven before it can consume implementation effort.
- **[Risk]** CI and metadata cleanup can regress public trust surfaces.  
  **Mitigation:** keep GitHub About, Pages, README, and validation scripts aligned as one workstream.

## Migration Plan

1. Consolidate the six closeout-oriented changes into this umbrella change.
2. Publish the umbrella proposal/design/tasks and delta specs covering governance, presentation,
   validation, tooling, AI collaboration, and release readiness.
3. Archive the superseded change directories under `openspec/changes/archive/YYYY-MM-DD-<name>/`.
4. Execute the closeout tasks in sequence: audit, docs/pages, bug revalidation and fixes, CI/tooling,
   AI guidance, metadata, final readiness.
5. Produce the final closeout checklist and archive the umbrella change once the repository meets it.

Rollback is straightforward: restore a superseded change from `openspec/changes/archive/` if the
umbrella change proves too coarse or loses required context.

## Repository Surface Audit Matrix

| Surface | Current State | Classification | Closeout Intent |
| --- | --- | --- | --- |
| `openspec/specs/` | Living capability specs | **keep** | Preserve as the only living requirements source |
| `openspec/changes/finalize-release-and-archive-readiness/` | Active umbrella change | **rewrite** | Use as the single closeout execution surface |
| `openspec/changes/archive/` | Newly populated with superseded changes | **keep** | Preserve history, but make it explicitly non-active |
| `specs/` | Archived historical spec tree | **archive** | Keep only as reference and label aggressively |
| `docs/README.md` | Good policy direction, but not yet final | **rewrite** | Make it the clear docs-surface index and policy file |
| `docs/.site/` | Checked-in generated Pages output | **remove** | Stop treating generated site output as source material |
| `docs/archive/` | Mixed historical material, some stale | **prune** | Retain only reference-value material with archive labeling |
| `docs/en/`, `docs/zh/` | Maintained doc trees, potentially overlapping with README/Pages | **keep + prune** | Keep as maintained reference paths with less duplication |
| `docs/website/` | Public site source with useful base | **rewrite** | Reposition as landing surface, not README mirror |
| `README*.md` | Repository entry copy | **keep + tighten** | Keep concise and aligned with Pages/About |
| `.github/workflows/` | Functional but heavier than closeout needs | **prune** | Keep only high-signal workflows aligned with repo scripts |
| `scripts/dev/preflight.sh` | Correct intent, previously wrong JSON counting | **keep + refine** | Preserve as the required repo/OpenSpec sanity check |
| `AGENTS.md`, `CLAUDE.md`, `.github/copilot-instructions.md` | Useful but partially overlapping | **merge + rewrite** | Keep distinct entry files with one shared workflow |
| `.claude/`, `.github/prompts/`, `.github/skills/` | Tool-specific assets with overlap risk | **prune** | Keep only assets that reinforce the shared workflow |
| Root AI client directories (`.agent/`, `.codex/`, `.opencode/`, etc.) | Local/tooling surfaces present in repo root | **ignore as contract** | Do not let optional local tool folders become project governance surfaces |
| `.clangd`, `.vscode/` | Working LSP/editor baseline | **keep + trim** | Keep clangd-centered configuration, reduce editor-specific noise |
| GitHub About / description / homepage / topics | Not yet aligned to final landing story | **rewrite** | Synchronize with README and Pages once landing copy stabilizes |

## Closeout Definition of Done

The repository is considered closeout-ready only when all of the following are true:

1. `openspec/specs/` is the only living requirements surface and only one active closeout change is in
   progress.
2. Historical specs, docs, and superseded changes are clearly marked or archived so they cannot be
   mistaken for active guidance.
3. README, Pages, and GitHub About tell the same project story without duplicating the same long-form
   explanation.
4. Historical bug reports have been revalidated and any confirmed closeout-critical defects are either
   fixed or explicitly deferred with rationale.
5. `./scripts/lint.sh format-check` and `./scripts/test.sh clang-debug` are green after the closeout
   diff set.
6. CI/CD, release, Pages, AI guidance, and local tooling all point back to the same minimal workflow
   instead of introducing competing primary entrypoints.
7. The final internal checklist and handoff material are sufficient for a later low-context maintainer
   or GLM pass to continue safely.

## Open Questions

- Should the final closeout checklist include one lightweight round-trip end-to-end command in
  addition to the default format-check and `clang-debug` gate?
