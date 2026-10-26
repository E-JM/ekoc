# Annex B. Diagnostics

This annex is normative. It specifies the structure and style of compile-time
diagnostics, the vocabularies of error and warning codes, the panic codes, and
the panic output of the CLI host.

A diagnostic is a compile-time report of a condition in a program
(Clause 2.4). Panic output is runtime output and is not a diagnostic
(Clause 9.2.3). Its format is normative in Annex B.6 because the test runner and
users depend on it.

## B.1 Diagnostic Structure

A diagnostic has five components.

| Component | Content | Requirement |
|---|---|---|
| Code | an `E-` or `W-` code | Annex B.3, Annex B.4 |
| Severity | error or warning | Clause 2.4 |
| Message | one line of text | Annex B.2 |
| Detail | zero or more indented lines | this clause |
| Position | a source location | implementation-defined (Clause 2.5) |

The message line has the form `code: message`. Detail lines follow the message
line, are indented by two spaces, and begin with a lowercase label followed by
`: `. The label `hint` is reserved for a repair suggestion.

```text
E-nominal-mismatch: `Person` does not flow into `Human` (both nominal)
  hint: convert via a shared anonymous shape, or rebuild field by field
```

A conforming compiler MUST report every violation of a static rule
(Clause 2.4). It MUST report an error for a violation and a warning for a
condition that this document classifies as a warning. It MUST NOT report an
error with the severity of a warning, and it MUST NOT suppress a warning that
this document requires.

A diagnostic MAY include a source position, additional detail lines, and a
hint (Clause 2.4). The format of a source position is implementation-defined
(Clause 2.5). A diagnostic MAY omit the position when no source location
applies.

The severity vocabulary has three members: error, warning, and none. A
condition that is neither an error nor a warning produces no diagnostic; the
severity none denotes that case and is not the severity of an emitted
diagnostic.

A compiler MAY report additional warnings beyond the vocabulary of Annex B.4; such
warnings are not part of this specification (Clause 2.4). A compiler MUST NOT
report an additional diagnostic that rejects a conforming program
(Clause 2.2).

Bit diagnostics are always errors (Clause 7.9.2). A diagnostic that carries a
code from Annex B.3 or Annex B.4 has the severity of its entry; every other diagnostic for
a static violation has the severity error (Clause 2.4).

A diagnostic is not a runtime report. A panic, an abort, and a native fault are
reported by the mechanisms of Annex B.5 and Annex B.6, not by an `E-` or `W-` code
(Clause 8.4.2, Clause 8.4.3).

## B.2 Message Style

Diagnostic messages are English. A message MUST NOT be translated.

Every diagnostic message satisfies the following rules.

1. The first character is lowercase, unless the message begins with a
   backticked name, in which case it is a backtick.
2. The message has no trailing period.
3. The message states the fact. It does not pass judgment and does not contain
   a slogan.
4. A repair suggestion appears on a `hint:` line, not in the message.
5. Names of types, functions, modules, and bindings are enclosed in backticks.
6. The message occupies exactly one line. It contains no line terminator.
7. A detail line begins with a lowercase label and a colon. The labels `hint`
   and `note` are reserved; an implementation MAY define further labels.
8. The message uses the terminology of Clause 4 and the spellings of
   Clause 7.9.8. Effect-system vocabulary is excluded (Clause 7.9.8).
9. The message identifier of an entry is its code. The wording is not an
   identifier.

```text
E-world-bit: body is world-touching but the declared arrow is pure
  hint: change `->` to `!->`, or remove the world-touching call
```

The wording of an entry is fixed when the entry is published. A later revision
of this document MAY reword a message, but it MUST NOT change the code or the
meaning of the entry (Annex B.3). Documentation, issues, and tests cite the code.

## B.3 Error Codes

An error code has the form `E-` followed by a kebab-case semantic word:
lowercase ASCII letters and hyphens. Numeric codes are not used. Codes are
greppable and stable.

A published code is never reused and never redefined. The code identifies the
condition, not the message. The vocabulary is open to growth: adding an entry
freezes it. This annex is the authority for the specified codes.

A conforming compiler MUST use the code in the table below for the condition
described by the entry. It MUST NOT use a listed code for a different
condition. A static violation for which this annex defines no code is reported
with an error diagnostic that identifies the violated clause (Clause 2.7); the
diagnostic carries an `E-` code chosen by the implementation that does not
collide with a code in the table.

| Code | Condition | Clause |
|---|---|---|
| `E-tab` | a tab character occurs as whitespace | Clause 5.2.3 |
| `E-name-case` | an identifier violates the case class of its name space | Clause 5.5.2 |
| `E-literal-overflow` | a numeric literal lies outside the range of its target type, or a floating literal overflows the finite range | Clause 5.7.1, Clause 5.7.2 |
| `E-cycle-import` | the module dependency graph contains a cycle | Clause 7.2.4 |
| `E-namespace-owner` | a type-namespace member is declared outside the module that owns the type | Clause 7.4.5 |
| `E-name-conflict` | two names conflict at one level, two constructors clash, or two imported module stems collide | Clause 7.4.6 |
| `E-nominal-mismatch` | a nominal type does not flow into a different nominal type | Clause 7.5.2 |
| `E-shape-mismatch` | a nominal type does not have the same shape as the anonymous target | Clause 7.5.5 |
| `E-phantom` | a type variable is phantom or occurs only in the result | Clause 7.6.2, Clause 7.6.5 |
| `E-exhaust` | a `case` expression is non-exhaustive, including a guarded case without a final unguarded arm | Clause 7.7.2, Clause 7.7.3 |
| `E-world-bit` | the declared world bit differs from the bit of the body, in either direction | Clause 7.9.2 |
| `E-open-bit-unfed` | an open bit in a signature has no parameter to feed it | Clause 7.9.6 |
| `E-open-bit-position` | `?` occurs in a position where no binding is generalized: a result type, a container element, or a record field | Clause 7.9.6 |
| `E-instantiation-depth` | generic instantiation does not terminate | Clause 8.7.3 |
| `E-no-eq` | equality, keying, or assertion over a type whose `eq` derivation halts at a capability or a function | Clause 9.8.4 |
| `E-json-shape` | a type has no default JSON isomorphic form, for example a dictionary with a non-string key | Clause 9.9.3 |
| `E-ctype-field` | a `ctype` field is neither a machine word nor a `ctype` record | Clause 10.3.3 |
| `E-extern-type` | an `extern` signature uses a type outside the marshalling whitelist, or contains a type variable or variadic parameter | Clause 10.2.3, Clause 10.3.5 |
| `E-extern-arrow` | an open arrow appears in an extern signature, as the declaration's own arrow or inside a parameter type | Clause 10.2.3 |
| `E-refutable-binding` | a refutable pattern in a value binding | Clause 7.7.1 |
| `E-reserved-symbol` | an `extern` names a reserved symbol | Clause 10.6 |

The table is the first batch of the vocabulary. A code for a condition outside
the table follows the same form and is subject to the same non-reuse rule. An
implementation MUST NOT assign a listed code to a condition outside its entry.

## B.4 Warnings

A warning is a diagnostic that does not reject a program and does not change
its meaning (Clause 2.4). A program that produces only warning diagnostics is a
conforming program.

The specified warning vocabulary is closed and contains exactly four codes.

| Code | Condition | Clause |
|---|---|---|
| `W-unreachable` | code follows an expression whose type is `Never` | Clause 8.4.4 |
| `W-unused` | a binding or an import is unused, counted over all compilation roots | Clause 9.14.6 |
| `W-redundant-import` | an explicit `import std` appears; the implicit import is always present | Clause 9.1.2 |
| `W-deprecated` | a member whose documentation carries `/// @deprecated` is used | Clause 11.5 |

A new `W-` code requires an amendment to this annex. Implementation discretion
does not add one. An implementation MAY report additional warnings outside the
vocabulary (Clause 2.4); such warnings are not part of this specification and
MUST NOT be presented as entries of this annex.

Warnings are not suppressible. There is no allow attribute, no pragma, and no
comment switch. Silencing a warning requires a change to the program: for
`W-unused`, rename the binding with a leading underscore or delete it; for
`W-unreachable`, delete the code or restructure the control flow; for
`W-redundant-import`, delete the explicit import.

The following rules apply to individual warnings.

- `W-unused` counts over every compilation root, test roots included
  (Clause 9.14.6). Parameters do not participate. A top-level binding whose
  name begins with `_` does not participate, and a local binding spelled `_x`
  does not participate (Clause 7.3.1).
- `W-unreachable` applies to code after an expression of type `Never`. A
  `Never` arm of a `case` or an `if` does not by itself trigger the warning
  (Clause 8.4.4).
- `W-redundant-import` applies to an explicit `import std` in any module,
  because every file is compiled as if it began with that import
  (Clause 7.2.5, Clause 9.1.2).
- `W-deprecated` applies to a member whose documentation carries
  `/// @deprecated`. The interface diff between compiler versions is the
  authority for the deprecation (Clause 11.5).

## B.5 Panic Codes

A panic code has the form `P-` followed by a kebab-case semantic word. The
`P-` prefix is the runtime sibling of `E-` and `W-` and follows the same code
discipline: a published code is never reused and never redefined.
Documentation and tests cite the code, not the message.

Panic codes are not diagnostics. A panic is an invariant-violation failure that
aborts the process (Clause 8.4.2). `panic` is pure and holds no capability, so
a panic call sets no world bit (Clause 9.2.3).

| Code | Origin | Message |
|---|---|---|
| `P-panic` | `panic(msg)`, and a standard-library invariant violation without a dedicated code | the argument, verbatim; for a standard-library invariant, the message of the violated invariant |
| `P-unwrap` | `Option.unwrap` on `None`; `Result.unwrap` on `Err` | `Option.unwrap on None` or `Result.unwrap on Err` |
| `P-expect` | `Option.expect`; `Result.expect` | the argument, verbatim |
| `P-assert` | `assert` on `False` | `assertion failed` |
| `P-assert-eq` | `assert_eq` mismatch; `assert_ne` mismatch | `assertion failed`, followed by `left:` and `right:` lines |
| `P-div` | division by zero | `division by zero` |
| `P-overflow` | arithmetic overflow | `arithmetic overflow` |

The message column gives the fixed text of the message. A message supplied by
the program is printed verbatim (Annex B.6). A `P-assert-eq` message carries two
detail lines, `left:` and `right:`, each holding the `inspect` form of one
operand (Clause 9.8.3). `P-overflow` covers `Int.abs(Int.min)` (Clause 9.6.1)
and time arithmetic outside the nanosecond domain (Clause 9.7.5).

A panic raised by a standard-library invariant violation for which the table
defines no dedicated code reports `P-panic` with the message of the violated
invariant. Such origins include an empty separator passed to `String.split`
(Clause 9.11.1), a zero bound passed to `Rand.int` (Annex C.9), and time
arithmetic outside the nanosecond domain (Clause 9.7.5). This default is part
of the `P-panic` entry; it does not create an additional code.

Stack overflow, memory exhaustion, and native faults are aborts, not panics
(Clause 8.4.3). They carry no `P-` code and are reported through the abort
form of Annex B.6.

## B.6 CLI Panic Output

The CLI host writes panic output to standard error. Panic output never flows
through `Out`: `panic` is pure and holds no capability (Clause 8.4.2).
Abort reporting is host behavior, not world contact.

| Element | Requirement |
|---|---|
| Channel | standard error, written by the host |
| First line | `panic [P-code]: message` |
| Message | the style of Annex B.2; a program-supplied message is verbatim |
| Backtrace | always printed, innermost frame first |
| Frame | `  at module.function (file:line:column)` |
| Termination | exit code `1` |
| Abort | `abort: reason` and a platform abort status |

**First line.** The first line is exactly one line of the form
`panic [P-code]: message`. The code is specified in Annex B.5. The message
follows Annex B.2. A program-supplied message, from `panic` or `expect`, is
printed verbatim and MAY span several lines; a line that begins with `at`
after the two-space indentation is a frame line.

**Backtrace.** The backtrace is always printed, with the innermost frame first.
Function names are fully qualified (Clause 7.4.4). Frames eliminated by self
tail calls do not appear (Clause 8.8.3). How frames are captured is
implementation-defined; the frame format is normative, and the textual form of
the backtrace beyond that format is unspecified (Clause 2.5).

```text
panic [P-assert-eq]: assertion failed
  left:  Person {name: "ada", age: 36}
  right: Person {name: "grace", age: 41}
  at person.make_person (person.eko:14:9)
  at std.assert_eq (std/assert.eko:3:5)
```

**Termination.** Under the CLI host, a panic terminates the process with exit
code `1` (Clause 9.2.3). Normal termination uses exit code `0`, and a
compile-time failure uses exit code `2`. An explicit exit code is requested
with `Proc.exit` (Clause 8.4.3).

**Abort class.** Stack overflow and memory exhaustion are aborts, not panics
(Clause 8.4.3, Clause 8.6.4). An abort writes a best-effort single line
`abort: reason` to standard error and terminates with the platform abort
status, which is distinguishable from panic's exit code. A native fault aborts
by the same path (Clause 10.4).

```text
abort: stack overflow
```

**Consumers.** The test runner reads the panic block from the child's standard
error and parses it according to this clause (Clause 9.14.6). Panic output is
observable only as standard error text; it is not a `Result`, not an
exception, and not a value of any Eko type (Clause 9.2.3).
