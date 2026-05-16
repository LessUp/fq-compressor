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
        title: "Architecture atlas",
        summary:
            "The site is organized around the same model as the codebase: parsing, analysis, block-local transforms, archive semantics, and targeted retrieval.",
        stages: [
            { label: "Ingest", detail: "FASTQ, FASTQ.gz, FASTQ.bz2, FASTQ.xz" },
            { label: "Analyze", detail: "global statistics, reorder planning, memory budget" },
            { label: "Compress", detail: "ABC, SCM, ID and quality transforms" },
            { label: "Store", detail: "FQC blocks, index, checksums, reorder metadata" },
            { label: "Retrieve", detail: "range decode, verify, original-order restore" }
        ],
        bullets: [
            {
                title: "Why the block matters",
                body: "The block is the unit that makes throughput, checksum scope, and random access compatible."
            },
            {
                title: "Where to continue",
                body: "Read the pipeline notes for flow control, then the format notes for indexed extraction."
            }
        ],
        architectureHref: "/en/architecture/",
        architectureLink: "Open architecture section"
    },
    zh: {
        title: "架构总览图",
        summary:
            "整个站点按与代码库相同的模型组织：解析、分析、block 级变换、归档语义，以及定向检索。",
        stages: [
            { label: "输入", detail: "FASTQ、FASTQ.gz、FASTQ.bz2、FASTQ.xz" },
            { label: "分析", detail: "全局统计、重排规划、内存预算" },
            { label: "压缩", detail: "ABC、SCM、ID 与质量值变换" },
            { label: "存储", detail: "FQC blocks、索引、校验和、重排元数据" },
            { label: "检索", detail: "范围解码、校验、原始顺序恢复" }
        ],
        bullets: [
            {
                title: "为什么 block 是核心边界",
                body: "block 让吞吐、校验作用域与随机访问在同一套设计里兼容。"
            },
            {
                title: "接下来该看哪里",
                body: "先读流水线，理解流控与并行，再读格式章节理解索引提取。"
            }
        ],
        architectureHref: "/zh/architecture/",
        architectureLink: "进入架构部分"
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <section class="wp-section">
        <div class="wp-section__split">
            <header class="wp-section__intro">
                <h2>{{ content.title }}</h2>
                <p>{{ content.summary }}</p>
                <a class="wp-link" :href="withBase(content.architectureHref)">{{ content.architectureLink }}</a>
            </header>

            <div class="atlas-flow" aria-label="Architecture atlas">
                <div v-for="stage in content.stages" :key="stage.label" class="atlas-flow__node">
                    <h3>{{ stage.label }}</h3>
                    <p>{{ stage.detail }}</p>
                </div>
            </div>
        </div>

        <div class="atlas-notes">
            <article v-for="item in content.bullets" :key="item.title" class="atlas-notes__item">
                <h3>{{ item.title }}</h3>
                <p>{{ item.body }}</p>
            </article>
        </div>
    </section>
</template>
