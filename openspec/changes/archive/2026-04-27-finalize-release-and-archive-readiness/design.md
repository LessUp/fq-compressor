## Context

The repository now has one active closeout-oriented OpenSpec change:

- `finalize-release-and-archive-readiness`

The five previously parallel closeout changes have already been absorbed and preserved under
`openspec/changes/archive/`:

- `2026-04-26-normalize-openspec-and-repo-governance`
- `2026-04-26-reposition-docs-and-pages`
- `2026-04-26-stabilize-baseline-health`
- `2026-04-26-standardize-ai-collaboration`
- `2026-04-26-streamline-engineering-tooling`

That consolidation fixed the governance drift, but the umbrella artifacts still partially narrate
the consolidation as future work. The remaining closeout effort therefore starts by refreshing this
change so the proposal, design, and tasks match the current one-change reality and can drive the
final repository hardening pass.

The repository is also in a mixed state:

- the default local validation gate is green again, so bug work must be evidence-driven rather than
  speculative
- documentation, Pages, and GitHub About still compete as entry surfaces
- AI entrypoints exist, but their authority boundaries are still easy to misread
- workflow/tooling complexity is higher than a closeout-stage project needs
- the next maintainer may arrive with low repository context, so the remaining closeout work needs a
  single execution document rather than another narrative summary

## Goals / Non-Goals

**Goals:**
- Use one umbrella change to drive the remaining closeout work and keep it as the only active
  change.
- Define what "archive ready" means internally without public deprecation messaging.
- Re-establish clear ownership boundaries across OpenSpec, docs, Pages, GitHub metadata, AI guidance,
  and engineering tooling.
- Revalidate historical defect reports before spending effort on code fixes.
- Keep the repository understandable and operational for sporadic future maintenance, review, or GLM
  handoff.

**Non-Goals:**
- Announcing public end-of-life or maintenance freeze messaging.
- Rewriting every historical artifact in one pass.
- Expanding CI, docs, or AI integration surface area beyond what closeout needs.
- Starting another parallel closeout change or re-splitting the work into multiple active changes.
- Promising that no future defect can ever exist.

## Operating Facts

The refreshed umbrella change must treat the following as current facts, not tentative goals:

1. `openspec list --json` reports exactly one active change:
   `finalize-release-and-archive-readiness`.
2. The default local gate is green and remains:
   `./scripts/lint.sh format-check` plus `./scripts/test.sh clang-debug`.
3. Any bug work in the remaining closeout must be triggered by present evidence from the current
   tree, current scripts, and current outputs.
4. Closeout posture is to finish, simplify, normalize, and prepare handoff—not to expand feature
   scope or publicly announce abandonment.
5. The umbrella artifacts themselves must become the execution contract for the remaining work.

## Execution Blueprint

### Phase order

The closeout work is executed in four phases and only one umbrella change remains active through all
four:

1. **Phase 1: aggressive review and normalization refactor (OpenSpec-driven)**  
   Recheck historical bug/diagnosis material, classify candidate defects, tighten the active versus
   archived source boundaries, and only spend implementation effort on confirmed closeout-critical
   issues while preserving the green default gate.
2. **Phase 2: engineering and GitHub integration convergence**  
   Simplify workflows, release-facing automation, Pages/public-entry surfaces, and repository
   metadata so they align with the same lean closeout story and script-based validation model.
3. **Phase 3: AI tooling and Vibe Coding configuration governance**  
   Consolidate AGENTS/CLAUDE/Copilot instructions, decide which AI assets remain authoritative,
   clarify MCP versus CLI skills versus optional helper boundaries, and keep clangd/editor support
   minimal and useful.
4. **Phase 4: final closeout planning, handoff, and archive readiness**  
   Produce the final checklist, record what must still stay green, document the handoff surface for
   low-context maintenance, run the archive-readiness review, and only then archive the umbrella
   change.

### Fact-source boundaries

The blueprint relies on explicit authority boundaries:

- `openspec/specs/` is the only living requirements source.
- `openspec/changes/finalize-release-and-archive-readiness/` is the only active closeout execution
  surface.
- `openspec/changes/archive/` and `specs/` are historical reference inputs only.
- Repository scripts define the default validation loop; ad hoc historical notes do not.
- Project-level AI instruction files may guide execution, but they must not conflict with OpenSpec,
  repository scripts, or the one-change closeout model.

### Evidence-driven bug handling

Historical bug reports are treated as leads, not truth. Every candidate issue must be rechecked and
classified before any fix work lands:

- **fixed:** current tree already resolves it
- **still present:** reproduced or otherwise evidenced in the current tree
- **stale:** based on removed code, superseded docs, or old workflow assumptions
- **unproven:** plausible, but lacking current evidence

Only the **still present** bucket becomes actionable closeout work. Fixes stay narrow, preserve the
default gate, and add focused coverage only when needed to keep regressions from silently returning.

## Decisions

### Use one umbrella change instead of parallel cleanup changes

`finalize-release-and-archive-readiness` is the single active closeout change. The remaining five
closeout-oriented changes are treated as superseded inputs: their scope is absorbed here and their
directories remain preserved under `openspec/changes/archive/`.

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

### Keep GitHub, release, and tooling governance under the same closeout contract

Workflow, release, Pages, metadata, AI guidance, and local tooling are separate surfaces but one
governance problem. The closeout design treats them as one convergence program so the repository
does not end with conflicting public storylines or multiple "primary" developer entrypoints.

**Alternatives considered**
- Let each surface evolve independently as long as it still works: rejected because it recreates
  drift and raises future maintenance cost.
- Freeze all tooling/docs surfaces exactly as-is: rejected because closeout still needs explicit
  simplification and handoff readiness.

## Risks / Trade-offs

- **[Risk]** The umbrella change becomes too broad.  
  **Mitigation:** organize it into four sequential phases with explicit decision gates and
  keep/delete classifications before broad cleanup.
- **[Risk]** Archiving superseded changes hides useful design context.  
  **Mitigation:** absorb their requirements and decisions into this design before moving them.
- **[Risk]** Historical bug review expands indefinitely.  
  **Mitigation:** require each candidate bug to be classified as fixed, still present, stale, or
  unproven before it can consume implementation effort.
- **[Risk]** CI and metadata cleanup can regress public trust surfaces.  
  **Mitigation:** keep GitHub About, Pages, README, and validation scripts aligned as one workstream.
- **[Risk]** AI/tooling guidance grows into another governance layer.  
  **Mitigation:** keep only the minimum project-level instructions and prefer boundary clarification
  over adding new assets.
- **[Risk]** A later maintainer cannot tell what "done" means.  
  **Mitigation:** Phase 4 must produce a concrete checklist, handoff surface, and archive-readiness
  review rather than a generic status summary.

## Migration Plan

1. Refresh the umbrella proposal/design/tasks so they record the already-completed consolidation and
   expose the remaining work as a four-phase execution plan.
2. Keep the superseded change directories under `openspec/changes/archive/YYYY-MM-DD-<name>/` as
   reference-only inputs for audit and execution.
3. Execute Phase 1 first so historical bug work and source-of-truth boundaries are normalized before
   deeper workflow/tooling cleanup.
4. Execute Phase 2 to converge GitHub-facing automation, release surfaces, Pages, and metadata onto
   the same closeout story and validation flow.
5. Execute Phase 3 to unify AI instructions, tooling boundaries, and optional helper surfaces without
   creating a new governance layer.
6. Execute Phase 4 to publish the final checklist, handoff, and archive-readiness review, then
   archive the umbrella change.

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
| release/version metadata surfaces | Useful but not yet fully converged | **tighten** | Keep release-facing anchors explicit, minimal, and consistent |
| `scripts/dev/preflight.sh` | Correct intent, previously wrong JSON counting | **keep + refine** | Preserve as the required repo/OpenSpec sanity check |
| `AGENTS.md`, `CLAUDE.md`, `.github/copilot-instructions.md` | Useful but partially overlapping | **merge + rewrite** | Keep distinct entry files with one shared workflow |
| `.claude/`, `.github/prompts/`, `.github/skills/` | Tool-specific assets with overlap risk | **prune** | Keep only assets that reinforce the shared workflow |
| MCP / CLI-skill / helper integration surfaces | Potentially useful but easy to over-govern | **clarify + prune** | Keep optional helpers subordinate to the core CLI/OpenSpec workflow |
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
8. No new parallel closeout change is opened and no public maintenance-freeze messaging is introduced
   as part of this work.

## Open Questions

- Should the final closeout checklist include one lightweight round-trip end-to-end command in
  addition to the default format-check and `clang-debug` gate?
