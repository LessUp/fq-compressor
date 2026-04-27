# fq-compressor OpenSpec closeout roadmap design

## Purpose

Define a project-level OpenSpec structure for `fq-compressor` that turns recent FASTQ compressor ecosystem research into a closeout-oriented requirements model and long-term roadmap.

This design is intentionally biased toward finishing, stabilizing, and proving the current product story rather than expanding product surface area.

## Context

The current repository already has useful living specs in `openspec/specs/*`, but it lacks a program-level layer that answers four higher-level questions:

1. Which capabilities still need active strengthening?
2. Which capabilities should be stabilized rather than expanded?
3. What sequence of work leads to a credible release-ready closeout?
4. Which OpenSpec changes should be allowed into the near-term queue?

The external research suggests the project has a strong architecture story, but it does not yet clearly justify a broad “state of the art” claim. The most credible differentiation path is:

- strong block-oriented archive design
- practical random access
- usable multithreaded engineering
- reproducible public benchmarking

## Goals

- Preserve `openspec/specs/*` as the living capability requirements source.
- Add a project-level roadmap layer above the capability specs.
- Make closeout status explicit so the project stops growing by default.
- Prioritize high-signal work that improves product coherence, benchmark credibility, and release readiness.

## Non-goals

- Do not create a broad new feature expansion roadmap.
- Do not treat speculative algorithm experiments as default roadmap commitments.
- Do not replace existing capability specs with a single monolithic roadmap spec.

## Scope

This roadmap covers the full public delivery surface:

- main C++ repository
- CLI behavior and file format commitments
- documentation site
- benchmark evidence chain
- release engineering and release presentation

## OpenSpec structure

The repository should keep the current capability-oriented structure in `openspec/specs/*`.

On top of that structure, the project should maintain a program-level roadmap document that defines:

- capability map
- capability status per domain
- phased closeout roadmap
- exit criteria for each phase
- candidate change queue

Two additional capabilities should be treated as first-class living specs:

### `docs-site`

This capability owns requirements for:

- project positioning
- install/onboarding flow
- benchmark explanation
- support boundaries and limitations
- avoiding README mirror duplication

### `release-engineering`

This capability owns requirements for:

- release artifacts
- support matrix
- version and compatibility statements
- release verification expectations
- closeout-oriented release simplification

## Capability state model

Each capability should be assigned one of four states:

| State | Meaning |
| --- | --- |
| `invest` | open research remains before making a durable commitment |
| `strengthen` | functionality exists, but product credibility depends on closing known gaps |
| `stabilize` | functionality is mostly present; focus on consistency, documentation, and evidence |
| `closeout` | capability is good enough for release posture; only minimal follow-up work should remain |

Recommended initial positioning:

| Capability | State | Rationale |
| --- | --- | --- |
| `random-access` | `strengthen` | core differentiator; must match real fast-path behavior and user story |
| `compression` | `stabilize` | architecture is strong, but broad feature expansion is not the priority |
| `decompression` | `stabilize` | must remain coherent with extraction and order restoration semantics |
| `file-format` | `stabilize` | important for durability and compatibility, but should not drift |
| `performance` | `strengthen` | current claims need stronger third-party-style evidence |
| `integrity` | `stabilize` | keep verification and reliability clear, not sprawling |
| `cli` | `stabilize` | user-facing promises should be simplified and made consistent |
| `build-system` | `stabilize` | keep minimal and reliable |
| `compatibility` | `stabilize` | clarify boundaries rather than expanding matrix aggressively |
| `atomic-write` | `closeout` | finish documentation and polish, avoid overbuilding |
| `quality-handling` | `invest` | bounded evaluation is still justified, but not open-ended expansion |
| `docs-site` | `strengthen` | key public surface still needs consolidation and credibility focus |
| `release-engineering` | `strengthen` | required to turn technical progress into a trustworthy release |

## Long-term roadmap

The roadmap should be organized by phase exit criteria, not by calendar duration.

### Phase 1: core path closeout

Primary capabilities:

- `random-access`
- `compression`
- `decompression`
- `file-format`

Objectives:

- close the gap between documented random-access positioning and actual main-path behavior
- align parallel compression behavior with reorder, reorder-map, and original-order restoration semantics
- stabilize index, footer, and version expectations in the FQC format

Exit criteria:

- compression, random extraction, original-order restoration, and verification form a consistent supported workflow
- specs, implementation, CLI help, and docs agree on the core workflow
- no core product claim depends on a non-primary or partially wired path

### Phase 2: benchmark hardening

Primary capabilities:

- `performance`
- `integrity`
- `docs-site`

Objectives:

- expand benchmarking beyond generic compressors into real FASTQ peers
- adopt public benchmark datasets with better short-read and long-read coverage
- publish a reproducible evidence chain for public claims

Exit criteria:

- benchmark runs are reproducible from scripts, datasets, and environment descriptions
- public performance claims trace back to repository-visible sources
- the project can clearly state where it wins, where it is competitive, and where it is not

### Phase 3: release readiness

Primary capabilities:

- `release-engineering`
- `build-system`
- `compatibility`
- `cli`

Objectives:

- make release artifacts, support boundaries, and verification steps explicit
- simplify release presentation and installation experience
- align README, docs site, CLI help, and release notes around one product story

Exit criteria:

- release requirements are explicit rather than tribal knowledge
- the public documentation surface communicates supported scenarios and known limits clearly
- a new user can install, validate, and compare the tool without reading source code

### Phase 4: closeout readiness

Primary capabilities:

- `docs-site`
- `compatibility`
- `atomic-write`
- `integrity`

Objectives:

- remove low-value expansion pressure
- reduce maintenance burden
- convert future work into a narrow, high-signal follow-up queue

Exit criteria:

- most capabilities sit in `stabilize` or `closeout`
- remaining follow-up items are limited, explicit, and non-expansive
- the public posture is “finished and maintainable,” not “still broadly expanding”

## Change queue policy

The near-term OpenSpec queue should stay intentionally small. A closeout project benefits more from finishing a few important changes than from opening many speculative tracks.

Recommended first-wave changes:

### `random-access-closeout`

Scope:

- close the main-path random-access and order-restoration story
- remove ambiguity between documented support and primary-path behavior
- align extraction semantics across docs, CLI, specs, and implementation

Why first:

- this is the clearest differentiator versus many FASTQ compressors
- it directly affects product credibility

### `benchmark-hardening`

Scope:

- add stronger public datasets
- add missing FASTQ-specialized comparison tools where practical
- define benchmark metrics and reproducibility requirements

Why first:

- performance claims are only as strong as the evidence chain behind them

### `release-readiness`

Scope:

- define release requirements, validation expectations, artifact matrix, and support boundaries
- reduce ambiguity in release presentation

Why first:

- this converts engineering progress into a trustworthy release surface

### `docs-site-consolidation`

Scope:

- reduce duplication with README
- make the docs site the primary public explanation layer
- ensure benchmark and support claims point to verifiable sources

Why first:

- documentation is part of product credibility, not just explanation

## Deferred candidates

The following areas may be explored later, but should not compete with the first-wave closeout queue unless the closeout path is already stable:

- alternate quality coders such as bounded rANS-based evaluation
- additional minimizer/hash experiments
- wider codec experimentation beyond the current justified stack
- broader platform or language-surface expansion

## Requirements writing guidance

When this design is translated into OpenSpec requirements:

- prefer explicit support boundaries over aspirational language
- require evidence for public benchmark claims
- require consistency across spec, implementation, CLI, docs, and release surfaces
- treat “closeout” as a real product state with stopping rules

## Acceptance condition for this design

This design is successful if it gives the project a stable answer to:

- what to strengthen now
- what to stabilize rather than expand
- how to decide when the project is release-ready
- which changes belong in the next execution queue
