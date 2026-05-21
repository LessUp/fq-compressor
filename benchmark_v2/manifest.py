from __future__ import annotations

from pathlib import Path
from typing import Any

import yaml

from benchmark_v2.models import ToolSpec, WorkloadSpec

_MANIFEST_DIR = Path(__file__).resolve().parent / "manifests"


def _load_yaml(name: str) -> tuple[Path, dict[str, Any]]:
    path = _MANIFEST_DIR / name
    try:
        with path.open("r", encoding="utf-8") as handle:
            data = yaml.safe_load(handle)
    except yaml.YAMLError as exc:
        raise ValueError(f"{path.name} is not valid YAML: {exc}") from exc

    if not isinstance(data, dict):
        raise ValueError(f"{path.name} must contain a top-level mapping")
    return path, data


def _load_entries(name: str, key: str) -> tuple[Path, list[dict[str, Any]]]:
    path, config = _load_yaml(name)
    entries = config.get(key)
    if not isinstance(entries, list):
        raise ValueError(f"{path.name} key '{key}' must contain a list")

    for index, entry in enumerate(entries, start=1):
        if not isinstance(entry, dict):
            raise ValueError(f"{path.name} entry #{index} must be a mapping")
    return path, entries


def _raise_on_duplicate_id(path: Path, kind: str, entry_id: str, seen_ids: set[str]) -> None:
    if entry_id in seen_ids:
        raise ValueError(f"{path.name} has duplicate {kind}_id '{entry_id}'")
    seen_ids.add(entry_id)


def _require_string(entry: dict[str, Any], key: str, path: Path, index: int, kind: str) -> str:
    if key not in entry:
        raise ValueError(f"{path.name} entry #{index} missing required key '{key}'")
    value = entry[key]
    if not isinstance(value, str) or not value:
        raise ValueError(f"{path.name} {kind} entry #{index} key '{key}' must be a non-empty string")
    return value


def _require_bool(entry: dict[str, Any], key: str, path: Path, index: int, kind: str) -> bool:
    if key not in entry:
        raise ValueError(f"{path.name} entry #{index} missing required key '{key}'")
    value = entry[key]
    if not isinstance(value, bool):
        raise ValueError(f"{path.name} {kind} entry #{index} key '{key}' must be a boolean")
    return value


def _require_int(entry: dict[str, Any], key: str, path: Path, index: int, kind: str) -> int:
    if key not in entry:
        raise ValueError(f"{path.name} entry #{index} missing required key '{key}'")
    value = entry[key]
    if not isinstance(value, int) or isinstance(value, bool):
        raise ValueError(f"{path.name} {kind} entry #{index} key '{key}' must be an integer")
    return value


def _require_string_list(
    entry: dict[str, Any], key: str, path: Path, index: int, kind: str
) -> tuple[str, ...]:
    if key not in entry:
        raise ValueError(f"{path.name} entry #{index} missing required key '{key}'")
    value = entry[key]
    if not isinstance(value, list) or any(not isinstance(item, str) or not item for item in value):
        raise ValueError(f"{path.name} {kind} entry #{index} key '{key}' must be a list of strings")
    return tuple(value)


def load_workloads() -> tuple[WorkloadSpec, ...]:
    tool_specs = load_tools()
    tools_by_id = {tool.tool_id: tool for tool in tool_specs}
    path, entries = _load_entries("workloads.yaml", "workloads")
    workloads: list[WorkloadSpec] = []
    seen_workload_ids: set[str] = set()
    for index, entry in enumerate(entries, start=1):
        workload_id = _require_string(entry, "workload_id", path, index, "workload")
        _raise_on_duplicate_id(path, "workload", workload_id, seen_workload_ids)
        layout = _require_string(entry, "layout", path, index, "workload")
        inputs = _require_string_list(entry, "inputs", path, index, "workload")
        read_limit = _require_int(entry, "read_limit", path, index, "workload")
        tier = _require_string(entry, "tier", path, index, "workload")
        comparable_tools = _require_string_list(entry, "comparable_tools", path, index, "workload")

        if layout not in {"single", "paired"}:
            raise ValueError(f"{path.name} workload '{workload_id}' has invalid layout '{layout}'")
        expected_inputs = 1 if layout == "single" else 2
        if len(inputs) != expected_inputs:
            raise ValueError(
                f"{path.name} workload '{workload_id}' with layout '{layout}' "
                f"must define exactly {expected_inputs} inputs"
            )
        if layout == "paired" and inputs[0] == inputs[1]:
            raise ValueError(
                f"{path.name} workload '{workload_id}' must define two distinct inputs"
            )
        if read_limit <= 0:
            raise ValueError(f"{path.name} workload '{workload_id}' read_limit must be positive")
        if not comparable_tools:
            raise ValueError(
                f"{path.name} workload '{workload_id}' must define at least one "
                "comparable_tools entry"
            )
        seen_comparable_tools: set[str] = set()
        for tool_id in comparable_tools:
            if tool_id in seen_comparable_tools:
                raise ValueError(
                    f"{path.name} workload '{workload_id}' has duplicate comparable_tools ID "
                    f"'{tool_id}'"
                )
            seen_comparable_tools.add(tool_id)
            tool = tools_by_id.get(tool_id)
            if tool is None:
                raise ValueError(
                    f"{path.name} workload '{workload_id}' references unknown comparable_tools ID "
                    f"'{tool_id}'"
                )
            if layout == "paired" and not tool.supports_paired:
                raise ValueError(
                    f"{path.name} workload '{workload_id}' references tool '{tool_id}' "
                    "that does not support paired workloads"
                )

        workloads.append(
            WorkloadSpec(
                workload_id=workload_id,
                layout=layout,
                inputs=inputs,
                read_limit=read_limit,
                tier=tier,
                comparable_tools=comparable_tools,
            )
        )
    return tuple(workloads)


def load_tools() -> tuple[ToolSpec, ...]:
    path, entries = _load_entries("tools.yaml", "tools")
    tools: list[ToolSpec] = []
    seen_tool_ids: set[str] = set()
    for index, entry in enumerate(entries, start=1):
        tool_id = _require_string(entry, "tool_id", path, index, "tool")
        _raise_on_duplicate_id(path, "tool", tool_id, seen_tool_ids)
        tools.append(
            ToolSpec(
                tool_id=tool_id,
                category=_require_string(entry, "category", path, index, "tool"),
                supports_paired=_require_bool(entry, "supports_paired", path, index, "tool"),
                compress_template=_require_string(entry, "compress_template", path, index, "tool"),
                decompress_template=_require_string(
                    entry, "decompress_template", path, index, "tool"
                ),
            )
        )
    return tuple(tools)
