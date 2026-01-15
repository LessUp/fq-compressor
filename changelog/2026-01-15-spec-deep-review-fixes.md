# Spec Deep Review Fixes

**Date**: 2026-01-15

## Summary

Applied all 15 fixes identified in the deep review of fq-compressor design specification.

## High Priority Fixes (3)

### #6: Sampling Strategy Boundary Conditions
- Changed sampling count to `min(1000, total_reads)`
- Added streaming mode default strategy (MEDIUM, conservative)
- Added `--scan-all-lengths` option for full file scanning
- Added dynamic detection documentation

### #7: --range Parameter Semantics
- Defined 1-based indexing (consistent with SAMtools, bedtools)
- Added error handling for invalid ranges
- Added support for omitted start/end (`:end`, `start:`)

### #11: --streams Non-all Mode Output Format
- Defined output formats for each combination:
  - `--streams id`: raw lines (no @ prefix)
  - `--streams seq`: raw sequences
  - `--streams qual`: raw quality values
  - `--streams id,seq`: FASTA format
- Added `--output-format` parameter

## Medium Priority Fixes (7)

### #2: ReorderMap Forward Compatibility
- Added `header_size` and `version` fields to ReorderMap struct

### #3: GlobalHeader Complete Layout
- Added complete C++ struct definition with byte offsets
- Specified UTF-8 encoding for filename (no null terminator)

### #4: CodecParams Detection Mechanism
- Clarified CodecParams is included in GlobalHeaderSize
- Added detection logic code example

### #5: Quality Discard Codec Semantics
- Documented `RAW (0x0) + size_qual=0` means "Quality discarded"
- Added `--placeholder-qual` configuration option

### #8: PE Consecutive Mode Range Semantics
- Clarified `--range` always uses archive_id
- Added `--range-pairs` parameter for PE pair-based access

### #9: Chunk-wise block_id Continuity
- Specified block_id is globally continuous across chunks

### #13: ID_MODE=discard Rebuild Format
- SE: `@{archive_id}`
- PE Interleaved: `@{pair_id}/{read_num}`
- PE Consecutive: `@{archive_id}`
- Added `--id-prefix` option

## Low Priority Fixes (5)

### #12: uniform_read_length=0 Ambiguity
- Documented length=0 reads are invalid

### #14: Empty File Handling
- Defined empty file produces valid .fqc with 0 blocks

### #15: max_block_bases CLI Parameter
- Added `--max-block-bases <bytes>` to requirements and tasks

## Files Modified

- `.kiro/specs/fq-compressor/design.md`
- `.kiro/specs/fq-compressor/requirements.md`
- `.kiro/specs/fq-compressor/tasks.md`
