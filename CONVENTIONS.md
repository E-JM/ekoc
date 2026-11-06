# ekoc coding conventions

English everywhere: code, identifiers, comments, diagnostics, commits.
ARCHITECTURE.md fixes the layering, this file the style; the spec governs
where they conflict.

## Code

- One public header per stage, holding only the output type and the entry
  point (ARCHITECTURE rule 2); everything else is `static`. A stage
  includes only `src/support/` and earlier stages' public headers: a
  violation is a bug even if it compiles.
- Soft limits: 50 lines per function, 400 per file. One function does one
  thing; guards over nesting; early returns over arrow code.
- Arenas only: no `malloc`, no object frees in stages (rule 5), though
  `support/` implements them. No stage aborts, exits, or panics —
  failures are diagnostics and the pipeline drains (rule 4).

## Comments

- A public header gives each function's contract in a few lines:
  inputs, ownership, failure.
- Say what the code does and why in its own terms: no clause, annex, or
  table numbers and no spec quotations. A diagnostic string may cite a
  clause where the suite demands it; a comment never does.
- No stage or milestone labels, no history, no fix narration, no notes to
  reviewers, no reference labels: history lives in git, the code states
  what holds.
- No banners, no commented-out code, no per-file license headers.

## Tests

- Every unit ships tests under `tests/unit/<unit>/`, run by `make test`;
  the harness is a header in `tests/`, no external framework. A failing
  test fails the build, and nothing is skipped, stubbed, or faked.
- Conformance and negative suites (Annex D) run only through the real
  pipeline, never unit-tested in bulk.

## Git

- One logical change per commit; every commit builds and passes
  `make test`.
- Subject `<area>: <imperative summary>` under 50 chars; areas:
  `support` `lex` `layout` `parse` `resolve` `check` `lower` `emit` `cli`
  `rt` `cap` `lib` `build` `test` `docs` `spec` `conv`. Body paragraphs
  are one English sentence each, saying why, not what.
