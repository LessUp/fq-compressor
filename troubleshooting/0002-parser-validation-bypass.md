# 0002 `FastqParser::validateSequence` ignored `validBases`

- Status: closed
- Severity: high
- Found in: v2 phase 2 (pre-75b7400)
- Related: src/io/fastq_parser.cpp

## Symptom

Expanding `ParserOptions::validBases` to include the full IUPAC alphabet did not change which
symbols the parser accepted. Non-A/C/G/T/N symbols were rejected even when the profile permitted
them.

## Reproduction

Configure a parser with `validBases` containing IUPAC ambiguity codes (R, Y, S, W, K, M, B, D, H,
V, upper and lower case) and feed a record containing one of them.

## Investigation

- First assumed the option was not being plumbed through the parser construction path; traced it
  and confirmed it reached `validateSequence`.
- Found that `validateSequence` ignored the option and called a hard-coded helper that only knew
  A/C/G/T/N.

## Root cause

`validateSequence` bypassed its configured alphabet and delegated to a hard-coded validator, so
`ParserOptions::validBases` was dead configuration for sequence validation.

## Fix

Route validation through the configured alphabet and update the hard-coded helper consistently so
both paths honour `validBases`. v2 now validates against the configured full upper/lower-case
IUPAC nucleotide alphabet and losslessly preserves exceptional symbols.

## Verification

`fastq_parser_test` and `v2_archive_engine_test` pass with mixed-symbol single-end and paired
inputs; exceptional bases survive a full compress/decompress round trip.

## Follow-up / prevention

The parser test matrix now includes IUPAC symbols so a future regression of this kind fails
locally.
