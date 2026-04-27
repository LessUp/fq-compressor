## Context

The repository is in closeout mode. Capability specs do not answer what to strengthen now, what to
stabilize rather than expand, and what order leads to release-ready closeout.

## Goals / Non-Goals

**Goals:**
- Keep `openspec/specs/*` as living capability requirements.
- Define a capability-state model (invest, strengthen, stabilize, closeout).
- Define phase-based exit criteria rather than calendar promises.
- Keep the near-term change queue intentionally small.

**Non-Goals:**
- No broad algorithm expansion.
- No multiple parallel roadmap changes.
- No monolithic roadmap spec replacing capability specs.

## Decisions

### Add roadmap layer above capability specs

Roadmap defines which capabilities to strengthen/stabilize and in what order, while capability
specs define the requirements themselves.

**Alternatives considered:**
- Embed roadmap state directly in each capability spec: rejected because it mixes temporal planning
  with timeless requirements.
- Use a project management tool outside the repository: rejected because it fragments the planning
  surface.

### Add new capabilities: docs-site and release-engineering

These capabilities govern ownership surfaces currently missing from `openspec/specs/`.

**Alternatives considered:**
- Treat documentation and releases as build-system concerns: rejected because they have distinct
  workflows and stakeholders.
- Define these capabilities later: rejected because roadmap planning requires complete capability
  coverage.

### Use four capability states: invest, strengthen, stabilize, closeout

- **invest**: active development, scope expansion allowed
- **strengthen**: improve quality and coverage, no scope expansion
- **stabilize**: bug fixes and polish only
- **closeout**: production-ready, maintenance mode

**Alternatives considered:**
- Use maturity labels (alpha, beta, RC): rejected because they imply versioning rather than work
  focus.
- Use three states (active, stable, done): rejected because it does not distinguish strengthening
  from stabilization.

### Use four closeout phases

1. **Core path closeout**: compress, decompress, random access, quality modes, CLI
2. **Benchmark hardening**: reproducible performance validation and CI integration
3. **Release readiness**: release artifacts, compatibility guarantees, migration guides
4. **Closeout readiness**: documentation completeness, archive old material, handoff preparation

**Alternatives considered:**
- Single-phase roadmap: rejected because it hides dependencies and makes parallel work unclear.
- Six or more phases: rejected because it over-specifies sequencing for a small team.

## Risks / Trade-offs

- **[Risk]** Roadmap could become a second source of truth separate from capability specs.  
  **Mitigation:** Keep capability requirements in spec deltas; roadmap only defines state and
  sequencing.
- **[Risk]** New capabilities (docs-site, release-engineering) could widen scope.  
  **Mitigation:** Narrow focus to ownership and release credibility, not marketing or
  infrastructure expansion.

## Migration Plan

1. Create scaffold: proposal.md, design.md, tasks.md.
2. Add roadmap doc: `openspec/roadmap/closeout.md`.
3. Add new capability specs: `docs-site.yaml`, `release-engineering.yaml`.
4. Add deltas to existing capabilities: `random-access`, `performance`, `compatibility`,
   `build-system`.
5. Validate with `./scripts/dev/preflight.sh`, `openspec list --json`,
   `./scripts/lint.sh format-check`, and `./scripts/test.sh clang-debug`.

Rollback is straightforward: delete the change folder and revert spec additions.

## Open Questions

- Should roadmap define explicit exit criteria for each phase or defer to capability acceptance
  criteria?
