from __future__ import annotations

import json
import subprocess
from dataclasses import asdict
from pathlib import Path
from time import perf_counter
from typing import TYPE_CHECKING

from benchmark_v2.models import BenchmarkResult, BenchmarkSuite, Layout, ToolSpec, WorkloadSpec
from benchmark_v2.prepare_inputs import prepare_workload
from benchmark_v2.tool_adapters import ToolAdapter, create_adapter

if TYPE_CHECKING:
    from typing import Literal


def run_matrix(
    workload: WorkloadSpec,
    data_root: Path,
    work_dir: Path,
    tools: list[ToolSpec],
    threads: list[int],
    runs: int,
) -> BenchmarkSuite:
    if not threads:
        raise ValueError("threads must not be empty")
    if any(thread_count <= 0 for thread_count in threads):
        raise ValueError("threads must be positive")

    workload_dir = work_dir / workload.workload_id
    prepared_inputs = prepare_workload(workload, data_root, workload_dir)
    results: list[BenchmarkResult] = []
    for tool in tools:
        adapter = create_adapter(tool)
        if workload.layout == "paired" and not adapter.supports_paired:
            continue
        for thread_count in _effective_thread_counts(adapter, threads):
            results.extend(
                run_best_of_n(adapter, prepared_inputs, workload.layout, thread_count, runs, workload_dir)
            )
    if not results:
        raise ValueError(f"no benchmark results produced for workload '{workload.workload_id}'")
    return BenchmarkSuite(
        workload_id=workload.workload_id,
        layout=workload.layout,
        results=tuple(results),
    )


def run_best_of_n(
    adapter: ToolAdapter,
    prepared_inputs: list[Path],
    layout: Layout,
    threads: int,
    runs: int,
    workload_dir: Path,
) -> list[BenchmarkResult]:
    if runs <= 0:
        raise ValueError(f"runs must be positive; got {runs}")

    best_compress: BenchmarkResult | None = None
    best_decompress: BenchmarkResult | None = None
    first_failure: list[BenchmarkResult] | None = None

    for run_index in range(1, runs + 1):
        run_dir = workload_dir / "runs" / adapter.tool_id / f"threads-{threads}" / f"run-{run_index}"
        run_dir.mkdir(parents=True, exist_ok=True)
        run_results = _run_once(adapter, prepared_inputs, layout, threads, run_dir)
        if len(run_results) == 2 and all(result.success for result in run_results):
            compress_result, decompress_result = run_results
            if best_compress is None or compress_result.elapsed_seconds < best_compress.elapsed_seconds:
                best_compress = compress_result
            if best_decompress is None or decompress_result.elapsed_seconds < best_decompress.elapsed_seconds:
                best_decompress = decompress_result
        elif first_failure is None:
            first_failure = run_results

    if best_compress is not None and best_decompress is not None:
        return [best_compress, best_decompress]
    if first_failure is not None:
        return first_failure
    raise AssertionError("unreachable")


def _effective_thread_counts(adapter: ToolAdapter, requested_threads: list[int]) -> list[int]:
    if adapter.supports_threads:
        return requested_threads
    return [1]


def export_suite_json(suite: BenchmarkSuite, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(asdict(suite), indent=2), encoding="utf-8")


def _run_once(
    adapter: ToolAdapter,
    prepared_inputs: list[Path],
    layout: Layout,
    threads: int,
    run_dir: Path,
) -> list[BenchmarkResult]:
    archive = run_dir / f"archive{adapter.archive_suffix}"
    restored_base = run_dir / "restored.fastq"
    paired = layout == "paired"
    input_bytes = sum(path.stat().st_size for path in prepared_inputs)

    compress_result = _run_command(
        adapter.compress_command(prepared_inputs, archive, threads),
        tool_id=adapter.tool_id,
        layout=layout,
        operation="compress",
        threads=threads,
        input_bytes=input_bytes,
        output_targets=(archive,),
        cwd=run_dir,
    )
    if not compress_result.success:
        return [compress_result]

    restored_outputs = adapter.restored_outputs(restored_base, paired)
    decompress_result = _run_command(
        adapter.decompress_command(archive, restored_base, threads, paired),
        tool_id=adapter.tool_id,
        layout=layout,
        operation="decompress",
        threads=threads,
        input_bytes=archive.stat().st_size,
        output_targets=restored_outputs,
        cwd=run_dir,
    )
    if not decompress_result.success:
        return [compress_result, decompress_result]

    if not _files_match(prepared_inputs, restored_outputs):
        return [
            compress_result,
            BenchmarkResult(
                tool_id=adapter.tool_id,
                layout=layout,
                operation="decompress",
                threads=threads,
                input_bytes=archive.stat().st_size,
                output_bytes=sum(path.stat().st_size for path in restored_outputs if path.exists()),
                elapsed_seconds=decompress_result.elapsed_seconds,
                success=False,
            ),
        ]
    return [compress_result, decompress_result]


def _run_command(
    command: list[str],
    *,
    tool_id: str,
    layout: Layout,
    operation: "Literal['compress', 'decompress']",
    threads: int,
    input_bytes: int,
    output_targets: tuple[Path, ...],
    cwd: Path,
) -> BenchmarkResult:
    for output_target in output_targets:
        output_target.unlink(missing_ok=True)
    started = perf_counter()
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    elapsed_seconds = perf_counter() - started
    success = completed.returncode == 0 and all(path.exists() for path in output_targets)
    output_bytes = sum(path.stat().st_size for path in output_targets if path.exists())
    return BenchmarkResult(
        tool_id=tool_id,
        layout=layout,
        operation=operation,
        threads=threads,
        input_bytes=input_bytes,
        output_bytes=output_bytes,
        elapsed_seconds=elapsed_seconds if success else max(elapsed_seconds, 0.0),
        success=success,
    )


def _files_match(expected: list[Path], actual: tuple[Path, ...]) -> bool:
    if len(expected) != len(actual):
        return False
    for expected_path, actual_path in zip(expected, actual):
        if expected_path.read_bytes() != actual_path.read_bytes():
            return False
    return True
