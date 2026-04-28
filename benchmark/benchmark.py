#!/usr/bin/env python3
"""
fq-compressor benchmark framework.

This runner records dataset provenance, explicit tool availability, and a tracked
comparison scope so public benchmark claims remain reproducible.
"""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import tempfile
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Optional

import yaml


@dataclass
class BenchmarkResult:
    """Result of a single benchmark run."""

    tool: str
    operation: str
    input_size: int
    output_size: int
    elapsed_seconds: float
    threads: int
    success: bool
    error: Optional[str] = None

    @property
    def ratio(self) -> float:
        if self.input_size == 0:
            return 0.0
        return self.output_size / self.input_size

    @property
    def speed_mb_s(self) -> float:
        if self.elapsed_seconds == 0:
            return 0.0
        return (self.input_size / 1024 / 1024) / self.elapsed_seconds

    @property
    def output_mb_s(self) -> float:
        if self.elapsed_seconds == 0:
            return 0.0
        return (self.output_size / 1024 / 1024) / self.elapsed_seconds

    @property
    def bits_per_base(self) -> float:
        if self.input_size == 0:
            return 0.0
        estimated_bases = self.input_size * 0.5
        return (self.output_size * 8) / estimated_bases


@dataclass
class ToolConfig:
    """Configuration for a compression tool."""

    name: str
    compress: str
    decompress: str
    extension: str
    category: str
    description: str
    version_cmd: str = ""
    version: str = "unknown"
    claim_scope: str = "local-supported"
    scope_note: str = ""


@dataclass
class DatasetConfig:
    """Manifest entry for a benchmark dataset."""

    dataset_id: str
    name: str
    accession: str = ""
    source: str = ""
    source_urls: list[str] = field(default_factory=list)
    benchmark_input: str = ""
    local_paths: list[str] = field(default_factory=list)
    workload: str = ""
    read_layout: str = ""
    benchmark_focus: str = ""
    claim_scope: str = ""
    requested_tools: list[str] = field(default_factory=list)
    preparation: list[str] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)

    def to_export(self) -> dict[str, Any]:
        return asdict(self)


@dataclass
class BenchmarkSuite:
    """Complete benchmark suite results."""

    timestamp: str
    input_file: str
    input_size: int
    requested_tools: list[str]
    tools_tested: list[str]
    unavailable_tools: list[dict[str, Any]] = field(default_factory=list)
    deferred_tools: list[dict[str, Any]] = field(default_factory=list)
    results: list[BenchmarkResult] = field(default_factory=list)
    system_info: dict[str, Any] = field(default_factory=dict)
    dataset: dict[str, Any] = field(default_factory=dict)
    benchmark_command: dict[str, Any] = field(default_factory=dict)


class BenchmarkRunner:
    """Main benchmark runner class."""

    def __init__(self, config_path: str | None = None, dataset_config_path: str | None = None):
        self.project_root = Path(__file__).resolve().parent.parent
        self.config_path = Path(config_path) if config_path else Path(__file__).parent / "tools.yaml"
        self.dataset_config_path = (
            Path(dataset_config_path) if dataset_config_path else Path(__file__).parent / "datasets.yaml"
        )
        self.tools: dict[str, ToolConfig] = {}
        self.datasets: dict[str, DatasetConfig] = {}
        self.settings: dict[str, Any] = {}
        self.fqc_binary = self._find_fqc_binary()
        self._load_config()
        self._load_datasets()

    def _find_fqc_binary(self) -> str:
        candidates = [
            self.project_root / "build/clang-release/src/fqc",
            self.project_root / "build/gcc-release/src/fqc",
            self.project_root / "build/clang-debug/src/fqc",
            self.project_root / "build/build/Release/src/fqc",
            self.project_root / "build/Release/src/fqc",
            self.project_root / "build/src/fqc",
        ]
        for candidate in candidates:
            if candidate.exists():
                return str(candidate)
        return shutil.which("fqc") or "fqc"

    def _load_config(self) -> None:
        with open(self.config_path, "r", encoding="utf-8") as handle:
            config = yaml.safe_load(handle) or {}

        self.settings = config.get("settings", {})

        for tool_id, tool_cfg in config.get("tools", {}).items():
            self.tools[tool_id] = ToolConfig(
                name=tool_cfg.get("name", tool_id),
                compress=tool_cfg.get("compress", ""),
                decompress=tool_cfg.get("decompress", ""),
                extension=tool_cfg.get("extension", ""),
                category=tool_cfg.get("category", "unknown"),
                description=tool_cfg.get("description", ""),
                version_cmd=tool_cfg.get("version_cmd", ""),
                claim_scope=tool_cfg.get("claim_scope", "local-supported"),
                scope_note=tool_cfg.get("scope_note", ""),
            )

    def _load_datasets(self) -> None:
        if not self.dataset_config_path.exists():
            return

        with open(self.dataset_config_path, "r", encoding="utf-8") as handle:
            config = yaml.safe_load(handle) or {}

        for dataset_id, dataset_cfg in config.get("datasets", {}).items():
            self.datasets[dataset_id] = DatasetConfig(
                dataset_id=dataset_id,
                name=dataset_cfg.get("name", dataset_id),
                accession=dataset_cfg.get("accession", ""),
                source=dataset_cfg.get("source", ""),
                source_urls=dataset_cfg.get("source_urls", []),
                benchmark_input=dataset_cfg.get("benchmark_input", ""),
                local_paths=dataset_cfg.get("local_paths", []),
                workload=dataset_cfg.get("workload", ""),
                read_layout=dataset_cfg.get("read_layout", ""),
                benchmark_focus=dataset_cfg.get("benchmark_focus", ""),
                claim_scope=dataset_cfg.get("claim_scope", ""),
                requested_tools=dataset_cfg.get("requested_tools", []),
                preparation=dataset_cfg.get("preparation", []),
                notes=dataset_cfg.get("notes", []),
            )

    def _replace_fqc_token(self, command: str) -> str:
        stripped = command.lstrip()
        if stripped == "fqc" or stripped.startswith("fqc "):
            prefix_len = len(command) - len(stripped)
            replaced = f"{shlex.quote(self.fqc_binary)}{stripped[3:]}"
            return f"{' ' * prefix_len}{replaced}"
        return command

    def _resolve_path(self, raw_path: str) -> Path:
        candidate = Path(raw_path)
        if candidate.is_absolute():
            return candidate
        return self.project_root / candidate

    def _get_tool_version(self, tool: ToolConfig) -> str:
        if not tool.version_cmd:
            return "unknown"

        command = self._replace_fqc_token(tool.version_cmd)
        try:
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                text=True,
                timeout=5,
                check=False,
            )
        except Exception:
            return "unknown"

        version = result.stdout.strip() or result.stderr.strip()
        return version.split("\n")[0][:80] if version else "unknown"

    def _check_tool_available(self, tool_id: str) -> bool:
        tool = self.tools.get(tool_id)
        if not tool:
            return False

        command = self._replace_fqc_token(tool.compress)
        first_token = shlex.split(command)[0]
        if first_token == self.fqc_binary:
            return Path(self.fqc_binary).exists() or shutil.which("fqc") is not None
        return shutil.which(first_token) is not None

    def _run_command(self, command: str, timeout: int = 3600) -> tuple[float, bool, str]:
        start = time.perf_counter()
        try:
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                text=True,
                timeout=timeout,
                check=False,
            )
            elapsed = time.perf_counter() - start
            if result.returncode != 0:
                error = (result.stderr or result.stdout or "command failed").strip()
                return elapsed, False, error[:300]
            return elapsed, True, ""
        except subprocess.TimeoutExpired:
            return float(timeout), False, "Timeout"
        except Exception as exc:
            return 0.0, False, str(exc)

    def _get_file_size(self, path: str) -> int:
        try:
            return os.path.getsize(path)
        except OSError:
            return 0

    def _get_system_info(self) -> dict[str, Any]:
        info: dict[str, Any] = {}

        try:
            result = subprocess.run("nproc", shell=True, capture_output=True, text=True, check=False)
            info["cpu_cores"] = int(result.stdout.strip())
        except Exception:
            info["cpu_cores"] = os.cpu_count() or 1

        try:
            with open("/proc/meminfo", "r", encoding="utf-8") as handle:
                first = next(line for line in handle if line.startswith("MemTotal:"))
            mem_kb = int(first.split()[1])
            info["memory_gb"] = round(mem_kb / 1024 / 1024, 1)
        except Exception:
            info["memory_gb"] = 0

        try:
            result = subprocess.run("uname -r", shell=True, capture_output=True, text=True, check=False)
            info["kernel"] = result.stdout.strip()
        except Exception:
            info["kernel"] = "unknown"

        return info

    def _prepare_input_file(self, input_file: str, work_dir: Path) -> str:
        input_path = Path(input_file)
        if input_path.suffix != ".gz":
            return str(input_path)

        prepared = work_dir / input_path.stem
        if prepared.exists():
            return str(prepared)

        command = f"gzip -dc {shlex.quote(str(input_path))} > {shlex.quote(str(prepared))}"
        _, success, error = self._run_command(command)
        if not success:
            raise RuntimeError(f"failed to expand input {input_file}: {error}")
        return str(prepared)

    def _get_effective_input_size(self, input_file: Path) -> int:
        with tempfile.TemporaryDirectory(prefix="benchmark-input-size-") as temp_dir:
            actual_input = self._prepare_input_file(str(input_file), Path(temp_dir))
            return self._get_file_size(actual_input)

    def get_dataset(self, dataset_id: str) -> DatasetConfig:
        dataset = self.datasets.get(dataset_id)
        if not dataset:
            raise KeyError(f"unknown dataset '{dataset_id}'")
        return dataset

    def list_tools(self) -> list[tuple[str, ToolConfig, bool]]:
        return [(tool_id, tool, self._check_tool_available(tool_id)) for tool_id, tool in self.tools.items()]

    def list_datasets(self) -> list[DatasetConfig]:
        return list(self.datasets.values())

    def benchmark_tool(
        self,
        tool_id: str,
        input_file: str,
        threads: int = 1,
        work_dir: str | None = None,
        verify: bool = True,
    ) -> list[BenchmarkResult]:
        tool = self.tools.get(tool_id)
        if not tool:
            return []

        results: list[BenchmarkResult] = []

        if work_dir:
            temp_dir = Path(work_dir)
            temp_dir.mkdir(parents=True, exist_ok=True)
            cleanup = False
        else:
            temp_dir = Path(tempfile.mkdtemp(prefix="benchmark-"))
            cleanup = True

        try:
            actual_input = self._prepare_input_file(input_file, temp_dir)
            input_size = self._get_file_size(actual_input)

            basename = Path(actual_input).stem
            compressed = temp_dir / f"{basename}{tool.extension}"
            decompressed = temp_dir / f"{basename}_{tool_id}_decompressed.fastq"

            format_args = {
                "input": shlex.quote(actual_input),
                "output": shlex.quote(str(compressed)),
                "decompressed": shlex.quote(str(decompressed)),
                "threads": threads,
            }

            compress_cmd = self._replace_fqc_token(tool.compress.format(**format_args))
            elapsed, success, error = self._run_command(
                compress_cmd, timeout=self.settings.get("timeout_seconds", 3600)
            )
            compressed_size = self._get_file_size(str(compressed)) if success else 0

            results.append(
                BenchmarkResult(
                    tool=tool_id,
                    operation="compress",
                    input_size=input_size,
                    output_size=compressed_size,
                    elapsed_seconds=elapsed,
                    threads=threads,
                    success=success,
                    error=error if not success else None,
                )
            )

            if success and compressed.exists():
                decompress_cmd = self._replace_fqc_token(tool.decompress.format(**format_args))
                elapsed, success, error = self._run_command(
                    decompress_cmd, timeout=self.settings.get("timeout_seconds", 3600)
                )
                decompressed_size = self._get_file_size(str(decompressed)) if success else 0

                result = BenchmarkResult(
                    tool=tool_id,
                    operation="decompress",
                    input_size=compressed_size,
                    output_size=decompressed_size,
                    elapsed_seconds=elapsed,
                    threads=threads,
                    success=success,
                    error=error if not success else None,
                )

                if verify and success and decompressed.exists() and decompressed_size != input_size:
                    result.success = False
                    result.error = f"Size mismatch: {decompressed_size} vs {input_size}"

                results.append(result)

        finally:
            if cleanup:
                shutil.rmtree(temp_dir, ignore_errors=True)

        return results

    def _classify_tools(self, tool_ids: list[str]) -> tuple[list[str], list[dict[str, Any]], list[dict[str, Any]]]:
        measured: list[str] = []
        unavailable: list[dict[str, Any]] = []
        deferred: list[dict[str, Any]] = []

        for tool_id in tool_ids:
            tool = self.tools.get(tool_id)
            if not tool:
                unavailable.append(
                    {
                        "tool": tool_id,
                        "name": tool_id,
                        "claim_scope": "unknown",
                        "reason": "tool is not configured in tools.yaml",
                    }
                )
                continue

            tool_status = {
                "tool": tool_id,
                "name": tool.name,
                "claim_scope": tool.claim_scope,
                "description": tool.description,
            }

            if tool.claim_scope == "deferred-specialized-peer":
                tool_status["reason"] = tool.scope_note or "comparison deferred by configuration"
                deferred.append(tool_status)
                continue

            if self._check_tool_available(tool_id):
                measured.append(tool_id)
            else:
                tool_status["reason"] = tool.scope_note or "tool not found on this machine"
                unavailable.append(tool_status)

        return measured, unavailable, deferred

    def run_benchmark(
        self,
        input_file: str | None = None,
        dataset_id: str | None = None,
        tool_ids: list[str] | None = None,
        threads_list: list[int] | None = None,
        runs: int = 3,
        verbose: bool = True,
        benchmark_command: dict[str, Any] | None = None,
    ) -> BenchmarkSuite:
        dataset_info: dict[str, Any] = {}
        resolved_input: Path | None = None

        if dataset_id:
            dataset = self.get_dataset(dataset_id)
            dataset_info = dataset.to_export()
            resolved_input = self._resolve_path(dataset.benchmark_input)
            if tool_ids is None:
                tool_ids = dataset.requested_tools or None
        elif input_file:
            resolved_input = Path(input_file).resolve()
            dataset_info = {
                "dataset_id": "ad-hoc-input",
                "name": "ad-hoc input",
                "benchmark_input": str(resolved_input),
                "claim_scope": "local-only",
            }

        if resolved_input is None:
            raise ValueError("either dataset_id or input_file is required")

        if not resolved_input.exists():
            raise FileNotFoundError(f"input file does not exist: {resolved_input}")

        if tool_ids is None:
            tool_ids = ["fqc", "gzip"]

        if threads_list is None:
            threads_list = self.settings.get("default_threads", [1, 4])

        measured_tools, unavailable_tools, deferred_tools = self._classify_tools(tool_ids)

        for tool_id in measured_tools:
            self.tools[tool_id].version = self._get_tool_version(self.tools[tool_id])

        suite = BenchmarkSuite(
            timestamp=datetime.now().isoformat(timespec="seconds"),
            input_file=str(resolved_input),
            input_size=self._get_effective_input_size(resolved_input),
            requested_tools=tool_ids,
            tools_tested=measured_tools,
            unavailable_tools=unavailable_tools,
            deferred_tools=deferred_tools,
            system_info=self._get_system_info(),
            dataset=dataset_info,
            benchmark_command=benchmark_command or {},
        )

        if verbose:
            print(f"\n{'=' * 72}")
            print("fq-compressor Benchmark Suite")
            print(f"{'=' * 72}")
            print(f"Dataset: {suite.dataset.get('name', 'N/A')}")
            if suite.dataset.get("accession"):
                print(f"Accession: {suite.dataset['accession']}")
            print(f"Input: {suite.input_file}")
            print(f"Input Size: {suite.input_size / 1024 / 1024:.2f} MB")
            print(f"Requested Tools: {', '.join(tool_ids)}")
            print(f"Measured Tools: {', '.join(measured_tools) if measured_tools else 'none'}")
            if unavailable_tools:
                missing = ", ".join(item["tool"] for item in unavailable_tools)
                print(f"Unavailable Tools: {missing}")
            if deferred_tools:
                deferred = ", ".join(item["tool"] for item in deferred_tools)
                print(f"Deferred Tools: {deferred}")
            print(f"Threads: {threads_list}")
            print(f"Runs per config: {runs}")
            print(f"{'=' * 72}\n")

        for tool_id in measured_tools:
            tool = self.tools[tool_id]
            if verbose:
                print(f"[{tool.name}] {tool.version}")

            for threads in threads_list:
                best_compress: BenchmarkResult | None = None
                best_decompress: BenchmarkResult | None = None

                for run in range(runs):
                    if verbose:
                        print(f"  Run {run + 1}/{runs} (threads={threads})...", end=" ", flush=True)

                    results = self.benchmark_tool(tool_id, str(resolved_input), threads)

                    for result in results:
                        if result.operation == "compress":
                            if best_compress is None or result.elapsed_seconds < best_compress.elapsed_seconds:
                                best_compress = result
                        elif best_decompress is None or result.elapsed_seconds < best_decompress.elapsed_seconds:
                            best_decompress = result

                    if verbose:
                        compress_result = next((item for item in results if item.operation == "compress"), None)
                        if compress_result and compress_result.success:
                            print(
                                f"ratio={compress_result.ratio:.4f}, "
                                f"speed={compress_result.speed_mb_s:.1f} MB/s"
                            )
                        elif compress_result:
                            print(f"FAILED: {compress_result.error}")
                        else:
                            print("no result")

                if best_compress:
                    suite.results.append(best_compress)
                if best_decompress:
                    suite.results.append(best_decompress)

        return suite

    def _tool_label(self, tool_id: str) -> str:
        tool = self.tools.get(tool_id)
        return tool.name if tool else tool_id

    def _compression_results(self, suite: BenchmarkSuite) -> list[BenchmarkResult]:
        return [result for result in suite.results if result.operation == "compress" and result.success]

    def _decompression_results(self, suite: BenchmarkSuite) -> list[BenchmarkResult]:
        return [result for result in suite.results if result.operation == "decompress" and result.success]

    def _dimension_summary(
        self,
        suite: BenchmarkSuite,
        label: str,
        results: list[BenchmarkResult],
        value_selector,
        lower_is_better: bool,
        formatter,
    ) -> dict[str, Any]:
        if not results:
            return {
                "dimension": label,
                "status": "unproven",
                "conclusion": f"No successful measurements were recorded for {label.lower()}.",
            }

        best = min(results, key=value_selector) if lower_is_better else max(results, key=value_selector)
        fqc_result = next((result for result in results if result.tool == "fqc"), None)

        if fqc_result is None:
            conclusion = f"fq-compressor was not measured for {label.lower()}."
            status = "unproven"
            gap_pct = None
        elif best.tool == "fqc":
            conclusion = (
                f"fq-compressor leads the measured set for {label.lower()} at "
                f"{formatter(value_selector(fqc_result))}."
            )
            status = "lead"
            gap_pct = 0.0
        else:
            best_value = value_selector(best)
            fqc_value = value_selector(fqc_result)
            if lower_is_better:
                gap_pct = ((fqc_value / best_value) - 1.0) * 100 if best_value else None
            else:
                gap_pct = ((best_value / fqc_value) - 1.0) * 100 if fqc_value else None

            qualifier = "competitive but not leading" if gap_pct is not None and gap_pct <= 10.0 else "not leading"
            conclusion = (
                f"fq-compressor is {qualifier} for {label.lower()} in the measured set; "
                f"{self._tool_label(best.tool)} is best at {formatter(best_value)}."
            )
            status = "competitive" if qualifier.startswith("competitive") else "not-leading"

        summary = {
            "dimension": label,
            "status": status,
            "best_tool": best.tool,
            "best_tool_name": self._tool_label(best.tool),
            "best_value": value_selector(best),
            "best_value_display": formatter(value_selector(best)),
            "conclusion": conclusion,
        }

        if fqc_result is not None:
            summary["fqc_value"] = value_selector(fqc_result)
            summary["fqc_value_display"] = formatter(value_selector(fqc_result))
        if gap_pct is not None:
            summary["gap_percent"] = round(gap_pct, 2)

        if label != "Compression Ratio":
            best_numeric = summary.get("best_value")
            if isinstance(best_numeric, (float, int)):
                summary["best_value_display"] = formatter(best_numeric)
            fqc_numeric = summary.get("fqc_value")
            if isinstance(fqc_numeric, (float, int)):
                summary["fqc_value_display"] = formatter(fqc_numeric)

        return summary

    def build_summary(self, suite: BenchmarkSuite) -> dict[str, Any]:
        compression_results = self._compression_results(suite)
        decompression_results = self._decompression_results(suite)

        summary = {
            "compression_ratio": self._dimension_summary(
                suite,
                "Compression Ratio",
                compression_results,
                lambda result: result.ratio,
                True,
                lambda value: f"{value:.4f} compressed/original",
            ),
            "compression_speed": self._dimension_summary(
                suite,
                "Compression Speed",
                compression_results,
                lambda result: result.speed_mb_s,
                False,
                lambda value: f"{value:.1f} MB/s",
            ),
            "decompression_speed": self._dimension_summary(
                suite,
                "Decompression Speed",
                decompression_results,
                lambda result: result.output_mb_s,
                False,
                lambda value: f"{value:.1f} MB/s output",
            ),
            "scope": {
                "requested_tools": suite.requested_tools,
                "measured_tools": suite.tools_tested,
                "unavailable_tools": suite.unavailable_tools,
                "deferred_tools": suite.deferred_tools,
                "note": (
                    "Specialized peers listed as deferred or unavailable are outside the verified comparison "
                    "scope for this tracked result set."
                ),
            },
        }

        return summary

    def generate_report(self, suite: BenchmarkSuite, output_path: str | None = None) -> str:
        summary = self.build_summary(suite)
        compression_results = {
            (result.tool, result.threads): result for result in self._compression_results(suite)
        }
        decompression_results = {
            (result.tool, result.threads): result for result in self._decompression_results(suite)
        }

        lines = [
            "# fq-compressor Benchmark Report",
            "",
            f"**Generated:** {suite.timestamp}",
            "",
            "## Dataset Provenance",
            "",
            f"- **Dataset:** {suite.dataset.get('name', 'N/A')}",
        ]

        if suite.dataset.get("accession"):
            lines.append(f"- **Accession:** `{suite.dataset['accession']}`")
        if suite.dataset.get("source"):
            lines.append(f"- **Source:** {suite.dataset['source']}")
        if suite.dataset.get("workload"):
            lines.append(f"- **Workload:** {suite.dataset['workload']}")
        if suite.dataset.get("read_layout"):
            lines.append(f"- **Read Layout:** {suite.dataset['read_layout']}")
        if suite.dataset.get("benchmark_focus"):
            lines.append(f"- **Benchmark Focus:** {suite.dataset['benchmark_focus']}")
        if suite.dataset.get("claim_scope"):
            lines.append(f"- **Claim Scope:** {suite.dataset['claim_scope']}")
        lines.append(f"- **Benchmark Input:** `{suite.input_file}`")
        lines.append(f"- **Input Size:** {suite.input_size / 1024 / 1024:.2f} MB")

        for url in suite.dataset.get("source_urls", []):
            lines.append(f"- **Source URL:** {url}")
        for step in suite.dataset.get("preparation", []):
            lines.append(f"- **Preparation:** `{step}`")
        for note in suite.dataset.get("notes", []):
            lines.append(f"- **Note:** {note}")

        lines.extend(
            [
                "",
                "## Reproduction",
                "",
                f"- **Command Path:** `{suite.benchmark_command.get('command_path', 'benchmark/benchmark.py')}`",
            ]
        )

        if suite.benchmark_command.get("invocation"):
            lines.append(f"- **Invocation:** `{suite.benchmark_command['invocation']}`")
        if suite.benchmark_command.get("working_directory"):
            lines.append(f"- **Working Directory:** `{suite.benchmark_command['working_directory']}`")

        lines.extend(
            [
                "",
                "## Tool Coverage",
                "",
                f"- **Requested Tools:** {', '.join(suite.requested_tools) if suite.requested_tools else 'none'}",
                f"- **Measured Tools:** {', '.join(suite.tools_tested) if suite.tools_tested else 'none'}",
            ]
        )

        if suite.unavailable_tools:
            for item in suite.unavailable_tools:
                lines.append(
                    f"- **Unavailable:** {item['tool']} ({item.get('claim_scope', 'unknown')}) - {item['reason']}"
                )
        else:
            lines.append("- **Unavailable:** none")

        if suite.deferred_tools:
            for item in suite.deferred_tools:
                lines.append(
                    f"- **Deferred:** {item['tool']} ({item.get('claim_scope', 'unknown')}) - {item['reason']}"
                )
        else:
            lines.append("- **Deferred:** none")

        lines.extend(
            [
                "",
                "## Test Environment",
                "",
                f"- **CPU Cores:** {suite.system_info.get('cpu_cores', 'N/A')}",
                f"- **Memory:** {suite.system_info.get('memory_gb', 'N/A')} GB",
                f"- **Kernel:** {suite.system_info.get('kernel', 'N/A')}",
                "",
                "## Compression Results",
                "",
                "| Tool | Threads | Ratio | Bits/Base | Compress (MB/s) | Decompress (MB/s output) |",
                "|------|---------|-------|-----------|-----------------|---------------------------|",
            ]
        )

        for key in sorted(compression_results.keys(), key=lambda item: (item[0], item[1])):
            tool_id, threads = key
            compress_result = compression_results[key]
            decompress_result = decompression_results.get(key)
            compress_speed = f"{compress_result.speed_mb_s:.3f}" if compress_result.speed_mb_s < 0.1 else f"{compress_result.speed_mb_s:.1f}"
            if decompress_result:
                value = decompress_result.output_mb_s
                decompress_speed = f"{value:.3f}" if value < 0.1 else f"{value:.1f}"
            else:
                decompress_speed = "N/A"
            lines.append(
                "| "
                f"{self._tool_label(tool_id)} | {threads} | {compress_result.ratio:.4f} | "
                f"{compress_result.bits_per_base:.2f} | {compress_speed} | "
                f"{decompress_speed} |"
            )

        failures = [result for result in suite.results if not result.success]
        if failures:
            lines.extend(["", "## Failures", ""])
            for result in failures:
                lines.append(
                    f"- `{result.tool}` {result.operation} (threads={result.threads}): {result.error or 'failed'}"
                )

        lines.extend(["", "## Dimension-Specific Conclusions", ""])
        for key in ("compression_ratio", "compression_speed", "decompression_speed"):
            lines.append(f"- {summary[key]['conclusion']}")

        lines.extend(
            [
                "",
                "## Scope Boundary",
                "",
                f"- {summary['scope']['note']}",
                "",
                "---",
                "*Report generated by the fq-compressor benchmark framework*",
            ]
        )

        report = "\n".join(lines)
        if output_path:
            output = Path(output_path)
            output.parent.mkdir(parents=True, exist_ok=True)
            with open(output, "w", encoding="utf-8") as handle:
                handle.write(report)
        return report

    def export_json(self, suite: BenchmarkSuite, output_path: str) -> None:
        data = {
            "timestamp": suite.timestamp,
            "input_file": suite.input_file,
            "input_size": suite.input_size,
            "dataset": suite.dataset,
            "benchmark_command": suite.benchmark_command,
            "requested_tools": suite.requested_tools,
            "measured_tools": suite.tools_tested,
            "unavailable_tools": suite.unavailable_tools,
            "deferred_tools": suite.deferred_tools,
            "system_info": suite.system_info,
            "tools": {
                tool_id: {
                    "name": tool.name,
                    "version": tool.version,
                    "category": tool.category,
                    "description": tool.description,
                    "claim_scope": tool.claim_scope,
                    "scope_note": tool.scope_note,
                }
                for tool_id, tool in self.tools.items()
                if tool_id in suite.requested_tools
            },
            "results": [asdict(result) for result in suite.results],
            "summary": self.build_summary(suite),
        }

        output = Path(output_path)
        output.parent.mkdir(parents=True, exist_ok=True)
        with open(output, "w", encoding="utf-8") as handle:
            json.dump(data, handle, indent=2)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="fq-compressor benchmark framework",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python benchmark.py --dataset err091571-local-supported --report benchmark/results/report.md
  python benchmark.py --input data.fastq --tools fqc,gzip,xz --threads 1 4
  python benchmark.py --list-datasets
        """,
    )

    parser.add_argument("-i", "--input", help="Input FASTQ/FASTQ.GZ file")
    parser.add_argument("--dataset", help="Dataset id from benchmark/datasets.yaml")
    parser.add_argument("--tools", help="Comma-separated list of tools to test")
    parser.add_argument("--all", action="store_true", help="Request every configured tool")
    parser.add_argument(
        "-t",
        "--threads",
        type=int,
        nargs="+",
        default=None,
        help="Thread counts to test (default: dataset or config defaults)",
    )
    parser.add_argument("-r", "--runs", type=int, default=3, help="Number of runs per configuration")
    parser.add_argument("--report", help="Output markdown report path")
    parser.add_argument("--json", help="Output JSON results path")
    parser.add_argument("-q", "--quiet", action="store_true", help="Quiet mode")
    parser.add_argument("--config", help="Path to tools.yaml config file")
    parser.add_argument("--dataset-config", help="Path to datasets.yaml config file")
    parser.add_argument("--list-tools", action="store_true", help="List configured tools and exit")
    parser.add_argument("--list-datasets", action="store_true", help="List datasets and exit")
    parser.add_argument(
        "--command-path",
        default="benchmark/benchmark.py",
        help="Command path shown in reproducibility metadata",
    )
    parser.add_argument("--invocation", help="Invocation shown in reproducibility metadata")

    args = parser.parse_args()

    runner = BenchmarkRunner(args.config, args.dataset_config)

    if args.list_tools:
        print("Configured tools:")
        for tool_id, tool, available in runner.list_tools():
            mark = "✓" if available else "✗"
            print(f"  [{mark}] {tool_id}: {tool.name} ({tool.claim_scope})")
        return

    if args.list_datasets:
        print("Configured datasets:")
        for dataset in runner.list_datasets():
            print(f"  - {dataset.dataset_id}: {dataset.name}")
        return

    if not args.input and not args.dataset:
        parser.error("either --input or --dataset is required")

    if args.all:
        tool_ids = list(runner.tools.keys())
    elif args.tools:
        tool_ids = [item.strip() for item in args.tools.split(",") if item.strip()]
    else:
        tool_ids = None

    if args.invocation:
        invocation = args.invocation
    else:
        invocation_parts = ["python3", "benchmark/benchmark.py"]
        if args.dataset:
            invocation_parts.extend(["--dataset", args.dataset])
        if args.input:
            invocation_parts.extend(["--input", args.input])
        if args.all:
            invocation_parts.append("--all")
        elif args.tools:
            invocation_parts.extend(["--tools", args.tools])
        if args.threads:
            invocation_parts.extend(["--threads", *[str(item) for item in args.threads]])
        invocation_parts.extend(["--runs", str(args.runs)])
        if args.report:
            invocation_parts.extend(["--report", args.report])
        if args.json:
            invocation_parts.extend(["--json", args.json])
        invocation = " ".join(shlex.quote(part) for part in invocation_parts)

    suite = runner.run_benchmark(
        input_file=args.input,
        dataset_id=args.dataset,
        tool_ids=tool_ids,
        threads_list=args.threads,
        runs=args.runs,
        verbose=not args.quiet,
        benchmark_command={
            "command_path": args.command_path,
            "runner": "benchmark/benchmark.py",
            "working_directory": str(runner.project_root),
            "invocation": invocation,
        },
    )

    if args.report:
        runner.generate_report(suite, args.report)
        if not args.quiet:
            print(f"Report saved to: {args.report}")

    if args.json:
        runner.export_json(suite, args.json)
        if not args.quiet:
            print(f"JSON saved to: {args.json}")

    if not args.quiet and not args.report:
        print("\n" + runner.generate_report(suite))


if __name__ == "__main__":
    main()
