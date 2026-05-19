<script setup>
import { withBase } from 'vitepress'
import { onMounted } from 'vue'

onMounted(() => {
  const lang = navigator.language?.toLowerCase() || ''
  const isZh = lang.startsWith('zh') || lang === 'zh-hans' || lang === 'zh-hant'
  const target = isZh ? 'zh/' : 'en/'
  window.location.replace(`${import.meta.env.BASE_URL}${target}`)
})
</script>

# Redirecting...

If you are not redirected automatically, continue to <a :href="withBase('/en/')">English</a> or
<a :href="withBase('/zh/')">简体中文</a>.
