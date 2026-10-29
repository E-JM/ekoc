# Annex C. Standard Library Inventory

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
| `Result` | `Result.traverse` | `([]t, (t) ?-> Result u e) ?-> Result []u e`; no receiver form | Clause 9.2.1 |
| `Result` | `Result.is_ok`, `Result.is_err` | `(Result t e) -> Bool` | Clause 9.2.1 |
| `Result` | `Result.unwrap` | `(Result t e) -> t` | Clause 9.2.1 |
| `Result` | `Result.unwrap_or` | `(Result t e, t) -> t` | Clause 9.2.1 |
| `Result` | `Result.expect` | `(Result t e, String) -> t` | Clause 9.2.1 |
| `Range` | `Range.to_list` | `(Range t) -> []t` | Clause 9.10.5 |
| `Range` | `Range.length` | `(Range t) -> Int` | Clause 9.10.5 |
| `Range` | `Range.each` | `(Range t, (t) ?-> _) ?-> _` | Clause 9.10.5 |
| `Range` | `Range.each_with_index` | `(Range t, (t, Int) ?-> _) ?-> _` | Clause 9.10.5 |
| `Range` | `Range.map` | `(Range t, (t) ?-> u) ?-> []u` | Clause 9.10.5 |
| `Range` | `Range.fold` | `(Range t, u, (u, t) ?-> u) ?-> u` | Clause 9.10.5 |
| `Range` | `Range.eq` | `(Range t, Range t) -> Bool` | Clause 9.10.5 |
| `Range` | `Range.lt`, `Range.le`, `Range.gt`, `Range.ge` | `(Range t, Range t) -> Bool` | Clause 9.10.5 |
| `Range` | `Range.show` | `(Range t) -> String` | Clause 9.10.5 |

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
`Err e` (Clause 9.2.1). `try` is the link appended by the inverted binding
`<-` (Clause 6.3.4), `map_err` is the conversion point of a chain, and all four
combinators are open-bit dispatchers (Clause 7.9.6). `unwrap` panics with
`P-unwrap` on `Err`; `expect` panics with `P-expect` and its message. Both are
pure (Clause 9.2.1).
`traverse` collects successful values in order, and the first `Err`
short-circuits (Clause 9.2.1).

**Range.** `Range t` is a compiler-supplied nominal type (Clause 9.1.1) with
no constructor and no pattern form; `..` and `..=` are its only construction
over `Int`, `UInt`, or `Char` endpoints (Clause 6.5.6). Its members are
compiler-supplied; the member semantics are Clause 9.10.5. `Range.eq`
compares the endpoints and the closed flag structurally, and `Range.show` uses
the operator form (`1..5`, `1..=5`).

## C.8 Int, UInt, Float, Machine Words

| Namespace | Member | Signature | Clause |
|---|---|---|---|
| `Int` | `Int.add`, `Int.sub`, `Int.mul`, `Int.quot`, `Int.rem`, `Int.div`, `Int.mod` | `(Int, Int) -> Int` | Clause 7.8.4 |
| `Int` | `Int.neg`, `Int.abs` | `(Int) -> Int` | Clause 7.8.4 |
| `Int` | `Int.pow` | `(Int, UInt) -> Int` | Clause 9.6.1 |
| `UInt` | `UInt.add`, `UInt.sub`, `UInt.mul`, `UInt.quot`, `UInt.rem` | `(UInt, UInt) -> UInt` | Clause 7.8.4 |
| `Float` | `Float.add`, `Float.sub`, `Float.mul`, `Float.div`, `Float.rem` | `(Float, Float) -> Float` | Clause 7.8.4 |
| `Float` | `Float.neg` | `(Float) -> Float` | Clause 7.8.4 |
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
| `Int`, `UInt`, `Float` | `T.to_i8` … `T.to_u64` | `(T) -> Option I*`, `(T) -> Option U*` | Clause 9.6.2 |
| `Int`, `UInt`, `Float` | `T.to_f32`, `T.to_f64` | `(T) -> F32`, `(T) -> F64` | Clause 9.6.2 |
| machine words | `I*.to_int`, `U*.to_uint`, `F*.to_float` | `(T) -> Int`, `(T) -> UInt`, `(T) -> Float` | Clause 10.3.1 |

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
