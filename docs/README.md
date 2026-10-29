# Eko Language Specification

**Version:** 0.0.1-draft  

## Abstract

This document specifies the Eko programming language: its lexical structure,
syntax, static semantics, dynamic semantics, standard library, native
interface, and the requirements on conforming implementations. Eko is a small,
statically typed, immutable language that compiles to machine code.

## Document structure

Clauses 1 through 11 and Annexes A through D are **normative**. Annexes E
through G are **informative** and do not form part of the specification. A
clause that is large enough to split is a directory whose `README.md` is the
clause head, with one file per section range; every other clause is one file.
Cross-references use section numbers, so a split never changes a reference.

| Clause | Title | File |
|---|---|---|
| 1 | Scope | [01-scope.md](01-scope.md) |
| 2 | Conformance | [02-conformance.md](02-conformance.md) |
| 3 | Normative References | [03-references.md](03-references.md) |
| 4 | Terms and Definitions | [04-definitions.md](04-definitions.md) |
| 5 | Lexical Structure | [05-lexical/README.md](05-lexical/README.md) |
| 5.1–5.3 | Source text, layout, tokens | [05-lexical/01-source-layout.md](05-lexical/01-source-layout.md) |
| 5.4–5.6 | Comments, identifiers, keywords | [05-lexical/02-names.md](05-lexical/02-names.md) |
| 5.7–5.9 | Numeric, character, string literals | [05-lexical/03-literals.md](05-lexical/03-literals.md) |
| 5.10–5.12 | Operators, delimiter match, exclusions, recovery | [05-lexical/04-operators.md](05-lexical/04-operators.md) |
| 6 | Syntax | [06-syntax/README.md](06-syntax/README.md) |
| 6.1–6.2 | Grammar conventions; compilation units and layout | [06-syntax/01-grammar-layout.md](06-syntax/01-grammar-layout.md) |
| 6.3 | Bindings and declarations | [06-syntax/02-bindings.md](06-syntax/02-bindings.md) |
| 6.4 | Functions and lambdas | [06-syntax/03-functions.md](06-syntax/03-functions.md) |
| 6.5–6.6 | Expressions; composite literals | [06-syntax/04-expressions.md](06-syntax/04-expressions.md) |
| 6.7–6.8 | case and if; patterns | [06-syntax/05-case-patterns.md](06-syntax/05-case-patterns.md) |
| 6.9–6.10 | Type expressions; excluded syntax | [06-syntax/06-types.md](06-syntax/06-types.md) |
| 7 | Static Semantics | [07-static-semantics/README.md](07-static-semantics/README.md) |
| 7.1–7.3 | Program, modules, visibility | [07-static-semantics/01-program.md](07-static-semantics/01-program.md) |
| 7.4 | Names and resolution | [07-static-semantics/02-names.md](07-static-semantics/02-names.md) |
| 7.5–7.6 | Types and generics | [07-static-semantics/03-types.md](07-static-semantics/03-types.md) |
| 7.7 | Patterns and exhaustiveness | [07-static-semantics/04-patterns.md](07-static-semantics/04-patterns.md) |
| 7.8 | Calls and operators | [07-static-semantics/05-calls.md](07-static-semantics/05-calls.md) |
| 7.9–7.10 | Capabilities, world bit, static errors | [07-static-semantics/06-bits.md](07-static-semantics/06-bits.md) |
| 8 | Dynamic Semantics | [08-dynamic-semantics/README.md](08-dynamic-semantics/README.md) |
| 8.1–8.2 | Evaluation; bindings and closures | [08-dynamic-semantics/01-evaluation.md](08-dynamic-semantics/01-evaluation.md) |
| 8.3–8.4 | Control flow; failure | [08-dynamic-semantics/02-control-failure.md](08-dynamic-semantics/02-control-failure.md) |
| 8.5–8.6 | Values and representation; memory management | [08-dynamic-semantics/03-values-memory.md](08-dynamic-semantics/03-values-memory.md) |
| 8.7–8.8 | Generic instantiation; iteration and tail calls | [08-dynamic-semantics/04-generics-iteration.md](08-dynamic-semantics/04-generics-iteration.md) |
| 8.9 | Concurrency | [08-dynamic-semantics/05-concurrency.md](08-dynamic-semantics/05-concurrency.md) |
| 8.10–8.11 | Cost model; excluded observations | [08-dynamic-semantics/06-cost.md](08-dynamic-semantics/06-cost.md) |
| 9 | Standard Library | [09-library/README.md](09-library/README.md) |
| 9.1–9.2 | Delivery and failure taxonomy | [09-library/01-delivery.md](09-library/01-delivery.md) |
| 9.3–9.7 | Scalars, time, and formatting | [09-library/02-scalars.md](09-library/02-scalars.md) |
| 9.8–9.9 | Derived services and JSON | [09-library/03-derived.md](09-library/03-derived.md) |
| 9.10–9.11 | Collections and text | [09-library/04-collections.md](09-library/04-collections.md) |
| 9.12–9.15 | File system, testing, exclusions | [09-library/05-system.md](09-library/05-system.md) |
| 10 | Native Interface | [10-native-interface/README.md](10-native-interface/README.md) |
| 10.1–10.2 | Model; extern declarations | [10-native-interface/01-model-extern.md](10-native-interface/01-model-extern.md) |
| 10.3 | C vocabulary | [10-native-interface/02-c-vocabulary.md](10-native-interface/02-c-vocabulary.md) |
| 10.4–10.6 | Failure at the boundary; callbacks; linking | [10-native-interface/03-boundary.md](10-native-interface/03-boundary.md) |
| 10.7–10.9 | Embedding ABI; capability channel; exclusions | [10-native-interface/04-embedding.md](10-native-interface/04-embedding.md) |
| 11 | Implementation Requirements | [11-implementation.md](11-implementation.md) |
| A | Grammar | [annex-a-grammar/README.md](annex-a-grammar/README.md) |
| A.1–A.2 | Notation; lexical grammar | [annex-a-grammar/01-notation-lexical.md](annex-a-grammar/01-notation-lexical.md) |
| A.3 | Syntactic grammar | [annex-a-grammar/02-syntactic.md](annex-a-grammar/02-syntactic.md) |
| A.4–A.6 | Layout algorithm; precedence; excluded productions | [annex-a-grammar/03-layout-precedence.md](annex-a-grammar/03-layout-precedence.md) |
| B | Diagnostics | [annex-b-diagnostics.md](annex-b-diagnostics.md) |
| C | Standard Library Inventory | [annex-c-inventory/README.md](annex-c-inventory/README.md) |
| C.1–C.2 | Free functions; types | [annex-c-inventory/01-surface-types.md](annex-c-inventory/01-surface-types.md) |
| C.3–C.8 | Collections, text, data, numerics | [annex-c-inventory/02-collections-numerics.md](annex-c-inventory/02-collections-numerics.md) |
| C.9 | Capability surface | [annex-c-inventory/03-capabilities.md](annex-c-inventory/03-capabilities.md) |
| C.10–C.12 | JSON; path; not in the library | [annex-c-inventory/04-time-path-exclusions.md](annex-c-inventory/04-time-path-exclusions.md) |
| C.13 | The std/time Companion | [annex-c-inventory/05-companions.md](annex-c-inventory/05-companions.md) |
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
