# fq-compressor Documentation Website

This site is the **public landing surface** for `fq-compressor`, not a second README.

## Surface ownership

- `pages/index.mdx` — project landing page with proof points and EN/ZH entry paths
- `pages/en/index.mdx` — English documentation hub
- `pages/zh/index.mdx` — Simplified Chinese documentation hub
- `pages/en/**`, `pages/zh/**` — maintained reference pages linked from the hubs

Keep the landing page concise and conversion-oriented. Put long-form reference material into the
language-specific documentation pages instead of duplicating it on the root page.

## Development

```bash
cd docs/website
npm ci
npm run dev
```

Open <http://localhost:3000>.

## Build

```bash
npm run build
# Output: docs/.site/
```

`docs/.site/` is generated output and must not be treated as source.

## Editing guidance

- Prefer updating existing pages over creating new parallel summaries.
- Keep English and Chinese navigation aligned, but do not mirror large paragraphs without a reason.
- When the project story changes, update the Pages root, README, and GitHub About together.
- When reference details change, update the specific EN/ZH page that owns the topic.
