import type { Theme } from "vitepress";
import DefaultTheme from "vitepress/theme";
import KnowledgeMap from "./components/KnowledgeMap.vue";
import MetricStrip from "./components/MetricStrip.vue";
import TopicCardGrid from "./components/TopicCardGrid.vue";
import "./style.css";

export default {
    extends: DefaultTheme,
    enhanceApp(ctx) {
        DefaultTheme.enhanceApp?.(ctx);
        ctx.app.component("MetricStrip", MetricStrip);
        ctx.app.component("KnowledgeMap", KnowledgeMap);
        ctx.app.component("TopicCardGrid", TopicCardGrid);
    }
} satisfies Theme;
