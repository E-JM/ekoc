# Annex C. Standard Library Inventory

This annex is normative. It is the complete inventory of the shipped surface:
the `std` root, the shipped companion `std/time`, and the capability module,
every free function, type, and namespace member, with its signature.
Signatures in this annex are normative. Semantics are owned by Clause 9 and by
the clauses cited in each row, and are cross-referenced rather than restated
here where those clauses state them.

The delivery model, the implicit import, and the companion modules are
specified in Clause 9.1. Companion modules are inventoried in C.13 (Clause
11.5). The shipped companion `std/time` is listed there, and a future shipped
companion is added there. An entry marked **derived** is
compiler-generated. An entry in C.9 is capability-module source or a
compiler-provided declaration (Clauses 9.1.1, 10.8). An entry in C.13 is
companion source or a compiler-supplied row attached to the companion. An
entry whose Clause is 7.5.4, 7.9.1, 8.9, or 10.3 is language or native
vocabulary rather than source. Every other entry is `std` root source (Clause
9.1.1). `Set` and `Range` are compiler-supplied nominal types with intrinsic
member rows (Clause 9.1.1).

**Notation.** Type variables are lowercase: `t`, `u`, `v`, `a`, `b`, `e`, `f`,
and `k`. The world-bit markers `!` and `?` are part of every signature
(Clause 7.9.3). The elided arrow forms `!` and `?` are written `!-> _` and
`?-> _`. Every signature takes its receiver as the first parameter; the member
form `x.f(y)` is the identity `f(x, y)` (Clause 6.5.2). A row MAY name several
members that share one signature.

| Sections | Subject | File |
|---|---|---|
| C.1–C.2 | Free functions; types | [01-surface-types.md](01-surface-types.md) |
| C.3–C.8 | Collections, text, data, numerics | [02-collections-numerics.md](02-collections-numerics.md) |
| C.9 | Capability surface | [03-capabilities.md](03-capabilities.md) |
| C.10–C.12 | JSON; path; not in the library | [04-time-path-exclusions.md](04-time-path-exclusions.md) |
| C.13 | The std/time Companion | [05-companions.md](05-companions.md) |
