# Reference Projects Directory

This directory is for storing source code of reference FASTQ compression tools for comparison and algorithm study.

## Reference Tools

- **Spring** - Assembly-based Compression (ABC) reference
- **Minicom** - Minimizer-based compression
- **fqzcomp5** - Quality value compression reference
- **repaq** - Extreme compression ratio reference
- **HARC** - Architecture reference
- **NanoSpring** - Nanopore-specific compression
- **pigz** - Parallel compression model
- **fastq-tools** - Framework reference

## Usage

These tools are stored here for:
1. Algorithm comparison and benchmarking
2. Source code reference during development
3. Performance comparison studies

**Note**: This directory is gitignored. You need to populate it manually with the reference project sources.

## Setup

To populate this directory, clone the reference projects:

```bash
cd ref-projects/
git clone https://github.com/gherhardt/spring.git
git clone https://github.com/gherhardt/minicom.git
# ... and so on for other reference projects
```
