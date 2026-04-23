## Context

Several AI tool folders already exist in the repository, but they do not share a strongly enforced
workflow. That makes it easy for cheaper models to skip preflight checks, drift away from OpenSpec,
or skip local validation before direct push.

## Goals / Non-Goals

**Goals:**
- Give every AI tool the same operational loop.
- Keep tool-specific instructions short and project-specific.
- Document how review models and subagents fit into the workflow.

**Non-Goals:**
- Supporting every possible AI client with bespoke documentation.
- Embedding large prompt libraries in the repository.
- Making optional AI review a hard gate for every direct push.

## Decisions

### Make OpenSpec and preflight non-optional

The shared workflow starts with preflight checks and OpenSpec state inspection. Tool-specific docs
then specialize around that common loop.

**Alternatives considered**
- Allow each tool family to define its own loop: rejected because it preserves drift.
- Put all guidance in one giant document only: rejected because Copilot and Claude benefit from a
  small project-local instruction file each.

### Keep review optional but explicit

`/review` remains a recommended checkpoint for risky or wide diffs, but it is not a hard requirement
before every direct push in this single-maintainer repository.

**Alternatives considered**
- Remove review guidance entirely: rejected because optional AI-assisted review is still useful for
  risky diffs.

## Risks / Trade-offs

- **[Risk]** Too much tool-specific guidance recreates the sprawl being removed.  
  **Mitigation:** keep one shared workflow plus a minimal per-tool entry file.
- **[Risk]** Strict workflow language can be ignored by low-cost models.  
  **Mitigation:** support the workflow with scripts and hooks, not just prose.

## Migration Plan

1. Publish the shared workflow in `AGENTS.md`.
2. Add focused Claude and Copilot instruction files.
3. Keep other tool-specific assets only if they reuse the same OpenSpec-first loop.

Rollback is low risk: the previous repo state can be restored by removing the new guidance files.

## Open Questions

- Which tool-specific folders should be tracked long term versus regenerated on demand?
