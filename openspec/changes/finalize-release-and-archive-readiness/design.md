## Context

The project is not being publicly sunset, but it is being prepared for a phase where maintenance is
much lighter. That means the repository must stay understandable and operational even if development
cadence drops significantly.

## Goals / Non-Goals

**Goals:**
- Define what "archive ready" means internally.
- Ensure GitHub metadata and documentation still explain the project clearly.
- Leave the repository without stale operational debris such as unmanaged active changes or lingering
  temporary cleanup artifacts.

**Non-Goals:**
- Publishing a public deprecation announcement.
- Creating long-term maintenance automation beyond what the repository already needs.
- Guaranteeing zero future bugs.

## Decisions

### Use an internal readiness checklist instead of public messaging

The closeout intent should shape contributor workflow and final validation without being announced in
the user-facing docs.

**Alternatives considered**
- Add explicit public archive messaging: rejected because the owner does not want to signal that now.
- Skip an explicit checklist: rejected because low-maintenance projects drift fastest without one.

### Closeout includes repository hygiene, not just code quality

An archive-ready state requires green validation, clear GitHub metadata, and intentionally managed
active changes.

**Alternatives considered**
- Define readiness only in terms of tests passing: rejected because the repository also needs clean
  operational state for later sporadic maintenance.

## Risks / Trade-offs

- **[Risk]** Internal readiness criteria can become too broad.  
  **Mitigation:** keep the checklist short and focused on repeatable closeout signals.
- **[Risk]** Metadata updates can lag behind documentation cleanup.  
  **Mitigation:** make metadata alignment an explicit closeout item.

## Migration Plan

1. Define the final closeout checklist.
2. Run the checklist after the earlier cleanup changes land.
3. Archive completed OpenSpec changes and prune stale operational state.

Rollback is simple: keep the repository in active-maintenance mode if the checklist reveals more work.

## Open Questions

- Which final release artifact checks belong in the last closeout pass versus normal release flow?
