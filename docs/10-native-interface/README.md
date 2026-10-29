# 10. Native Interface

This clause specifies the native interface: the `extern` declaration, the C
vocabulary admitted in extern signatures, the rules for linking and symbols,
the embedding ABI, and the channel through which the capability module reaches
the operating system. It also specifies the conditions under which a program
leaves the language guarantee (Clause 2.6). The world bit is specified in
Clause 7.9, value representation and memory management in Clauses 8.5 and 8.6,
and standard-library delivery in Clause 9.1; this clause does not restate those
rules.

| Sections | Subject | File |
|---|---|---|
| 10.1–10.2 | Model; extern declarations | [01-model-extern.md](01-model-extern.md) |
| 10.3 | C vocabulary | [02-c-vocabulary.md](02-c-vocabulary.md) |
| 10.4–10.6 | Failure at the boundary; callbacks; linking | [03-boundary.md](03-boundary.md) |
| 10.7–10.9 | Embedding ABI; capability channel; exclusions | [04-embedding.md](04-embedding.md) |
