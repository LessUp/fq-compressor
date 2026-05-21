from __future__ import annotations

from dataclasses import dataclass
from typing import Literal

Layout = Literal["single", "paired"]
Operation = Literal["compress", "decompress"]

_VALID_LAYOUTS = ("single", "paired")
_VALID_OPERATIONS = ("compress", "decompress")


def _validate_layout(value: str, *, field_name: str) -> None:
    if value not in _VALID_LAYOUTS:
        valid_layouts = ", ".join(_VALID_LAYOUTS)
        raise ValueError(f"{field_name} must be one of: {valid_layouts}; got {value!r}")


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
    operation: Operation
    threads: int
    input_bytes: int
    output_bytes: int
    elapsed_seconds: float
    success: bool

    def __post_init__(self) -> None:
        _validate_layout(self.layout, field_name="layout")
        if self.operation not in _VALID_OPERATIONS:
            valid_operations = ", ".join(_VALID_OPERATIONS)
            raise ValueError(
                f"operation must be one of: {valid_operations}; got {self.operation!r}"
            )


@dataclass(frozen=True)
class BenchmarkSuite:
    workload_id: str
    layout: Layout
    results: tuple[BenchmarkResult, ...]

    def __post_init__(self) -> None:
        _validate_layout(self.layout, field_name="layout")
        for index, result in enumerate(self.results, start=1):
            if result.layout != self.layout:
                raise ValueError(
                    f"suite layout {self.layout!r} does not match result #{index} "
                    f"layout {result.layout!r}"
                )
