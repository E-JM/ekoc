# 7. Static Semantics

This clause specifies the static semantics of Eko: the conditions under which
a program is well formed. It defines program and module structure, name
spaces, resolution, visibility, type identity, generic instantiation, pattern
typing, exhaustiveness, call and operator typing, capabilities, and the world
bit.

A program that violates a rule of this clause is rejected by an error
diagnostic (Clause 2.4). Clause 7.10 enumerates the categories of required
rejection; Annex B owns the diagnostic codes. Evaluation is Clause 8. The
failure taxonomy is Clause 9.2 and is only cross-referenced here.

| Sections | Subject | File |
|---|---|---|
| 7.1–7.3 | Program, modules, visibility | [07-program.md](01-program.md) |
| 7.4 | Names and resolution | [07-names.md](02-names.md) |
| 7.5–7.6 | Types and generics | [07-types.md](03-types.md) |
| 7.7 | Patterns and exhaustiveness | [07-patterns.md](04-patterns.md) |
| 7.8 | Calls and operators | [07-calls.md](05-calls.md) |
| 7.9–7.10 | Capabilities, world bit, static errors | [07-bits.md](06-bits.md) |
