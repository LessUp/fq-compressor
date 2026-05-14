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
        kicker: "System profile",
        title: "Reference metrics, but kept in engineering context",
        summary:
            "fq-compressor is presented here as a measurable system: ratio, throughput, indexed access, and pipeline design are treated as connected evidence.",
        linkLabel: "Review benchmark framing",
        linkHref: "/en/benchmarks/",
        metrics: [
            {
                value: "3.97×",
                label: "Compression ratio",
                detail: "Current public positioning for Illumina datasets"
            },
            {
                value: "11.9 MB/s",
                label: "Compression throughput",
                detail: "Multithreaded compression figure tracked in project materials"
            },
            {
                value: "62.3 MB/s",
                label: "Decompression throughput",
                detail: "Read-back speed matters as much as archival density"
            },
            {
                value: "O(1)",
                label: "Random access",
                detail: "Archive layout is designed for indexed retrieval without full expansion"
            }
        ]
    },
    zh: {
        kicker: "系统画像",
        title: "指标是入口，但必须放回工程语境里阅读",
        summary:
            "这里把 fq-compressor 视为可度量的工程系统：压缩率、吞吐、随机访问与流水线结构需要一起成立，而不是孤立的宣传数字。",
        linkLabel: "查看基准测试框架",
        linkHref: "/zh/benchmarks/",
        metrics: [
            {
                value: "3.97×",
                label: "压缩率",
                detail: "当前公开定位中的 Illumina 数据集结果"
            },
            {
                value: "11.9 MB/s",
                label: "压缩吞吐",
                detail: "项目材料中跟踪的多线程压缩速度"
            },
            {
                value: "62.3 MB/s",
                label: "解压吞吐",
                detail: "回读速度与归档密度同样重要"
            },
            {
                value: "O(1)",
                label: "随机访问",
                detail: "归档格式围绕无需全量展开的索引检索设计"
            }
        ]
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <div class="portal-shell">
        <section class="portal-section metric-strip" :aria-label="content.title">
            <div class="portal-section__header">
                <div>
                    <p class="portal-kicker">{{ content.kicker }}</p>
                    <h2>{{ content.title }}</h2>
                </div>
                <p class="metric-strip__summary">
                    {{ content.summary }}
                    <a :href="withBase(content.linkHref)">{{ content.linkLabel }}</a>
                </p>
            </div>
            <div class="metric-strip__grid">
                <article v-for="metric in content.metrics" :key="metric.label" class="metric-strip__item">
                    <p class="metric-strip__value">{{ metric.value }}</p>
                    <h3>{{ metric.label }}</h3>
                    <p>{{ metric.detail }}</p>
                </article>
            </div>
        </section>
    </div>
</template>
