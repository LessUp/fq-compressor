# fq-compressor benchmark-hardening design

## Purpose

Define the next closeout sub-project for `fq-compressor`: make benchmark claims reproducible,
tool coverage explicit, and public performance positioning evidence-based.

This design intentionally treats "are we first-tier?" as a measurement question, not a marketing
assumption.

## Why this sub-project comes next

The current repository can no longer safely rely on benchmark copy that mixes:

- historical numbers without visible result artifacts
- competitor tables that are broader than the locally reproducible tool set
- dataset references (`fq-data/...`) that are not present in the repository
- public docs that imply stronger evidence than the current benchmark harness provides

The closeout roadmap already marks `performance` as `strengthen`. This sub-project is the most
direct way to answer whether `fq-compressor` is currently top-tier on compression ratio,
compression speed, or decompression speed under reproducible conditions.

## Problem statement

Today the benchmark surface has four linked weaknesses:

1. **Dataset drift** - benchmark docs point at local files that are not in the repository.
2. **Tool drift** - the benchmark config names tools that are often unavailable in the actual
   environment.
3. **Report drift** - public benchmark pages contain static tables and at least one inconsistent
   ratio claim.
4. **Positioning drift** - the project wants a "first-tier" answer, but the current evidence is not
   strong enough to justify one.

## Goals

- Create one benchmark-hardening execution surface under OpenSpec.
- Define a reproducible benchmark input model with dataset provenance and tool-availability rules.
- Refresh the benchmark report so published claims are backed by repository-visible evidence.
- Produce a current conclusion about whether `fq-compressor` is first-tier on the tested workload
  dimensions.

## Non-goals

- Do not broaden this work into a new algorithm-improvement project.
- Do not promise comparison against every FASTQ compressor in one pass.
- Do not claim first-tier status on workloads that were not actually benchmarked.
- Do not redesign the whole docs site or release flow in this sub-project.

## Scope

This sub-project covers four repository surfaces:

- `openspec/changes/benchmark-hardening/`
- `benchmark/` scripts and tool configuration
- benchmark-facing docs in `docs/benchmark/`, `docs/en/...`, `docs/website/...`
- benchmark result artifacts that are generated from fresh runs and summarized in tracked docs

## Recommended approach

Use a **three-layer evidence model**:

### Layer 1: Benchmark contract

Add a new OpenSpec change named `benchmark-hardening` that states:

- what counts as a benchmark-ready dataset
- how tool availability is handled
- which metrics are required for public claims
- how benchmark conclusions are allowed to be phrased

This prevents future benchmark copy from outrunning the harness.

### Layer 2: Reproducible harness

Harden the benchmark scripts around explicit inputs and explicit absences:

- add a benchmark dataset manifest rather than hard-coding missing `fq-data/...` paths
- record dataset provenance, compression context, and environment metadata in the result JSON
- distinguish:
  - **verified compared** tools
  - **configured but unavailable** tools
  - **deferred competitors** that require later integration

The harness must tell the truth even when a competitor cannot be run locally.

### Layer 3: Evidence-backed reporting

Update tracked benchmark docs to:

- remove unsupported static comparison tables
- point readers to the exact result set and workload scope
- state the conclusion conservatively:
  - where `fq-compressor` is competitive
  - where it clearly leads
  - where it is not currently first-tier

## Sub-project decomposition

This work is still small enough for one implementation plan if it is decomposed into four bounded
tasks:

1. **OpenSpec change + benchmark policy**
2. **Dataset/tool manifest and harness cleanup**
3. **Fresh benchmark run and result capture**
4. **Report refresh and positioning update**

That keeps "measure honestly" separate from "improve the algorithm later."

## Architecture

```text
Dataset manifest + tool config
            |
            v
    benchmark runner
            |
            v
   structured results JSON
            |
            v
 report generator / docs refresh
            |
            v
 public conclusion:
 "top-tier on X / not top-tier on Y / unproven on Z"
```

## Design decisions

### Prefer manifest-driven datasets over repo-local hard-coded paths

The current `fq-data/...` references are not present in the repository. The benchmark system should
use a tracked manifest that can describe:

- dataset name
- source/provenance
- expected local path
- read type / workload type
- whether the dataset is required or optional

This keeps the docs honest and the runner deterministic.

### Treat competitor availability as data, not a silent skip

A missing tool is itself important benchmark context. The runner should surface availability in the
result summary instead of quietly shrinking the comparison set.

### Publish conclusions by dimension, not one global rank

The right output is not "we are first-tier" or "we are not." The right output is a matrix:

| Dimension | Expected outcome type |
| --- | --- |
| Compression ratio | compare against FASTQ-specific tools where available |
| Compression speed | compare against both FASTQ-specific and general tools |
| Decompression speed | compare against both FASTQ-specific and general tools |
| Random access / engineering usability | document separately from raw throughput |

This avoids a false single-number ranking.

### Keep algorithm-improvement ideas out of this change

If the benchmarks show `fq-compressor` is not first-tier on a key dimension, that result should feed
the next change. This change should stop at honest measurement and reporting.

## Risks and mitigations

- **Risk:** competitor tools are unavailable or too expensive to integrate immediately.  
  **Mitigation:** classify them explicitly as deferred instead of faking comparison coverage.

- **Risk:** no suitable public dataset is locally available.  
  **Mitigation:** make dataset acquisition/provenance part of the benchmark contract and run the
  first refreshed report on at least one repository-supported dataset profile.

- **Risk:** docs still drift from generated results.  
  **Mitigation:** require tracked benchmark pages to summarize generated artifacts instead of
  duplicating hand-maintained numbers.

- **Risk:** the answer is "not first-tier yet."  
  **Mitigation:** treat that as a useful closeout outcome and feed it into the next optimization
  change instead of softening the report language.

## Exit criteria

This sub-project is complete when:

- a `benchmark-hardening` change exists with proposal, design, tasks, and benchmark spec deltas
- the benchmark harness can describe dataset provenance and tool availability explicitly
- a fresh benchmark result set exists for the supported local comparison set
- public benchmark docs summarize that result set without unsupported historical claims
- the repository can answer, with evidence, whether `fq-compressor` is first-tier on the measured
  dimensions and workloads

## Expected deliverable to the user

After implementation, the repository should be able to say one of the following with evidence:

- `fq-compressor` is first-tier on the tested compression ratio workloads
- `fq-compressor` is first-tier on the tested speed workloads
- `fq-compressor` is competitive but not first-tier on one or more tested dimensions
- some dimensions remain unproven because the required peer tools or datasets are still deferred

That is the standard this design is optimizing for.
