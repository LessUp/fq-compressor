# fq-compressor Documentation Website

Modern documentation built with **Next.js 14** + **Nextra 3.x**.

## 🚀 Features

- **Next.js 14** - App Router, React Server Components
- **Nextra 3.x** - Modern documentation framework
- **Tailwind CSS** - Utility-first styling
- **Bilingual** - English and Chinese support
- **Dark Mode** - Automatic light/dark theme
- **Full-text Search** - Built-in search functionality
- **MDX** - Interactive documentation with React components

## 📁 Structure

```
docs/website/
├── pages/              # Documentation pages
│   ├── index.mdx      # Language selection landing
│   ├── en/            # English docs
│   └── zh/            # Chinese docs
├── public/            # Static assets
├── styles/            # Global CSS
├── theme.config.tsx   # Nextra theme config
└── package.json       # Dependencies
```

## 🛠️ Development

```bash
cd docs/website
npm install
npm run dev
```

Open http://localhost:3000

## 📦 Build

```bash
npm run build
# Output: docs/.site/
```

## 📝 Adding Content

### Create New Page

```bash
# English
vi pages/en/category/page-name.mdx

# Chinese
vi pages/zh/category/page-name.mdx
```

### Update Navigation

Edit `_meta.json` in each directory:

```json
{
  "page-name": "Display Title",
  "other-page": "Other Title"
}
```

### MDX Features

```mdx
import { Callout, Tabs, Steps } from 'nextra/components'

<Callout type="info">
  Important information here
</Callout>

<Tabs items={['Unix', 'Windows']}>
  <Tabs.Tab>Unix content</Tabs.Tab>
  <Tabs.Tab>Windows content</Tabs.Tab>
</Tabs>

<Steps>
### Step 1
Content
### Step 2
Content
</Steps>
```

## 🌐 i18n

Language routing is automatic:
- `/` - Language selection
- `/en/*` - English
- `/zh/*` - Chinese

Update `theme.config.tsx` to modify language switcher.

## 🎨 Theming

Customize in `theme.config.tsx` and `styles/globals.css`.

## 📄 License

MIT - Same as fq-compressor project.
