# Annex C. Standard Library Inventory

## C.1 Free Functions

The implicit import opens exactly three free names (Clause 9.1.2). `show`,
`inspect`, and `panic` are compiler-generated services, not opened names; their
free-call forms in the table are the identities of Clause 6.5.2. No other free
function exists in `std` (Clause 9.1.1).

| Function | Signature | Kind | Clause |
|---|---|---|---|
| `show` | `(t) -> String` | derived | Clause 9.8.1 |
| `inspect` | `(t) -> String` | derived | Clause 9.8.3 |
| `panic` | `(String) -> Never` | intrinsic | Clause 9.2.3 |
| `assert` | `(c: Bool) -> _` | source | Clause 9.14.3 |
| `assert_eq` | `(a: t, b: t) -> _` | source | Clause 9.14.3 |
| `assert_ne` | `(a: t, b: t) -> _` | source | Clause 9.14.3 |

`show` dispatches to `T.show` and uses the derived default when the type
declares no override (Clause 9.8.2). `inspect` is always derived and is never
overridable (Clause 9.8.3). `panic` is pure: aborting is not world contact
(Clause 9.2.3). `assert`, `assert_eq`, and `assert_ne` are pure, report
failure by panic, and lock the compared arguments to one type
(Clause 9.14.3).

**Operator targets.** Operator sugar introduces no free name. Each operator is
the namespace member below, by operand type (Clause 7.8.3).

| Expression | Target member |
|---|---|
| `a + b`, `a - b`, `a * b` | `Int.add`, `Int.sub`, `Int.mul`; the `UInt` and `Float` analogues |
| `a / b` | `Int.quot`, `UInt.quot`, `Float.div` |
| `a % b` | `Int.rem`, `UInt.rem`, `Float.rem` |
| `-a` | `Int.neg`, `Float.neg`; there is no `UInt.neg` |
| `a == b`, `a != b` | `T.eq`, `T.ne`, on the value layer only |
| `a < b`, `a <= b`, `a > b`, `a >= b` | `T.lt`, `T.le`, `T.gt`, `T.ge`, on the value layer only |
| `a && b`, `a \|\| b` | `if` sugar; there is no function |
| `b.not()` | `Bool.not` |
| `xs ++ ys` | `List.append`, `String.append`, `Bytes.append`, `Dict.merge` |

## C.2 Types

| Type | Kind | Clause |
|---|---|---|
| `Bool` | ADT with two nullary constructors | Clause 9.3 |
| `Char` | scalar; Unicode scalar values | Clause 9.4.1 |
| `String` | immutable UTF-8 sequence | Clause 9.4.2 |
| `Bytes` | immutable byte sequence | Clause 9.5.1 |
| `Int` | signed 64-bit integer | Clause 9.6.1 |
| `UInt` | unsigned 64-bit integer | Clause 9.6.1 |
| `Float` | IEEE 754 binary64 | Clause 9.6.1 |
| `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `F32`, `F64` | machine words | Clause 10.3.1 |
| `Option t` | built-in ADT; constructors `Some t` and `None` | Clause 9.2.2 |
| `Result t e` | built-in ADT; constructors `Ok t` and `Err e` | Clause 9.2.1 |
| `Set t` | compiler-supplied nominal generic container | Clause 9.1.1, Clause 9.10.3 |
| `Range t` | compiler-supplied nominal generic type; no constructor; the operators are its only construction | Clause 6.5.6, Clause 9.1.1, Clause 9.10.5 |
| `ParseError` | closed ADT | Clause 9.6.3 |
| `IoError` | closed ADT | Clause 9.12.2 |
| `OpenMode` | ADT with five nullary constructors | Clause 9.12.1 |
| `File` | opaque handle | Clause 9.12.2 |
| `FileInfo` | record | Clause 9.12.7 |
| `Time`, `Mono`, `Duration` | nominal single-variant types declared by the `std/time` companion | Clause 9.1.3, Clause 9.7.1 |
| `Json` | ADT with seven variants | Clause 9.9.1 |
| `Never` | the bottom type, a type of the language | Clause 7.5.4 |
| `_` | the unit type, a type of the language | Clause 7.5.4 |
| `Scope`, `Task t` | concurrency scope and task handle | Clause 8.9.1 |
| `Out`, `In`, `Fs`, `Env`, `Args`, `Clock`, `Rand`, `Proc`, `Conc` | capabilities | Clause 7.9.1 |
| `C.Ptr.Void`, `C.Size`, `C.In T` | native vocabulary | Clause 10.3.2, Clause 10.3.4 |

The variant set of each type is stated with the type's own clause: the table
above gives the clause for every row, and `Range` is Clause 9.10.5. `Set`,
`Range`, `File`, `Scope`, `Task t`, the capabilities, and the machine words
have no public variant declaration; their interfaces are the namespaces of
Annex C.5, Annex C.7, Annex C.8, and Annex C.9. `Set` and `Range` are
compiler-supplied (Clause 9.1.1). `Range` has no constructor and no pattern
form; `..` and `..=` are its only construction (Clause 6.5.6). `ParseError`
and `IoError` are closed and are not extended by programs (Clause 9.6.3,
Clause 9.12.2).

**Derived type-namespace members.** Every type namespace carries the services
below. The availability column restricts the types that carry the member.

| Member | Signature | Availability | Clause |
|---|---|---|---|
| `T.show` | `(t) -> String` | every type; overridable | Clause 9.8.2 |
| `T.inspect` | `(t) -> String` | every type; not overridable | Clause 9.8.3 |
| `T.eq` | `(t, t) -> Bool` | value layer, compiler-owned; records, tuples, ADTs, and containers whose derivation reaches no capability or function; overridable by a `T.eq:` declaration in the declaring module | Clause 9.8.4 |
| `T.ne` | `(t, t) -> Bool` | every type that carries `T.eq` | Clause 9.8.4 |
| `T.lt` | `(t, t) -> Bool` | value layer except `Bool`; ADTs other than `Bool` by declaration order | Clause 9.8.5 |
| `T.le`, `T.gt`, `T.ge` | `(t, t) -> Bool` | every type that carries `T.lt` | Clause 9.8.5 |
| `T.with` | `(t, partial record) -> t` | records | Clause 9.8.6 |
| `T.to_json` | `(t) -> Json` | types in the JSON isomorphism | Clause 9.9.3 |
| `T.from_json` | `(Json) -> Result t ParseError` | types in the JSON isomorphism | Clause 9.9.3 |

The value layer is `Int`, `UInt`, `Float`, the machine words, `Bool`, `Char`,
`String`, and `Bytes` (Clause 9.8.4). The `T.with` signature is not
expressible in the type syntax; its second argument is a partial field
literal, legal only in that position (Clause 9.8.6). Capabilities and
functions carry no `eq`, no `lt`, and no JSON pair; use of such a member is
`E-no-eq` or `E-json-shape` (Annex B.3). `show` on a capability produces
`<cap>` and on a function `<fn>` (Clause 9.8.3). `Bool` has one non-derived
member, `Bool.not: (Bool) -> Bool` (Clause 9.3). The `C` namespace contributes
`C.get_errno: () -> I32` and `C.Size.to_uint: (C.Size) -> UInt`
(Clause 10.3.2).
