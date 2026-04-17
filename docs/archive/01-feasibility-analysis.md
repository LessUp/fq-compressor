# Research Report 01: Project Feasibility & Architecture Analysis

## 1. Project Goal
Develop a high-performance FASTQ compressor (`fq-compressor`) combining:
- **Core Algorithm**: **Spring** (Sequence Reordering/Encoding) for high compression ratio.
- **Framework**: **fastq-tools** (C++20, Modern CMake) for robust engineering.
- **Parallelism**: **Intel oneTBB** + **Pigz model** (Producer-Consumer Pipeline).
- **Format**: Block-based format supporting **Random Access**.

## 2. Feasibility Assessment

### Technical Feasibility
| Component | Status | Assessment |
|-----------|--------|------------|
| **C++ Standard** | C++20 | **High**. Modern C++ features (Ranges, Concepts) generally improve code quality and safety. fastq-tools already uses this. |
| **Parallelism** | Intel TBB | **High**. TBB's `parallel_pipeline` is perfect for the "Read -> Compress -> Write" IO-bound model. |
| **Spring Integration** | Vendor | **Medium/Hard**. Spring is a complex research codebase. Integrating its core (Encoder/RefGen) requires careful refactoring to support "Block Reset" mechanisms needed for random access. |
| **Random Access** | Block Index | **High**. Standard techniques (bgzf-style blocking) work well. |

### Architecture Compatibility
1.  **IO Layer**: fastq-tools' IO design can be reused but needs extension for block-level IO.
2.  **Logging**: **spdlog** is chosen for stability (with potential migration to **Quill** for ultra-low latency).
3.  **CLI**: **CLI11** is chosen for its header-only convenience and modern API.

## 3. Risk Analysis
*   **Memory Usage**: Spring's reordering algorithm is memory intensive. **Mitigation**: Use Block-based processing to cap memory usage per thread/block.
*   **Implementation Complexity**: Writing a reordering engine from scratch is too hard. **Mitigation**: Vendor/Fork Spring's existing implementation.
*   **License**: Spring uses a non-commercial research license. This limits `fq-compressor` to non-commercial use unless rewritten.

## 4. Conclusion
The project is feasible and represents a "Best of Both Worlds" approach. By wrapping the high-ratio Spring algorithm in a robust, parallel framework, we can achieve SOTA compression with industrial-strength usability.
