# Benchmark Report: small20k-single

## Measured Results

| Tool | Ratio | Compress MiB/s | Decompress MiB/s |
| --- | ---: | ---: | ---: |
| fqc | 4.04x (T4) | 0.10 (T4) | 1.56 (T4) |
| gzip | 3.25x (T1) | 3.48 (T1) | 67.86 (T1) |
| xz | 4.05x (T1) | 0.23 (T1) | 23.55 (T1) |
| bzip2 | 3.98x (T1) | 6.42 (T1) | 3.20 (T1) |

## Peer Standing

- Compression ratio: `competitive` (rank 2/4, best `xz`)
- Compression speed: `lagging` (rank 4/4, best `bzip2`)
- Decompression speed: `lagging` (rank 4/4, best `gzip`)

## Conclusion

fq-compressor stands in the competitive compression-ratio tier, lagging compression-speed tier, and lagging decompression-speed tier for workload `small20k-single`.
