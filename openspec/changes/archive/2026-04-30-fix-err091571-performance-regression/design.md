## Context

`benchmark-hardening` produced the first repository-tracked public benchmark artifact for `ERR091571`, and it exposed a clear regression in the current `fq-compressor` path on short-read Illumina data. On the pinned 2,000-record subset, `fq-compressor` is still far slower than the local-supported tool set. On the exploratory 20,000-record subset, runtime grows sharply enough that the workload no longer fits the closeout validation loop.

This change needs a focused performance-debugging design rather than another benchmark-surface change. The repository already has the evidence chain, local quick-check inputs, and larger local validation inputs. What is missing is a disciplined way to locate the hot path, constrain fixes to the smallest correct surface, and prove the fix on both fast and realistic local data.

Relevant constraints:

- The project is in closeout mode, so this should prefer a narrow regression fix over broad algorithm redesign.
- The root cause may span parser, pipeline, reorder, or ABC hot paths, so the change is cross-cutting enough to justify a design doc.
- Public benchmark claims must remain tied to tracked result artifacts, so the benchmark evidence must be refreshed after the fix.

## Goals / Non-Goals

**Goals:**

- Identify the dominant runtime regression affecting `fq-compressor` on the measured `ERR091571` short-read workload.
- Implement the smallest effective fix that materially improves compression and decompression runtime on the tracked workload.
- Add targeted regression coverage so future changes can detect the same slowdown before it reaches benchmark docs again.
- Use two validation tiers:
  - small local data for rapid functional verification
  - large local data for performance, stability, memory, and I/O checks
- Refresh the tracked benchmark artifact so the improvement is visible in repository-controlled evidence.

**Non-Goals:**

- Do not redesign the full ABC/SCM architecture.
- Do not broaden this change into universal benchmark leadership claims.
- Do not add new benchmark competitors or new public datasets in this change.
- Do not weaken correctness, format guarantees, or random-access behavior in exchange for speed.

## Decisions

### Diagnose with repository-visible profiling checkpoints before optimizing

The first implementation step should isolate where time is going on the tracked workload: input expansion, parsing, global analysis, reordering, block compression, or writing. This can be done with lightweight instrumentation, existing logging, targeted timers, and benchmark harness output before any deeper refactor is attempted.

**Alternatives considered:**

- Make speculative algorithm changes immediately -> Too risky in closeout mode and likely to hide the real bottleneck.
- Use only external profiler output without repository-visible checkpoints -> Harder to preserve as regression knowledge in tests and docs.

### Keep fixes localized to the dominant short-read hot path

Once the hotspot is known, the implementation should prefer the smallest localized improvement in the responsible module rather than a broad pipeline rewrite. Likely surfaces are short-read parser behavior, block sizing, reorder costs, or unnecessary work triggered by benchmark-scale inputs.

**Alternatives considered:**

- Rework the full compression pipeline -> Too broad for a regression fix and too risky for closeout mode.
- Add workload-specific hacks only in benchmark scripts -> Would hide, not fix, the core regression.

### Formalize two validation tiers in the change tasks

The implementation should explicitly separate validation tiers:

- small data: `/home/shane/data/test/small/test_1.fq.gz`, `/home/shane/data/test/small/test_2.fq.gz`
- large data: `/home/shane/data/test/big/test_bgi_1.fq.gz`, `/home/shane/data/test/big/test_bgi_2.fq.gz`

Small data keeps the iteration loop fast for correctness checks. Large data is used only when measuring runtime, memory pressure, I/O behavior, and stability.

**Alternatives considered:**

- Use only the tracked public subset for all validation -> Too small to expose some scaling and stability behavior.
- Use only the large local data -> Too slow for rapid development and regression isolation.

### Refresh the tracked `ERR091571` artifact after the fix

The final proof should remain the tracked benchmark artifact because that is what public docs now cite. If the fix changes the measured outcome, the result files and benchmark-facing docs should be refreshed in the same change.

**Alternatives considered:**

- Stop at internal profiling data -> Not enough to update repository-facing conclusions.
- Refresh docs without regenerating artifacts -> Violates the benchmark contract established in the previous change.

## Risks / Trade-offs

- [Root cause is deeper than expected] -> Start with narrow instrumentation and stop broadening once the dominant cost is isolated.
- [Performance fix regresses correctness or archive semantics] -> Keep default test gates and add targeted regression coverage before claiming success.
- [Improvement appears only on tiny samples] -> Require re-checks on the large local data and the tracked `ERR091571` artifact.
- [Local optimizations improve general tools comparison but not specialized-peer standing] -> Report the refreshed evidence honestly and leave specialized-peer claims scoped.
- [Memory or I/O regressions are introduced while chasing throughput] -> Include large-data validation that specifically checks runtime, memory, and I/O behavior.

## Migration Plan

1. Add profiling-oriented checkpoints or targeted timing visibility for the measured path.
2. Reproduce the regression on the tracked `ERR091571` subset and on the larger local validation inputs.
3. Implement the smallest fix in the responsible hot path.
4. Add or update regression tests and benchmark-oriented checks.
5. Re-run quick functional validation on the small local data.
6. Re-run performance, stability, memory, and I/O checks on the large local data.
7. Regenerate `benchmark/results/err091571-local-supported.{json,md}` and update benchmark docs if conclusions change.

## Open Questions

- Is the dominant regression in parsing, reordering, or compression proper?
- Does the slowdown primarily affect compression, decompression, or both when the workload scales beyond the pinned subset?
- Can the fix be expressed as a config/default change for short-read workloads, or does it require code-path changes inside the pipeline?
