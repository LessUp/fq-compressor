from __future__ import annotations

from dataclasses import dataclass
from typing import Literal

Layout = Literal["single", "paired"]


@dataclass(frozen=True)
class WorkloadSpec:
    workload_id: str
    layout: Layout
    inputs: tuple[str, ...]
    read_limit: int
    tier: str
    comparable_tools: tuple[str, ...]


@dataclass(frozen=True)
class ToolSpec:
    tool_id: str
    category: str
    supports_paired: bool
    compress_template: str
    decompress_template: str


@dataclass(frozen=True)
class BenchmarkResult:
    tool_id: str
    layout: Layout
    operation: str
    threads: int
    input_bytes: int
    output_bytes: int
    elapsed_seconds: float
    success: bool


@dataclass(frozen=True)
class BenchmarkSuite:
    workload_id: str
    layout: Layout
    results: tuple[BenchmarkResult, ...]
