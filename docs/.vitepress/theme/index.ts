import type { Theme } from "vitepress";
import DefaultTheme from "vitepress/theme";
import ArchitectureAtlas from "./components/ArchitectureAtlas.vue";
import CitationCluster from "./components/CitationCluster.vue";
import EvidenceGrid from "./components/EvidenceGrid.vue";
import ReadingTracks from "./components/ReadingTracks.vue";
import "./style.css";

export default {
    extends: DefaultTheme,
    enhanceApp(ctx) {
        DefaultTheme.enhanceApp?.(ctx);
        ctx.app.component("EvidenceGrid", EvidenceGrid);
        ctx.app.component("ArchitectureAtlas", ArchitectureAtlas);
        ctx.app.component("ReadingTracks", ReadingTracks);
        ctx.app.component("CitationCluster", CitationCluster);
    }
} satisfies Theme;
