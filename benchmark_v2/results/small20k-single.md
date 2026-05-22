# Benchmark Report: small20k-single

## Measured Results

| Tool | Ratio | Compress MiB/s | Decompress MiB/s |
| --- | ---: | ---: | ---: |
| fqc | 4.04x | 0.10 | 1.56 |
| gzip | 3.25x | 3.48 | 67.86 |
| xz | 4.05x | 0.23 | 23.55 |
| bzip2 | 3.98x | 6.42 | 3.20 |

## Peer Standing

- Compression ratio: `competitive` (rank 2/4, best `xz`)
- Compression speed: `lagging` (rank 4/4, best `bzip2`)
- Decompression speed: `lagging` (rank 4/4, best `gzip`)

## Conclusion

fq-compressor stands in the competitive compression-ratio tier, lagging compression-speed tier, and lagging decompression-speed tier for workload `small20k-single`.
