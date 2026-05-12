#!/usr/bin/env node

const fs = require('fs')
const path = require('path')

function generateSitemap() {
  const baseUrl = 'https://lessup.github.io/fq-compressor'
  const siteDir = path.join(__dirname, '..', '..', '.site')

  const pages = [
    { url: '/', priority: '1.0' },
    { url: '/en', priority: '1.0' },
    { url: '/zh', priority: '1.0' },
    { url: '/en/getting-started/installation', priority: '0.8' },
    { url: '/en/getting-started/quickstart', priority: '0.8' },
    { url: '/en/getting-started/cli-usage', priority: '0.8' },
    { url: '/en/core-concepts/architecture', priority: '0.7' },
    { url: '/en/core-concepts/algorithms', priority: '0.7' },
    { url: '/en/core-concepts/fqc-format', priority: '0.7' },
    { url: '/en/performance/benchmark', priority: '0.6' },
    { url: '/en/development/contributing', priority: '0.5' },
    { url: '/en/reference/faq', priority: '0.5' },
    { url: '/zh/getting-started/installation', priority: '0.8' },
    { url: '/zh/getting-started/quickstart', priority: '0.8' },
    { url: '/zh/getting-started/cli-usage', priority: '0.8' },
    { url: '/zh/core-concepts/architecture', priority: '0.7' },
    { url: '/zh/core-concepts/algorithms', priority: '0.7' },
    { url: '/zh/core-concepts/fqc-format', priority: '0.7' },
    { url: '/zh/performance/benchmark', priority: '0.6' },
    { url: '/zh/development/contributing', priority: '0.5' },
    { url: '/zh/reference/faq', priority: '0.5' },
  ]

  const today = new Date().toISOString().split('T')[0]

  const sitemap = `<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
${pages.map(page => `  <url>
    <loc>${baseUrl}${page.url}</loc>
    <lastmod>${today}</lastmod>
    <changefreq>weekly</changefreq>
    <priority>${page.priority}</priority>
  </url>`).join('\n')}
</urlset>
`

  const sitemapPath = path.join(siteDir, 'sitemap.xml')
  fs.writeFileSync(sitemapPath, sitemap)
  console.log('✓ Generated sitemap.xml')

  // Generate robots.txt
  const robots = `User-agent: *
Allow: /

Sitemap: ${baseUrl}/sitemap.xml
`
  fs.writeFileSync(path.join(siteDir, 'robots.txt'), robots)
  console.log('✓ Generated robots.txt')
}

generateSitemap()
