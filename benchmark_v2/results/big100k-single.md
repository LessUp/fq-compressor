# Benchmark Report: big100k-single

## Measured Results

| Tool | Ratio | Compress MiB/s | Decompress MiB/s |
| --- | ---: | ---: | ---: |
| fqc | 4.86x (T4) | 0.10 (T4) | 2.12 (T4) |
| gzip | 3.89x (T1) | 4.24 (T1) | 79.07 (T1) |
| xz | 5.16x (T1) | 0.24 (T1) | 27.56 (T1) |
| bzip2 | 4.88x (T1) | 7.93 (T1) | 4.27 (T1) |

## Peer Standing

- Compression ratio: `lagging` (rank 3/4, best `xz`)
- Compression speed: `lagging` (rank 4/4, best `bzip2`)
- Decompression speed: `lagging` (rank 4/4, best `gzip`)

## Conclusion

fq-compressor stands in the lagging compression-ratio tier, lagging compression-speed tier, and lagging decompression-speed tier for workload `big100k-single`.
