# ekoc coding conventions

English everywhere: code, identifiers, comments, diagnostics, commit
messages. These rules bind all C in this repository. ARCHITECTURE.md fixes
the layering; this file fixes the style. Where a spec clause and this file
conflict, the spec governs.

## Build

- C99, one compiler (`cc`), one build tool (`make`). No other toolchain
  dependency, ever (docs/archive/toolchain.md §1).
- Warning flags are non-negotiable and warnings are errors:
  `-std=c99 -Wall -Wextra -Wpedantic -Werror`.
- Generated tables are committed; regeneration is a tool under `tools/`,
  never a build step.

## Naming

- Files and directories: `snake_case.c/.h`, one domain per file
  (`arena.c`, `diag.c`).
- Cross-module symbols carry the `ek_` prefix; types `ek_arena`, functions
  `ek_arena_new`. `static` file-local names drop the prefix.
- Enumerator and macro constants are uppercase: `EK_SEV_ERROR`.
- Names say what, not how: `ek_diag_emit`, not `ek_diag_do_emit_impl2`.
- Header guards: `EK_<DIR>_<FILE>_H`.

## Structure

- One public header per stage directory, exporting only the output type
  and the entry point (ARCHITECTURE rule 2). Everything else is `static`.
- A stage includes only `src/support/` and public headers of earlier
  stages. A violation is a bug regardless of what compiles.
- Soft limits, split when exceeded: function 50 lines, file 400 lines.
- One function does one thing; guards over nesting; early returns over
  arrow code.

## Memory and errors

- Stages allocate from arenas only; no `malloc` in stage code, no object
  frees. Arenas are freed wholesale (ARCHITECTURE rule 5). `support/`
  itself implements the arenas and may use `malloc` internally.
- No stage aborts, exits, or panics. Failures are diagnostics in the
  diagnostics buffer; the pipeline drains (ARCHITECTURE rule 4).

## Comments

- Comments are rare and state why — a constraint the code cannot show.
  Never what the next line does, never change narration.
- Comments carry no history and no process: no milestone or task
  references, no "was X, now Y", no bug or fix narration, no notes to
  reviewers. The code states what holds; history lives in git.
- No banner comments, no commented-out code, no license headers per file
  (LICENSE covers the tree).

## Tests

- Every unit ships with tests under `tests/unit/<unit>/`, built and run
  by `make test`. The harness is a header in `tests/`; no external test
  framework.
- Tests fail the build when they fail. No skipped, stubbed, or faked
  results; a failing test is fixed before anything new lands.
- Conformance and negative suites (`tests/`, Annex D) are run only by the
  real pipeline; they are never unit-tested in bulk.

## Git

- One logical change per commit; the tree builds and `make test` passes
  at every commit.
- Subject: `<area>: <imperative summary>` under 50 chars. Areas:
  `support`, `lex`, `layout`, `parse`, `resolve`, `check`, `lower`,
  `emit`, `cli`, `rt`, `cap`, `lib`, `build`, `test`, `docs`, `spec`,
  `conv`. Body paragraphs are 1–2 sentences each, English, and explain
  why, not what.
