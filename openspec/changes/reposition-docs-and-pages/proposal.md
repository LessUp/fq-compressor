## Why

The repository documentation and GitHub Pages surface currently compete with each other instead of
forming a clear onboarding path. Closeout work needs a lean bilingual doc set and a landing page that
helps new users understand, install, and evaluate the project quickly.

## What Changes

- Reposition GitHub Pages as the main project landing surface.
- Keep documentation bilingual but intentionally lean.
- Align GitHub About metadata with the final Pages and README story.

## Capabilities

### New Capabilities

- `project-presentation`: defines the landing-page, documentation, and repository-metadata
  requirements for presenting `fq-compressor`.

### Modified Capabilities

- `cli`: clarify which documentation surfaces should link users to installation, quick start, and
  command reference.

## Impact

- `README.md`
- `docs/README.md`
- `docs/website/`
- `.github/workflows/pages.yml`
- GitHub repository description, homepage, and topics
