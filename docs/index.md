<script setup>
import { withBase } from 'vitepress'
import { onMounted } from 'vue'

onMounted(() => {
  const target = navigator.language?.toLowerCase().startsWith('zh') ? 'zh/' : 'en/'
  window.location.replace(`${import.meta.env.BASE_URL}${target}`)
})
</script>

# Redirecting...

If you are not redirected automatically, continue to <a :href="withBase('/en/')">English</a> or
<a :href="withBase('/zh/')">简体中文</a>.
