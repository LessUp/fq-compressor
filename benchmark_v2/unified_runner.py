from __future__ import annotations

import argparse
import sys
from pathlib import Path

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from benchmark_v2.evidence import (
    build_evidence_report,
    classify_tools,
    collect_system_info,
    collect_tool_metadata,
    export_evidence_report_json,
    export_evidence_report_markdown,
    load_dataset_metadata,
)
from benchmark_v2.manifest import load_tools
from benchmark_v2.runner import run_prepared_matrix


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--workload-id", required=True)
    parser.add_argument("--layout", choices=("single", "paired"), default="single")
    parser.add_argument("--tools")
    parser.add_argument("--threads", type=int, nargs="+", required=True)
    parser.add_argument("--runs", type=int, required=True)
    parser.add_argument("--json", required=True)
    parser.add_argument("--report", required=True)
    parser.add_argument("--command-path", required=True)
    parser.add_argument("--invocation", required=True)
    parser.add_argument("--working-directory", required=True)
    parser.add_argument("--dataset-config")
    parser.add_argument("--dataset-id")
    return parser


def main() -> None:
    parser = _build_parser()
    args = parser.parse_args()

    input_path = Path(args.input).resolve()
    if not input_path.exists():
        parser.error(f"input file does not exist: {input_path}")
    if any(thread_count <= 0 for thread_count in args.threads):
        parser.error("threads must be positive")
    if args.runs <= 0:
        parser.error("runs must be positive")

    tools_by_id = {tool.tool_id: tool for tool in load_tools()}
    if args.dataset_config and args.dataset_id:
        dataset = load_dataset_metadata(Path(args.dataset_config), args.dataset_id)
        requested_tool_ids = tuple(
            part.strip()
            for part in (args.tools.split(",") if args.tools else dataset.get("requested_tools", []))
            if part.strip()
        )
    else:
        dataset = {
            "dataset_id": "ad-hoc-input",
            "name": "ad-hoc input",
            "benchmark_input": str(input_path),
            "claim_scope": "local-only",
        }
        requested_tool_ids = tuple(
            part.strip() for part in (args.tools.split(",") if args.tools else ["fqc", "gzip"]) if part.strip()
        )

    measured_tool_ids, unavailable_tools, deferred_tools = classify_tools(requested_tool_ids, tools_by_id)
    selected_tools = [tools_by_id[tool_id] for tool_id in measured_tool_ids if tool_id in tools_by_id]
    suite = run_prepared_matrix(
        workload_id=args.workload_id,
        layout=args.layout,
        prepared_inputs=[input_path],
        work_dir=Path(args.working_directory) / "build" / "benchmark_v2" / args.workload_id,
        tools=selected_tools,
        threads=list(args.threads),
        runs=args.runs,
    )

    benchmark_command = {
        "command_path": args.command_path,
        "working_directory": args.working_directory,
        "invocation": args.invocation,
    }
    report = build_evidence_report(
        suite=suite,
        requested_tool_ids=requested_tool_ids,
        measured_tool_ids=measured_tool_ids,
        unavailable_tools=unavailable_tools,
        deferred_tools=deferred_tools,
        benchmark_command=benchmark_command,
        dataset=dataset,
        tools_metadata=collect_tool_metadata(tools_by_id, requested_tool_ids),
        system_info=collect_system_info(),
    )
    export_evidence_report_json(report, Path(args.json))
    export_evidence_report_markdown(report, Path(args.report))


if __name__ == "__main__":
    main()
