# 0004 `GzipStreamBuf` move assignment lost buffered output

- Status: closed
- Severity: high
- Found in: v2 phase 3 (pre-75b7400)
- Related: src/io/compressed_stream.cpp

## Symptom

Concatenated gzip input could lose data across a stream move: the second gzip member produced
truncated or empty output after the owning stream was moved.

## Reproduction

Feed a concatenated `.gz` (two or more members) through a `GzipStreamBuf` that is move-assigned
between members.

## Investigation

- First added concatenated-gzip support and saw intermittent short reads on multi-member inputs.
- Truncated the input to two small members and confirmed the second member's first chunk was
  missing exactly when a move had occurred.
- Inspected the move-assignment operator: it transferred zlib state but not the unconsumed bytes
  already decoded into the streambuf's get area.

## Root cause

The move-assignment operator destroyed the source's put/get buffer without flushing or transferring
the decoded-but-unread bytes, so any inflate output sitting in the buffer at move time was lost.

## Fix

Preserve unread inflate output across move assignment: transfer the pending get-area bytes into the
destination before releasing the source buffer.

## Verification

`compressed_stream_test` exercises concatenated gzip plus move; `v2_archive_engine_test` round-trips
concatenated-gzip stdin. Static analysis (clang-tidy) flagged the original defect and was used to
confirm the fix.

## Follow-up / prevention

The compressed-stream test now includes a concatenated-gzip-with-move case; the bugprone clang-tidy
check runs in `./scripts/lint.sh lint`.
