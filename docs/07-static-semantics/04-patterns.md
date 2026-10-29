# 7. Static Semantics

## 7.7 Patterns and Exhaustiveness

A pattern is checked against the type of the scrutinee. The pattern grammar is
Clause 6.8; this clause gives the typing and exhaustiveness rules.

### 7.7.1 Pattern Typing

| Pattern | Scrutinee type | Binding |
|---|---|---|
| variable `x` | any `T` | `x: T` |
| wildcard `_` | any `T` | none |
| literal | value-layer `T` | none; literal placed at `T` |
| zero-payload constructor | nominal ADT declaring it | none |
| constructor with payload | nominal ADT; payload type | payload pattern at the payload type |
| tuple `{P, Q, ...}` | tuple of matching arity | component types |
| record `{f}` or `{f: P}` | record with field `f` | field type |
| list `{}`, `{h, ..t}`, `{.., P}`, `{_, ..}`, fixed-length `{P, Q, ...}` | list type | element and list types |
| or `P \| Q` | same `T` | same names at the same types |

A constructor pattern's constructor MUST belong to the scrutinee's nominal ADT
and MUST be visible at the pattern position; a module-qualified constructor is
legal (Clause 7.4.4). A multi-component payload has a tuple type, and the
payload pattern has the corresponding tuple shape (Clause 6.3.5).

A record pattern matches a subset of fields. Every field named in the pattern
MUST be a field of the record type; a misspelling is an error, and unwritten
fields are unconstrained. A tuple pattern MUST have the scrutinee's arity. A
list pattern is checked against the list element type: `{}` is the empty list,
a rest binds the remaining list, and a fixed-length pattern binds elements.
The expected type decides whether a brace pattern is a tuple pattern or a list
pattern, as it decides a composite literal (Clause 6.8.1).

A pattern binder's type is taken from the instantiated scrutinee type, and a
constructor payload type is the instantiated payload (Clause 7.6.4). Within
one pattern, a binding name MUST occur at most once; `_` is exempt, and the
rule applies separately to each or-alternative (Clause 6.8.6). Every
alternative of an or-pattern MUST bind the same set of names at the same types
(Clause 6.8.3).

A literal pattern's literal is placed at the scrutinee type (Clause 5.7.4); an
out-of-range numeric literal is `E-literal-overflow` (Annex B). A literal
pattern is a `String`, an integer, a float, a negative numeric literal, or a
zero-payload constructor (Clause 6.8.5); an interpolated string is not a
pattern. A value binding admits only irrefutable patterns: `_`, a variable, a
tuple or group pattern of irrefutable patterns, and a record pattern whose
subpatterns are irrefutable. Constructor, literal, list, and or-patterns occur
only in `case` arms; a refutable pattern in a value binding is
`E-refutable-binding` (Annex B). Parameter positions admit an identifier, `_`,
or a tuple parameter group, and no other pattern form. Record, constructor,
list, literal, and or-patterns are rejected there; `<-` inherits the
restriction (Clause 7.8.1).

Dictionary patterns, as-patterns, and conjunctive patterns do not exist; a
program MUST NOT use them (Clause 6.8.7). A `case` over several scrutinees has
one pattern per scrutinee; each pattern is checked against its scrutinee's
type, and the arm matches when every pattern matches (Clause 6.7.2). The arm's
patterns bind together (Clause 6.8.6); a name repeated across them is a name
conflict.

### 7.7.2 Exhaustiveness
A `case` MUST be exhaustive; a non-exhaustive `case` is `E-exhaust`
(Annex B). Coverage is judged by patterns alone, so a diverging body neither
adds nor removes it (Clause 7.7.4), and by constructors at every depth rather
than by inhabitance. A `_` or variable pattern covers every value of its type,
and a final all-`_` arm makes the `case` exhaustive.

A scrutinee with case structure is covered as follows. A tuple pattern covers
a tuple of its arity when each component pattern covers its component type. A
record pattern covers a record when every field it names is covered; unwritten
fields are unconstrained, so one record pattern can cover the type. An ADT arm
covers a constructor when its head is that constructor and its payload pattern
covers the payload type, recursively for a constructor payload and by these
rules for a tuple, record, or list payload. For a list, `{}` covers the empty
list; a fixed-length pattern covers lists of its arity only, and only when
each component covers the element type; `{h, ..t}` and `{.., p}` match
non-empty lists by head and by last element and cover every non-empty list
only when that component covers the element type, which `{_, ..}` does. The
empty list and the non-empty lists MUST both be covered.

A scalar scrutinee has no case structure: a literal pattern alone never covers
its type, coverage comes from a variable or `_`, and a single-scrutinee `case`
therefore MUST end with such an arm. An or-pattern covers what its
alternatives cover, each alternative being considered. Checking is against the
instantiated union, so `Result t Never` still requires an `Err` arm.

A `case` over several scrutinees is checked over the product of their types.
An arm covers a combination when each component pattern covers its scrutinee,
judged column by column: a constructor column owes coverage of its payload
over exactly the arms that survive every split of the other columns, a scalar
column splits the literals its arms name from the catch-all answering the
rest, and a scrutinee without case structure is covered only by a variable or
`_` in its column. The arms MUST cover every combination; a final all-`_` arm
does so but is not required when the columns already cover it.
### 7.7.3 Guards and Exhaustiveness

An arm MAY carry a guard: a pattern, `if`, a `Bool` expression, `->`, and a
result (Clause 6.7.3). The pattern binds before the guard is checked. The
guard MUST have type `Bool` and MAY use the pattern's bindings. If the pattern
matches and the guard is false, matching continues with the next arm.

A `case` containing a guarded arm MUST end with an unguarded arm.
Exhaustiveness is judged on the unguarded arms only; a guarded arm does not
contribute coverage.

A guard occurs only at arm level (Clause 6.7.3). Its world bit is ORed into
the enclosing context like any other expression (Clause 7.9.4). A `case` with
no guards follows Clause 7.7.2.

### 7.7.4 Never and Arm Typing

Every arm of a `case` MUST have the same type. A `Never` arm adapts to the
type of the other arms (Clause 7.5.4); if every arm is `Never`, the `case` is
`Never`. The scrutinee and all guards contribute their bits to the enclosing
context; `case` has no separate bit rule (Clause 7.9.4).

An arm is always a pattern followed by `->` and an expression. A bare
diverging expression is not an arm (Clause 6.7.1); it is a syntax error. A
`Never` arm's pattern covers like any other arm's — exhaustiveness belongs to
patterns, never to divergence (Clause 7.7.2).

The `if` expression requires a `Bool` condition. Its two branches MUST have
the same type. An omitted `else` is `else: _`, so a then-branch without an
`else` MUST have type `_` or `Never` (Clause 6.7.4). `if` is sugar for a
`case` over `Bool`; its condition and both branches contribute bits by OR
(Clause 7.9.4).

A declared return position is an adaptation position for `Never`:
```eko
die: (msg: String) -> Never = panic(msg)

value: Int = if True: 1 else: die("no")
```
Unreachable code after a call of type `Never` is legal and produces
W-unreachable, not an error (Clause 8.4.4). A `Never` result does not set the
world bit; `panic` and `Option.expect` are pure (Clause 7.9.2).
