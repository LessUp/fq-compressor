## Context

The repository already contains useful benchmark and architecture material, but the entry experience
is fragmented. GitHub Pages should attract and orient users, while the documentation tree should stay
lean enough to maintain during closeout.

## Goals / Non-Goals

**Goals:**
- Give the Pages root a clear landing-page role.
- Keep English and Chinese onboarding paths available.
- Remove install/build workflow churn from the Pages pipeline.

**Non-Goals:**
- Building a large marketing site.
- Rewriting every documentation page in the repository.
- Adding analytics or growth tooling.

## Decisions

### Keep a thin bilingual information architecture

The site should provide a strong entry page plus focused EN/ZH landing pages rather than a sprawling
doc maze.

**Alternatives considered**
- Maintain a large full-fidelity doc tree: rejected because it is expensive to keep current.
- Collapse everything into README only: rejected because Pages should still be useful on its own.

### Make Pages builds deterministic

The workflow should respect the checked-in lockfile and avoid deleting it during CI.

**Alternatives considered**
- Reinstall dependencies from scratch with `npm install`: rejected because it mutates the dependency
  surface and makes Pages less reproducible.

## Risks / Trade-offs

- **[Risk]** A leaner doc set can remove context advanced users still value.  
  **Mitigation:** keep deep-dive pages that explain architecture and benchmark evidence.
- **[Risk]** Pages can drift from README again.  
  **Mitigation:** align the top-level value proposition and quick links across both surfaces.

## Migration Plan

1. Improve the Pages entry surface and build workflow.
2. Align README/docs responsibilities with the landing-page role.
3. Update GitHub About metadata once the landing path is stable.

Rollback is simple: restore the previous entry page and workflow if the new landing surface regresses.

## Open Questions

- Which low-value documentation pages should be archived instead of rewritten?
