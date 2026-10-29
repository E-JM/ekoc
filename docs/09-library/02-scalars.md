# 9. Standard Library

## 9.3 Bool

```eko
Bool: type = True | False
```

`Bool` is a standard-library ADT with exactly two constructors. There is no
truthiness: the condition of an `if` is a `Bool`, and no other value is
coerced to one (Clause 8.3.2). The logical operators `&&` and `||` are sugar
for `if` (Clause 7.8.5); there is no logical function. `Bool.not` is an
ordinary member applied as `b.not()`. There is no prefix negation operator:
the token `!` is reserved for the world bit (Clause 7.9), so `!b` is not a
negation and does not type-check. `==` and `!=` are defined on `Bool` (Clause
9.8.4). `Bool` has no ordering (Clause 9.8.5). `show` and `inspect` yield
`True` and `False`.

## 9.4 Char and String

Text is a sequence of Unicode scalar values. `Char` and `String` are distinct
types and do not interchange.

### 9.4.1 Char

`Char` is a standard-library scalar type (Clause 8.5.1). Its domain is the
Unicode scalar values: `0..0x10FFFF` excluding the surrogate range. One `Char`
is exactly one code point; a grapheme cluster is a sequence of code points and
is not a `Char`.

Character escapes are those of String literals plus `\'` (Clause 5.8). `show`
yields the character bare; `inspect` yields the quoted literal form (Clause
9.8.2). `==` and `<` order by code point (Clauses 9.8.4 and 9.8.5).
`Char.to_code` is total and yields the code point as `Int`. `Char.from_code`
is a domain check: a value outside the scalar-value domain yields `None`
(Clause 9.2.2).

### 9.4.2 String

A `String` is a UTF-8 sequence that is always valid UTF-8. The invariant holds
for every constructed value; no standard-library operation produces an invalid
`String`. The encoding is variable width. Index syntax does not exist for any
type (Clause 6.10); `String.at` is the accessor, has cost O(n), yields `Option
Char`, and returns `None` for an out-of-range index. `String.length` counts
Unicode scalar values, not bytes. `String.append` and `++` perform an O(n)
full copy. `String.chars` produces `[]Char` and composes with the `List`
namespace; no String-specific iteration function exists. An empty `String` is
`""`. Values are immutable and are represented and reclaimed under Clauses 8.5
and 8.6.

### 9.4.3 Bridges

The bridge members are `String.chars`, `String.from_char`,
`String.from_chars`, `Char.to_code`, and `Char.from_code`. `Char` is not a
one-character `String`. The concatenation of a `Char` and a `String` is a type
error:

```eko
'a' ++ "bc"        // error: `Char` and `String` do not interchange
```

Conversion is explicit. `String.from_char` is the singleton conversion and
`String.from_chars` the batch conversion. Range endpoints are `Int`, `UInt`,
and `Char`; `'a'..'z'` iterates code points (Clause 6.5.6). Byte length is
obtained through `String.to_bytes().length()` (Clause 9.5.2); no `byte_length`
member exists. Grapheme cluster segmentation is not part of the standard
library.

## 9.5 Bytes

### 9.5.1 Identity

`Bytes` is a standard-library built-in composite: an immutable, contiguous
sequence of bytes. It is not a list; list operations and list patterns do not
apply. It is not one of the four composite formers (Clause 7.5.3).

| Property | `String` | `Bytes` |
|---|---|---|
| Element | Unicode scalar value | `Int` in `0..255` |
| Invariant | always valid UTF-8 | none |
| Access | `at`, O(n), explicit | `get`, O(1), explicit |
| Length | scalar count | byte count |
| Iteration | `chars` produces `[]Char` | `to_ints` produces `[]Int` |
| Literal | string literal | none |

Element values are plain `Int` in `0..255`. Machine words exist for layout and
ferrying, never for byte computation (Clause 10.3.1). `get` is the only entry
point; index syntax does not exist (Clause 6.10). There is no byte literal,
and construction goes through functions. Immutability, sharing, and reference
counting follow Clauses 8.5 and 8.6. `Bytes` is not a capability.

### 9.5.2 Services

`Bytes.length` is O(1). `Bytes.get` is O(1) and yields `Option Int`; an index
outside the block yields `None`. `Bytes.append` performs an O(n) full copy;
`b1 ++ b2` is defined as `Bytes.append(b1, b2)`. `Bytes.to_ints` is the bridge
to `[]Int`. `Bytes.from_ints` is a domain check: an element outside `0..255`
yields `None`. `Bytes.to_u8s` and `Bytes.from_u8s` are O(1) zero-copy bridges
between `Bytes` and `[]U8`; `from_u8s` returns `Option Bytes`, and every `U8`
fits. `Bytes.to_hex` yields lowercase hexadecimal with no separators.
`Bytes.from_hex` is input validation and returns `Result Bytes ParseError`; an
empty string yields `Ok` an empty `Bytes` (Clause 9.5.3). `String.to_bytes` is
total and encodes UTF-8. `Bytes.to_string` is input validation and returns
`Result String ParseError`. `==` on `Bytes` is structural deep equality, byte
by byte (Clause 9.8.4). `<` is byte lexicographic order (Clause 9.8.5). The
empty byte sequence is `"".to_bytes()`; there are no `Bytes.new` and
`Bytes.empty` members. The remaining members and signatures are listed in
Annex C.

### 9.5.3 UTF-8 and Hex

The failure taxonomy of Clause 9.2 maps onto the binary domain as follows:

| Operation | Failure form |
|---|---|
| `String.to_bytes` (encoding) | none; total |
| `Bytes.to_string` (decoding untrusted bytes) | `Result String ParseError` with the first invalid offset and its context |
| `Bytes.from_ints` (per-byte construction) | `Option Bytes`; an out-of-range element yields `None` |
| `Bytes.from_hex` (hex parsing) | `Result Bytes ParseError`; odd length or a non-hexadecimal character yields `Err` |

A decode failure such as the byte sequence `ed a0 80` yields `Err` with a
payload identifying the first invalid offset and the surrounding bytes. Exact
message wording follows Annex B; the payload shape is normative.

```eko
s: String = "hi"
b: Bytes = "hi".to_bytes()

Bytes.to_string(s.to_bytes()).eq(Ok s)
Bytes.from_hex(b.to_hex()).eq(Ok b)
```

A decoded `String` is always valid UTF-8, so the first property holds in both
directions. The hex property holds for every `Bytes` value.

### 9.5.4 Byte I/O

`File.read_bytes` reads sequentially at most the requested number of bytes and
never reports `InvalidData`; every byte sequence is legal. A short read means
end of input and returns `Ok` with a short `Bytes` (Clause 9.2.4). `File.read`
retains the text gate: non-UTF-8 content yields `Err InvalidData` (Clause
9.12.2). The raw-byte entry point is `File.read_bytes`; refusal and acceptance
are both available. The `In` console layer is text-only and has no byte gate.
`Fs.with_file` and inverted-binding chains apply to byte operations unchanged
(Clause 9.12.4). `OpenMode` is not extended for bytes (Clause 9.12.1).

### 9.5.5 Display

`show` and `inspect` yield the `Bytes.from_hex` form. The structural channel
does not truncate; long byte strings are printed in full, and the printed form
is legal program text that evaluates to the original value. The binary
computation surface is `Int` in `0..255` for element values, the `UInt`
bitwise family for bit operations (Clause 9.6.5), and `0x` or `0b` integer
literals for constants (Clause 5.7). Byte literals, index syntax, mutable
`Bytes`, builder or resize vocabulary, encoding families other than UTF-8,
digest or CRC functions, and implicit `Bytes`/`String` conversion are
excluded.

## 9.6 Numeric Types and Conversions

### 9.6.1 Numeric Types

The domain numeric types are `Int`, `UInt`, and `Float`. `Int` is a 64-bit
two's-complement integer, `UInt` is a 64-bit unsigned integer, and `Float` is
an IEEE 754 binary64 value (Clause 3). The machine words `I8`, `I16`, `I32`,
`I64`, `U8`, `U16`, `U32`, `U64`, `F32`, and `F64` are layout and ferrying
types. They have equality, ordering, `show`, and size-family conversions, and
no arithmetic or bitwise operations (Clause 10.3.1). Numeric literals take
their type from the expected type (Clause 5.7.4). Arithmetic is specified in
Clause 7.8.4. Integer overflow panics; it does not wrap. Division by zero
panics. `Int.abs` panics on `Int.min`. `Float` follows IEEE 754: `NaN` and the
infinities are values, `NaN` is not equal to itself, and `-0.0` compares equal
to `0.0`. There are no implicit numeric conversions; both operands of a
numeric operator MUST have the same type.

```eko
1 == 1.0           // error: `Int` and `Float` are distinct types; convert explicitly
```

`Int.min`, `Int.max`, and `UInt.max` are the range constants. `Int.pow` takes
`(Int, UInt)`, is `1` at exponent zero, and panics on overflow under the
arithmetic rule of Clause 7.8.4; there is no exponentiation operator
(Clause 6.10).

### 9.6.2 Conversion Families

A conversion between domain types reports failure through Clause 9.2.

| Conversion | Form | Rule |
|---|---|---|
| `UInt.to_int` | `Option Int` | a value above `Int.max` yields `None` |
| `Int.to_uint` | `Option UInt` | a negative value yields `None` |
| `Int.to_float`, `UInt.to_float` | `Float` | total; values above 2^53 round to nearest |
| `Float.to_int` | `Option Int` | `NaN`, infinity, or out of range yields `None`; a fraction truncates toward zero |
| `Float.to_uint` | `Option UInt` | `NaN`, infinity, or out of range yields `None`; the negative domain is checked before truncation |
| numeric `parse` | `Result t ParseError` | input validation |

`Float.to_int` and `Float.to_uint` are not saturating. `NaN` maps to `None`,
not to zero, and an infinity maps to `None`, not to a maximum. `Float.to_uint`
checks the sign before truncation; `-0.5` yields `None` and `-0.0` yields
`Some 0`. Size-family conversions between domain words and machine words
follow Clause 10.3.1: domain to machine integer is `Option` on range, domain
to `F32` or `F64` is total with rounding, and machine to domain is total in
the widening directions listed in Annex C.8. A narrowing direction composes a
widening conversion with a checked domain conversion, such as `UInt.to_int`.
Conversion names are spelled in full. The complete family is listed in Annex
C.

### 9.6.3 ParseError

```eko
ParseError: type = Empty | Invalid String | OutOfRange
```

`Empty` reports a required value that is missing. `Invalid` reports malformed
input and carries a detail string. `OutOfRange` reports a value outside the
limits of the target type. The type is closed and is not extended by users.
`ParseError` is the error payload of the parse family, including `Int.parse`,
`UInt.parse`, `Float.parse`, the base-specific parsers, `Json.parse`,
`Time.parse`, and `Duration.parse` (Clauses 9.9 and 9.7).

### 9.6.4 Round Trips

`parse` and `show` are mutual inverses on the domain numeric types:
`parse(show(x)).eq(Ok x)`.

For `Int` and `UInt` the property is exact. For `Float` it is guaranteed by
the shortest round-trip form of `show` (Clause 9.8.3). `NaN` is an exception
under IEEE equality: the round trip preserves `NaN` identity but acceptance
cannot be tested with `==`, so a test MUST use `NaN` equivalence (Clause
9.14.3). `Int.parse` and `UInt.parse` accept an optional sign and decimal
digits only. `Float.parse` accepts an optional sign, decimal digits with an
optional fraction and exponent, and the spellings `NaN`, `Inf`, `-Inf`, and
`+Inf`. A leading digit is required; `.5` is not accepted and `0.5` is.

### 9.6.5 Bitwise Family

Bitwise operations are functions on `UInt`, and on `UInt` only. There are no
bitwise operators and no bitwise functions on `Int` or on machine words. `Int`
converts through `to_uint` and back. `UInt.band`, `UInt.bor`, `UInt.bxor`,
`UInt.bnot`, `UInt.shl`, and `UInt.shr` are total and compute on the ring of
integers modulo 2^64. Truncation is the semantics, not an overflow. A shift
count of 64 or more yields zero.

### 9.6.6 Wrapping Family

`Int.wrapping_add`, `Int.wrapping_sub`, `Int.wrapping_mul`,
`Int.wrapping_neg`, and the `UInt` analogues compute on the ring of integers
modulo 2^64 and never panic. Overflow panics by default (Clause 7.8.4); a
wrapping operation is an explicit function call.

## 9.7 Time

### 9.7.1 Time, Mono, Duration

```eko
Time:     type = Time Int
Mono:     type = Mono Int
Duration: type = Duration Int
```

The types and their source members are declared by the companion module
`std/time` (Clause 9.1.3), not by the `std` root. A bare `import std/time`
opens the companion's public types into unqualified scope, makes its
constructors visible for anchoring, and opens the module stem `time`: the
types are named `Time`, `Mono`, and `Duration`, constructor forms are `Time 0`
and `Duration 500`, and their members are `time.Duration.secs`. The qualified
spellings `time.Time`, `time.Mono`, and `time.Duration` are legal alternatives
(Clause 7.4.4). The companion import is required to name a type; the implicit
import opens no time name. Receiver calls on time values (`t.add(d)`,
`d.to_ns()`) resolve through the type namespace and require no import (Clause
7.4.3). The two parsers of Clause 9.7.4 are compiler-supplied intrinsic rows
of the companion (Clause 9.1.1); `ParseError` remains in the root (Clause
9.6.3).

The payload is a signed 64-bit nanosecond count. `Time` is a wall-clock point:
nanoseconds since 1970-01-01T00:00:00Z. `Mono` is a monotonic reading with an
arbitrary epoch; only relative meaning is defined. `Duration` is a signed
duration. The three quantities have mutually exclusive semantics: an instant,
a measurement, and a difference. A wall clock can jump and a monotonic clock
cannot; the two readings are distinct types. All time arithmetic is a named
function on values: `Time.sub`, `Time.add`, `Time.to_ns`, `Mono.sub`,
`Mono.add`, `Mono.to_ns`, `Duration.add`, `Duration.mul`, `Duration.neg`,
`Duration.secs`, `Duration.millis`, and `Duration.to_ns`. `Time.sub` and
`Mono.sub` yield a `Duration`; `Time.add` and `Mono.add` shift by a
`Duration`; `Duration.add` sums durations and `Duration.neg` negates one;
`to_ns` exposes the signed nanosecond payload; `Duration.secs` and
`Duration.millis` construct from the named unit; `Duration.mul` scales, and a
negative factor flips the sign. No time operator exists. No operator reaches
the payload, so arithmetic and unit mixing cannot occur through an operator.
Constructor application after `import std/time` constructs a value from
nanoseconds (`Duration 500`, `Time 0`); `time.Duration 500` and `time.Time 0`
are the qualified alternatives; `to_ns` is its inverse. Every member in this
subsection is pure. Equality and ordering are the derived members `T.eq` and
`T.lt`, by payload recursion over the single variant (Clauses 9.8.4 and 9.8.5).

### 9.7.2 Clock Services

The members of this subsection and of Clause 9.7.3 are declared by the
capability module (Clause 9.1.1). The capability module carries one ordinary
import, `std/time`, for the time types in `FileInfo.mtime`, the `_ClockB`
bundle, and `Clock.sleep` (Clause 9.1.3). Reading the clock sets the world
bit: two readings can differ. Sleeping sets the world bit: the process yields
and time passes. Both operations require a `Clock` capability (Clause 7.9.1).
`Clock.now` yields a timestamp. `Clock.mono` yields a monotonic reading;
elapsed time and ordering are measured on `Mono`, not on `Time`. `Time.lt` is
reading order; precedence is decided with `Mono.lt`. `Clock.sleep` returns
immediately for a negative duration. Sleep granularity is host-dependent.
Timeouts and cancellation belong to concurrency (Clause 8.9); the standard
library adds no timeout vocabulary.

### 9.7.3 Frozen Clocks

`Clock.frozen(t)` is pure and constructs a clock with constant readings: `now`
always yields `t`, `mono` always yields the monotonic epoch `Mono 0`, a fixed
reading independent of `t`, and `sleep` returns immediately. The world bit is
unchanged: `now` and `mono` on a frozen clock still set the bit (Clause
7.9.4). Frozen clocks are the deterministic clock used by tests (Clause
9.14.5). No `Clock.frozen_mono` exists; a clock has one wall-clock reading and
one monotonic reading.

### 9.7.4 Display

| Type | `show` | `inspect` |
|---|---|---|
| `Time` | ISO 8601 UTC form `2026-10-22T01:29:00.123Z`, fractional seconds as needed, at most nine digits | `Time 1792632540123000000` |
| `Duration` | decimal seconds `0.5s`, trailing zeros removed | `Duration 500000000` |
| `Mono` | `Mono 89347123456` | `Mono 89347123456` |

`Time.show` and `Duration.show` are `std/time` overrides of the default
display machinery (Clause 9.8.2). `inspect` is always the constructor form, so
the displayed value is writable after `import std/time`: the import opens the
type and constructor names, so the bare forms `Time 1792632540123000000`,
`Duration 500000000`, and `Mono 89347123456` are writable. UTC is the only
zone. `Time.parse` accepts the ISO 8601 UTC forms that `Time.show` produces,
and `Duration.parse` accepts decimal seconds. Both return a `Result` whose
error type is `ParseError` and satisfy the round-trip property of Clause 9.6.4.

### 9.7.5 Domain and Overflow

The payload is a signed 64-bit nanosecond count: a span of approximately 292
years in each direction. The wall-clock domain is approximately 1677-09-21 to
2262-04-11, and `Duration` has the same span. Arithmetic outside the domain
panics under the arithmetic rule (Clause 7.8.4). A panic does not set the
world bit (Clause 9.2.3). Leap seconds are not represented. The POSIX epoch
maps directly, a day is always 86400 seconds, and alignment policy belongs to
the host. The following are outside the time surface: a single type merging
wall-clock and monotonic readings, time operator sugar, a local timezone or
calendar family (`Date`, `DateTime`), and a public construction right for
`Clock`. Capabilities are opaque (Clause 7.9.1); `Clock.frozen` is a pure
derivation, not a constructor for arbitrary clocks.
