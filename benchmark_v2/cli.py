from __future__ import annotations

import argparse
import sys
from pathlib import Path

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from benchmark_v2.manifest import load_tools, load_workloads
from benchmark_v2.prepare_inputs import prepare_workload
from benchmark_v2.report import build_report, export_report_json, export_report_markdown
from benchmark_v2.runner import export_suite_json, run_matrix


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--list-workloads", action="store_true")
    group.add_argument("--list-tools", action="store_true")
    subparsers = parser.add_subparsers(dest="command")
    prepare_parser = subparsers.add_parser("prepare")
    prepare_parser.add_argument("--workload", required=True)
    prepare_parser.add_argument("--data-root", required=True)
    prepare_parser.add_argument("--output-dir", required=True)
    run_parser = subparsers.add_parser("run")
    run_parser.add_argument("--workload", required=True)
    run_parser.add_argument("--data-root", required=True)
    run_parser.add_argument("--tools")
    run_parser.add_argument("--threads", type=int, nargs="+", required=True)
    run_parser.add_argument("--runs", type=int, required=True)
    run_parser.add_argument("--json")
    run_parser.add_argument("--report")
    return parser


def _get_workload(workload_id: str):
    for workload in load_workloads():
        if workload.workload_id == workload_id:
            return workload
    raise ValueError(f"unknown workload: {workload_id}")


def _get_tools(tool_ids: tuple[str, ...]):
    tools_by_id = {tool.tool_id: tool for tool in load_tools()}
    selected = []
    for tool_id in tool_ids:
        tool = tools_by_id.get(tool_id)
        if tool is None:
            raise ValueError(f"unknown tool: {tool_id}")
        selected.append(tool)
    return selected


def _validate_threads(threads: list[int]) -> list[int]:
    if any(thread_count <= 0 for thread_count in threads):
        raise ValueError("threads must be positive")
    return threads


def main() -> None:
    parser = _build_parser()
    args = parser.parse_args()

    if args.command is not None:
        if args.list_workloads:
            parser.error("cannot combine --list-workloads with subcommands")
        if args.list_tools:
            parser.error("cannot combine --list-tools with subcommands")

    if args.list_workloads:
        for workload in load_workloads():
            print(workload.workload_id)
        return

    if args.list_tools:
        for tool in load_tools():
            print(tool.tool_id)
        return

    if args.command == "prepare":
        try:
            workload = _get_workload(args.workload)
            outputs = prepare_workload(
                workload,
                data_root=Path(args.data_root),
                output_dir=Path(args.output_dir),
            )
        except (OSError, ValueError) as exc:
            parser.error(str(exc))
        for output in outputs:
            print(output)
        return

    if args.command == "run":
        try:
            workload = _get_workload(args.workload)
            tool_ids = (
                tuple(part.strip() for part in args.tools.split(",") if part.strip())
                if args.tools
                else workload.comparable_tools
            )
            suite = run_matrix(
                workload=workload,
                data_root=Path(args.data_root),
                work_dir=Path(__file__).resolve().parent.parent / "build" / "benchmark_v2",
                tools=_get_tools(tool_ids),
                threads=_validate_threads(list(args.threads)),
                runs=args.runs,
            )
        except (OSError, ValueError) as exc:
            parser.error(str(exc))
        has_failures = any(not result.success for result in suite.results)
        report = None
        if args.report:
            try:
                report = build_report(suite)
            except ValueError as exc:
                parser.error(str(exc))
            export_report_markdown(report, Path(args.report))
        if args.json:
            if report is not None:
                export_report_json(report, Path(args.json))
            else:
                export_suite_json(suite, Path(args.json))
        else:
            print(report["conclusion"] if report is not None else suite)
        raise SystemExit(1 if has_failures else 0)

    parser.error("one of the arguments --list-workloads --list-tools or a subcommand is required")


if __name__ == "__main__":
    main()
