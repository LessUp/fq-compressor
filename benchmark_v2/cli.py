from __future__ import annotations

import argparse
import sys
from pathlib import Path

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from benchmark_v2.manifest import load_tools, load_workloads
from benchmark_v2.prepare_inputs import prepare_workload


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
    return parser


def _get_workload(workload_id: str):
    for workload in load_workloads():
        if workload.workload_id == workload_id:
            return workload
    raise ValueError(f"unknown workload: {workload_id}")


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
        workload = _get_workload(args.workload)
        outputs = prepare_workload(
            workload,
            data_root=Path(args.data_root),
            output_dir=Path(args.output_dir),
        )
        for output in outputs:
            print(output)
        return

    parser.error("one of the arguments --list-workloads --list-tools or a subcommand is required")


if __name__ == "__main__":
    main()
