## Context

The repository has accumulated multiple generations of specs, docs, and AI-facing files. Contributors
can currently find relevant guidance in `openspec/`, `specs/`, `docs/`, root markdown files, and
tool-specific folders, which increases drift and makes low-cost model execution less reliable.

## Goals / Non-Goals

**Goals:**
- Make it obvious where active requirements live.
- Label archived material clearly enough that models do not edit the wrong source.
- Define a repeatable governance policy for future closeout edits.

**Non-Goals:**
- Rewriting every archived document immediately.
- Removing all historical artifacts in one sweep.
- Introducing a new planning system beyond OpenSpec.

## Decisions

### OpenSpec remains the active contract

`openspec/specs/` is the canonical requirements surface, and change work should always flow through
`openspec/changes/<name>/`.

**Alternatives considered**
- Keep `specs/` and `openspec/` in parallel: rejected because it preserves duplicate truth.
- Move everything into a custom governance directory: rejected because it dilutes OpenSpec.

### Archived material stays visible but explicitly non-authoritative

Historical specs and docs may remain for reference, but they must be labeled and linked as archive
material so both humans and models avoid editing them as if they were active.

**Alternatives considered**
- Delete all historical material immediately: rejected because some reference value remains.
- Preserve archive material without labeling: rejected because that is the current failure mode.

## Risks / Trade-offs

- **[Risk]** Aggressive cleanup can remove context still useful for release or migration work.  
  **Mitigation:** classify material as keep, archive, merge, or remove before deleting it.
- **[Risk]** New governance rules can become another layer of documentation.  
  **Mitigation:** keep governance rules short and colocated with the tools they affect.

## Migration Plan

1. Inventory current repository surfaces.
2. Label active versus archived sources explicitly.
3. Update contributor-facing instructions to point to the living surfaces only.
4. Remove or merge redundant assets once replacement guidance exists.

Rollback is low risk: restore removed references if a downstream workflow depends on them.

## Open Questions

- Which tool-specific folders should remain tracked versus being documented as optional local assets?
