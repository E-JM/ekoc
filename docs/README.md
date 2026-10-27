# Eko Language Specification

**Version:** 0.0.1-draft  

## Abstract

This document specifies the Eko programming language: its lexical structure,
syntax, static semantics, dynamic semantics, standard library, native
interface, and the requirements on conforming implementations. Eko is a small,
statically typed, immutable language that compiles to machine code.

## Document structure

Clauses 1 through 11 and Annexes A through D are **normative**. Annexes E
through G are **informative** and do not form part of the specification.

| Clause | Title | File |
|---|---|---|
| 1 | Scope | [01-scope.md](01-scope.md) |
| 2 | Conformance | [02-conformance.md](02-conformance.md) |
| 3 | Normative References | [03-references.md](03-references.md) |
| 4 | Terms and Definitions | [04-definitions.md](04-definitions.md) |
| 5 | Lexical Structure | [05-lexical.md](05-lexical.md) |
| 6 | Syntax | [06-syntax.md](06-syntax.md) |
| 7 | Static Semantics | [07-static-semantics.md](07-static-semantics.md) |
| 8 | Dynamic Semantics | [08-dynamic-semantics.md](08-dynamic-semantics.md) |
| 9 | Standard Library | [09-library.md](09-library.md) |
| 10 | Native Interface | [10-native-interface.md](10-native-interface.md) |
| 11 | Implementation Requirements | [11-implementation.md](11-implementation.md) |
| A | Grammar | [annex-a-grammar.md](annex-a-grammar.md) |
| B | Diagnostics | [annex-b-diagnostics.md](annex-b-diagnostics.md) |
| C | Standard Library Inventory | [annex-c-inventory.md](annex-c-inventory.md) |
| D | Conformance Suite | [annex-d-conformance.md](annex-d-conformance.md) |
| E | Design Rationale | [annex-e-rationale.md](annex-e-rationale.md) |
| F | Chinese Terminology | [annex-f-glossary-zh.md](annex-f-glossary-zh.md) |
| G | Bibliography | [annex-g-bibliography.md](annex-g-bibliography.md) |

## Design record

[archive/](archive/) is the non-normative design record. It preserves rejected
alternatives, prior art, and rationale: the reasons the language is as it is,
and the reasons it is not otherwise. The normative clauses state **what** the
language is; the design record states **why**. Where the two disagree, the
normative clauses prevail.
