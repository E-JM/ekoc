# 2. Conformance

## 2.1 Conforming implementation

A **conforming implementation** is an implementation that satisfies every
MUST and MUST NOT requirement of this document and that implements every
normative facility of Clauses 5 through 11 and Annexes A through D.

An implementation MAY provide facilities beyond those specified. Such
facilities MUST NOT change the meaning of a conforming program, and an
implementation MUST NOT require them for a program to conform.

## 2.2 Conforming program

A **conforming program** is a program that satisfies every static rule of this
document and whose execution does not invoke undefined behavior.

A conforming implementation MUST accept every conforming program and MUST
preserve its dynamic semantics as specified in Clause 8.

A conforming implementation MUST reject every program that violates a static
rule. Rejection is reported as a diagnostic (Clause 2.4). A conforming
implementation MUST NOT accept a program whose rejection is required by this
document.

## 2.3 Implementation classes

This document distinguishes four implementation classes.

| Class | Responsibility |
|---|---|
| **Compiler** | parses, checks, and compiles Eko source; reports diagnostics; emits code that satisfies Clause 8 |
| **Host** | starts a program, constructs capabilities, supplies the native surface, implements the embedding ABI, and defines CLI-level behavior where this document permits |
| **Library** | provides the standard library surface of Clause 9 and Annex C |
| **Toolchain** | supplies the tooling services of Clause 11.3, the panic output and exit status of Annex B.6, and the runner of Annex D.6 |

A product MAY combine these classes. The requirements apply to each class
independently: a compiler that emits code for a host that does not satisfy the
host requirements is not, as a product, a conforming implementation. The
Library class's distribution obligation is fixed by Clause 11.1.

## 2.4 Diagnostics

A **diagnostic** is a compile-time report of a condition in a program.

- A conforming compiler MUST report every violation of a static rule.
- A conforming compiler MUST report the diagnostic with the severity required
  by this document: **error** for a violation, **warning** for a condition
  that this document classifies as a warning.
- A diagnostic MUST identify the rule it reports: by a code from Annex B
  where one is specified, and otherwise by the clause of this document that
  the report addresses.
- A diagnostic MAY include additional detail, a hint, and a source position.
  The format of a diagnostic is specified in Annex B.
- A compiler MUST NOT treat an error as a warning, and MUST NOT suppress a
  warning that this document requires.
- A compiler MAY report additional warnings, provided that they do not reject
  a conforming program. Additional warnings are not part of this
  specification.

An error diagnostic reports that the program is not conforming. A program that
produces only warning diagnostics is conforming; warnings do not change its
meaning.

## 2.5 Implementation-defined and unspecified behavior

Where this document says that a behavior is **implementation-defined**, the
implementation chooses the behavior and MUST document the choice. Where it says
that a behavior is **unspecified**, any conforming choice is permitted and the
choice need not be documented.

The following are implementation-defined:

| Behavior | Clause |
|---|---|
| the maximum supported nesting depth of types | 7.5.1, 7.5.7 |
| the error bound of floating-point library functions | 9.13.2 |
| the ordering produced by an inconsistent `sort_by` comparator | 9.10.1, Annex C |
| the exact bytes of a `ctype` record beyond the fields declared | 10.3.3 |
| the mechanism by which a shim is attached to a module | 10.3.6 |
| the representation of opaque values | 8.5.3 |
| the magnitude of a dictionary merge | 8.10 |
| the format of source positions in diagnostics | Annex B |
| how backtrace frames are captured | B.6 |
| the cache directory used by a compiler | 11.2 |
| the result of reading platform metadata such as symbolic links | 9.12.6 |
| the scheduling order of tests | 9.14.6 |

The following are unspecified:

| Behavior | Clause |
|---|---|
| the timing of reclamation beyond the specified guarantee | 8.6.2 |
| whether an atomic reference count is chosen at allocation or by promotion | 8.6.3 |
| the machine code generated for a construct | 8.10 |
| the granularity of stack-overflow diagnostics | 8.8.4 |
| the textual form of a panic backtrace beyond the specified frame format | Annex B |

## 2.6 Undefined behavior

Safe Eko has no undefined behavior. A conforming program that stays within the
language and the standard library cannot corrupt memory, read uninitialized
storage, or observe an invalid value.

The native interface (Clause 10) is the sole source of undefined behavior:

- calling an `extern` whose declaration does not match the native symbol;
- a `Pure` declaration whose target touches the world or is not reentrant;
- a `C.In` declaration whose target retains or writes through the pointer;
- a callback that outlives its call;
- a shim whose C code has undefined behavior.

These conditions are outside the language guarantee. An implementation MAY
detect them, but is not required to.

## 2.7 Conformance suite

Annex D specifies the conformance suite. A conforming implementation SHOULD
pass every test in the suite. The required outcome of each test class, and the
behavior of a runner, are specified in Annex D.

The suite is a reading of this document, not a substitute for it. A test that
conflicts with a normative clause is an error in the suite.
