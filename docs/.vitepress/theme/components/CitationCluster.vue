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
        title: "Research apparatus",
        columns: [
            {
                title: "References",
                items: [
                    "SPRING for assembly-based compression framing",
                    "fqzcomp for quality-value modeling trade-offs",
                    "NanoSpring for long-read comparison context"
                ],
                href: "/en/research/references"
            },
            {
                title: "Comparative study",
                items: [
                    "What fq-compressor keeps from prior art",
                    "Which assumptions it rejects",
                    "Where code-local evidence still limits public claims"
                ],
                href: "/en/research/open-source-comparative-study"
            },
            {
                title: "Evolution notes",
                items: [
                    "Why closeout mode changes documentation priorities",
                    "How simplification beats surface-area growth",
                    "What remains intentionally deferred"
                ],
                href: "/en/research/evolution-notes"
            }
        ],
        linkLabel: "Continue reading"
    },
    zh: {
        title: "研究附录",
        columns: [
            {
                title: "参考文献",
                items: [
                    "SPRING 作为 assembly-based compression 的问题框架",
                    "fqzcomp 作为质量值建模取舍样本",
                    "NanoSpring 作为长读长方向的对照背景"
                ],
                href: "/zh/research/references"
            },
            {
                title: "对照研究",
                items: [
                    "fq-compressor 从前人工作里保留了什么",
                    "它明确放弃了哪些假设",
                    "哪些公开主张仍受仓库证据边界约束"
                ],
                href: "/zh/research/open-source-comparative-study"
            },
            {
                title: "演进思考",
                items: [
                    "为什么 closeout mode 改变了文档重点",
                    "为什么简化优先于表面扩张",
                    "哪些内容被刻意延后"
                ],
                href: "/zh/research/evolution-notes"
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
            <h2>{{ content.title }}</h2>
        </header>

        <div class="citation-cluster">
            <article v-for="column in content.columns" :key="column.title" class="citation-cluster__column">
                <h3>{{ column.title }}</h3>
                <ul>
                    <li v-for="item in column.items" :key="item">{{ item }}</li>
                </ul>
                <a class="wp-link" :href="withBase(column.href)">{{ content.linkLabel }}</a>
            </article>
        </div>
    </section>
</template>
