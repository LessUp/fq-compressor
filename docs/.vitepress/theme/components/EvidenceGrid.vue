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
    console.warn(`[EvidenceGrid] Invalid locale: ${props.locale}, expected 'en' or 'zh'`);
}

const copy = {
    en: {
        kicker: "Performance ledger",
        title: "Public claims stay attached to method, artifacts, and retrieval cost.",
        summary:
            "This portal does not separate headline numbers from archive semantics. Every metric is paired with the subsystem, methodology, or repository artifact that makes it auditable.",
        cards: [
            {
                serial: "01",
                value: "3.97x",
                title: "Archive density",
                body: "Compression ratio is presented as a bounded result, not as a floating claim detached from dataset scope.",
                facts: ["ERR091571 smoke-scale artifact", "Public benchmark report in repo"],
                href: "/en/benchmarks/",
                link: "Trace the evidence"
            },
            {
                serial: "02",
                value: "11.9 MB/s",
                title: "Write-path throughput",
                body: "Compression speed is interpreted through the pipeline, chunking, and backpressure design, not as an isolated stopwatch number.",
                facts: ["Pipeline topology", "Block-local work scheduling"],
                href: "/en/architecture/pipeline",
                link: "Inspect the pipeline"
            },
            {
                serial: "03",
                value: "62.3 MB/s",
                title: "Read-back speed",
                body: "Decode speed stays in scope because random access is only useful if retrieval remains practical under real archive workflows.",
                facts: ["Decompression path", "Original-order restore boundary"],
                href: "/en/whitepaper/",
                link: "Read the algorithm brief"
            },
            {
                serial: "04",
                value: "O(1)",
                title: "Random access",
                body: "Indexed lookup is treated as a first-class contract. The format and block map are part of the public thesis, not an implementation footnote.",
                facts: ["FQC block index", "Range decode without full expansion"],
                href: "/en/architecture/format-random-access",
                link: "Study the format"
            }
        ],
        introHref: "/en/benchmarks/methodology",
        introLink: "Open methodology"
    },
    zh: {
        kicker: "性能账本",
        title: "所有公开主张都要和方法、产物、检索成本绑在一起。",
        summary:
            "这个站点不会把 headline 数字和归档语义拆开看。每个指标都会连回某个子系统、方法学约束，或仓库中的可追溯产物。",
        cards: [
            {
                serial: "01",
                value: "3.97x",
                title: "归档密度",
                body: "压缩比被当作有边界的结果来呈现，而不是脱离数据集范围自由漂浮的口号。",
                facts: ["ERR091571 smoke-scale 产物", "仓库内公开 benchmark 报告"],
                href: "/zh/benchmarks/",
                link: "追踪证据链"
            },
            {
                serial: "02",
                value: "11.9 MB/s",
                title: "压缩吞吐",
                body: "压缩速度必须放回流水线、chunk 切分和背压设计中理解，而不是单独的计时数字。",
                facts: ["流水线拓扑", "block 级并行调度"],
                href: "/zh/architecture/pipeline",
                link: "查看流水线"
            },
            {
                serial: "03",
                value: "62.3 MB/s",
                title: "回读速度",
                body: "解压速度仍被放进主叙事，因为只有当检索代价可接受时，随机访问才真正有意义。",
                facts: ["解压路径", "原始顺序恢复边界"],
                href: "/zh/whitepaper/",
                link: "阅读算法综述"
            },
            {
                serial: "04",
                value: "O(1)",
                title: "随机访问",
                body: "索引定位被视为一等契约。格式和 block map 是公开论证的一部分，不是实现尾注。",
                facts: ["FQC block 索引", "无需全量展开的范围解码"],
                href: "/zh/architecture/format-random-access",
                link: "研究格式"
            }
        ],
        introHref: "/zh/benchmarks/methodology",
        introLink: "查看方法学"
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <section class="wp-section wp-section--wide">
        <div class="wp-evidence-grid">
            <header class="wp-section__intro wp-evidence-grid__intro">
                <p class="wp-kicker">{{ content.kicker }}</p>
                <h2>{{ content.title }}</h2>
                <p>{{ content.summary }}</p>
                <a class="wp-link" :href="withBase(content.introHref)">{{ content.introLink }}</a>
            </header>
            <article
                v-for="card in content.cards"
                :key="card.title"
                class="wp-evidence-grid__card"
            >
                <div class="wp-evidence-grid__header">
                    <span class="wp-evidence-grid__serial">{{ card.serial }}</span>
                    <p class="wp-evidence-grid__value">{{ card.value }}</p>
                </div>
                <h3>{{ card.title }}</h3>
                <p>{{ card.body }}</p>
                <ul class="wp-evidence-grid__facts">
                    <li v-for="fact in card.facts" :key="fact">{{ fact }}</li>
                </ul>
                <a :href="withBase(card.href)">{{ card.link }}</a>
            </article>
        </div>
    </section>
</template>
