// =============================================================================
// fq-compressor - Legacy Pipeline Nodes Compatibility Include
// =============================================================================

#ifndef FQC_PIPELINE_PIPELINE_NODES_H
#define FQC_PIPELINE_PIPELINE_NODES_H

// Legacy compatibility include kept for existing downstream code.
// New integration code should include fqc/pipeline/pipeline.h instead.
#define FQC_PIPELINE_NODES_IS_LEGACY_COMPAT_HEADER 1

#include "fqc/pipeline/decompressor_node.h"
#include "fqc/pipeline/fastq_writer_node.h"
#include "fqc/pipeline/fqc_reader_node.h"
#include "fqc/pipeline/node_common.h"
#include "fqc/pipeline/reader_node.h"
#include "fqc/pipeline/writer_node.h"

#endif  // FQC_PIPELINE_PIPELINE_NODES_H
