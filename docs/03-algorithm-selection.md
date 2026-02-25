# Research Report 03: Algorithm Selection & Random Access

## 1. Sequence Compression: ABC Strategy
### Candidates
*   **Spring**: The SOTA reference-free compressor. Uses purely structural reordering/encoding.
*   **fqzcomp5**: Uses statistical context mixing. Good, but less effective than Spring on high-depth data.
*   **Mincom**: Similar to Spring (Minimizer Bucketing), simpler but less comprehensive.

### Selection: Spring Core Integration (ABC)
We adopt the **Assembly-based Compression (ABC)** strategy:
`Minimizer Bucketing -> Reordering -> Consensus -> Delta Encoding`.
**Implementation**: Vendor/Fork Spring's core. Do not reimplement from scratch (too complex).

## 2. Quality Compression: SCM vs ACO
### Candidates
*   **ACO (Adaptive Coding Order)**: Academic algorithm. High claimed ratio, but low engineering maturity.
*   **fqzcomp5 (SCM)**: Industry standard. Robust, context-based arithmetic coding.

### Selection: fqzcomp5-style SCM
We choose **Statistical Context Mixing (SCM)**.
**Rationale**: fqzcomp5 is battle-tested. ACO is experimental and risky. Stability is prioritized.

## 3. Random Access Utility
**Question**: Is Random Access useful?
**Answer**: **Yes, Critical.**

### Use Cases
1.  **Parallel Processing (Scatter-Gather)**: Allows splitting multi-TB files across 1000 cluster nodes for parallel alignment. Impossible without block-based RA.
2.  **Quality Control (QC)**: Instant sampling of reads from the middle of a file without reading the whole stream.
3.  **Visualization (IGV)**: Inspecting specific reads by ID.
4.  **Distributed Computing (Spark/Hadoop)**: Native support for HDFS split processing.

### Implementation
**Scheme A (Block Independence)**:
*   File is split into e.g., 10MB blocks.
*   Each block is independently compressed (Context Reset).
*   **Trade-off**: Slightly lower compression ratio (< 1-2% loss) vs O(1) Access Speed.
*   **Verdict**: Worth it for an industrial tool.
