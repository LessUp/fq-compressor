# CLI Reference

fq-compressor provides four main commands: `compress`, `decompress`, `info`, and `verify`.

## Global Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message |
| `--version` | Show version information |
| `-v, --verbose` | Enable verbose logging |

## compress

Compress a FASTQ file into the `.fqc` archive format.

```bash
fqc compress [OPTIONS] -i <input> -o <output>
```

### Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `-i, --input` | PATH | *required* | Input FASTQ file (`.fastq`, `.fq`, `.fastq.gz`, `.fq.gz`) |
| `-o, --output` | PATH | *required* | Output `.fqc` archive file |
| `-I, --input2` | PATH | — | Second input file for paired-end data |
| `-t, --threads` | INT | CPU cores | Number of parallel threads |
| `-l, --level` | INT | 5 | Compression level (1=fast, 9=best) |
| `--memory-limit` | INT | 8192 | Memory budget in MB |
| `--pipeline` | FLAG | off | Enable 3-stage parallel pipeline |
| `--no-reorder` | FLAG | off | Disable read reordering (preserves original order) |

### Examples

```bash
# Basic compression
fqc compress -i reads.fastq -o reads.fqc

# High compression with 4 threads
fqc compress -i reads.fastq -o reads.fqc -l 9 -t 4

# Paired-end compression
fqc compress -i R1.fastq.gz -I R2.fastq.gz -o paired.fqc

# Pipeline mode with memory limit
fqc compress -i large.fastq -o large.fqc --pipeline --memory-limit 16384
```

## decompress

Decompress a `.fqc` archive back to FASTQ format.

```bash
fqc decompress [OPTIONS] -i <input> -o <output>
```

### Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `-i, --input` | PATH | *required* | Input `.fqc` archive file |
| `-o, --output` | PATH | *required* | Output FASTQ file |
| `-O, --output2` | PATH | — | Second output file for paired-end data |
| `-t, --threads` | INT | CPU cores | Number of parallel threads |
| `--range` | STR | — | Extract specific read range (e.g., `1000:2000`) |
| `--pipeline` | FLAG | off | Enable 3-stage parallel pipeline |

### Examples

```bash
# Basic decompression
fqc decompress -i reads.fqc -o reads.fastq

# Random access: extract reads 500-1500
fqc decompress -i reads.fqc --range 500:1500 -o subset.fastq

# Paired-end decompression
fqc decompress -i paired.fqc -o R1.fastq -O R2.fastq
```

## info

Display metadata and statistics about a `.fqc` archive.

```bash
fqc info [OPTIONS] <input>
```

### Output Fields

| Field | Description |
|-------|-------------|
| Read count | Total number of reads in the archive |
| Original size | Uncompressed size in bytes |
| Compressed size | Archive size in bytes |
| Compression ratio | Original / Compressed |
| Bits per base | Average compressed bits per DNA base |
| Block count | Number of independent blocks |
| Read length | Min / Max / Median read length |

## verify

Verify the integrity of a `.fqc` archive.

```bash
fqc verify [OPTIONS] <input>
```

Performs comprehensive integrity validation:
- File header magic number and version check
- Block-level CRC32 verification
- Index consistency check
- Footer checksum validation

### Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | I/O error |
| 2 | Format error |
| 3 | Compression error |
| 4 | Validation error |
| 5 | Internal error |
