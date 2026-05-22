from __future__ import annotations

import json
import math
from dataclasses import asdict
from pathlib import Path

from benchmark_v2.models import BenchmarkResult, BenchmarkSuite

_BYTES_PER_MIB = 1024 * 1024
_FOCUS_TOOL_ID = "fqc"


def classify_position(rank: int, total: int) -> str:
    if total <= 0:
        raise ValueError("total must be positive")
    if rank <= 0 or rank > total:
        raise ValueError(f"rank must be between 1 and {total}; got {rank}")
    if total == 1:
        return "standalone"
    if rank == 1:
        return "leader"
    if rank <= max(2, math.ceil(total / 3)):
        return "competitive"
    return "lagging"


def build_peer_standing(
    metrics_by_tool: list[dict[str, float | str]],
    *,
    tool_id: str,
    metric_key: str,
    higher_is_better: bool,
) -> dict[str, object]:
    ordered = sorted(metrics_by_tool, key=lambda item: float(item[metric_key]), reverse=higher_is_better)
    rank = next(index for index, item in enumerate(ordered, start=1) if item["tool_id"] == tool_id)
    return {
        "tool_id": tool_id,
        "metric": metric_key,
        "rank": rank,
        "total": len(ordered),
        "position_band": classify_position(rank, len(ordered)),
        "best_tool_id": ordered[0]["tool_id"],
        "metric_value": float(next(item[metric_key] for item in ordered if item["tool_id"] == tool_id)),
        "best_metric_value": float(ordered[0][metric_key]),
    }


def build_report(suite: BenchmarkSuite, *, focus_tool_id: str = _FOCUS_TOOL_ID) -> dict[str, object]:
    metrics_by_tool = _build_tool_metrics(suite)
    focus_metrics = next((item for item in metrics_by_tool if item["tool_id"] == focus_tool_id), None)
    if focus_metrics is None:
        raise ValueError(f"tool '{focus_tool_id}' does not have a complete successful benchmark summary")

    peer_standing = {
        "compression_ratio": build_peer_standing(
            metrics_by_tool, tool_id=focus_tool_id, metric_key="compression_ratio", higher_is_better=True
        ),
        "compression_mib_per_s": build_peer_standing(
            metrics_by_tool,
            tool_id=focus_tool_id,
            metric_key="compression_mib_per_s",
            higher_is_better=True,
        ),
        "decompression_mib_per_s": build_peer_standing(
            metrics_by_tool,
            tool_id=focus_tool_id,
            metric_key="decompression_mib_per_s",
            higher_is_better=True,
        ),
    }

    conclusion = (
        _build_conclusion(suite.workload_id, peer_standing)
    )

    return {
        "workload_id": suite.workload_id,
        "layout": suite.layout,
        "focus_tool_id": focus_tool_id,
        "suite": asdict(suite),
        "tool_metrics": metrics_by_tool,
        "peer_standing": peer_standing,
        "conclusion": conclusion,
    }


def export_report_json(report: dict[str, object], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(report, indent=2), encoding="utf-8")


def export_report_markdown(report: dict[str, object], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(render_report_markdown(report), encoding="utf-8")


def render_report_markdown(report: dict[str, object]) -> str:
    tool_rows = [
        "| Tool | Ratio | Compress MiB/s | Decompress MiB/s |",
        "| --- | ---: | ---: | ---: |",
    ]
    for metrics in report["tool_metrics"]:
        tool_rows.append(
            "| "
            f"{metrics['tool_id']} | "
            f"{float(metrics['compression_ratio']):.2f}x | "
            f"{float(metrics['compression_mib_per_s']):.2f} | "
            f"{float(metrics['decompression_mib_per_s']):.2f} |"
        )

    peer_lines = []
    for title, key in (
        ("Compression ratio", "compression_ratio"),
        ("Compression speed", "compression_mib_per_s"),
        ("Decompression speed", "decompression_mib_per_s"),
    ):
        standing = report["peer_standing"][key]
        peer_lines.append(
            "- "
            f"{title}: `{standing['position_band']}` "
            f"(rank {standing['rank']}/{standing['total']}, best `{standing['best_tool_id']}`)"
        )

    return "\n".join(
        [
            f"# Benchmark Report: {report['workload_id']}",
            "",
            "## Measured Results",
            "",
            *tool_rows,
            "",
            "## Peer Standing",
            "",
            *peer_lines,
            "",
            "## Conclusion",
            "",
            str(report["conclusion"]),
            "",
        ]
    )


def _build_tool_metrics(suite: BenchmarkSuite) -> list[dict[str, float | str]]:
    grouped: dict[str, dict[str, list[BenchmarkResult]]] = {}
    observed_tool_ids = {result.tool_id for result in suite.results}
    for result in suite.results:
        if not result.success:
            continue
        operations = grouped.setdefault(result.tool_id, {})
        operations.setdefault(result.operation, []).append(result)

    metrics_by_tool: list[dict[str, float | str]] = []
    for tool_id, operations in grouped.items():
        compress_results = operations.get("compress", [])
        decompress_results = operations.get("decompress", [])
        if not compress_results or not decompress_results:
            continue
        metrics_by_tool.append(
            {
                "tool_id": tool_id,
                "compression_ratio": max(_compression_ratio(result) for result in compress_results),
                "compression_mib_per_s": max(
                    _compress_speed_mib_per_s(result) for result in compress_results
                ),
                "decompression_mib_per_s": max(
                    _decompress_speed_mib_per_s(result) for result in decompress_results
                ),
            }
        )

    if not metrics_by_tool:
        raise ValueError(f"workload '{suite.workload_id}' does not contain any complete successful tool runs")
    complete_tool_ids = {metric["tool_id"] for metric in metrics_by_tool}
    if complete_tool_ids != observed_tool_ids:
        raise ValueError(
            f"workload '{suite.workload_id}' has an incomplete benchmark suite; "
            "cannot build peer-standing conclusions"
        )
    return metrics_by_tool


def _build_conclusion(workload_id: str, peer_standing: dict[str, dict[str, object]]) -> str:
    ratio_band = str(peer_standing["compression_ratio"]["position_band"])
    compress_band = str(peer_standing["compression_mib_per_s"]["position_band"])
    decompress_band = str(peer_standing["decompression_mib_per_s"]["position_band"])

    if ratio_band == compress_band == decompress_band == "standalone":
        return (
            "fq-compressor is the only measured tool for workload "
            f"`{workload_id}`, so this report is standalone evidence rather than a peer-ranking claim."
        )

    return (
        "fq-compressor stands in the "
        f"{ratio_band} compression-ratio tier, "
        f"{compress_band} compression-speed tier, "
        f"and {decompress_band} decompression-speed tier "
        f"for workload `{workload_id}`."
    )


def _compression_ratio(result: BenchmarkResult) -> float:
    if result.output_bytes <= 0:
        raise ValueError(f"tool '{result.tool_id}' produced zero-byte compressed output")
    return result.input_bytes / result.output_bytes


def _compress_speed_mib_per_s(result: BenchmarkResult) -> float:
    return result.input_bytes / result.elapsed_seconds / _BYTES_PER_MIB


def _decompress_speed_mib_per_s(result: BenchmarkResult) -> float:
    return result.output_bytes / result.elapsed_seconds / _BYTES_PER_MIB
