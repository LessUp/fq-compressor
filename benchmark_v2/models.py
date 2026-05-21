from __future__ import annotations

from dataclasses import dataclass
from math import isfinite
from string import Formatter
from typing import Literal

Layout = Literal["single", "paired"]
Operation = Literal["compress", "decompress"]

_VALID_LAYOUTS = ("single", "paired")
_VALID_OPERATIONS = ("compress", "decompress")


def _validate_layout(value: str, *, field_name: str) -> None:
    if value not in _VALID_LAYOUTS:
        valid_layouts = ", ".join(_VALID_LAYOUTS)
        raise ValueError(f"{field_name} must be one of: {valid_layouts}; got {value!r}")


def _validate_non_empty_string(value: object, *, field_name: str) -> None:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field_name} must be a non-empty string")


def _coerce_tuple(values: object, *, field_name: str) -> tuple[object, ...]:
    if isinstance(values, tuple):
        return values
    if isinstance(values, list):
        return tuple(values)
    raise ValueError(f"{field_name} must be a tuple or list")


def _validate_string_members(values: tuple[object, ...], *, field_name: str) -> None:
    for index, value in enumerate(values, start=1):
        if not isinstance(value, str) or not value:
            raise ValueError(f"{field_name} member #{index} must be a non-empty string")


def _validate_positive_int(value: object, *, field_name: str) -> None:
    if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
        raise ValueError(f"{field_name} must be positive")


def _validate_non_negative_int(value: object, *, field_name: str) -> None:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise ValueError(f"{field_name} must be non-negative")


def _validate_bool(value: object, *, field_name: str) -> None:
    if not isinstance(value, bool):
        raise ValueError(f"{field_name} must be a boolean")


def _template_placeholders(template: str) -> set[str]:
    placeholders: set[str] = set()
    for _, field_name, _, _ in Formatter().parse(template):
        if field_name is None:
            continue
        root_name = field_name.split(".", 1)[0].split("[", 1)[0]
        if root_name:
            placeholders.add(root_name)
    return placeholders


def _validate_template_placeholders(
    template: str,
    *,
    field_name: str,
    required_placeholders: tuple[str, ...],
) -> None:
    placeholders = _template_placeholders(template)
    missing = [name for name in required_placeholders if name not in placeholders]
    if missing:
        required = ", ".join(required_placeholders)
        raise ValueError(f"{field_name} requires required placeholders: {required}")

    allowed_placeholders = set(required_placeholders)
    unknown = sorted(placeholders - allowed_placeholders)
    if unknown:
        unknown_placeholders = ", ".join(unknown)
        raise ValueError(f"{field_name} has unknown placeholders: {unknown_placeholders}")


@dataclass(frozen=True)
class WorkloadSpec:
    workload_id: str
    layout: Layout
    inputs: tuple[str, ...]
    read_limit: int
    tier: str
    comparable_tools: tuple[str, ...]

    def __post_init__(self) -> None:
        _validate_non_empty_string(self.workload_id, field_name="workload_id")
        _validate_layout(self.layout, field_name="layout")
        inputs = _coerce_tuple(self.inputs, field_name="inputs")
        object.__setattr__(self, "inputs", inputs)
        _validate_string_members(inputs, field_name="inputs")
        if self.layout == "single":
            if len(inputs) != 1:
                raise ValueError("single workloads must define exactly 1 input")
        elif len(inputs) != 2 or inputs[0] == inputs[1]:
            raise ValueError("paired workloads must define exactly 2 distinct inputs")
        _validate_positive_int(self.read_limit, field_name="read_limit")
        _validate_non_empty_string(self.tier, field_name="tier")
        comparable_tools = _coerce_tuple(self.comparable_tools, field_name="comparable_tools")
        object.__setattr__(self, "comparable_tools", comparable_tools)
        _validate_string_members(comparable_tools, field_name="comparable_tools")
        if not comparable_tools:
            raise ValueError("comparable_tools must contain at least one tool ID")
        if len(set(comparable_tools)) != len(comparable_tools):
            raise ValueError("comparable_tools must not contain duplicates")


@dataclass(frozen=True)
class ToolSpec:
    tool_id: str
    category: str
    supports_paired: bool
    compress_template: str
    decompress_template: str

    def __post_init__(self) -> None:
        _validate_non_empty_string(self.tool_id, field_name="tool_id")
        _validate_non_empty_string(self.category, field_name="category")
        _validate_bool(self.supports_paired, field_name="supports_paired")
        _validate_non_empty_string(self.compress_template, field_name="compress_template")
        _validate_template_placeholders(
            self.compress_template,
            field_name="compress_template",
            required_placeholders=("input", "output"),
        )
        _validate_non_empty_string(self.decompress_template, field_name="decompress_template")
        _validate_template_placeholders(
            self.decompress_template,
            field_name="decompress_template",
            required_placeholders=("output", "decompressed"),
        )


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
        _validate_non_empty_string(self.tool_id, field_name="tool_id")
        _validate_layout(self.layout, field_name="layout")
        if self.operation not in _VALID_OPERATIONS:
            valid_operations = ", ".join(_VALID_OPERATIONS)
            raise ValueError(
                f"operation must be one of: {valid_operations}; got {self.operation!r}"
            )
        _validate_positive_int(self.threads, field_name="threads")
        _validate_non_negative_int(self.input_bytes, field_name="input_bytes")
        _validate_non_negative_int(self.output_bytes, field_name="output_bytes")
        if (
            not isinstance(self.elapsed_seconds, (int, float))
            or isinstance(self.elapsed_seconds, bool)
            or not isfinite(self.elapsed_seconds)
            or self.elapsed_seconds < 0
        ):
            raise ValueError("elapsed_seconds must be non-negative")
        _validate_bool(self.success, field_name="success")
        if self.success and self.elapsed_seconds <= 0:
            raise ValueError("elapsed_seconds must be positive for successful benchmark results")


@dataclass(frozen=True)
class BenchmarkSuite:
    workload_id: str
    layout: Layout
    results: tuple[BenchmarkResult, ...]

    def __post_init__(self) -> None:
        _validate_non_empty_string(self.workload_id, field_name="workload_id")
        _validate_layout(self.layout, field_name="layout")
        results = _coerce_tuple(self.results, field_name="results")
        object.__setattr__(self, "results", results)
        if not results:
            raise ValueError("results must contain at least one BenchmarkResult")
        for index, result in enumerate(results, start=1):
            if not isinstance(result, BenchmarkResult):
                raise ValueError(f"results item #{index} must be a BenchmarkResult")
            if result.layout != self.layout:
                raise ValueError(
                    f"suite layout {self.layout!r} does not match result #{index} "
                    f"layout {result.layout!r}"
                )
