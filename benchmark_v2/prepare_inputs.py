from __future__ import annotations

import gzip
from pathlib import Path

from benchmark_v2.models import WorkloadSpec


def subset_fastq(src_gz: Path, dst_fastq: Path, read_limit: int) -> Path:
    records_written = 0
    with (
        gzip.open(src_gz, "rt", encoding="ascii", errors="ignore") as fin,
        dst_fastq.open("w", encoding="ascii") as fout,
    ):
        while records_written < read_limit:
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
            fout.write(header)
            fout.write(sequence)
            fout.write(plus)
            fout.write(quality)
            records_written += 1
    return dst_fastq


def prepare_workload(spec: WorkloadSpec, data_root: Path, output_dir: Path) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    resolved = [data_root / relative for relative in spec.inputs]
    outputs = [output_dir / f"{spec.workload_id}_R1.fastq"]
    if spec.layout == "paired":
        outputs.append(output_dir / f"{spec.workload_id}_R2.fastq")
    temporary_outputs = [output_dir / f".{output.name}.tmp" for output in outputs]
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
