# Research Report 02: Strategic Evaluation (Strategy & Scope)

## 1. Reference-based vs Reference-free Support
### Analysis
*   **Reference-based (CRAM-like)**:
    *   *Pros*: Highest theoretical compression ratio (store only differences).
    *   *Cons*: External dependency "Hell". Loss of reference = Loss of data. High latency due to alignment.
    *   *Unsuitability*: Not suitable for general archiving or *de novo* sequencing.
*   **Reference-free (Spring/Mincom)**:
    *   *Pros*: **Self-contained**. No external dependencies. Easier data management.
    *   *Performance*: Spring's internal reordering creates an "Ad-hoc Reference", achieving nearly the same ratio as Ref-based tools.

### Decision
**Primary Strategy: Reference-free**.
Rationale: For a general-purpose archival tool, self-containment is critical. The "Assembly-based Compression" strategy provides the benefits of ref-based compression without the external dependency.

## 2. Lossy vs Lossless Features
### Sequence (Read Bases)
*   **Decision: Mandatory Lossless**.
*   Rationale: Sequence is the source of truth. Any loss here compromises downstream variant calling.

### Quality Scores
*   **Decision: Hybrid Mode**.
*   **Default: Lossless**. For medical/high-precision use.
*   **Optional: Lossy**.
    *   **Illumina Binning (8-level)**: Industry standard. Safe for most uses.
    *   **SCM (Statistical Context Mixing)**: fqzcomp5-style modeling. High ratio without explicit binning distortion.

### Read IDs (Headers)
*   **Decision: Hybrid Mode**.
*   **Default: Lossless**.
*   **Optional: Tokenized/Indexed**. Replaces complex machine IDs with simple counters (1, 2, 3...). Essential for high-ratio archiving.

## 3. Stream Separation (Columnar)
To maximize compression, data is split into independent streams:
1.  **ID Stream**: Delta encoding + General compression.
2.  **Sequence Stream**: ABC Strategy (Reordering + Consensus).
3.  **Quality Stream**: SCM Strategy (Context Arithmetic).

This allows using the *best* algorithm for each data type.
