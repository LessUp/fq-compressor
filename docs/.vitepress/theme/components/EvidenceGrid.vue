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

const copy = {
    en: {
        title: "The claim is not a ratio, it is a system contract.",
        summary:
            "fq-compressor should be read as a coupled design: compression density, decode speed, indexed access, and repository-verifiable evidence need to hold together.",
        cards: [
            {
                value: "3.97×",
                title: "Archive density",
                body: "Public positioning centers on short-read Illumina compression, but the number only matters when paired with method and provenance.",
                href: "/en/benchmarks/",
                link: "Trace the evidence"
            },
            {
                value: "11.9 MB/s",
                title: "Compression throughput",
                body: "The write path is treated as a pipeline concern, not a benchmark footnote.",
                href: "/en/architecture/pipeline",
                link: "Inspect the pipeline"
            },
            {
                value: "62.3 MB/s",
                title: "Read-back speed",
                body: "Decompression is part of the product promise because retrieval cost decides whether archives remain practical.",
                href: "/en/whitepaper/",
                link: "Read the whitepaper"
            },
            {
                value: "O(1)",
                title: "Random access",
                body: "The FQC format is shaped so block lookup survives without full expansion.",
                href: "/en/architecture/format-random-access",
                link: "Study the format"
            }
        ]
    },
    zh: {
        title: "真正的主张不是压缩率，而是一份系统契约。",
        summary:
            "fq-compressor 应被理解为耦合设计：压缩密度、解码速度、索引访问与仓库可追溯证据必须同时成立。",
        cards: [
            {
                value: "3.97×",
                title: "归档密度",
                body: "当前公开定位强调短读长 Illumina 压缩，但数字只有和方法、出处一起看才有意义。",
                href: "/zh/benchmarks/",
                link: "追踪证据链"
            },
            {
                value: "11.9 MB/s",
                title: "压缩吞吐",
                body: "写入路径被当作流水线设计问题，而不是 benchmark 尾注。",
                href: "/zh/architecture/pipeline",
                link: "查看流水线"
            },
            {
                value: "62.3 MB/s",
                title: "回读速度",
                body: "解压是产品承诺的一部分，因为检索成本决定归档是否真的可用。",
                href: "/zh/whitepaper/",
                link: "阅读白皮书"
            },
            {
                value: "O(1)",
                title: "随机访问",
                body: "FQC 格式围绕 block 定位塑形，不要求先全量展开。",
                href: "/zh/architecture/format-random-access",
                link: "研究格式"
            }
        ]
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <section class="wp-section wp-section--wide">
        <div class="evidence-grid">
            <header class="wp-section__intro">
                <h2>{{ content.title }}</h2>
                <p>{{ content.summary }}</p>
            </header>
            <article
                v-for="card in content.cards"
                :key="card.title"
                class="evidence-grid__card"
            >
                <p class="evidence-grid__value">{{ card.value }}</p>
                <h3>{{ card.title }}</h3>
                <p>{{ card.body }}</p>
                <a :href="withBase(card.href)">{{ card.link }}</a>
            </article>
        </div>
    </section>
</template>
