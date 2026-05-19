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
    console.warn(`[CitationCluster] Invalid locale: ${props.locale}, expected 'en' or 'zh'`);
}

const copy = {
    en: {
        kicker: "Citation apparatus",
        title: "The public story is backed by papers, repositories, and local evidence anchors.",
        columns: [
            {
                title: "Core literature",
                items: [
                    {
                        code: "[R1]",
                        label: "SPRING paper",
                        note: "Closest paper-level frame for assembly-based compression and reversible reordering."
                    },
                    {
                        code: "[R2]",
                        label: "fqzcomp repository",
                        note: "Quality-value coding reference for stream-specific trade-offs."
                    },
                    {
                        code: "[R3]",
                        label: "NanoSpring paper",
                        note: "Long-read comparator that clarifies what fq-compressor is not tuned for first."
                    }
                ],
                href: "/en/research/references"
            },
            {
                title: "Comparator repositories",
                items: [
                    {
                        code: "[C1]",
                        label: "Spring",
                        note: "Upstream reference for ordering plus consensus-and-delta reasoning."
                    },
                    {
                        code: "[C2]",
                        label: "HARC",
                        note: "FASTQ-specialized comparator for architecture and scope choices."
                    },
                    {
                        code: "[C3]",
                        label: "fqzcomp",
                        note: "Compact quality model used as a pressure test for stream design."
                    }
                ],
                href: "/en/research/open-source-comparative-study"
            },
            {
                title: "Repository evidence",
                items: [
                    {
                        code: "[E1]",
                        label: "benchmark/results/",
                        note: "Tracked machine-readable and narrative benchmark artifacts."
                    },
                    {
                        code: "[E2]",
                        label: "openspec/specs/",
                        note: "Live requirements and capability boundaries."
                    },
                    {
                        code: "[E3]",
                        label: "vendor/spring-core/",
                        note: "License-bounded extracted reference code used for study."
                    }
                ],
                href: "/en/research/"
            }
        ],
        linkLabel: "Continue reading"
    },
    zh: {
        kicker: "引文系统",
        title: "公开叙事背后必须站着论文、仓库和本地证据锚点。",
        columns: [
            {
                title: "核心文献",
                items: [
                    {
                        code: "[R1]",
                        label: "SPRING 论文",
                        note: "最接近 assembly-based compression 与可逆重排框架的论文来源。"
                    },
                    {
                        code: "[R2]",
                        label: "fqzcomp 仓库",
                        note: "质量值编码取舍的重要外部参照。"
                    },
                    {
                        code: "[R3]",
                        label: "NanoSpring 论文",
                        note: "帮助说明 fq-compressor 没有优先围绕长读长场景优化。"
                    }
                ],
                href: "/zh/research/references"
            },
            {
                title: "对照仓库",
                items: [
                    {
                        code: "[C1]",
                        label: "Spring",
                        note: "read ordering 与 consensus-and-delta 推理最重要的上游参考。"
                    },
                    {
                        code: "[C2]",
                        label: "HARC",
                        note: "适合比较 FASTQ 专用压缩器的架构与范围。"
                    },
                    {
                        code: "[C3]",
                        label: "fqzcomp",
                        note: "质量值建模是否值得独立成流的重要对照物。"
                    }
                ],
                href: "/zh/research/open-source-comparative-study"
            },
            {
                title: "仓库证据",
                items: [
                    {
                        code: "[E1]",
                        label: "benchmark/results/",
                        note: "已跟踪的机器可读与叙事型 benchmark 产物。"
                    },
                    {
                        code: "[E2]",
                        label: "openspec/specs/",
                        note: "当前有效的需求与能力边界。"
                    },
                    {
                        code: "[E3]",
                        label: "vendor/spring-core/",
                        note: "用于研究的提取参考代码和许可边界。"
                    }
                ],
                href: "/zh/research/"
            }
        ],
        linkLabel: "继续阅读"
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <section class="wp-section wp-section--wide">
        <header class="wp-section__intro">
            <p class="wp-kicker">{{ content.kicker }}</p>
            <h2>{{ content.title }}</h2>
        </header>

        <div class="wp-citation-cluster">
            <article v-for="column in content.columns" :key="column.title" class="wp-citation-cluster__column">
                <h3>{{ column.title }}</h3>
                <ul>
                    <li v-for="item in column.items" :key="item.code">
                        <span class="wp-citation-cluster__code">{{ item.code }}</span>
                        <div>
                            <strong>{{ item.label }}</strong>
                            <p>{{ item.note }}</p>
                        </div>
                    </li>
                </ul>
                <a class="wp-link" :href="withBase(column.href)">{{ content.linkLabel }}</a>
            </article>
        </div>
    </section>
</template>
