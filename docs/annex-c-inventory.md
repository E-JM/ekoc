# Annex C. Standard Library Inventory

This annex is normative. It is the complete inventory of the shipped surface:
the `std` module and the capability module, every free function, type, and
namespace member, with its signature. Signatures in this annex are normative.
Semantics are owned by Clause 9 and are cross-referenced, not restated here.

The delivery model, the implicit import, and the companion modules are
specified in Clause 9.1. Companion modules under `std/` are outside this annex
(Clause 9.1.3). An entry marked **derived** is compiler-generated; an entry in
C.9 is capability-module source or a compiler-provided declaration (Clauses
9.1.1, 10.8); every other entry is `std` source (Clause 9.1.1).

**Notation.** Type variables are lowercase: `t`, `u`, `v`, `a`, `b`, `e`, `f`,
and `k`. The world-bit markers `!` and `?` are part of every signature
(Clause 7.9.3). The elided arrow forms `!` and `?` are written `!-> _` and
`?-> _`. Every signature takes its receiver as the first parameter; the member
form `x.f(y)` is the identity `f(x, y)` (Clause 6.5.2). A row MAY name several
members that share one signature.

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
| `Range t` | range with `Int`, `UInt`, or `Char` endpoints; the operators are its only construction | Clause 6.5.6 |
| `Option t` | built-in ADT; constructors `Some t` and `None` | Clause 9.2.2 |
| `Result t e` | built-in ADT; constructors `Ok t` and `Err e` | Clause 9.2.1 |
| `Set t` | nominal generic container | Clause 9.10.3 |
| `ParseError` | closed ADT | Clause 9.6.3 |
| `IoError` | closed ADT | Clause 9.12.2 |
| `OpenMode` | ADT with five nullary constructors | Clause 9.12.1 |
| `File` | opaque handle | Clause 9.12.2 |
| `FileInfo` | record | Clause 9.12.7 |
| `Time`, `Mono`, `Duration` | nominal single-variant types | Clause 9.7.1 |
| `Json` | ADT with seven variants | Clause 9.9.1 |
| `Never` | the bottom type, a type of the language | Clause 7.5.4 |
| `_` | the unit type, a type of the language | Clause 7.5.4 |
| `Scope`, `Task t` | concurrency scope and task handle | Clause 8.9.1 |
| `Out`, `In`, `Fs`, `Env`, `Args`, `Clock`, `Rand`, `Proc`, `Conc` | capabilities | Clause 7.9.1 |
| `C.Ptr.Void`, `C.Size`, `C.In T` | native vocabulary | Clause 10.3.2, Clause 10.3.4 |

The declarations whose variants are fixed by this document are the following.

```eko
Bool:       type = True | False
Option:     type t = Some t | None
Result:     type t e = Ok t | Err e
Range:      type t = _Range [start: t, end: t, closed: Bool]
ParseError: type = Empty | Invalid String | OutOfRange
IoError:    type = NotFound | Permission | Exists | NotFile | Closed | InvalidData String | Other String
OpenMode:   type = Read | Write | Append | New | Update
FileInfo:   type = [size: UInt, mtime: Time, is_dir: Bool]
Time:       type = Time Int
Mono:       type = Mono Int
Duration:   type = Duration Int
Json:       type = Null | Bool Bool | Int Int | Float Float | Str String | Arr []Json | Obj ([String]Json)
```

`Set`, `File`, `Scope`, `Task t`, the capabilities, and the machine words have
no public variant declaration; their interfaces are the namespaces of Annex C.5,
Annex C.8, and Annex C.9. `ParseError` and `IoError` are closed and are not extended by
programs (Clause 9.6.3, Clause 9.12.2).

**Derived type-namespace members.** Every type namespace carries the services
below. The availability column restricts the types that carry the member.

| Member | Signature | Availability | Clause |
|---|---|---|---|
| `T.show` | `(t) -> String` | every type; overridable | Clause 9.8.2 |
| `T.inspect` | `(t) -> String` | every type; not overridable | Clause 9.8.3 |
| `T.eq` | `(t, t) -> Bool` | value layer, compiler-owned; records, tuples, ADTs, and containers whose derivation reaches no capability or function | Clause 9.8.4 |
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

## C.3 List

The namespace of `[]t` is `List` (Clause 7.4.3). All members are pure unless
their signature carries a world-bit marker.

| Member | Signature | Clause |
|---|---|---|
| `List.length` | `([]t) -> Int` | Clause 9.10.1 |
| `List.get` | `([]t, Int) -> Option t` | Clause 9.10.1 |
| `List.reverse` | `([]t) -> []t` | Clause 9.10.1 |
| `List.append` | `([]t, []t) -> []t` | Clause 9.10.1 |
| `List.map` | `([]t, (t) ?-> u) ?-> []u` | Clause 9.10.1 |
| `List.filter` | `([]t, (t) ?-> Bool) ?-> []t` | Clause 9.10.1 |
| `List.fold` | `([]t, u, (u, t) ?-> u) ?-> u` | Clause 9.10.1 |
| `List.each` | `([]t, (t) ?-> _) ?-> _` | Clause 9.10.1 |
| `List.each_with_index` | `([]t, (t, Int) ?-> _) ?-> _` | Clause 9.10.1 |
| `List.any`, `List.all` | `([]t, (t) ?-> Bool) ?-> Bool` | Clause 9.10.1 |
| `List.find_by` | `([]t, (t) ?-> Bool) ?-> Option t` | Clause 9.10.1 |
| `List.sort` | `([]t) -> []t` | Clause 9.10.1 |
| `List.sort_by` | `([]t, (t, t) ?-> Bool) ?-> []t` | Clause 9.10.1 |
| `List.zip_with` | `([]t, []u, (t, u) ?-> v) ?-> []v` | Clause 9.10.1 |
| `List.take`, `List.drop` | `([]t, Int) -> []t` | Clause 9.10.1 |
| `List.take_while`, `List.drop_while` | `([]t, (t) ?-> Bool) ?-> []t` | Clause 9.10.1 |
| `List.join` | `([]t, String) -> String` | Clause 9.10.1 |
| `List.slice` | `([]t, Int, Int) -> Option []t` | Clause 9.10.1 |

`get` is O(1); index syntax does not exist for any structure (Clause 6.10).
`append` is the body of `++` (Clause 7.8.6). `map`, `filter`, `fold`, `each`,
`each_with_index`, `any`, `all`, `find_by`, `sort_by`, `zip_with`,
`take_while`, and `drop_while` are open-bit dispatchers (Clause 7.9.6).
`take` and `drop` truncate: a count beyond the length takes or drops the whole
list, and a negative count counts as zero. `slice` is a domain check: a
partially out-of-range request yields `None`, and the range is never clamped
(Clause 9.10.1). `sort` uses the derived ordering `<`; instantiation at a
record type is a compile error because a record has no ordering
(Clause 9.8.5). `sort_by` is stable, and an inconsistent comparator
terminates with an implementation-defined order (Clause 2.5). `join` uses the
derived `show` form of each element; an element type without `show` is a
compile error (Clause 9.8.1). `List.eq` is derived (Annex C.2); `List` has no
derived ordering.

## C.4 Dict

The namespace of `[k]v` is `Dict` (Clause 7.4.3). All members are pure.

| Member | Signature | Clause |
|---|---|---|
| `Dict.length` | `([k]v) -> Int` | Clause 9.10.2 |
| `Dict.get` | `([k]v, k) -> Option v` | Clause 9.10.2 |
| `Dict.insert` | `([k]v, k, v) -> [k]v` | Clause 9.10.2 |
| `Dict.remove` | `([k]v, k) -> [k]v` | Clause 9.10.2 |
| `Dict.merge` | `([k]v, [k]v) -> [k]v` | Clause 9.10.2 |
| `Dict.map` | `([k]v, (k, v) ?-> u) ?-> [k]u` | Clause 9.10.2 |
| `Dict.filter` | `([k]v, (k, v) ?-> Bool) ?-> [k]v` | Clause 9.10.2 |
| `Dict.fold` | `([k]v, u, (u, k, v) ?-> u) ?-> u` | Clause 9.10.2 |
| `Dict.each` | `([k]v, (k, v) ?-> _) ?-> _` | Clause 9.10.2 |

`get` is O(1), and iteration follows insertion order (Clause 9.10.2).
`insert` and `remove` return a new dictionary and never mutate the receiver;
removing an absent key returns the original. `merge` is the body of `++`, and
the right side wins on a duplicate key (Clause 7.8.6). The key domain is the
set of data types with structural equality; a capability or a function is not
a key (Clause 9.10.4). `Dict.eq` is derived (Annex C.2); `Dict` has no derived
ordering.

## C.5 Set

`Set` is a nominal generic type with no notation and no literal
(Clause 9.10.3). All members are pure.

| Member | Signature | Clause |
|---|---|---|
| `Set.of` | `([]t) -> Set t` | Clause 9.10.3 |
| `Set.to_list` | `(Set t) -> []t` | Clause 9.10.3 |
| `Set.contains` | `(Set t, t) -> Bool` | Clause 9.10.3 |
| `Set.insert` | `(Set t, t) -> Set t` | Clause 9.10.3 |
| `Set.remove` | `(Set t, t) -> Set t` | Clause 9.10.3 |
| `Set.length` | `(Set t) -> Int` | Clause 9.10.3 |
| `Set.union` | `(Set t, Set t) -> Set t` | Clause 9.10.3 |
| `Set.intersect` | `(Set t, Set t) -> Set t` | Clause 9.10.3 |
| `Set.diff` | `(Set t, Set t) -> Set t` | Clause 9.10.3 |

`Set.of` deduplicates and keeps the first occurrence. Iteration and `to_list`
follow insertion order (Clause 9.10.3). `insert` and `remove` return a new
set; inserting a present element or removing an absent element returns the
original. `union` places the left set first, followed by the new elements of
the right set in right-set order; `intersect` and `diff` preserve left order.
The element domain is the key domain (Clause 9.10.4). `Set.empty` does not
exist: the empty set is `Set.of({})` with an expected type (Clause 9.10.3).
`Set.eq` is derived (Annex C.2); `Set` has no derived ordering.

## C.6 String

The namespace of `String` is `String` (Clause 7.4.3).

| Member | Signature | Clause |
|---|---|---|
| `String.length` | `(String) -> Int` | Clause 9.4.2 |
| `String.at` | `(String, Int) -> Option Char` | Clause 9.4.2 |
| `String.chars` | `(String) -> []Char` | Clause 9.4.2 |
| `String.from_char` | `(Char) -> String` | Clause 9.4.2 |
| `String.from_chars` | `([]Char) -> String` | Clause 9.4.2 |
| `String.append` | `(String, String) -> String` | Clause 9.4.2 |
| `String.lines` | `(String) -> []String` | Clause 9.11.1 |
| `String.split` | `(String, String) -> []String` | Clause 9.11.1 |
| `String.contains` | `(String, String) -> Bool` | Clause 9.11.1 |
| `String.starts_with`, `String.ends_with` | `(String, String) -> Bool` | Clause 9.11.1 |
| `String.replace`, `String.replace_first` | `(String, String, String) -> String` | Clause 9.11.1 |
| `String.slice` | `(String, Int, Int) -> Option String` | Clause 9.11.1 |
| `String.trim`, `String.trim_start`, `String.trim_end` | `(String) -> String` | Clause 9.11.1 |
| `String.to_bytes` | `(String) -> Bytes` | Clause 9.5.3 |
| `String.pad_left`, `String.pad_right` | `(String, UInt, Char) -> String` | Clause 9.13.1 |

`length` counts Unicode scalar values. `at` is O(n) and yields `None` when the
index is outside the scalar domain. `chars` is the bridge to `List`; no
String-specific iteration function exists (Clause 9.4.2). `append` is the body
of `++` and costs O(n) (Clause 7.8.6). `split` splits on every occurrence of a
non-empty separator and keeps empty fields; an empty separator is a panic
(Annex B.5). `lines` splits on `'\n'` and drops the final empty element when
the string ends with `'\n'` (Clause 9.11.1). `slice` uses the Unicode scalar
domain, returns `None` when out of range, and never clamps (Clause 9.11.1).
`trim`, `trim_start`, and `trim_end` remove Unicode White_Space
(Clause 9.11.1). `to_bytes` is total and produces UTF-8; `Bytes.to_string` is
the decoding direction and returns a `Result` (Clause 9.5.3). `pad_left` and
`pad_right` pad to the requested width and never truncate (Clause 9.13.1).
`String.eq`, `String.ne`, and the ordering members are compiler-owned on the
value layer (Clause 9.8.4, Clause 9.8.5).

## C.7 Char, Bytes, Option, Result, Range

| Namespace | Member | Signature | Clause |
|---|---|---|---|
| `Char` | `Char.to_code` | `(Char) -> Int` | Clause 9.4.3 |
| `Char` | `Char.from_code` | `(Int) -> Option Char` | Clause 9.4.3 |
| `Bytes` | `Bytes.length` | `(Bytes) -> Int` | Clause 9.5.2 |
| `Bytes` | `Bytes.get` | `(Bytes, Int) -> Option Int` | Clause 9.5.2 |
| `Bytes` | `Bytes.append` | `(Bytes, Bytes) -> Bytes` | Clause 9.5.2 |
| `Bytes` | `Bytes.to_ints` | `(Bytes) -> []Int` | Clause 9.5.2 |
| `Bytes` | `Bytes.from_ints` | `([]Int) -> Option Bytes` | Clause 9.5.2 |
| `Bytes` | `Bytes.to_u8s` | `(Bytes) -> []U8` | Clause 9.5.2 |
| `Bytes` | `Bytes.from_u8s` | `([]U8) -> Option Bytes` | Clause 9.5.2 |
| `Bytes` | `Bytes.to_hex` | `(Bytes) -> String` | Clause 9.5.3 |
| `Bytes` | `Bytes.from_hex` | `(String) -> Result Bytes ParseError` | Clause 9.5.3 |
| `Bytes` | `Bytes.to_string` | `(Bytes) -> Result String ParseError` | Clause 9.5.3 |
| `Option` | `Option.map` | `(Option t, (t) ?-> u) ?-> Option u` | Clause 9.2.2 |
| `Option` | `Option.and_then` | `(Option t, (t) ?-> Option u) ?-> Option u` | Clause 9.2.2 |
| `Option` | `Option.or_else` | `(Option t, () ?-> Option t) ?-> Option t` | Clause 9.2.2 |
| `Option` | `Option.filter` | `(Option t, (t) ?-> Bool) ?-> Option t` | Clause 9.2.2 |
| `Option` | `Option.is_some`, `Option.is_none` | `(Option t) -> Bool` | Clause 9.2.2 |
| `Option` | `Option.unwrap` | `(Option t) -> t` | Clause 9.2.2 |
| `Option` | `Option.unwrap_or` | `(Option t, t) -> t` | Clause 9.2.2 |
| `Option` | `Option.expect` | `(Option t, String) -> t` | Clause 9.2.2 |
| `Option` | `Option.get` | `(Option ([]t), Int) -> Option t` | Clause 9.2.2 |
| `Option` | `Option.to_result` | `(Option t, e) -> Result t e` | Clause 9.2.2 |
| `Result` | `Result.try` | `(Result t e, (t) ?-> Result u e) ?-> Result u e` | Clause 9.2.1 |
| `Result` | `Result.map` | `(Result t e, (t) ?-> u) ?-> Result u e` | Clause 9.2.1 |
| `Result` | `Result.map_err` | `(Result t e, (e) ?-> f) ?-> Result t f` | Clause 9.2.1 |
| `Result` | `Result.traverse` | `([]t, (t) ?-> Result u e) ?-> Result []u e` | Clause 9.2.1 |
| `Result` | `Result.is_ok`, `Result.is_err` | `(Result t e) -> Bool` | Clause 9.2.1 |
| `Result` | `Result.unwrap` | `(Result t e) -> t` | Clause 9.2.1 |
| `Result` | `Result.unwrap_or` | `(Result t e, t) -> t` | Clause 9.2.1 |
| `Result` | `Result.expect` | `(Result t e, String) -> t` | Clause 9.2.1 |
| `Range` | `Range.to_list` | `(Range t) -> []t` | Clause 9.10 |
| `Range` | `Range.length` | `(Range t) -> Int` | Clause 9.10 |
| `Range` | `Range.each` | `(Range t, (t) ?-> _) ?-> _` | Clause 9.10 |
| `Range` | `Range.each_with_index` | `(Range t, (t, Int) ?-> _) ?-> _` | Clause 9.10 |
| `Range` | `Range.map` | `(Range t, (t) ?-> u) ?-> []u` | Clause 9.10 |
| `Range` | `Range.fold` | `(Range t, u, (u, t) ?-> u) ?-> u` | Clause 9.10 |

**Char.** `Char` is a value-layer scalar type ordered by code point
(Clause 9.4.3). **Bytes.** `Bytes` is an immutable byte sequence with no
literal (Clause 9.5.1); all its members are pure (Clause 9.5.2). `length`
counts bytes and is O(1). `get` is O(1) and yields `None` outside the range;
elements are `Int` in `0..255` (Clause 9.5.2). `append` is the body of `++`
and costs O(n) (Clause 7.8.6). `from_ints` is a domain check, and an element
outside `0..255` yields `None`. `from_hex` is input validation, and an odd
length or a non-hexadecimal character yields `Err` (Clause 9.5.3). `to_u8s`
and `from_u8s` are zero-copy (Clause 9.5.2). `to_string` reports the first
invalid offset on failure; `to_bytes` on `String` is total (Clause 9.5.3).
`Bytes.eq` and `Bytes.lt` are compiler-owned; `<` is byte lexicographic order
(Clause 9.8.5).

**Option.** `Option t` is a built-in ADT with constructors `Some t` and `None`
(Clause 9.2.2). `unwrap` panics with `P-unwrap` on `None`; `expect` panics with
`P-expect` and its message. Both are pure (Clause 9.2.2). `Option.get` is the
nested-access helper for `Option ([]t)`, and `to_result` maps `None` to
`Err e`, the bridge used by `<-` chains (Clause 9.2.2). `map`, `and_then`,
`or_else`, and `filter` are open-bit dispatchers (Clause 7.9.6).

**Result.** `Result t e` is a built-in ADT with constructors `Ok t` and
`Err e` (Clause 9.2.1). `try`, `map`, `map_err`, and `traverse` are the
combinators behind the inverted binding `<-` (Clause 6.3.4) and are open-bit
dispatchers (Clause 7.9.6). `unwrap` panics with `P-unwrap` on `Err`; `expect`
panics with `P-expect` and its message. Both are pure (Clause 9.2.1).
`traverse` collects successful values in order, and the first `Err`
short-circuits (Clause 9.2.1).

**Range.** `Range t` is constructed by `..` and `..=` over `Int`, `UInt`, or
`Char` endpoints (Clause 6.5.6). The callback shapes are the `List` shapes of
Annex C.3. `Range.map` produces a list, and `Range.fold` folds in iteration order
(Clause 9.10).

## C.8 Int, UInt, Float, Machine Words

| Namespace | Member | Signature | Clause |
|---|---|---|---|
| `Int` | `Int.add`, `Int.sub`, `Int.mul`, `Int.quot`, `Int.rem`, `Int.div`, `Int.mod` | `(Int, Int) -> Int` | Clause 9.6.1 |
| `Int` | `Int.neg`, `Int.abs` | `(Int) -> Int` | Clause 9.6.1 |
| `Int` | `Int.pow` | `(Int, UInt) -> Int` | Clause 9.6.1 |
| `UInt` | `UInt.add`, `UInt.sub`, `UInt.mul`, `UInt.quot`, `UInt.rem` | `(UInt, UInt) -> UInt` | Clause 9.6.1 |
| `Float` | `Float.add`, `Float.sub`, `Float.mul`, `Float.div`, `Float.rem` | `(Float, Float) -> Float` | Clause 9.6.1 |
| `Float` | `Float.neg` | `(Float) -> Float` | Clause 9.6.1 |
| `Float` | `Float.abs`, `Float.floor`, `Float.ceil`, `Float.trunc`, `Float.round` | `(Float) -> Float` | Clause 9.13.2 |
| `Float` | `Float.pow` | `(Float, Float) -> Float` | Clause 9.13.2 |
| `Float` | `Float.sqrt`, `Float.exp`, `Float.log`, `Float.log2`, `Float.log10` | `(Float) -> Float` | Clause 9.13.2 |
| `Float` | `Float.sin`, `Float.cos`, `Float.tan`, `Float.asin`, `Float.acos`, `Float.atan` | `(Float) -> Float` | Clause 9.13.2 |
| `Float` | `Float.atan2` | `(Float, Float) -> Float` | Clause 9.13.2 |
| `Float` | `Float.is_nan`, `Float.is_inf`, `Float.is_finite` | `(Float) -> Bool` | Clause 9.13.2 |
| `Int`/`UInt` | `UInt.to_int`, `Int.to_uint` | `(UInt) -> Option Int`, `(Int) -> Option UInt` | Clause 9.6.2 |
| `Int`/`UInt`/`Float` | `Int.to_float`, `UInt.to_float` | `(Int) -> Float`, `(UInt) -> Float` | Clause 9.6.2 |
| `Float` | `Float.to_int`, `Float.to_uint` | `(Float) -> Option Int`, `(Float) -> Option UInt` | Clause 9.6.2 |
| `Int`/`UInt`/`Float` | `Int.parse`, `UInt.parse`, `Float.parse` | `(String) -> Result t ParseError` | Clause 9.6.2 |
| `Int`/`UInt` | `Int.to_hex`, `Int.to_oct`, `Int.to_bin`; the `UInt` analogues | `(Int) -> String`, `(UInt) -> String` | Clause 9.13.1 |
| `Int`/`UInt` | `Int.parse_hex`, `Int.parse_oct`, `Int.parse_bin`; the `UInt` analogues | `(String) -> Result Int ParseError`, `(String) -> Result UInt ParseError` | Clause 9.6.2 |
| `Float` | `Float.to_fixed`, `Float.to_exp` | `(Float, UInt) -> String` | Clause 9.13.1 |
| `UInt` | `UInt.band`, `UInt.bor`, `UInt.bxor` | `(UInt, UInt) -> UInt` | Clause 9.6.5 |
| `UInt` | `UInt.bnot` | `(UInt) -> UInt` | Clause 9.6.5 |
| `UInt` | `UInt.shl`, `UInt.shr` | `(UInt, UInt) -> UInt` | Clause 9.6.5 |
| `Int`/`UInt` | `Int.wrapping_add`, `Int.wrapping_sub`, `Int.wrapping_mul` | `(Int, Int) -> Int` | Clause 9.6.6 |
| `Int` | `Int.wrapping_neg` | `(Int) -> Int` | Clause 9.6.6 |
| `UInt` | `UInt.wrapping_add`, `UInt.wrapping_sub`, `UInt.wrapping_mul` | `(UInt, UInt) -> UInt` | Clause 9.6.6 |
| `UInt` | `UInt.wrapping_neg` | `(UInt) -> UInt` | Clause 9.6.6 |
| machine words | `T.eq`, `T.ne`, `T.lt`, `T.le`, `T.gt`, `T.ge` | `(T, T) -> Bool` | Clause 10.3.1 |
| machine words | `T.show` | `(T) -> String` | Clause 10.3.1 |
| machine words | `T.to_hex_pad` | `(T, UInt) -> String` | Clause 9.13.1 |

`Int` is a signed 64-bit integer; overflow is a panic under the one arithmetic
rule of the language (Clause 7.8.4). `quot` and `rem` truncate toward zero;
`div` and `mod` are the floor family (Clause 7.8.4). Division by zero panics
with `P-div`, and overflow panics with `P-overflow` (Annex B.5). `abs(Int.min)`
panics because the result is not representable (Clause 9.6.1). The constants
`Int.min`, `Int.max`, and `UInt.max` are values of their types
(Clause 9.6.1). There is no `UInt.neg`; unsigned values are non-negative
(Clause 9.6.1).

`Float` is IEEE 754 binary64. NaN and the infinities are values, not errors:
`sqrt(-1.0)` is `NaN`, `log(0.0)` is `-Inf`, and the predicates distinguish
them (Clause 9.13.2). `trunc` rounds toward zero and is the basis of `to_int`;
`round` rounds half to even; `atan2` is quadrant-correct; `is_inf` does not
distinguish signs (Clause 9.13.2). The maximum error of a floating-point
library function is implementation-defined (Clause 2.5, Clause 9.13.2).
`Float.eq` follows IEEE 754: `NaN == NaN` is `False` (Clause 9.8.4).

A conversion into a narrower domain is `Option`; a conversion that cannot fail
is total (Clause 9.6.2). `Float.to_int` and `Float.to_uint` yield `None` for
NaN, an infinity, or an out-of-range value; a non-negative fractional value
truncates toward zero (Clause 9.6.2). `parse` and `show` are mutual inverses
for non-NaN values, and the Float round trip uses the shortest round-trip
`show` (Clause 9.6.4). `to_hex`, `to_oct`, and `to_bin` are total; negative
`Int` values are rendered in 64-bit two's-complement form (Clause 9.13.1).
`parse_hex`, `parse_oct`, and `parse_bin` accept an optional sign, no prefix,
and case-insensitive digits. `Int.parse` and `UInt.parse` accept an optional
sign and decimal digits. `Float.parse` also accepts `NaN`, `Inf`, `-Inf`, and
`+Inf` (Clause 9.6.2, Clause 9.6.4). `to_fixed` and `to_exp` are total; NaN
and the infinities print as themselves (Clause 9.13.1).

The bitwise family is defined on `UInt` only, on the ring ℤ/2⁶⁴, and never
panics (Clause 9.6.5). There are no bitwise operators (Clause 7.8.3); a shift
count of 64 or more yields `0`. The wrapping family computes on the same ring
and never panics (Clause 9.6.6); overflow panics by default, and wrapping is an
explicit function call (Clause 7.8.4).

The machine words are `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`,
`F32`, and `F64` (Clause 10.3.1). They hold, compare, show, and ferry; they do
not compute. `to_hex_pad` zero-pads to at least the requested number of digits
and never truncates (Clause 9.13.1).

The size-family conversions are members of the domain namespace. On a domain
word in `Int`, `UInt`, or `Float`, the members `to_i8`, `to_i16`, `to_i32`,
`to_i64`, `to_u8`, `to_u16`, `to_u32`, `to_u64`, `to_f32`, and `to_f64`
convert to the named machine word. A conversion to an integer machine word
yields `Option`; a conversion to `F32` or `F64` is total and rounds. The
reverse family is named `from_i8` through `from_f64`; a `from_` conversion is
total and exists only where the machine domain is included in the domain word
(Clause 9.6.2, Clause 10.3.1).

## C.9 Capability Surface

Capabilities are opaque, unforgeable values (Clause 7.9.1). Every member of a
capability namespace takes its capability as the first parameter; `Scope.spawn`,
`Task.join`, and `Task.cancel` take their handle first, as the members of
`File` do. A member marked `!` sets the world bit;
a derivation is pure. `File`, `Scope`, and `Task t` are handles rather than
capabilities; their namespaces are included here because each is reached only
through a capability member.
Every member in this section is capability-module source (Clause 9.1.1), except
the fifteen compiler-provided declarations (Clause 10.8): `Proc.exit`, the four
concurrency members, the eight `File` members, and the two advancing derivations
`In.from_lines` and `Rand.with_seed`.

| Capability | Member | Signature | Clause |
|---|---|---|---|
| `Out` | `Out.print`, `Out.println` | `(Out, String) !-> _` | Clause 7.9.1 |
| `Out` | `Out.tee` | `(Out, (String) !-> _) -> Out` | Clause 9.14.5 |
| `Out` | `Out.null` | `() -> Out` | Clause 9.14.5 |
| `In` | `In.read_line` | `(In) !-> Result (Option String) IoError` | Clause 7.9.1 |
| `In` | `In.empty` | `() -> In` | Clause 9.14.5 |
| `In` | `In.from_lines` | `([]String) -> In` | Clause 9.14.5 |
| `Fs` | `Fs.open` | `(Fs, String, OpenMode) !-> Result File IoError` | Clause 9.12.2 |
| `Fs` | `Fs.with_file` | `(Fs, String, OpenMode, (File) !-> a) !-> Result a IoError` | Clause 9.12.4 |
| `Fs` | `Fs.exists` | `(Fs, String) !-> Bool` | Clause 9.12.2 |
| `Fs` | `Fs.read` | `(Fs, String) !-> Result String IoError` | Clause 9.12.2 |
| `Fs` | `Fs.write`, `Fs.append` | `(Fs, String, String) !-> Result _ IoError` | Clause 9.12.2 |
| `Fs` | `Fs.read_bytes` | `(Fs, String) !-> Result Bytes IoError` | Clause 9.12.2 |
| `Fs` | `Fs.append_bytes` | `(Fs, String, Bytes) !-> Result _ IoError` | Clause 9.12.2 |
| `Fs` | `Fs.list_dir` | `(Fs, String) !-> Result []String IoError` | Clause 9.12.6 |
| `Fs` | `Fs.mkdir`, `Fs.mkdir_all`, `Fs.rm`, `Fs.rm_dir` | `(Fs, String) !-> Result _ IoError` | Clause 9.12.6 |
| `Fs` | `Fs.rename` | `(Fs, String, String) !-> Result _ IoError` | Clause 9.12.6 |
| `Fs` | `Fs.stat` | `(Fs, String) !-> Result FileInfo IoError` | Clause 9.12.6 |
| `Fs` | `Fs.realpath` | `(Fs, String) !-> Result String IoError` | Clause 9.12.6 |
| `Fs` | `Fs.null` | `() -> Fs` | Clause 9.14.5 |
| `Fs` | `Fs.root` | `(Fs, String) -> Fs` | Clause 9.12.5 |
| `Fs` | `Fs.read_only` | `(Fs) -> Fs` | Clause 9.12.5 |
| `File` | `File.read` | `(File, Int) !-> Result String IoError` | Clause 9.12.2 |
| `File` | `File.read_bytes` | `(File, Int) !-> Result Bytes IoError` | Clause 9.12.2 |
| `File` | `File.read_line` | `(File) !-> Result (Option String) IoError` | Clause 9.12.2 |
| `File` | `File.seek` | `(File, UInt) !-> Result _ IoError` | Clause 9.12.2 |
| `File` | `File.write` | `(File, String) !-> Result _ IoError` | Clause 9.12.2 |
| `File` | `File.write_bytes` | `(File, Bytes) !-> Result _ IoError` | Clause 9.12.2 |
| `File` | `File.flush` | `(File) !-> Result _ IoError` | Clause 9.12.2 |
| `File` | `File.close` | `(File) !-> Result _ IoError` | Clause 9.12.2 |
| `Env` | `Env.get` | `(Env, String) !-> Option String` | Clause 7.9.1 |
| `Env` | `Env.frozen` | `([String]String) -> Env` | Clause 9.14.5 |
| `Args` | `Args.all` | `(Args) !-> []String` | Clause 7.9.1 |
| `Args` | `Args.frozen` | `([]String) -> Args` | Clause 9.14.5 |
| `Clock` | `Clock.now` | `(Clock) !-> Time` | Clause 9.7.2 |
| `Clock` | `Clock.mono` | `(Clock) !-> Mono` | Clause 9.7.2 |
| `Clock` | `Clock.sleep` | `(Clock, Duration) !-> _` | Clause 9.7.2 |
| `Clock` | `Clock.frozen` | `(Time) -> Clock` | Clause 9.7.3 |
| `Rand` | `Rand.int` | `(Rand, UInt) !-> UInt` | Clause 7.9.1 |
| `Rand` | `Rand.with_seed` | `(Int) -> Rand` | Clause 9.14.5 |
| `Proc` | `Proc.exit` | `(Proc, Int) !-> Never` | Clause 8.4.3 |
| `Conc` | `Conc.scope` | `(Conc, (Scope) ?-> a) ?-> a` | Clause 8.9.1 |
| `Scope` | `Scope.spawn` | `(Scope, () ?-> b) ?-> Task b` | Clause 8.9.1 |
| `Task` | `Task.join` | `(Task b) -> Option b` | Clause 8.9.2 |
| `Task` | `Task.cancel` | `(Task b) -> _` | Clause 8.9.2 |

`read_line` yields `Ok None` at end of input; EOF is not an error
(Clause 9.2.4). `Out.tee` writes once and reports once, invoking its observer
on each write. `Out.null` swallows every write, `In.empty` reports EOF
immediately, and `In.from_lines` yields the given lines in order and then EOF
(Clause 9.14.5). `with_file` closes the handle on both the value path and the
error path (Clause 9.12.4). `read` keeps the text gate and reports invalid
UTF-8 as `Err InvalidData`; `read_bytes` never reports `InvalidData`
(Clause 9.12.2). `list_dir` returns names, not paths, in lexicographic order
and excludes `.` and `..` (Clause 9.12.6). `Fs.null` is a pure derivation that
reports `Err NotFound` for every path (Clause 9.14.5). `Fs.root` and
`Fs.read_only` are pure derivations; `root` restricts a subtree without an
existence check, and `read_only` withdraws write rights (Clause 9.12.5).
`close` advances the handle's generation; an operation on a stale handle,
including a second `close`, yields `Err Closed` (Clause 9.12.3). A short read
is end of input, not an error: the call returns `Ok` with a short value
(Clause 9.12.2).

`Env.get` yields `None` for an unset variable; presence is a domain check
(Clause 7.9.1). `Env.frozen` reads a fixed table and is pure (Clause 9.14.5).
`Args.all` yields the argument vector without the program name. `Args.frozen`
reads a fixed vector and is pure (Clause 7.9.1). `Clock.now` reads the wall
clock, `Clock.mono` reads the monotonic clock, and `Clock.sleep` accepts a
negative duration by returning immediately (Clause 9.7.2). `Clock.frozen` is a
pure derivation whose `now` and `mono` are constant and whose `sleep` returns
immediately (Clause 9.7.3). `Rand.int` is uniform over the range from zero to
the bound, exclusive of the bound; a zero bound is an invariant violation and
panics (Clause 9.2.3, Annex B.5). `Rand.with_seed` fixes the order; the sequence itself is
an implementation detail (Clause 9.14.5). `Proc.exit` terminates the process
abruptly with the given exit code (Clause 8.4.3). `Conc.scope` and
`Scope.spawn` are open-bit dispatchers; a child's world bit joins the open bit
(Clause 8.9.1). `Task.join` and `Task.cancel` are pure; `join` yields `None`
when the task was cancelled (Clause 8.9.2).

## C.10 Time, Mono, Duration, Json

| Namespace | Member | Signature | Clause |
|---|---|---|---|
| `Time` | `Time.sub` | `(Time, Time) -> Duration` | Clause 9.7.1 |
| `Time` | `Time.add` | `(Time, Duration) -> Time` | Clause 9.7.1 |
| `Time` | `Time.to_ns` | `(Time) -> Int` | Clause 9.7.1 |
| `Time` | `Time.parse` | `(String) -> Result Time ParseError` | Clause 9.7.4 |
| `Mono` | `Mono.sub` | `(Mono, Mono) -> Duration` | Clause 9.7.1 |
| `Mono` | `Mono.add` | `(Mono, Duration) -> Mono` | Clause 9.7.1 |
| `Mono` | `Mono.to_ns` | `(Mono) -> Int` | Clause 9.7.1 |
| `Duration` | `Duration.add` | `(Duration, Duration) -> Duration` | Clause 9.7.1 |
| `Duration` | `Duration.mul` | `(Duration, Int) -> Duration` | Clause 9.7.1 |
| `Duration` | `Duration.neg` | `(Duration) -> Duration` | Clause 9.7.1 |
| `Duration` | `Duration.secs`, `Duration.millis` | `(Int) -> Duration` | Clause 9.7.1 |
| `Duration` | `Duration.to_ns` | `(Duration) -> Int` | Clause 9.7.1 |
| `Duration` | `Duration.parse` | `(String) -> Result Duration ParseError` | Clause 9.7.4 |
| `Json` | `Json.parse` | `(String) -> Result Json ParseError` | Clause 9.9.2 |
| `Json` | `Json.show` | `(Json) -> String` | Clause 9.9.2 |

`Time`, `Mono`, and `Duration` are nominal single-variant types with an `Int`
nanosecond payload (Clause 9.7.1). All their members are pure. `Time.show` is
ISO 8601 in UTC, `Duration.show` is decimal seconds, and `Mono.show` has no
human form; `inspect` is the constructor form for all three (Clause 9.7.4).
The payload is Int64 nanoseconds, a span of about ±292 years; arithmetic
outside the domain panics under the arithmetic rule (Clause 9.7.5). There is
no operator sugar: every operation is a named function (Clause 9.7.1).
`Time.parse` accepts the ISO 8601 UTC forms that `Time.show` produces, and
`Duration.parse` accepts decimal seconds (Clause 9.7.4).

`Json` is an ADT with seven variants (Clause 9.9.1). `parse` is input
validation and reuses `ParseError` (Clause 9.9.2). `show` is minimal and emits
no whitespace; `inspect` is the constructor form (Clause 9.9.2). `Obj`
preserves insertion order, so JSON text is reproducible (Clause 9.9.1). Number
text follows the variant: `Int` and `Float` are separate variants, and a
`Float` uses the shortest round trip (Clause 9.9.2). The derived `to_json` and
`from_json` are the type-namespace members of Annex C.2 (Clause 9.9.3).

## C.11 Path

`Path` is a pure namespace over `String`; path vocabulary is lexical and does
not touch the file system (Clause 9.11.2).

| Member | Signature | Clause |
|---|---|---|
| `Path.join` | `(String, String) -> String` | Clause 9.11.2 |
| `Path.normalize` | `(String) -> String` | Clause 9.11.2 |
| `Path.is_absolute` | `(String) -> Bool` | Clause 9.11.2 |

`join` uses the host separator. `normalize` collapses separators and `.` and
resolves `..` lexically. `is_absolute` applies the platform rule. Resolution
through the file system is `Fs.realpath` (Clause 9.12.6).

## C.12 Not in the Library

The following are absent from the standard library. The list is normative and
is owned by Clause 9.15.

- No world-touching free function. Every world-touching function is a member
  of a capability namespace (Clause 9.1.2).
- No format-string language or format DSL; formatting is named functions over
  `String` (Clause 9.13.1). No regular expressions and no Unicode case mapping
  (Clause 9.15).
- No encoding family beyond UTF-8; UTF-16, latin-1, and GBK are library domain
  (Clause 9.15).
- No byte literal, mutable `Bytes`, builder, resize vocabulary, `Bytes.new`,
  `Bytes.empty`, or `byte_length`; the empty byte string is `"".to_bytes()`
  and byte length is `s.to_bytes().length()` (Clause 9.5.2,
  Clause 9.5.5).
- No digest, CRC, bignum, complex number, or matrix (Clause 9.15).
- No YAML or TOML, no JSON pretty printer, and no JSON path query
  (Clause 9.9.5).
- No `Set` notation, literal, `Set.empty`, or ordering (Clause 9.10.3).
- No timezone or calendar type; UTC is the only zone, and `Date` and
  `DateTime` are absent (Clause 9.7.5). No `Clock.frozen_mono` and no
  construction right for a clock derivation (Clause 9.7.3).
- No `Rights` type; attenuation is the pure combinators `Fs.root` and
  `Fs.read_only` (Clause 9.12.5). No FFI capability; native code is authorized
  at the link layer (Clause 10.1).
- No unstable sorting; `List.sort_by` is stable, and an inconsistent
  comparator yields an implementation-defined order (Clause 2.5).
- No index syntax and no String-specific iteration function; the entry points
  are `get`, `at`, and `chars` (Clause 6.10, Clause 9.4.2). No operator on a
  type outside the value layer; equality and ordering there are the members
  `T.eq` and `T.lt`, called explicitly (Clause 9.8.4, Clause 9.8.5).
- No user extension of the closed unions `ParseError` and `IoError`
  (Clause 9.6.3, Clause 9.12.2).

Additions to this inventory are specification changes. The standard library
is versioned with the compiler, and its opened names are part of its interface
(Clause 9.1.4).
