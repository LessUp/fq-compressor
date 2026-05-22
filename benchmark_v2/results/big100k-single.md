# Benchmark Report: big100k-single

## Measured Results

| Tool | Ratio | Compress MiB/s | Decompress MiB/s |
| --- | ---: | ---: | ---: |
| fqc | 4.86x | 0.10 | 2.12 |
| gzip | 3.89x | 4.24 | 79.07 |
| xz | 5.16x | 0.24 | 27.56 |
| bzip2 | 4.88x | 7.93 | 4.27 |

## Peer Standing

- Compression ratio: `lagging` (rank 3/4, best `xz`)
- Compression speed: `lagging` (rank 4/4, best `bzip2`)
- Decompression speed: `lagging` (rank 4/4, best `gzip`)

## Conclusion

fq-compressor stands in the lagging compression-ratio tier, lagging compression-speed tier, and lagging decompression-speed tier for workload `big100k-single`.
