# Design Records

This directory is the **design record** of the Eko language: rejected
alternatives, prior-art comparisons, and the deeper rationale that does not
belong in the normative reference. It is restructured from the original
Chinese ruling archive (`legacy/`).

**Who needs this:** compiler implementers and language maintainers. Ordinary
users do not — the eleven chapters are the complete user-facing surface.

**Status of every file here: non-normative.** The rulings live in the
chapters; a record defers to its chapter and cites it. Where an early ruling
was later revised while writing the reference (module paths, `main`'s return
type, the `Pure` declaration scope, `0x`/`0b` literals, the warning channel),
the record follows the current reference; the chapter always wins.

## Index

| Record | Covers chapter | Source topics |
|---|---|---|
| [syntax.md](syntax.md) | 02-syntax.md | layout, operators, literals, numeric vocabulary |
| [control.md](control.md) | 03-control.md | bindings, lambdas, branching, patterns, `Never`, iteration |
| [types.md](types.md) | 04-types.md | declarations, nominality, composites, generics notation |
| [names.md](names.md) | 05-names.md | UFCS, structural access, modules, visibility, prelude |
| [world.md](world.md) | 06-world.md | capabilities, world bit, failure, handles, concurrency |
| [runtime.md](runtime.md) | 07-runtime.md | representation, acyclicity, memory strategy, monomorphization |
| [builtins.md](builtins.md) | 08-builtins.md | text, conversions, derived services, JSON, bytes, time |
| [library.md](library.md) | 09-library.md | standard library surface, testing, diagnostics |
| [ffi.md](ffi.md) | 11-ffi.md | extern, `Pure`, `C.*`, `ctype`, embedding ABI, prelude channel |

## How to read a record

Each record has three kinds of content:

- **Rejected alternatives** — the option space that was weighed, with the
  reason each losing candidate lost. These are the load-bearing content:
  the reference states what *is*; the record preserves why the rest *is not*.
- **Prior art** — how other languages answer the same question and where
  Eko deliberately departs.
- **Rationale** — arguments and invariants that justify a ruling but would
  bloat the normative text.

Exclusion lists in the chapters state the exclusions themselves; the records
here are their evidence base.
