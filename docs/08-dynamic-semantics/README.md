# 8. Dynamic Semantics

This clause specifies the evaluation of a conforming Eko program. Evaluation
maps a well-typed expression and an environment to a value, or diverges, or
aborts. Static conditions are specified in Clause 7. The classification of
failures is specified in Clause 9.2. The world bit and its typing rules are
specified in Clause 7.9. Examples in this clause are non-normative.

| Sections | Subject | File |
|---|---|---|
| 8.1–8.2 | Evaluation; bindings and closures | [01-evaluation.md](01-evaluation.md) |
| 8.3–8.4 | Control flow; failure | [02-control-failure.md](02-control-failure.md) |
| 8.5–8.6 | Values and representation; memory management | [03-values-memory.md](03-values-memory.md) |
| 8.7–8.8 | Generic instantiation; iteration and tail calls | [04-generics-iteration.md](04-generics-iteration.md) |
| 8.9 | Concurrency | [05-concurrency.md](05-concurrency.md) |
| 8.10–8.11 | Cost model; excluded observations | [06-cost.md](06-cost.md) |
