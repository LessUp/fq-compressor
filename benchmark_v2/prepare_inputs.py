from __future__ import annotations

import gzip
import tempfile
from pathlib import Path

from benchmark_v2.models import WorkloadSpec


def subset_fastq(src_gz: Path, dst_fastq: Path, read_limit: int) -> Path:
    records_written = 0
    with (
        gzip.open(src_gz, "rt", encoding="ascii") as fin,
        dst_fastq.open("w", encoding="ascii") as fout,
    ):
        while records_written < read_limit:
            record_index = records_written + 1
            header = fin.readline()
            if header == "":
                raise ValueError(
                    f"{src_gz} contains only {records_written} complete reads, "
                    f"fewer than requested {read_limit}"
                )
            sequence = fin.readline()
            plus = fin.readline()
            quality = fin.readline()
            if "" in (sequence, plus, quality):
                raise ValueError(f"{src_gz} ends mid-record after {records_written} complete reads")
            if not header.startswith("@"):
                raise ValueError(
                    f"{src_gz} malformed FASTQ record #{record_index}: "
                    "header line must start with '@'"
                )
            if not plus.startswith("+"):
                raise ValueError(
                    f"{src_gz} malformed FASTQ record #{record_index}: "
                    "separator line must start with '+'"
                )
            if len(sequence.rstrip("\r\n")) != len(quality.rstrip("\r\n")):
                raise ValueError(
                    f"{src_gz} malformed FASTQ record #{record_index}: "
                    "sequence and quality lengths must match"
                )
            fout.write(header)
            fout.write(sequence)
            fout.write(plus)
            fout.write(quality)
            records_written += 1
    return dst_fastq


def _resolve_output_path(output_dir: Path, filename: str) -> Path:
    output_dir_resolved = output_dir.resolve()
    candidate = (output_dir_resolved / filename).resolve()
    try:
        relative_candidate = candidate.relative_to(output_dir_resolved)
    except ValueError as exc:
        raise ValueError(
            f"output path {filename!r} escapes output_dir {output_dir_resolved}"
        ) from exc
    return output_dir_resolved / relative_candidate


def prepare_workload(spec: WorkloadSpec, data_root: Path, output_dir: Path) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    output_dir_resolved = output_dir.resolve()
    data_root_resolved = data_root.resolve()
    resolved: list[Path] = []
    for relative in spec.inputs:
        candidate = (data_root / relative).resolve()
        try:
            candidate.relative_to(data_root_resolved)
        except ValueError as exc:
            raise ValueError(
                f"input path {relative!r} escapes data_root {data_root_resolved}"
            ) from exc
        resolved.append(candidate)
    outputs = [_resolve_output_path(output_dir, f"{spec.workload_id}_R1.fastq")]
    if spec.layout == "paired":
        outputs.append(_resolve_output_path(output_dir, f"{spec.workload_id}_R2.fastq"))
    temporary_outputs: list[Path] = []
    for output in outputs:
        with tempfile.NamedTemporaryFile(
            dir=output_dir_resolved,
            prefix=f".{output.name}.",
            suffix=".tmp",
            delete=False,
        ) as temp_file:
            temporary_outputs.append(Path(temp_file.name))
    try:
        for temp_output in temporary_outputs:
            temp_output.unlink(missing_ok=True)
        for src, temp_output in zip(resolved, temporary_outputs):
            subset_fastq(src, temp_output, spec.read_limit)
        for temp_output, output in zip(temporary_outputs, outputs):
            temp_output.replace(output)
    except Exception:
        for temp_output in temporary_outputs:
            temp_output.unlink(missing_ok=True)
        raise
    return outputs
