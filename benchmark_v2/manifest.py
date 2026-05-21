from __future__ import annotations

from pathlib import Path

import yaml

from benchmark_v2.models import ToolSpec, WorkloadSpec

_MANIFEST_DIR = Path(__file__).resolve().parent / "manifests"


def _load_yaml(name: str) -> dict:
    with (_MANIFEST_DIR / name).open("r", encoding="utf-8") as handle:
        return yaml.safe_load(handle) or {}


def load_workloads() -> tuple[WorkloadSpec, ...]:
    config = _load_yaml("workloads.yaml")
    workloads = config.get("workloads") or []
    return tuple(
        WorkloadSpec(
            workload_id=entry["workload_id"],
            layout=entry["layout"],
            inputs=tuple(entry.get("inputs") or ()),
            read_limit=entry["read_limit"],
            tier=entry["tier"],
            comparable_tools=tuple(entry.get("comparable_tools") or ()),
        )
        for entry in workloads
    )


def load_tools() -> tuple[ToolSpec, ...]:
    config = _load_yaml("tools.yaml")
    tools = config.get("tools") or []
    return tuple(
        ToolSpec(
            tool_id=entry["tool_id"],
            category=entry["category"],
            supports_paired=entry["supports_paired"],
            compress_template=entry["compress_template"],
            decompress_template=entry["decompress_template"],
        )
        for entry in tools
    )
