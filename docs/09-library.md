# 9. Standard Library

This clause specifies the semantics of the standard library. Annex C is the authoritative inventory of
signatures and members; a signature appears here only where a rule requires it. A `!` marks a
world-touching service and a `?` an open bit; capability and world-bit typing is specified in Clause 7.9.

## 9.1 Delivery Model

### 9.1.1 Delivery Surfaces

The compiler release carries two surfaces.

The **capability module** is compiler-embedded Eko source. It declares the members of the capability
namespaces and the types of their signatures: `File`, `OpenMode`, `FileInfo`, `IoError`, `Scope`, and
`Task t`. It is part of the language definition, it is versioned with the compiler, it has no import
path, and no tool replaces it. Its channel to the operating system is the injected primitives of
Clause 10.8.

The **standard library** is source-delivered: an ordinary Eko module named `std`, whose source is
`lib/std.eko`, plus the companion modules under `lib/std/` (Clause 9.1.3). Both are versioned with the
compiler. The module is target-independent. It declares the types whose bare names are part of the
language vocabulary and their namespace members. It declares no free function other than the assert
family of Clause 9.14.3. The standard library contains no capability content: no capability type, no
member of a capability namespace, no world-touching declaration.
The compiler generates the default `show` and `inspect`, structural `eq`, derived `lt`, `with`,
`to_json`, `from_json` (Clause 9.8), the `panic` intrinsic (Clause 9.2.3), and the memory operations of
Clause 8.6. These services attach to every type namespace as defaults. Every other standard-library
service is ordinary Eko source.
A standard-library type namespace takes universal members only. Ownership of a type namespace belongs to
the module that declares the type (Clause 7.4.5). The capability module performs every world contact of
the shipped surface through the injected primitives of Clause 10.8; the standard library performs none
and declares no world-touching free function.

### 9.1.2 Implicit Import

Every source file is compiled as if it began with `import std`. The injection is a constant of the
language definition and does not depend on the contents of the file. An explicit `import std` is
redundant and reports `W-redundant-import` (Annex B). The stem `std` and the path prefix `std/` are
reserved; a user module MUST NOT be named `std` (Clause 7.2.2).
The names declared by the capability module resolve in every source file without import: the capability
namespaces through the type namespaces, as `List.map` does, and their types (`File`, `OpenMode`,
`FileInfo`, `IoError`, `Scope`, `Task t`, and the capability types) as bare names. No import path names
the capability module (Clause 9.1.1).
The names opened into unqualified scope are exactly `show`, `inspect`, and `panic`, which are
compiler-generated, and `assert`, `assert_eq`, and `assert_ne`, which are std source. No later version
opens another free name. `show` and `inspect` are generic over every type, and the opened name spans all
type namespaces at once. Type-attached functions such as `List.map` resolve through the type namespace
and are not opened. An opened name and a file's top-level binding occupy the same layer: a same-name
collision is an error (Clause 7.4.6), while local shadowing remains legal. The qualified spelling
`std.show` is legal and denotes the same binding.

### 9.1.3 Companion Modules

A companion module is an ordinary module under the reserved `std/` path. The import `import std/math`
opens the module named `math`; the stem enters scope only on import, and no name other than the path
prefix is reserved. A companion follows the ordinary module rules of Clause 7.2: one file is one
module, interfaces are extracted from source, imports are read-only grants, and the dependency graph is
acyclic. A companion declares no free name that the implicit import could open.
The admission condition is common utility: the unit is worth versioning with the compiler and is not
needed by most files.

### 9.1.4 Versioning

The standard library and the capability module are versioned with the compiler. The interface diff
between two compiler versions, over both surfaces and covering members added or removed and signature
or world-bit changes, is the library changelog and the compatibility verdict.
The language has no signature overloading, so any change to an existing signature is breaking. Adding a
member to a type namespace is breaking by default: resolution station 2 precedes station 3
(Clause 7.4.3), so a new member can capture receiver calls that previously resolved to the calling
module's own function. The opened free-name set is closed across versions.
A deprecated member carries `/// @deprecated` in its documentation and reports `W-deprecated` at use
sites (Clause 11.5 and Annex B). Deprecation adds no language surface.

## 9.2 Failure Taxonomy

Every failure in the standard library is one of three forms. The classification is stated once
here; the rest of this clause and Clauses 7 and 8 cross-reference it.

| Form | Condition | Reason carried |
|---|---|---|
| `Result t e` | input validation: external data is untrusted and the caller distinguishes causes | yes, payload `e` |
| `Option t` | domain check: whether a value lies in a domain, not why | no |
| `panic` | invariant violation: a program assumption is false | message |

A recoverable failure caused by data outside the program is a `Result`. A question of membership or
presence within an already-trusted value is an `Option`. A violated invariant is a panic. The three
forms are exhaustive; a library operation MUST NOT invent a fourth failure channel. End of input is a
normal outcome and is never reported as a failure (Clause 9.2.4).

### 9.2.1 Result

```eko
Result: type t e = Ok t | Err e
```

`Result` is a built-in standard-library ADT, sealed against extension, with constructors available to
programs. A recoverable failure MUST be represented as `Result`, and the `Err` payload MUST carry a
reason sufficient for the caller to branch. A standard-library error type is a closed union and matches
over it are exhaustive; `IoError` (Clause 9.12.2) and `ParseError` (Clause 9.6.3) are such types.
The combinators are open-bit dispatchers:

```eko
Result.try:      (Result t e, (t) ?-> Result u e) ?-> Result u e
Result.map:      (Result t e, (t) ?-> u)          ?-> Result u e
Result.map_err:  (Result t e, (e) ?-> f)          ?-> Result t f
Result.traverse: ([]t, (t) ?-> Result u e)        ?-> Result []u e
```

`try` applies a `Result`-returning callback to an `Ok` payload and propagates `Err`. `map` applies a
callback to an `Ok` payload and wraps the result in `Ok`. `map_err` applies a callback to an `Err`
payload and wraps the result in `Err`. `traverse` applies a `Result`-returning callback to every element
and collects the successes; an `Err` yields `Err`. A pure callback makes the call pure; a
world-touching callback makes it world-touching (Clause 7.9).
The inverted binding `<-` appends a last-position callback and requires the explicit `.try()` at each
propagation point. The error type MUST agree at every link, and `map_err` is the conversion point
(Clause 8.2.4). `unwrap_or` supplies a default. `unwrap` and `expect` assert that the value is `Ok`;
both panic on `Err` and both are pure (Clause 9.2.3). The remaining `Result` members are listed in
Annex C.

### 9.2.2 Option

```eko
Option: type t = Some t | None
```

`None` answers a domain question negatively and carries no reason. A domain check MUST be represented as
`Option`; it MUST NOT be converted to `Result` without an explicit reason at the conversion site.
Every index-based accessor yields `Option`. `List.get` is O(1) and `String.at` is O(n) (Clause 8.10);
out-of-range access yields `None` and never panics. The same rule covers `String.slice`, `List.slice`,
`Bytes.get`, `Char.from_code`, `Bytes.from_ints`, `Env.get`, and `Dict.get`. Conversions that can fail
on range yield `Option` (Clause 9.6.2).
Consumption is explicit. A `case` branches on `Some` and `None`; `unwrap_or` supplies a default;
`to_result` converts `None` to `Err` with a caller-supplied reason; `expect` and `unwrap` assert an
invariant and panic on `None`. `Option.get` is the list-nested accessor and passes `None` through;
payload-dispatched accessors do not exist. `Option` also represents a cancelled task join (Clause 8.9).

### 9.2.3 Panic

A panic is the failure form for an invariant violation. It is not recoverable, it is not a value, and it
MUST NOT be used for input validation or for a domain check.

```eko
panic: (String) -> Never
```

`panic` is pure: aborting is not world contact, so the call sets no world bit (Clause 7.9). It aborts
the process without unwinding (Clause 8.4.2). A panic has no handler and no catch form.
The standard panic origins are `panic`; `Option.unwrap`, `Result.unwrap`, `Option.expect`, and
`Result.expect`; the assert family; division by zero; arithmetic overflow; `Int.abs` on `Int.min`;
`String.split` with an empty separator; `Rand.int` with a zero bound; and time arithmetic outside the
`Int` nanosecond domain. Panic output, the `P-` code set, and the exit code are specified in Annex B.
Panic output is runtime output, not a diagnostic.

### 9.2.4 EOF

End of input is not an error. A line read at end of input returns `Ok None`; a sequential read that
reaches end of input returns `Ok` with the available prefix. A line read has type
`Result (Option String) IoError`, so `Ok (Some line)`, `Ok None`, and `Err e` are matched exhaustively.
An empty line is `Ok (Some "")` and is distinct from end of input. `File.read_bytes` reports a short
read as `Ok` with a short `Bytes` and never reports `InvalidData` (Clause 9.5.4). An implementation
MUST NOT report end of input as `Err`, and MUST NOT use an empty string, a sentinel value, or a
distinguished byte sequence as the end-of-input signal.

## 9.3 Bool

```eko
Bool: type = True | False
```

`Bool` is a standard-library ADT with exactly two constructors. There is no truthiness: the condition of
an `if` is a `Bool`, and no other value is coerced to one (Clause 8.3.2). The logical operators `&&` and
`||` are sugar for `if` (Clause 7.8.5); there is no logical function.
`Bool.not` is an ordinary member applied as `b.not()`. There is no prefix negation operator: the token
`!` is reserved for the world bit (Clause 7.9), so `!b` is not a negation and does not type-check.
`==` and `!=` are defined on `Bool` (Clause 9.8.4). `Bool` has no ordering (Clause 9.8.5). `show` and
`inspect` yield `True` and `False`.

## 9.4 Char and String

Text is a sequence of Unicode scalar values. `Char` and `String` are distinct types and do not
interchange.

### 9.4.1 Char

`Char` is a standard-library scalar type (Clause 8.5.1). Its domain is the Unicode scalar values:
`0..0x10FFFF` excluding the surrogate range. One `Char` is exactly one code point; a grapheme cluster is
a sequence of code points and is not a `Char`.

Character escapes are those of String literals plus `\'` (Clause 5.8). `show` yields the character bare;
`inspect` yields the quoted literal form (Clause 9.8.2). `==` and `<` order by code point (Clauses 9.8.4
and 9.8.5). `Char.to_code` is total and yields the code point as `Int`. `Char.from_code` is a domain
check: a value outside the scalar-value domain yields `None` (Clause 9.2.2).

### 9.4.2 String

A `String` is a UTF-8 sequence that is always valid UTF-8. The invariant holds for every constructed
value; no standard-library operation produces an invalid `String`.
The encoding is variable width. Index syntax does not exist for any type (Clause 6.10); `String.at` is
the accessor, has cost O(n), yields `Option Char`, and returns `None` for an out-of-range index.
`String.length` counts Unicode scalar values, not bytes. `String.append` and `++` perform an O(n) full
copy. `String.chars` produces `[]Char` and composes with the `List` namespace; no String-specific
iteration function exists. An empty `String` is `""`. Values are immutable and are represented and
reclaimed under Clauses 8.5 and 8.6.

### 9.4.3 Bridges

The bridge members are `String.chars`, `String.from_char`, `String.from_chars`, `Char.to_code`, and
`Char.from_code`. `Char` is not a one-character `String`. The concatenation of a `Char` and a `String`
is a type error:

```eko
'a' ++ "bc"        // error: `Char` and `String` do not interchange
```

Conversion is explicit. `String.from_char` is the singleton conversion and `String.from_chars` the
batch conversion. Range endpoints are `Int`, `UInt`, and `Char`; `'a'..'z'` iterates code points
(Clause 6.5.6). Byte length is obtained through `String.to_bytes().length()` (Clause 9.5.2); no
`byte_length` member exists. Grapheme cluster segmentation is not part of the standard library.

## 9.5 Bytes

### 9.5.1 Identity

`Bytes` is a standard-library built-in composite: an immutable, contiguous sequence of bytes. It is not
a list; list operations and list patterns do not apply. It is not one of the four composite formers
(Clause 7.5.3).

| Property | `String` | `Bytes` |
|---|---|---|
| Element | Unicode scalar value | `Int` in `0..255` |
| Invariant | always valid UTF-8 | none |
| Access | `at`, O(n), explicit | `get`, O(1), explicit |
| Length | scalar count | byte count |
| Iteration | `chars` produces `[]Char` | `to_ints` produces `[]Int` |
| Literal | string literal | none |

Element values are plain `Int` in `0..255`. Machine words exist for layout and ferrying, never for byte
computation (Clause 10.3.1). `get` is the only entry point; index syntax does not exist (Clause 6.10).
There is no byte literal, and construction goes through functions. Immutability, sharing, and reference
counting follow Clauses 8.5 and 8.6. `Bytes` is not a capability.

### 9.5.2 Services

`Bytes.length` is O(1). `Bytes.get` is O(1) and yields `Option Int`; an index outside the block yields
`None`. `Bytes.append` performs an O(n) full copy; `b1 ++ b2` is defined as `Bytes.append(b1, b2)`.
`Bytes.to_ints` is the bridge to `[]Int`. `Bytes.from_ints` is a domain check: an element outside
`0..255` yields `None`. `Bytes.to_u8s` and `Bytes.from_u8s` are O(1) zero-copy bridges between `Bytes`
and `[]U8`; `from_u8s` returns `Option Bytes`, and every `U8` fits.
`Bytes.to_hex` yields lowercase hexadecimal with no separators. `Bytes.from_hex` is input validation and
returns `Result Bytes ParseError`; an empty string yields `Ok` an empty `Bytes` (Clause 9.5.3).
`String.to_bytes` is total and encodes UTF-8. `Bytes.to_string` is input validation and returns
`Result String ParseError`.
`==` on `Bytes` is structural deep equality, byte by byte (Clause 9.8.4). `<` is byte lexicographic
order (Clause 9.8.5). The empty byte sequence is `"".to_bytes()`; there are no `Bytes.new` and
`Bytes.empty` members. The remaining members and signatures are listed in Annex C.

### 9.5.3 UTF-8 and Hex

The failure taxonomy of Clause 9.2 maps onto the binary domain as follows:

| Operation | Failure form |
|---|---|
| `String.to_bytes` (encoding) | none; total |
| `Bytes.to_string` (decoding untrusted bytes) | `Result String ParseError` with the first invalid offset and its context |
| `Bytes.from_ints` (per-byte construction) | `Option Bytes`; an out-of-range element yields `None` |
| `Bytes.from_hex` (hex parsing) | `Result Bytes ParseError`; odd length or a non-hexadecimal character yields `Err` |

A decode failure such as the byte sequence `ed a0 80` yields `Err` with a payload identifying the first
invalid offset and the surrounding bytes. Exact message wording follows Annex B; the payload shape is
normative.

```eko
s: String = "hi"
b: Bytes = "hi".to_bytes()

Bytes.to_string(s.to_bytes()).eq(Ok s)
Bytes.from_hex(b.to_hex()).eq(Ok b)
```

A decoded `String` is always valid UTF-8, so the first property holds in both directions. The hex
property holds for every `Bytes` value.

### 9.5.4 Byte I/O

`File.read_bytes` reads sequentially at most the requested number of bytes and never reports
`InvalidData`; every byte sequence is legal. A short read means end of input and returns `Ok` with a
short `Bytes` (Clause 9.2.4). `File.read` retains the text gate: non-UTF-8 content yields
`Err InvalidData` (Clause 9.12.2). The raw-byte entry point is `File.read_bytes`; refusal and acceptance
are both available. The `In` console layer is text-only and has no byte gate.
`Fs.with_file` and inverted-binding chains apply to byte operations unchanged (Clause 9.12.4).
`OpenMode` is not extended for bytes (Clause 9.12.1).

### 9.5.5 Display

`show` and `inspect` yield the `Bytes.from_hex` form. The structural channel does not truncate; long
byte strings are printed in full, and the printed form is legal program text that evaluates to the
original value. The binary computation surface is `Int` in `0..255` for element values, the `UInt`
bitwise family for bit operations (Clause 9.6.5), and `0x` or `0b` integer literals for constants
(Clause 5.7). Byte literals, index syntax, mutable `Bytes`, builder or resize vocabulary, encoding
families other than UTF-8, digest or CRC functions, and implicit `Bytes`/`String` conversion are
excluded.

## 9.6 Numeric Types and Conversions

### 9.6.1 Numeric Types

The domain numeric types are `Int`, `UInt`, and `Float`. `Int` is a 64-bit two's-complement integer,
`UInt` is a 64-bit unsigned integer, and `Float` is an IEEE 754 binary64 value (Clause 3).
The machine words `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `F32`, and `F64` are layout and
ferrying types. They have equality, ordering, `show`, and size-family conversions, and no arithmetic or
bitwise operations (Clause 10.3.1).
Numeric literals take their type from the expected type (Clause 5.7.4). Arithmetic is specified in
Clause 7.8.4. Integer overflow panics; it does not wrap. Division by zero panics. `Int.abs` panics on
`Int.min`. `Float` follows IEEE 754: `NaN` and the infinities are values, `NaN` is not equal to itself,
and `-0.0` compares equal to `0.0`. There are no implicit numeric conversions; both operands of a
numeric operator MUST have the same type.

```eko
1 == 1.0           // error: `Int` and `Float` are distinct types; convert explicitly
```

`Int.min`, `Int.max`, and `UInt.max` are the range constants.

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

`Float.to_int` and `Float.to_uint` are not saturating. `NaN` maps to `None`, not to zero, and an
infinity maps to `None`, not to a maximum. `Float.to_uint` checks the sign before truncation; `-0.5`
yields `None` and `-0.0` yields `Some 0`.
Size-family conversions between domain words and machine words follow Clause 10.3.1: domain to machine
integer is `Option` on range, domain to `F32` or `F64` is total with rounding, and machine to domain is
total in the widening directions listed in Annex C.8. A narrowing direction composes a widening
conversion with a checked domain conversion, such as `UInt.to_int`. Conversion names are spelled in
full. The complete family is listed in Annex C.

### 9.6.3 ParseError

```eko
ParseError: type = Empty | Invalid String | OutOfRange
```

`Empty` reports a required value that is missing. `Invalid` reports malformed input and carries a detail
string. `OutOfRange` reports a value outside the limits of the target type. The type is closed and is
not extended by users. `ParseError` is the error payload of the parse family, including `Int.parse`,
`UInt.parse`, `Float.parse`, the base-specific parsers, `Json.parse`, `Time.parse`, and
`Duration.parse` (Clauses 9.9 and 9.7).

### 9.6.4 Round Trips

`parse` and `show` are mutual inverses on the domain numeric types: `parse(show(x)).eq(Ok x)`.

For `Int` and `UInt` the property is exact. For `Float` it is guaranteed by the shortest round-trip form
of `show` (Clause 9.8.3). `NaN` is an exception under IEEE equality: the round trip preserves `NaN`
identity but acceptance cannot be tested with `==`, so a test MUST use `NaN` equivalence (Clause 9.14.3).
`Int.parse` and `UInt.parse` accept an optional sign and decimal digits only. `Float.parse` accepts an
optional sign, decimal digits with an optional fraction and exponent, and the spellings `NaN`, `Inf`,
`-Inf`, and `+Inf`. A leading digit is required; `.5` is not accepted and `0.5` is.

### 9.6.5 Bitwise Family

Bitwise operations are functions on `UInt`, and on `UInt` only. There are no bitwise operators and no
bitwise functions on `Int` or on machine words. `Int` converts through `to_uint` and back.
`UInt.band`, `UInt.bor`, `UInt.bxor`, `UInt.bnot`, `UInt.shl`, and `UInt.shr` are total and compute on
the ring of integers modulo 2^64. Truncation is the semantics, not an overflow. A shift count of 64 or
more yields zero.

### 9.6.6 Wrapping Family

`Int.wrapping_add`, `Int.wrapping_sub`, `Int.wrapping_mul`, `Int.wrapping_neg`, and the `UInt` analogues
compute on the ring of integers modulo 2^64 and never panic. Overflow panics by default (Clause 7.8.4);
a wrapping operation is an explicit function call.

## 9.7 Time

### 9.7.1 Time, Mono, Duration

```eko
Time:     type = Time Int
Mono:     type = Mono Int
Duration: type = Duration Int
```

The payload is a signed 64-bit nanosecond count. `Time` is a wall-clock point: nanoseconds since
1970-01-01T00:00:00Z. `Mono` is a monotonic reading with an arbitrary epoch; only relative meaning is
defined. `Duration` is a signed duration.
The three quantities have mutually exclusive semantics: an instant, a measurement, and a difference. A
wall clock can jump and a monotonic clock cannot; the two readings are distinct types.
All time arithmetic is a named function on values: `Time.sub`, `Time.add`, `Time.to_ns`, `Mono.sub`,
`Mono.add`, `Mono.to_ns`, `Duration.add`, `Duration.mul`, `Duration.neg`, `Duration.secs`,
`Duration.millis`, and `Duration.to_ns`. `Time.sub` and `Mono.sub` yield a `Duration`; `Time.add` and
`Mono.add` shift by a `Duration`; `Duration.add` sums durations and `Duration.neg` negates one; `to_ns`
exposes the signed nanosecond payload; `Duration.secs` and `Duration.millis` construct from the named
unit; `Duration.mul` scales, and a negative factor flips the sign. No time operator exists. No operator
reaches the payload, so arithmetic and unit mixing cannot occur through an operator. Constructor
application constructs a value from nanoseconds (`Duration 500`, `Time 0`); `to_ns` is its inverse.
Every member in this subsection is pure. Equality and ordering are the derived members `T.eq` and `T.lt`, by payload
recursion over the single variant (Clauses 9.8.4 and 9.8.5).

### 9.7.2 Clock Services

The members of this subsection and of Clause 9.7.3 are declared by the capability module (Clause 9.1.1).
Reading the clock sets the world bit: two readings can differ. Sleeping sets the world bit: the process
yields and time passes. Both operations require a `Clock` capability (Clause 7.9.1).
`Clock.now` yields a timestamp. `Clock.mono` yields a monotonic reading; elapsed time and ordering are
measured on `Mono`, not on `Time`. `Time.lt` is reading order; precedence is decided with `Mono.lt`.
`Clock.sleep` returns immediately for a negative duration. Sleep granularity is host-dependent.
Timeouts and cancellation belong to concurrency (Clause 8.9); the standard library adds no timeout
vocabulary.

### 9.7.3 Frozen Clocks

`Clock.frozen(t)` is pure and constructs a clock with constant readings: `now` always yields `t`, `mono`
always yields one fixed reading, and `sleep` returns immediately. The world bit is unchanged: `now` and
`mono` on a frozen clock still set the bit (Clause 7.9.4). Frozen clocks are the deterministic clock
used by tests (Clause 9.14.5). No `Clock.frozen_mono` exists; a clock has one wall-clock reading and one
monotonic reading.

### 9.7.4 Display

| Type | `show` | `inspect` |
|---|---|---|
| `Time` | ISO 8601 UTC form `2026-10-22T01:29:00.123Z`, fractional seconds as needed, at most nine digits | `Time 1792632540123000000` |
| `Duration` | decimal seconds `0.5s`, trailing zeros removed | `Duration 500000000` |
| `Mono` | `Mono 89347123456` | `Mono 89347123456` |

`Time.show` and `Duration.show` are standard-library overrides of the default display machinery
(Clause 9.8.2). `inspect` is always the constructor form, so the displayed value is writable. UTC is
the only zone. `Time.parse` accepts the ISO 8601 UTC forms that `Time.show` produces, and
`Duration.parse` accepts decimal seconds. Both return a `Result` whose error type is `ParseError` and
satisfy the round-trip property of Clause 9.6.4.

### 9.7.5 Domain and Overflow

The payload is a signed 64-bit nanosecond count: a span of approximately 292 years in each direction.
The wall-clock domain is approximately 1677-09-21 to 2262-04-11, and `Duration` has the same span.
Arithmetic outside the domain panics under the arithmetic rule (Clause 7.8.4). A panic does not set the
world bit (Clause 9.2.3).
Leap seconds are not represented. The POSIX epoch maps directly, a day is always 86400 seconds, and
alignment policy belongs to the host.
The following are outside the time surface: a single type merging wall-clock and monotonic readings,
time operator sugar, a local timezone or calendar family (`Date`, `DateTime`), and a public construction
right for `Clock`. Capabilities are opaque (Clause 7.9.1); `Clock.frozen` is a pure derivation, not a
constructor for arbitrary clocks.

## 9.8 Derived Services

### 9.8.1 show and inspect

| Channel | Name | Semantics | Overridable |
|---|---|---|---|
| display | `show` | human-readable rendering | yes, by a namespace declaration |
| structural | `inspect` | faithful literal form | no; always derived |

`show` is the display channel used by string interpolation and logging. `inspect` is the structural
channel used by diagnostics, tests, and debugging. Both are available as free functions and as
type-namespace members: `show(t)`, `t.show()`, `Money.show(m)`, `inspect(t)`, `t.inspect()`.

### 9.8.2 Default show and Overrides

The default `show` is `inspect` with the top-level `String` quotes removed. A `String` nested inside a
composite keeps its quotes. Interpolation uses `show`: `"{name}"` produces the bare name.
A type's declaring module MAY override `show` with an ordinary namespace declaration. The override adds
no syntax and follows namespace ownership (Clause 7.4.5). An override outside the declaring module is
an error (`E-namespace-owner`, Annex B); orphan overrides do not exist. A type without an override
takes the default.

```eko
Money: type = [cents: Int, cur: String]

Money.show: (m: Money) -> String = "{m.cur} {m.cents}"

m: Money = {cents: 12500, cur: "USD"}
"{m}"          // USD 12500
inspect(m)     // {cents: 12500, cur: "USD"}
```

### 9.8.3 inspect Forms

| Type | `inspect` |
|---|---|
| scalars | `3`, `0.5`, `NaN`, `True`, `'a'` |
| `String`, `Bytes` | `"wd"`, `Bytes.from_hex("4142")` |
| list, tuple, dictionary, record | `{1, 2, 3}`, `{"1": 1}`, `{name: "wd"}` |
| ADT | constructor application: `Man`, `Some 3`, `Err "x"` |
| `_`, capability, function | `_`, `<cap>`, `<fn>` |

Empty containers are `{}`. Record fields appear in declaration order. Dictionaries appear in insertion
order, and iteration follows that order. The time types are specified in Clause 9.7.4.

### 9.8.4 Equality

`==` and `!=` are defined on exactly the value layer: `Int`, `UInt`, `Float`, the machine words, `Bool`,
`Char`, `String`, and `Bytes`. On this domain `a == b` is identical to `a.eq(b)`, and the `eq` and `ne`
names are compiler-owned.
For every other type, equality is the member `T.eq`: records, tuples, ADTs, `Option`, `Result`, the time
types, `Range`, `Set`, `Dict`, `List`, and `Json`. `T.ne` is the negation of `T.eq`. Both operands MUST
have the same type; a comparison between different types is a compile error.
`T.eq` is derived structurally and materialized on demand. Records and tuples compare field by field in
declaration order with short-circuit. ADTs compare when constructors match and payloads compare
recursively. Containers compare element by element. Each field or payload compares through that type's
`eq` member, so derivation composes at every level.
A declaration `T.eq:` in the module that declares `T` replaces the derivation entirely (Clause 7.4.5).
Derivation halts at capabilities and functions, recursively: a type with a capability or function field
has no `eq`, and equality, keying, or assertion over it is a compile error (`E-no-eq`, Annex B).
`Float.eq` follows IEEE 754: `NaN == NaN` is `False`. This rule also governs `Float` dictionary keys
and `Set` elements (Clause 9.10.4).

### 9.8.5 Ordering

`<`, `<=`, `>`, and `>=` follow the same shape as equality. They are defined on the value layer with an
arithmetic or textual order: the numeric types and machine words, `Char` by code point, `String`
lexicographically, and `Bytes` by byte order. They are compiler-owned members.
`Bool` has no ordering; `True` and `False` are not ordered. Partition with `filter`.
An ADT derives `T.lt` by declaration order, comparing payloads recursively in order.
`Sex: type = Man | Woman` implies `Man.lt(Woman)` is `True`. The derived member backs no operator; the
call is `a.lt(b)`.
Records have no derived ordering. A field set has no magnitude semantics; sorting a record collection
requires `sort_by` and an explicit comparator (Clause 9.10.1). A hand-written `T.lt` is an ordinary
member and is called explicitly.

### 9.8.6 with

`with` constructs a record from an existing record and a partial field literal. The partial field
literal is legal only in the argument position of `with` (Clause 6.6.3). It is checked layer by layer
against the field types; a misspelling is an error.

The second parameter type of `T.with`, `(t, partial fields) -> t`, is not expressible in the type
syntax. `with` is a compiler-generated service whose type is a localized exception.
The original value is unchanged. `with` is the record update vocabulary; `insert` and `remove` belong to
dictionaries, and `append` and `merge` belong to the body of `++`.

### 9.8.7 Dispatch

`show`, `inspect`, `eq`, `lt`, `with`, `to_json`, and `from_json` are compile-time members of every type
namespace: the declaration where the type declares one, the derived default otherwise. World bits do
not participate; every one of these services is pure. The free function `show(t)` and the member
`t.show()` denote the same call, and `inspect(t)` and `t.inspect()` likewise. On the value layer `a == b`
and `a.eq(b)` coincide, and interpolation applies `show`. `to_json` and `from_json` follow the same
dispatch model and admit no override (Clause 9.9.5). Dispatch
is compile-time and uses no runtime reflection (Clause 8.7.2).

## 9.9 JSON

### 9.9.1 The Json Type

```eko
Json: type = Null | Bool Bool | Int Int | Float Float | Str String | Arr []Json | Obj ([String]Json)
```

`Json` is a standard-library ADT with seven variants. `Obj` is a string-keyed dictionary that preserves
insertion order; `Json.show` follows that order, so JSON text is reproducible. Construction is
check-only: constructors are applied by juxtaposition, and literals land by expected type
(`Obj {"a": Int 1}`). `Int` and `Float` are separate variants. An integer literal remains `Int` on a
round trip; the variant, not the surface spelling, decides the interpretation.

### 9.9.2 Parsing and Showing

```eko
Json.parse: (String) -> Result Json ParseError
Json.show:  (Json) -> String
```

`Json.parse` is input validation and reuses `ParseError` (Clauses 9.2 and 9.6.3). Its payload identifies the
offset and context of the first error. `Json.show` produces the minimal form with no whitespace. It is a
standard-library override of `show`; interpolation `"{j}"` produces JSON text directly. `inspect(j)`
remains the constructor form (Clause 9.8.3).
A `Float` uses the shortest round trip: `3.5` prints `3.5` and `3.0` prints `3.0`. An `Int` prints
without a fractional part. String escapes follow RFC 8259, including `\uXXXX`. Surrogate pairs are
parsed as pairs; a lone surrogate is rejected. Every `String` is valid UTF-8.
`parse(show(j)).eq(Ok j)` holds. Non-finite floats render as `NaN`, `Inf`, and `-Inf`, following the
`Float` display vocabulary, and `Json.parse` accepts those tokens back. These spellings are an Eko-side
extension of strict JSON text; conversion for strict external consumers is a library concern.

### 9.9.3 Derived Mapping

Every data type derives `to_json: (t) -> Json` and `from_json: (Json) -> Result t ParseError` by
default.

| Eko type | JSON form |
|---|---|
| record, string-keyed dictionary | `Obj` with field names as keys |
| list, tuple, `Set` | `Arr` in iteration order |
| `String`, `Char`, `Bytes` | `Str`; `Bytes` in lowercase hexadecimal |
| `Int`, `Float`, `Bool`, `_` | number, `true` or `false`, `null` |
| ADT | external tagging (Clause 9.9.4) |
| non-string-keyed dictionary, capability, function | no default; compile error (`E-json-shape`) |

Every value whose type participates satisfies `from_json(to_json(v)).eq(Ok v)`. A missing field, an
extra key, an unknown variant, or a payload type mismatch yields `Err`. Capabilities and functions have
no equality (Clause 9.8.4) and do not enter the mapping. `Bool` is special-cased to JSON booleans. A
user-declared enumeration still uses external tagging as strings; a declared type does not inherit the
`Bool` mapping. `Bytes` maps to a lowercase hex string, the same basis as `to_hex` and `from_hex`
(Clause 9.5.3); the field type determines decoding.

### 9.9.4 External Tagging

An ADT maps by external tagging. A zero-payload variant maps to a JSON string. A payload-bearing variant
maps to a single-key object: a single payload is embedded directly, and multiple payloads are an array
under the key (for example `Ok 3` maps to `{"Ok": 3}`).

`from_json` reports an unknown variant or key, a missing field, or a payload type mismatch as `Err`,
with an `Invalid` payload naming the unknown variant where applicable. Constructor fields are checked at
compile time, so a misspelling is a compile error (Clause 6.6).

### 9.9.5 Derivation and Generics

The derived pair is fixed. `to_json` and `from_json` are derived for every suitable type and admit no
override. The pair is an involution: `from_json(to_json(x))` yields `x`. A representation choice such
as field renaming, omission, or versioning is a protocol decision. It is written as a named codec pair
in the protocol's module; it is not an override of the derived pair.
`to_json` and `from_json` join the type-variable capability set (Clause 7.6.6). A generic function MAY
call them on a value of type `t`; at instantiation the concrete derivation or declaration applies. A
capability, a function, or a non-string-keyed dictionary at an instantiation site is an error, by the
same rule as `eq`. Dispatch is compile-time generic dispatch (Clause 9.8.7). There is no runtime type
information and no reflection surface.
Pretty printing, path queries, and format-string DSLs are outside the standard library. YAML and TOML
are outside the standard library; JSON is the only built-in serialization surface.

## 9.10 Collections

### 9.10.1 List

A list is a homogeneous, immutable sequence with structural sharing. Patterns are inductive
(`{h, ..t}`), and `List.get` is O(1). `List.append` is O(|xs|) new nodes with the right operand shared
(Clause 8.10).
`List.map` preserves order and length, `List.filter` preserves order, and `List.fold` proceeds left to
right. `List.each` and `List.each_with_index` visit in order. `List.any` and `List.all` short-circuit.
`List.reverse` is total. `List.take` and `List.drop` truncate: a count beyond the length takes or drops
everything, and a negative count counts as zero.
`List.map`, `List.filter`, `List.fold`, `List.each`, `List.any`, `List.all`, `List.find_by`,
`List.take_while`, `List.drop_while`, `List.sort_by`, and `List.zip_with` are open-bit dispatchers; a
pure callback makes the call pure, and a world-touching callback makes it world-touching (Clause 7.9).
Iteration combinators and tail positions follow Clause 8.8.
`List.slice` takes a start and a length and yields `Option []t`. It never clamps: a request that does
not lie entirely inside the list yields `None`. `List.join` concatenates the derived `show` forms with a
separator; an element type without `show` is a compile error, by the same rule as interpolation.
`List.sort` uses the derived `<` ordering; a record has no `<`, so instantiating `List.sort` on a record
is a compile error. `List.sort_by` takes an explicit less-than comparator. Both sorts are stable, and
`List.sort_by` runs in O(n log n). An inconsistent comparator terminates with an implementation-defined
order (Clause 2.5).

### 9.10.2 Dict

A dictionary is an immutable, persistent mapping from a key domain to values (Clause 9.10.4).
`Dict.get` is O(1) and yields `Option v`. `Dict.insert` and `Dict.remove` return a new dictionary;
removing an absent key returns the original. `Dict.merge` and `++` return a dictionary in which the
right operand wins on duplicate keys.
`Dict.map`, `Dict.filter`, `Dict.fold`, and `Dict.each` dispatch on the key and value in insertion order.
Iteration over a dictionary follows insertion order; this order is part of the observable result of
`to_list`, `show`, and JSON rendering of a string-keyed dictionary. Dictionaries are persistent
structures. The magnitude of `Dict.merge` is implementation-defined (Clause 8.10).

### 9.10.3 Set

`Set t` is an ordinary nominal standard-library generic type. It has no notation and no literal; the
four composite forms are unchanged.
`Set.of` constructs a set from a list and deduplicates, keeping the first occurrence. `Set.to_list`
yields insertion order. `Set.contains` is a domain check. `Set.insert` and `Set.remove` never mutate the
receiver; a no-op returns the original set. `Set.union` preserves the left order first and appends new
right-set elements in right order. `Set.intersect` and `Set.diff` preserve left order. `Set` derives
`eq` structurally. It has no `<`; structural equality carries no ordering, by the same rule as for
`Dict`.
The empty set is `Set.of({})` with an expected type supplying the element type. `Set.empty: () -> Set t`
violates the result-only prohibition on type variables (Clause 7.6.5); no such member exists.

### 9.10.4 Key Domains

The key domain is the set of data types that have `eq`. Structural equality is key equality. Lists,
records, tuples, ADTs, `Bytes`, `Set`, and dictionaries themselves are admissible keys; the type
`[[]String]Int` is well formed. Capabilities and functions are excluded; their equality derivation
halts (Clause 9.8.4).
A `Float` key follows IEEE equality. Since `NaN != NaN`, a `NaN` key always misses. `0.0` and `-0.0`
compare equal and are the same key.
The element domain of `Set` is the same key domain. A dictionary with a non-string key domain has no
default JSON mapping (`E-json-shape`, Annex B).

## 9.11 Text Utilities

### 9.11.1 String

`String.contains`, `String.starts_with`, and `String.ends_with` test substring, prefix, and suffix
presence. The empty string is contained in every string.
`String.replace` replaces every occurrence; `String.replace_first` replaces the first occurrence only.
`String.slice` takes a start and a length in the Unicode scalar domain, yields `Option String`, and never
clamps; a request that does not lie entirely inside the string yields `None`. Its cost is O(n).
`List.take` and `List.drop` truncate; `String.slice` does not.
`String.trim`, `String.trim_start`, and `String.trim_end` remove Unicode `White_Space` at the requested
ends.
`String.split(s, sep)` splits on every occurrence of `sep` and keeps empty fields, whether leading,
trailing, or between adjacent separators. The separator MUST be non-empty; an empty separator is a panic
(Clause 9.2.3). `String.lines` splits on `'\n'` and drops the final empty element when the input ends
with `'\n'`.

```eko
"a,b".split(",")       // {"a", "b"}
",,x,".split(",")      // {"", "", "x", ""}
"a\nb\n".lines()       // {"a", "b"}
```

Unicode case mapping and regular expressions are not part of the standard library.

### 9.11.2 Path

Paths are `String` values. The path vocabulary is pure and lexical: `Path.join` joins two components
with the host separator; `Path.normalize` collapses separators and `.`, and resolves `..` lexically;
`Path.is_absolute` applies the platform rule.
`Path.normalize` is syntax, not semantics: it does not consult the file system and cannot know whether a
component is a symbolic link. Resolution against the working directory and through symbolic links is
`Fs.realpath` (Clause 9.12.6). No `Path` nominal type exists.

## 9.12 File System and Handles

Every member and every type of this clause is declared by the capability module (Clause 9.1.1).

### 9.12.1 OpenMode

```eko
OpenMode: type = Read | Write | Append | New | Update
```

| Variant | Meaning |
|---|---|
| `Read` | open an existing file for reading; absence yields `Err NotFound` |
| `Write` | create or truncate |
| `Append` | create or append |
| `New` | create only if absent; an existing path yields `Err Exists` |
| `Update` | read and write without truncation |

The five variants cover the open surface. There is no flags integer and no separate rights datum.

### 9.12.2 File Operations

`Fs.open` yields an opaque `File` handle. `File.read` reads sequentially at most the requested number of
characters and rejects non-UTF-8 content with `Err InvalidData`. `File.read_bytes` is the raw-byte
counterpart and never reports `InvalidData` (Clause 9.5.4). `File.read_line` reads one line without its
terminating `'\n'` and reports end of input as `Ok None` (Clause 9.2.4). `File.write` and
`File.write_bytes` write at the current position. `File.seek` sets an absolute position from the start;
a position past end of file yields `Err InvalidData`. `File.flush` and `File.close` complete the
surface.
Every operation in this subsection is world-touching and sets the bit (Clause 7.9). `File` is not
constructible by a program; it is obtained from `Fs.open` or `Fs.with_file`. The `Fs` namespace also
provides whole-path mirrors: `Fs.read`, `Fs.write`, `Fs.append`, `Fs.read_bytes`, `Fs.append_bytes`,
and `Fs.exists`. The text mirrors retain the UTF-8 gate; the byte mirrors do not. `Fs.exists` yields
`Bool`.
`IoError` is the closed error union of the file-system surface and carries the reason required by
Clause 9.2.1:

```eko
IoError: type =
    | NotFound
    | Permission
    | Exists
    | NotFile
    | Closed
    | InvalidData String
    | Other String
```

`NotFound` reports an absent path. `Permission` reports denied access. `Exists` reports an exclusive
creation over an existing path. `NotFile` reports a directory or other non-regular file where a regular
file is required. `Closed` reports a stale handle (Clause 9.12.3). `InvalidData` carries a content
detail, including non-UTF-8 text. `Other` carries a system message for failures without a dedicated
variant.
`IoError` is closed and its variants are fixed. Detail failures such as a non-empty directory passed to
`rm_dir` or a cross-device rename travel through `Other String`. The runtime retries interrupted system
calls internally; `Interrupted` never surfaces.

### 9.12.3 Generations

A handle carries a generation. `File.close` increments the generation of the slot, and every older copy
of the handle becomes stale. Any operation on a stale handle, including a second `close`, yields
`Err Closed`.
Leaks are permitted. Without `close`, the operating system reclaims the resource at process exit. No
finalizer is promised (Clause 8.6). Staleness is decided by the language-level generation, not by the
number the operating system assigns to a descriptor; descriptor reuse by the host cannot make a stale
handle valid.

### 9.12.4 Fs.with_file

```eko
Fs.with_file: (Fs, String, OpenMode, (File) !-> a) !-> Result a IoError
```

`Fs.with_file` opens the path, applies the callback, and closes the handle on both the value path and
the error path. A panic is an abort: the operating system reclaims the resource and no unwinding
occurs.
The callback type is `(File) !-> a` and has no open bit. `Fs.with_file` is world-touching, so `OR`
absorbs the callback's bit (Clause 7.9.5); a pure callback flows in by bit adaptation and nothing is
generalized.
A fallible callback produces a nested result: the callback's `Result` is the payload of the outer
`Result`, which covers opening and closing the channel. A single error channel is obtained by handling
the callback's failure and ending in `_`, or by flattening the two levels with a `<-` chain
(Clause 8.2.4).

### 9.12.5 Attenuation

There is no `Rights` type. The attenuation combinators are `Fs.root` and `Fs.read_only`.

`Fs.root` restricts a file-system capability to a subtree and does not check existence. `Fs.read_only`
withdraws write rights. Both are pure and set no bit (Clause 7.9). They compose, and they only narrow
the grant: `Fs.read_only(Fs.root(fs, "/tmp/work"))`.

### 9.12.6 Directory Operations

`Fs.list_dir` yields the entry names in deterministic lexicographic order, excluding `.` and `..`. A
path that is not a directory yields `Err NotFile`. The result contains names, not paths; join with
`Path.join(dir, name)` (Clause 9.11.2).
`Fs.mkdir` creates one level; an existing path yields `Err Exists`. `Fs.mkdir_all` creates the whole
chain; an already existing directory yields `Ok`. `Fs.rm` removes a file; a directory yields
`Err NotFile`. `Fs.rm_dir` removes an empty directory; a non-empty directory or a non-directory yields
`Err` with `Other`. `Fs.rename` renames or moves; a cross-device move and similar failures yield `Err`
with `Other`. `Fs.stat` yields `FileInfo` (Clause 9.12.7). `Fs.realpath` resolves `.`, `..`, and
symbolic links through the file system; a relative input is resolved against the process working
directory. The result of reading platform metadata such as symbolic links is implementation-defined
(Clause 2.5).
All operations in this subsection set the world bit (Clause 7.9). Error mapping uses the closed
`IoError` of Clause 9.12.2 and adds no variant, so existing exhaustive matches remain exhaustive.

### 9.12.7 FileInfo

```eko
FileInfo: type = [size: UInt, mtime: Time, is_dir: Bool]
```

`size` is the file size in bytes. For a directory, `size` is a platform-defined value. `mtime` is a
wall-clock time (Clause 9.7.1); its display form is ISO 8601 UTC (Clause 9.7.4). `is_dir` reports
whether the path is a directory.

## 9.13 Formatting and Mathematics

### 9.13.1 Formatting

Formatting is performed by named functions. `show` and string interpolation remain the shortest
round-trip display channel; there is no format-string language and no second interpolation channel.
`Int.to_hex`, `Int.to_oct`, and `Int.to_bin` yield the shortest lowercase form for non-negative values.
A negative value is rendered in 64-bit two's-complement form. The `UInt` family is the unsigned mirror.
`Int.parse_hex`, `Int.parse_oct`, and `Int.parse_bin` are input validation and return
`Result Int ParseError`. They accept no prefix, accept an optional sign, and are case-insensitive. The
`UInt` family mirrors them and shares `ParseError`.
`Float.to_fixed` renders fixed point with a requested number of fractional digits. `Float.to_exp`
renders scientific notation with a requested number of mantissa digits. Both use nearest decimal
rounding with ties away from zero. Both are total: `NaN` and the infinities render as themselves, and
formatting never returns `Result`.
`String.pad_left` and `String.pad_right` pad to a requested width and never truncate; a string already
at the requested width is returned unchanged. Fixed width hexadecimal is `T.to_hex_pad` on machine
words; it zero-pads to at least the requested number of digits and never truncates.

The complete formatting inventory is listed in Annex C.

### 9.13.2 Float Mathematics

`Float.sqrt`, `Float.exp`, `Float.log`, `Float.log2`, and `Float.log10` are total functions.
Out-of-domain input is a value, not an error: `sqrt(-1)` is `NaN`, `log(0)` is `-Inf`, and `log(-1)` is
`NaN`.
`Float.sin`, `Float.cos`, `Float.tan`, `Float.asin`, `Float.acos`, and `Float.atan` operate in radians;
`asin(2)` is `NaN`. `Float.atan2` is quadrant-correct. `Float.pow` follows IEEE 754; `pow(0, 0)` is `1`
and `pow(1, Inf)` is `1`.
`Float.is_nan`, `Float.is_inf`, and `Float.is_finite` are predicates. `Float.is_inf` does not
distinguish signs. `Float.abs`, `Float.floor`, `Float.ceil`, `Float.trunc`, and `Float.round` are total;
`trunc` rounds toward zero and `round` rounds half to even.
`Int.abs` follows the arithmetic rule and panics on `Int.min`, whose absolute value is not representable.
No wrapping form exists.
The checked gate sits at the conversion boundary: `Float.to_int` and `Float.to_uint` return `None` for
`NaN`, the infinities, and out-of-range values (Clause 9.6.2). Inside the value domain, operations are
total. The maximum error bound of the floating-point library functions is implementation-defined
(Clause 2.5). Determinism is claimed per platform, not across platforms.
Bignum arithmetic, complex numbers, and matrices are not part of the standard library.

## 9.14 Testing

### 9.14.1 The test Slot

A `test` binding binds a test, does not enter the module interface, and is compiled only for a test
root. The binding MUST appear at module top level and MUST use `=`. Rebinding and inverted binding do
not apply.
A test body is an ordinary block or a single expression on the same line. Its value is discarded. It has no arrow and no world bit to
reconcile; world-touching calls inside it are evaluated under Clause 7.9 without exemption. Test names
are snake_case. A duplicate test name within a module is a compile error (`E-name-conflict`, Annex B).
The report name is `module.test_name`.
The slot word `test` is not a reserved word. It is recognized only in the annotation position after `:`;
a module named `test` and a local binding named `test` remain legal.

### 9.14.2 Location

There is no test module kind. A module containing `test` bindings is an ordinary module, and `test` is
an additional class of top-level binding.
Tests reside in the module under test. A test shares the module scope with top-level bindings, so a
private function is directly callable. No binding is made public for testing, and the interface is
unaffected.
A test helper is a private top-level binding with a leading `_`. Test bindings are absent from non-test
compilation; a helper used only by tests raises no warning.
A cross-file test is an ordinary module plus `import`. Only the public surface of the target is
reachable there; private names are tested in their own module. A test-file suffix is not part of the
language.

### 9.14.3 Assertions

```eko
assert:    (c: Bool) -> _
assert_eq: (a: t, b: t) -> _
assert_ne: (a: t, b: t) -> _
```

`assert` panics when its argument is `False`. `assert_eq` panics when the arguments are not equal;
`assert_ne` panics when they are equal. All three are standard-library free functions in the implicitly
opened scope (Clause 9.1.2). They are pure, always enabled, and require no configuration.
`assert_eq` and `assert_ne` lock both arguments to the same type. `==`, `inspect`, and the structural
derivation are compile-time generic dispatch (Clause 9.8.7). Capabilities and functions have no `==`, so
an assertion over them is a compile error (`E-no-eq`, Annex B).
A failure message uses `inspect`, the structural channel, which is never overridable. A user-supplied
failure message is written with `panic` and an explicit message. Assertion failure is a panic. The host
running the tests captures the panic per test and records that test as failed; outside a test run, a
panic aborts the program (Clause 8.4.2). Panic output follows Annex B.

### 9.14.4 Table-Driven Tests

A table-driven test is a list of rows together with the `each` dispatcher. The table literal carries an
annotation; a composite literal does not stand alone (Clause 6.6.2). The `each` callback takes one
parameter, so tuple elements are not destructured automatically; destructuring occurs on a binding line
(Clause 6.3.2).

```eko
// context: `Sex` and `_parse_sex` are declared by the module under test.
parse_corners: test =
    rows: [][String, Result Sex String] =
        {{"man", Ok Man}, {"woman", Ok Woman}, {"x", Err "unknown sex: x"}}
    rows.each((row):
        (s, want) = row
        assert_eq(_parse_sex(s), want))
```

The first failure panics and stops the test; that test is recorded as failed. A multi-parameter callback
such as `(p, i):` is a parameter group, not destructuring (Clause 6.4.3). Tuple splitting is explicit,
on a binding line or in a `case` pattern (Clause 6.8).

### 9.14.5 Determinism

Code under test consumes capabilities; a test supplies derived capabilities. Mocking is performed by
capability derivation; no separate framework exists. The derivation combinators of this subsection are
members of the capability namespaces and are declared by the capability module (Clause 9.1.1).

| Derivation | Behavior |
|---|---|
| `Out.null` | swallows every write |
| `Out.tee` | writes once and reports once; assertions ride along with the stream |
| `In.empty` | every read is end of input (`read_line` yields `Ok None`) |
| `In.from_lines` | yields the given lines in order, then end of input |
| `Fs.null` | every path yields `Err NotFound` |
| `Env.frozen` | reads the fixed table |
| `Args.frozen` | reads the fixed argument vector |
| `Clock.frozen` | reads the fixed time; `sleep` returns immediately |
| `Rand.with_seed` | the seed fixes the order; the sequence itself is not promised across versions |

`Out.tee` has signature `(Out, (String) !-> _) -> Out`. Construction is pure. The observer fires once
per write, and its bit is absorbed by the `!` of the bundle's write closure (Clause 7.9.5). A pure
observer flows
in by adaptation.
Tests cannot obtain real capabilities. Construction right belongs to the host, and `main`'s parameter
table is the only entrance (Clause 7.9.1); a test has no parameter table. Derived combinators are the
test's entire world. Integration tests against real input and output belong to the host.
The world bits of the functions under test are unchanged. A test feeds a real capability value; it
relaxes no check and uses the same compiler and the same rules as ordinary code.

### 9.14.6 Roots and Execution

A test root is a module containing at least one `test` binding. The runner collects tests by root,
compiles and executes root by root, and captures panic per test. The dependency closure is the ordinary
acyclic module graph (Clause 7.2.4). Panic capture is a process boundary: the runner reads the panic
block from the child's standard error and parses it under Annex B.
`test` bindings do not enter the interface, so separate compilation is unaffected. A module without
tests is an ordinary dependency under test compilation and produces no test surface. There are no
ambient tests. `W-unused` is counted over all compilation roots, test roots included; parameters do not
participate (Annex B).
The scheduling order of tests is implementation-defined (Clause 2.5). Parallelism is host domain. Tests
share no state: values are immutable and the top level is pure, so parallel execution is permitted.

## 9.15 Library Exclusions

The following are not part of the standard library:

- a format-string or printf-style DSL;
- regular expressions;
- Unicode case mapping;
- an encoding family other than UTF-8, including UTF-16, latin-1, and GBK;
- byte literals, index syntax, mutable `Bytes`, and builder or resize vocabulary;
- digest or CRC functions;
- bignum arithmetic, complex numbers, and matrices;
- YAML and TOML;
- pretty printing as a default or as a `show` form;
- a local timezone, `Date`, or `DateTime`;
- `Set` notation and `Set` literals;
- a payload-dispatched accessor, or a compound word such as `get_or`: `Option.get` is an ordinary
  function, and `get` composes with `unwrap_or`;
- a `Rights` type or a rights datum;
- an FFI capability;
- world-touching free functions and ambient input or output;
- a test framework, a test-file suffix, a test module kind, a second compiler mode, attribute
  annotations, `describe`/`it`, nested suites, setup or teardown hooks, a mock framework,
  parameterized-test syntax, a test bit exemption, and relaxed checking;
- unstable sorting as an option;
- runtime reflection over types.

Additions to the library inventory are specification changes (Clause 9.1.4).
