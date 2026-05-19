<script setup lang="ts">
import { withBase } from "vitepress";
import { computed } from "vue";

const props = withDefaults(
    defineProps<{
        locale?: "en" | "zh";
    }>(),
    {
        locale: "en"
    }
);

if (import.meta.env.DEV && props.locale && !["en", "zh"].includes(props.locale)) {
    console.warn(`[ArchitectureAtlas] Invalid locale: ${props.locale}, expected 'en' or 'zh'`);
}

const copy = {
    en: {
        kicker: "System blueprint",
        title: "The archive is built as a chain of explicit contracts.",
        summary:
            "fq-compressor is easier to audit when each phase has a clear boundary: ingest, analysis, block transforms, archive materialization, and selective retrieval.",
        stages: [
            {
                serial: "01",
                label: "Ingest",
                detail: "FASTQ plus compressed FASTQ streams enter through parser and stream adapters.",
                anchor: "io/fastq_parser + io/compressed_stream"
            },
            {
                serial: "02",
                label: "Analyze",
                detail: "Global statistics establish reorder intent, chunk sizing, and memory discipline.",
                anchor: "algo/global_analyzer + common/memory_budget"
            },
            {
                serial: "03",
                label: "Compress",
                detail: "Block-local transforms split sequence, ID, and quality responsibilities across dedicated codecs.",
                anchor: "algo/block_compressor + quality/id streams"
            },
            {
                serial: "04",
                label: "Store",
                detail: "FQC writes blocks, checksums, reorder metadata, and the lookup structures needed later.",
                anchor: "format/fqc_writer + format/index tables"
            },
            {
                serial: "05",
                label: "Retrieve",
                detail: "Readers can verify, range decode, or restore original order without replaying the whole archive.",
                anchor: "format/fqc_reader + pipeline/decompressor"
            }
        ],
        legend: [
            {
                title: "Why the block matters",
                body: "The block is the smallest unit that still carries compression leverage, checksum scope, and direct lookup."
            },
            {
                title: "Where integrity lives",
                body: "Checksums and verify flows live at the archive boundary, which keeps retrieval semantics inspectable."
            },
            {
                title: "Where to continue",
                body: "Read pipeline for concurrency and flow control, then format and random access for the archive contract."
            }
        ],
        architectureHref: "/en/architecture/",
        architectureLink: "Open system design"
    },
    zh: {
        kicker: "系统蓝图",
        title: "整个归档是由一串显式契约拼起来的。",
        summary:
            "只要每个阶段的边界足够清晰，fq-compressor 就更容易被审计：输入、分析、block 级变换、归档落盘，以及选择性检索。",
        stages: [
            {
                serial: "01",
                label: "输入",
                detail: "FASTQ 以及压缩 FASTQ 流，经由 parser 和流适配器进入系统。",
                anchor: "io/fastq_parser + io/compressed_stream"
            },
            {
                serial: "02",
                label: "分析",
                detail: "全局统计负责建立重排意图、chunk 切分与内存纪律。",
                anchor: "algo/global_analyzer + common/memory_budget"
            },
            {
                serial: "03",
                label: "压缩",
                detail: "block 级变换把序列、ID、质量值拆分给不同编码器处理。",
                anchor: "algo/block_compressor + quality/id streams"
            },
            {
                serial: "04",
                label: "落盘",
                detail: "FQC writer 负责写出 blocks、校验和、重排元数据以及后续检索要用的索引。",
                anchor: "format/fqc_writer + format/index tables"
            },
            {
                serial: "05",
                label: "检索",
                detail: "读取端可以在不重放整个归档的情况下完成校验、范围解码或原始顺序恢复。",
                anchor: "format/fqc_reader + pipeline/decompressor"
            }
        ],
        legend: [
            {
                title: "为什么 block 是核心边界",
                body: "block 是同时承载压缩收益、校验作用域和直接定位能力的最小单元。"
            },
            {
                title: "完整性写在哪里",
                body: "校验和 verify 流程被放在归档边界，这让检索语义保持可检查。"
            },
            {
                title: "接下来该看哪里",
                body: "先读流水线理解并行与流控，再读格式与随机访问理解归档契约。"
            }
        ],
        architectureHref: "/zh/architecture/",
        architectureLink: "进入系统设计"
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <section class="wp-section wp-section--wide wp-section--atlas">
        <div class="wp-section__split">
            <header class="wp-section__intro">
                <p class="wp-kicker">{{ content.kicker }}</p>
                <h2>{{ content.title }}</h2>
                <p>{{ content.summary }}</p>
                <a class="wp-link" :href="withBase(content.architectureHref)">{{ content.architectureLink }}</a>
            </header>

            <div class="wp-atlas-flow" aria-label="Architecture atlas">
                <div v-for="stage in content.stages" :key="stage.label" class="wp-atlas-flow__node">
                    <p class="wp-atlas-flow__eyebrow">{{ stage.serial }}</p>
                    <h3>{{ stage.label }}</h3>
                    <p>{{ stage.detail }}</p>
                    <p class="wp-atlas-flow__anchor">{{ stage.anchor }}</p>
                </div>
            </div>
        </div>

        <div class="wp-atlas-notes">
            <article v-for="item in content.legend" :key="item.title" class="wp-atlas-notes__item">
                <h3>{{ item.title }}</h3>
                <p>{{ item.body }}</p>
            </article>
        </div>
    </section>
</template>
