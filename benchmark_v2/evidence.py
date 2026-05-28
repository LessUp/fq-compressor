from __future__ import annotations

import json
import os
import shlex
import shutil
import subprocess
from pathlib import Path
from typing import Any

from benchmark_v2.models import ToolSpec
from benchmark_v2.report import build_report
from benchmark_v2.tool_adapters import _resolve_fqc_binary


def load_dataset_metadata(config_path: Path, dataset_id: str) -> dict[str, Any]:
    import yaml

    with config_path.open("r", encoding="utf-8") as handle:
        config = yaml.safe_load(handle) or {}

    dataset = (config.get("datasets") or {}).get(dataset_id)
    if not isinstance(dataset, dict):
        raise ValueError(f"unknown dataset: {dataset_id}")
    return {"dataset_id": dataset_id, **dataset}


def classify_tools(
    requested_tool_ids: tuple[str, ...], tools_by_id: dict[str, ToolSpec]
) -> tuple[list[str], list[dict[str, Any]], list[dict[str, Any]]]:
    measured: list[str] = []
    unavailable: list[dict[str, Any]] = []
    deferred: list[dict[str, Any]] = []

    for tool_id in requested_tool_ids:
        tool = tools_by_id.get(tool_id)
        if tool is None:
            unavailable.append(
                {
                    "tool": tool_id,
                    "name": tool_id,
                    "claim_scope": "unknown",
                    "reason": "tool is not configured in benchmark_v2/manifests/tools.yaml",
                }
            )
            continue

        status = {
            "tool": tool_id,
            "name": tool.name,
            "claim_scope": tool.claim_scope,
            "description": tool.description,
        }
        if tool.claim_scope == "deferred-specialized-peer":
            status["reason"] = tool.scope_note or "comparison deferred by configuration"
            deferred.append(status)
            continue

        if is_tool_available(tool):
            measured.append(tool_id)
        else:
            status["reason"] = tool.scope_note or "tool not found on this machine"
            unavailable.append(status)

    return measured, unavailable, deferred


def collect_tool_metadata(tools_by_id: dict[str, ToolSpec], requested_tool_ids: tuple[str, ...]) -> dict[str, Any]:
    metadata: dict[str, Any] = {}
    for tool_id in requested_tool_ids:
        tool = tools_by_id.get(tool_id)
        if tool is None:
            continue
        metadata[tool_id] = {
            "name": tool.name,
            "version": get_tool_version(tool),
            "category": tool.category,
            "description": tool.description,
            "claim_scope": tool.claim_scope,
            "scope_note": tool.scope_note,
        }
    return metadata


def get_tool_version(tool: ToolSpec) -> str:
    if not tool.version_cmd:
        return "unknown"
    try:
        result = subprocess.run(
            tool.version_cmd,
            shell=True,
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
    except Exception:
        return "unknown"

    version = result.stdout.strip() or result.stderr.strip()
    return version.splitlines()[0][:80] if version else "unknown"


def is_tool_available(tool: ToolSpec) -> bool:
    if tool.tool_id == "fqc":
        try:
            _resolve_fqc_binary()
            return True
        except ValueError:
            return False

    first_token = shlex.split(tool.compress_template)[0]
    return shutil.which(first_token) is not None


def collect_system_info() -> dict[str, Any]:
    info: dict[str, Any] = {}
    info["cpu_cores"] = os.cpu_count() or 1

    try:
        with Path("/proc/meminfo").open("r", encoding="utf-8") as handle:
            first = next(line for line in handle if line.startswith("MemTotal:"))
        mem_kb = int(first.split()[1])
        info["memory_gb"] = round(mem_kb / 1024 / 1024, 1)
    except Exception:
        info["memory_gb"] = 0

    try:
        result = subprocess.run(
            ["uname", "-r"], capture_output=True, text=True, timeout=5, check=False
        )
        info["kernel"] = result.stdout.strip() or "unknown"
    except Exception:
        info["kernel"] = "unknown"

    return info


def build_evidence_report(
    *,
    suite,
    requested_tool_ids: tuple[str, ...],
    measured_tool_ids: list[str],
    unavailable_tools: list[dict[str, Any]],
    deferred_tools: list[dict[str, Any]],
    benchmark_command: dict[str, Any],
    dataset: dict[str, Any],
    tools_metadata: dict[str, Any],
    system_info: dict[str, Any],
    focus_tool_id: str = "fqc",
) -> dict[str, Any]:
    report = build_report(suite, focus_tool_id=focus_tool_id)
    report["kind"] = "benchmark_report_v2"
    report["requested_tools"] = list(requested_tool_ids)
    report["measured_tools"] = measured_tool_ids
    report["unavailable_tools"] = unavailable_tools
    report["deferred_tools"] = deferred_tools
    report["benchmark_command"] = benchmark_command
    report["dataset"] = dataset
    report["tools"] = tools_metadata
    report["system_info"] = system_info
    return report


def export_evidence_report_json(report: dict[str, Any], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(report, indent=2), encoding="utf-8")


def export_evidence_report_markdown(report: dict[str, Any], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(render_evidence_report_markdown(report), encoding="utf-8")


def render_evidence_report_markdown(report: dict[str, Any]) -> str:
    tool_rows = [
        "| Tool | Ratio | Compress MiB/s | Decompress MiB/s |",
        "| --- | ---: | ---: | ---: |",
    ]
    for metrics in report["tool_metrics"]:
        tool_rows.append(
            "| "
            f"{metrics['tool_id']} | "
            f"{float(metrics['compression_ratio']):.2f}x "
            f"(T{int(metrics['compression_ratio_threads'])}) | "
            f"{float(metrics['compression_mib_per_s']):.2f} "
            f"(T{int(metrics['compression_mib_per_s_threads'])}) | "
            f"{float(metrics['decompression_mib_per_s']):.2f} "
            f"(T{int(metrics['decompression_mib_per_s_threads'])}) |"
        )

    dataset = report["dataset"]
    benchmark_command = report["benchmark_command"]
    return "\n".join(
        [
            f"# Benchmark Report: {report['workload_id']}",
            "",
            "## Dataset Provenance",
            "",
            f"- **Dataset:** {dataset.get('name', report['workload_id'])}",
            f"- **Claim Scope:** {dataset.get('claim_scope', 'local-only')}",
            f"- **Benchmark Input:** `{dataset.get('benchmark_input', report['suite'].get('workload_id', 'unknown'))}`",
            "",
            "## Reproduction",
            "",
            f"- **Command Path:** `{benchmark_command.get('command_path', 'unknown')}`",
            f"- **Invocation:** `{benchmark_command.get('invocation', 'unknown')}`",
            f"- **Working Directory:** `{benchmark_command.get('working_directory', 'unknown')}`",
            "",
            "## Tool Coverage",
            "",
            f"- **Requested Tools:** {', '.join(report['requested_tools'])}",
            f"- **Measured Tools:** {', '.join(report['measured_tools'])}",
            "",
            "## Measured Results",
            "",
            *tool_rows,
            "",
            "## Conclusion",
            "",
            str(report["conclusion"]),
            "",
        ]
    )
