# Briefing

Use this page as the reading contract for the portal. It defines what fq-compressor is trying to prove in public, how the site is organized, and which route is fastest for different readers.

<div class="wp-grid wp-grid--two">
  <article class="wp-card">
    <p class="wp-kicker">Portal contract</p>
    <ul class="wp-list">
      <li>The site argues for fq-compressor as a systems design, not a standalone benchmark headline.</li>
      <li>Every durable public claim should resolve to architecture, methodology, or tracked repository artifacts.</li>
      <li>The reading order is shaped for advanced GitHub readers who want auditability first.</li>
    </ul>
  </article>
  <article class="wp-card">
    <p class="wp-kicker">What this portal is not</p>
    <ul class="wp-list">
      <li>Not a README mirror</li>
      <li>Not a feature checklist detached from evidence</li>
      <li>Not a frontend-heavy showcase that hides the engineering substance</li>
    </ul>
  </article>
</div>

## Default reading order

1. Read [Algorithms](/en/whitepaper/) to understand the compression thesis.
2. Read [System Design](/en/architecture/) to see where those ideas live in the codebase and archive format.
3. Read [Performance](/en/benchmarks/) to understand what the repository currently proves, and where the proof surface stops.
4. Read [Operations](/en/academy/) if your question is practical: install, run, verify, contribute.
5. Read [References](/en/research/) if your question is bibliographic, comparative, or historical.

## Reader intents

| Reader | Start here | Then read | Main outcome |
| --- | --- | --- | --- |
| Staff-level reviewer | [Algorithms](/en/whitepaper/) | [Performance](/en/benchmarks/) | Judge whether the public story is technically credible |
| Contributor | [System Design](/en/architecture/) | [Operations](/en/academy/) | Understand boundaries before editing |
| Operator | [Operations](/en/academy/) | [System Design](/en/architecture/) | Run and verify archives without guessing format rules |
| Research reader | [References](/en/research/) | [Algorithms](/en/whitepaper/) | Place fq-compressor in the wider FASTQ compression landscape |

## Why this structure exists

fq-compressor is easier to trust when the documentation behaves like a technical dossier. The portal therefore keeps the top-level route shallow, the evidence claims explicit, and the supporting bibliography visible instead of burying it in a side note.

<ReadingTracks locale="en" />
