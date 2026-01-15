// =============================================================================
// fq-compressor - High Performance FASTQ Compressor
// =============================================================================
// Main entry point for the fqc command-line tool.
// =============================================================================

#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "fq-compressor v0.1.0" << std::endl;
    std::cout << "High-performance FASTQ compressor with random access support" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: fqc <command> [options]" << std::endl;
    std::cout << "Commands: compress, decompress, info, verify" << std::endl;
    std::cout << std::endl;
    std::cout << "Run 'fqc --help' for more information." << std::endl;
    return 0;
}
