# 1. Scope

## 1.1 Purpose

This document specifies the Eko programming language. It defines:

1. the lexical structure of Eko source text (Clause 5);
2. the syntax of Eko programs (Clause 6 and Annex A);
3. the static semantics: names, types, capabilities, and the conditions under
   which a program is well formed (Clause 7);
4. the dynamic semantics: evaluation, failure, representation, memory, and
   concurrency (Clause 8);
5. the standard library that every conforming implementation provides
   (Clause 9 and Annex C);
6. the native interface and embedding ABI (Clause 10);
7. the requirements on compilers, hosts, and toolchains (Clause 11).

## 1.2 Application

This document applies to:

- implementations of Eko compilers;
- implementations of Eko hosts, including the command-line host and embedding
  hosts;
- implementations of the Eko standard library;
- authors of Eko programs;
- authors of Eko conformance tests.

## 1.3 Exclusions

This document does not specify:

- the internal design of a compiler, except where an implementation requirement
  is stated normatively;
- the byte-level layout of values, except for `ctype` records (Clause 10);
- the command-line interface of any tool, except where a toolchain contract is
  stated in Clause 11;
- libraries other than the standard library;
- the design rationale for the language. Rationale is recorded in
  [archive/](archive/) and summarized in Annex E.

## 1.4 Conventions

### 1.4.1 Normative language

The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY**
are to be interpreted as described in RFC 2119 and RFC 8174.

- **MUST** and **MUST NOT** state absolute requirements. A conforming
  implementation that does not satisfy them is non-conforming.
- **SHOULD** and **SHOULD NOT** state requirements that may be waived only for
  a documented reason, with the consequences understood.
- **MAY** states an option that an implementation or program is free to take or
  leave.

A statement in the present indicative that defines a term, a syntax, or a
semantic rule is normative even when it does not contain a key word. For
example, "`Bool` has exactly two constructors" is normative.

The following terms have the meanings given in Clause 2:

| Term | Meaning |
|---|---|
| implementation-defined | the implementation chooses, and MUST document the choice |
| unspecified | any conforming choice is permitted; the choice need not be documented |
| undefined behavior | no requirement is placed on the behavior |

### 1.4.2 Grammar notation

Grammars in this document use the following EBNF notation:

| Form | Meaning |
|---|---|
| `name` | a nonterminal |
| `'text'` | a terminal token or literal character sequence |
| `A B` | `A` followed by `B` |
| `A \| B` | `A` or `B` |
| `[A]` | `A` is optional |
| `{A}` | `A` occurs zero or more times |
| `(A)` | grouping |
| `ε` | the empty production |
| `(* ... *)` | a prose comment |

The lexical grammar (Clause 5 and Annex A.1) consumes characters and produces
tokens. The syntactic grammar (Clause 6 and Annex A.2) consumes tokens and
produces syntax trees. The layout algorithm (Clause 6.2 and Annex A.3)
inserts block boundaries between tokens.

A grammar production defines the shape of a construct, not its indentation.
Layout is defined separately and applies to all productions uniformly.

### 1.4.3 Cross-references

Cross-references use the form **Clause X.Y** or **Clause X.Y.Z**, where `X` is
the clause number and `Y` and `Z` are section numbers. The clause table in
[README.md](README.md) maps clauses to files. References to annexes use
**Annex A**, **Annex B**, and so on.

A reference to "this clause" means the smallest enclosing numbered clause.

### 1.4.4 Examples

Code examples are illustrative and are **not normative**. A normative rule is
stated in prose; an example shows one program that the rule admits or rejects.

- Examples that are intended to be valid Eko are marked `eko`.
- Examples that are intended to be rejected are marked with a comment
  `// error:` or `// invalid:` and are explained in the surrounding prose.
- Output examples are marked `text`.
- Grammar fragments are marked `ebnf`.
- C code is marked `c`.

An example that is not marked as invalid is intended to be a valid Eko program,
but the normative text, not the example, determines conformance.

An example may be a complete declaration or a fragment. A clause may state an
**example context**: a block of declarations that the examples in that clause
use. A name not declared in a block comes from that context or from the
standard library, and a block may declare additional local names. The context
is illustrative and is not part of the specification.

### 1.4.5 Terminology

Defined terms are introduced in Clause 4 and are set in **bold** at their
definition site. The same spelling is used throughout the normative clauses.
Annex F provides Chinese translations for readers; the English terms are
authoritative.
