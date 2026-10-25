# Design Records

This directory is the **design record** of the Eko language: rejected
alternatives, prior-art comparisons, and the rationale that does not belong in
the normative specification.

**Who needs this:** compiler implementers and language maintainers. Ordinary
users do not; Clauses 1 through 11 and Annexes A through D are the complete
user-facing surface.

**Status of every file here: non-normative.** The rulings live in the
normative clauses; a record defers to its clause and cites it. Where a record
and a normative clause disagree, the clause prevails.

## Index

| Record | Covers | Source topics |
|---|---|---|
| [syntax.md](syntax.md) | Clauses 5, 6 | layout, operators, literals, numeric vocabulary |
| [control.md](control.md) | Clauses 6.3, 6.7, 6.8, 7.7, 8.2, 8.3, 8.8 | bindings, lambdas, branching, patterns, `Never`, iteration |
| [types.md](types.md) | Clauses 7.5, 7.6 | declarations, nominality, composites, generics notation |
| [names.md](names.md) | Clauses 7.2, 7.3, 7.4 | UFCS, structural access, modules, visibility, the standard library |
| [world.md](world.md) | Clauses 7.9, 8.4, 8.9, 9.12 | capabilities, world bit, failure, handles, concurrency |
| [runtime.md](runtime.md) | Clauses 8.5–8.8, 8.10 | representation, acyclicity, memory strategy, monomorphization |
| [builtins.md](builtins.md) | Clauses 9.3–9.9 | text, conversions, derived services, JSON, bytes, time |
| [library.md](library.md) | Clauses 9.10–9.15, Annex B | standard library surface, testing, diagnostics |
| [ffi.md](ffi.md) | Clause 10 | extern, `Pure`, `C.*`, `ctype`, embedding ABI, the capability channel |
| [stdlib.md](stdlib.md) | Clauses 9.1, 9.8, 11, Annex C | library name and delivery, implicit import, operators and equality, derived services, toolchain |

## How to read a record

Each record has three kinds of content:

- **Rejected alternatives** — the option space that was weighed, with the
  reason each losing candidate lost. These are the load-bearing content: the
  specification states what *is*; the record preserves why the rest *is not*.
- **Prior art** — how other languages answer the same question and where Eko
  deliberately departs.
- **Rationale** — arguments and invariants that justify a ruling but would
  bloat the normative text.

Exclusion lists in the normative clauses state the exclusions themselves; the
records here are their evidence base.
