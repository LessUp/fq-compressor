# Bibliography and repositories

Use this page as the citation key for the public documentation set.

## Numbered literature

1. **[R1]** Chandak, Shubham, et al. [SPRING: a next-generation compressor for FASTQ files](https://doi.org/10.1093/bioinformatics/btz074).  
   The closest paper-level framing for assembly-based compression, reversible reordering, and consensus-style sequence reduction.
2. **[R2]** Bonfield, James K. [fqzcomp repository](https://github.com/jkbonfield/fqzcomp).  
   A compact quality-value compression reference that sharpens the discussion around stream-specific coding trade-offs.
3. **[R3]** Sahlin, Kristoffer, et al. [NanoSpring: reference-free lossless compression of nanopore sequencing reads](https://doi.org/10.1101/2021.06.09.447198).  
   A useful long-read comparator when explaining what fq-compressor is not optimized around first.

## Comparator repositories

1. **[C1]** [shubhamchandak94/Spring](https://github.com/shubhamchandak94/Spring)  
   Closest external repository for the assembly-based compression framing.
2. **[C2]** [shubhamchandak94/HARC](https://github.com/shubhamchandak94/HARC)  
   FASTQ-specialized comparator for architecture and project-scope trade-offs.
3. **[C3]** [jkbonfield/fqzcomp](https://github.com/jkbonfield/fqzcomp)  
   Quality-modeling comparator that pressures fq-compressor's stream decomposition.
4. **[C4]** [qm2/NanoSpring](https://github.com/qm2/NanoSpring)  
   Long-read comparison target used to explain scope boundaries.

## Repository-local evidence anchors

1. **[E1]** [`benchmark/results/`](https://github.com/LessUp/fq-compressor/tree/master/benchmark/results)  
   Machine-readable and narrative benchmark outputs used by the performance section.
2. **[E2]** [`vendor/spring-core/`](https://github.com/LessUp/fq-compressor/tree/master/vendor/spring-core)  
   Extracted Spring-derived reference code with an explicit license boundary.
3. **[E3]** [`ref-projects/`](https://github.com/LessUp/fq-compressor/tree/master/ref-projects)  
   Repository-local comparison targets and study notes.
4. **[E4]** Archived project notes
   Historical requirements and capability boundaries for the project.
