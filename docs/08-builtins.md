# Built-in Data Types and Services

This chapter specifies the data types and services that the prelude provides without an import (05-names.md §11): `Bool`, `Char` and `String`, `Bytes`, the numeric conversion families, `Time`/`Mono`/`Duration`, the derived services `show`/`inspect`/`==`/`<`/`with`, and the JSON surface with `to_json`/`from_json`. Values of these types are immutable. Signatures, costs, and failure forms stated here are normative. A `!` in a signature marks a world-touching service (06-effects.md §5); the unit type is spelled `_`.

## 1. Bool

```eko
Bool: type = True | False
```

`Bool` is a prelude ADT. There is no truthiness: the condition of an `if` is a `Bool`, and no other value is coerced to one (03-control.md §4). `not` is an ordinary function member of `Bool`, applied as `b.not()`; there is no prefix negation operator. The token `!` is reserved for the world bit (02-syntax.md §6.3, 06-effects.md §5), so `!b` is not a negation and does not type-check. Equality is structural and derived (§6.4). Ordering is deliberately not defined for `Bool` (§6.5).

## 2. Char and String

Text is a sequence of Unicode scalar values. Eko types characters and strings separately: `Char` is one scalar value, `String` is a UTF-8 sequence, and the two do not interchange.

### 2.1 Char

`Char` is a prelude scalar type. Its domain is the Unicode scalar values (USV): `0..0x10FFFF`, excluding the surrogate range. One `Char` is exactly one code point; a grapheme cluster is a sequence property and is not a `Char`.

```eko
c: Char = 'a'
accented: Char = 'é'      // non-ASCII written directly
nl: Char = '\n'           // escapes as in String, plus \'
```

`show('a')` yields `a` (top-level bare, the interpolation channel); `inspect('a')` yields `'a'` (literal form). This is the single `String` rule applied in parallel (§6.3). `==` and `<` are built in, ordered by code point.

### 2.2 String

A `String` is a UTF-8 sequence with the invariant that it is always valid UTF-8. The encoding is variable width, so O(1) indexing does not physically exist:

- `s[i]` does not exist. Index syntax does not exist anywhere in the language: lists and dictionaries use `get`, and strings additionally have the explicit O(n) `at` (02-syntax.md §7).
- `String.at: (String, Int) -> Option Char`. The cost is O(n) and is explicit in the service. An out-of-range index yields `None`.
- `String.length: (String) -> Int` counts scalar values, not bytes: `"héllo".length() == 5`. Counting bytes would hide a unit error.
- Character-level iteration uses the bridge: `"abc".chars().map(f)`. `chars` produces `[]Char` and composes with the `List` namespace, so there are no String-specific iteration functions.

### 2.3 Bridges

```eko
String.chars:      (String) -> []Char
String.from_char:  (Char) -> String         // single character
String.from_chars: ([]Char) -> String       // batch
Char.to_code:      (Char) -> Int            // code point
Char.from_code:    (Int) -> Option Char     // invalid code point → None
```

- `Char` is not a one-character `String`: `'a' ++ "bc"` is a type error; conversion is explicit.
- Range endpoints are `Int`, `UInt`, and `Char`: `'a'..'z'` iterates code points.
- Grapheme clusters are library domain (09-library.md §4).
- Byte length is obtained as `s.to_bytes().length()` (§3.2); `byte_length` is not a word.

## 3. Bytes

### 3.1 Identity

`String` is text and `Bytes` is bytes; the only gate between the two is UTF-8 encoding and decoding (§3.3). `Bytes` is a prelude built-in composite: an immutable sequence of bytes, in the same family as `String` and not one of the four composite formers (04-types.md §2). There is no byte literal; construction goes through functions. Element values are plain `Int` in `0..255` (domain words); machine words (`U8`, `I32`, …) exist for layout and ferrying, never computation (`11-ffi.md §3.1`). `get` is the only entry point, and index syntax is banned as for every type in the language. Immutability, boxing, sharing, and reference counting follow 07-runtime.md §4 with no new rule. The name is PascalCase; `<bytes>` is not used because `Bytes` has structure and is not a capability.

| | `String` | `Bytes` |
|---|---|---|
| Element | `Char` scalar | `Int` in `0..255` |
| Promise | always valid UTF-8 | none; arbitrary bytes |
| Access | `at`, O(n), explicit | `get`, O(1), explicit |
| Length | `length` counts scalars | `length` counts bytes |
| Iteration | `chars -> []Char` | `to_ints -> []Int` |
| Literal | `"…"` | none; construct via functions |

`Bytes` is a contiguous block, not a list; element values are plain `Int` 0..255. A list contract promises structural sharing and inductive patterns (`{h, ..t}`), while a byte block needs contiguity and offsets; sharing one name for both would split the cost model inside one type. A dedicated element type with identity is available as an ordinary user declaration, and byte computations such as `b1 * 256 + b2` stay in `Int`.

### 3.2 Services

| Service | Signature | Semantics and cost |
|---|---|---|
| `Bytes.length` | `(Bytes) -> Int` | byte count; O(1) |
| `Bytes.get` | `(Bytes, Int) -> Option Int` | out of range `None`; O(1) |
| `Bytes.append` | `(Bytes, Bytes) -> Bytes` | concatenation; O(n) full copy |
| `Bytes.to_ints` | `(Bytes) -> []Int` | bridge; iteration uses `List` combinators |
| `Bytes.from_ints` | `([]Int) -> Option Bytes` | per element `0..255`; out of range `None` (domain check) |
| `Bytes.to_u8s` | `(Bytes) -> []U8` | zero copy; O(1) — both sides immutable |
| `Bytes.from_u8s` | `([]U8) -> Option Bytes` | zero copy; O(1); per element fit is guaranteed by the element type |
| `Bytes.to_hex` | `(Bytes) -> String` | lowercase, contiguous; O(n) |
| `Bytes.from_hex` | `(String) -> Result Bytes ParseError` | input validation; empty string → `Ok` an empty `Bytes` |
| `String.to_bytes` | `(String) -> Bytes` | UTF-8 encoding; total function |
| `Bytes.to_string` | `(Bytes) -> Result String ParseError` | UTF-8 decoding; failure payload carries the first invalid offset and context |

- `++` is extended to `Bytes`: `b1 ++ b2 ≡ Bytes.append(b1, b2)`. Precedence and associativity follow `++` (02-syntax.md §6).
- `==` is structural deep equality, byte by byte (§6.4). `<` is byte lexicographic order and is a hand-defined builtin (§6.5); `sort_by` is available.
- The empty byte string is `"".to_bytes()`; there are no `Bytes.new`/`Bytes.empty` words.

### 3.3 UTF-8 coding and hex

The three-way failure taxonomy (§4.1) lands in the binary domain as follows:

| Property | Word | Failure form |
|---|---|---|
| Encoding (a `String` is always valid UTF-8) | `String.to_bytes` | none — total function |
| Decoding (external bytes are untrusted; the caller needs to know why) | `Bytes.to_string` | `Err (Invalid …)`; payload carries the first invalid offset and its context |
| Per-byte construction (asks whether, not why) | `Bytes.from_ints` | `None` |
| Hex parsing (input validation) | `Bytes.from_hex` | `Err`; odd length or non-hexadecimal character |

Round-trip properties:

```eko
Bytes.to_string(s.to_bytes())  ==  Ok s      // String is always valid UTF-8; holds in both directions
Bytes.from_hex(b.to_hex())     ==  Ok b      // hex round trip
```

A decode failure such as `"ed a0 80"` (the UTF-8 form of a surrogate pair) yields `Err` with a payload of the form `"offset 0: ed a0 80"`. Exact message wording belongs to the diagnostics vocabulary (09-library.md §7).

### 3.4 Byte I/O

```eko
File.read_bytes:  (File, Int) !-> Result Bytes IoError      // sequential read of at most n bytes
File.write_bytes: (File, Bytes) !-> Result _ IoError        // write at the current position
Fs.read_bytes:    (Fs, String) !-> Result Bytes IoError     // convenience mirror: whole file
Fs.append_bytes:  (Fs, String, Bytes) !-> Result _ IoError  // convenience mirror
```

- `read_bytes` never returns `InvalidData`: every byte sequence is legal. A short read means EOF and returns `Ok` with a short `Bytes` — EOF is not an error (06-effects.md §3).
- `File.read` keeps the text gate: it still returns `Err InvalidData` for non-UTF-8 (06-effects.md §4). That refusal is not a dead end: `read_bytes` is the raw-byte entry point, so refusal and acceptance each have their place.
- `with_file` callbacks and `<-` chains work as usual; `OpenMode` is unchanged. The `In` console layer is text-only and has no byte gate.

```eko
png_magic: []Int = {137, 80, 78, 71, 13, 10, 26, 10}

is_png: (fs: Fs, path: String) !-> Result Bool IoError =
    with_file(fs, path, Read, (f):
        head <- f.read_bytes(8).try()
        Ok head.to_ints() == png_magic)
```

### 3.5 Display

`show` and `inspect` have the same form for `Bytes`: the constructor application form `Bytes.from_hex("…")`.

```eko
b := "hi".to_bytes()
"{b}"        // Bytes.from_hex("6869")
inspect(b)   // Bytes.from_hex("6869")
```

This is the same constructor application form used by `Some 3` (§6.3). The principle that what you see can be written applies **as an expression**: the inspect output is legal program text, and evaluating it returns the original value. Long byte strings are printed in full — the structural channel never truncates; truncation inside test assertions is the user's concern.

**The binary domain.**

| Surface | Specification |
|---|---|
| Value | `Bytes` and all pure services (§3.2) |
| I/O | `read_bytes`/`write_bytes` (06-effects.md §4.2) |
| Byte computation | domain word `Int` in `0..255`; machine words never compute (`11-ffi.md §3.1`) |
| Bitwise operations | the `UInt` function family (§4.6); no operators (`02-syntax.md §7`) |
| Literals | `0x`/`0b` (`02-syntax.md §4.1`) |
| `Bytes` ↔ `[]U8` | `to_u8s`/`from_u8s`, zero copy (§3.2) |

The computation surface is domain words: a byte is a natural number `0..255`
— the same move as `Char.to_code` — and machine words are for layout and
ferrying, never for arithmetic.

**Exclusions.** The following are not part of the binary domain:

- Byte literals (`0x…` as a byte construction form, `b"…"`, `bytes"…"`). Construction goes through functions; `0x` literals are integer literals (`02-syntax.md §4.1`), not byte construction.
- Index syntax `b[i]`. `get` is the only entry point.
- Mutable `Bytes`, builder, or resize vocabulary. Values are immutable; `append` is an O(n) full copy, recorded as such; batch construction goes through `from_ints`.
- Other encoding families in the prelude (UTF-16, latin-1, GBK). UTF-8 is the only built-in gate; encoding conversion is library domain.
- Digest or CRC functions in the prelude. They are algorithm domain; `get` and `to_ints` are sufficient for library implementations.
- Implicit `Bytes`/`String` conversion. The only gate is `to_bytes`/`to_string`, explicit and carrying failure semantics.

## 4. Conversions

### 4.1 The failure taxonomy

| Nature of failure | Vocabulary | Example |
|---|---|---|
| **Input validation**: external data is untrusted; the caller needs to know why | `Result` | `IoError` (06-effects.md §3) |
| **Domain check**: asks whether, not why | `Option` | `Char.from_code` (§2.3) |
| **Invariant violation**: the programmer's own error | `panic` | conversions never belong here |

In one line: **input goes through `Result`, domain checks go through `Option`, and conversions never panic**. An informed assertion uses `expect` or `unwrap` explicitly; both are pure (03-control.md §6).

### 4.2 Conversion families

| Function | Signature | Semantics |
|---|---|---|
| `UInt.to_int` | `(UInt) -> Option Int` | a `UInt` above `Int.max` is `None` — Int64 cannot losslessly hold the whole UInt64 domain |
| `Int.to_uint` | `(Int) -> Option UInt` | a negative value is `None` |
| `Int.to_float` / `UInt.to_float` | `(Int) -> Float` / `(UInt) -> Float` | total; values above `2^53` round to nearest (precision loss is not failure) |
| `Float.to_int` | `(Float) -> Option Int` | NaN, infinity, or out of range → `None`; a fractional value truncates toward zero and that is not a failure (`3.7 → Some 3`, `-0.5 → Some 0`) — the same basis as `/` and `%` (02-syntax.md §6) |
| `Float.to_uint` | `(Float) -> Option UInt` | NaN, infinity, or out of range → `None`; a negative value is checked first (`-0.5 → None`; `-0.0` yields `Some 0`); a non-negative fractional value truncates toward zero and that is not a failure (`3.7 → Some 3`) — the same basis as `/` and `%` |
| `Int.parse` / `UInt.parse` / `Float.parse` | `(String) -> Result t ParseError` (`t` is the respective numeric type) | input validation |
| `Char.to_code` / `String.from_char` | `(Char) -> Int` / `(Char) -> String` | total; completes the singleton slot of the char bridge family (§2.3) |

`Float.to_int` and `Float.to_uint` are deliberately not saturating conversions: mapping NaN to 0 and infinity to MAX would silently misrepresent the value. `Option` rejects that misrepresentation. `Float.to_uint` checks the negative domain before truncation; a negative fraction is not truncated to 0, because that would discard the sign — another misrepresentation.

### 4.3 ParseError

```eko
ParseError: type = Empty | Invalid String | OutOfRange
```

The variants follow the `IoError` criterion: a name is deserved only when callers branch on it differently. `Empty` signals a required value that is missing, `Invalid` reports malformed input, and `OutOfRange` reports a value beyond its limits. The type is closed and is not extended by users.

### 4.4 Round trips

```eko
parse(show(x)) == Ok x        // non-NaN: exact for Int/UInt; Float guaranteed by the shortest round-trip show
                              // NaN exception: under IEEE ==, NaN != NaN; accept by NaN equivalence
```

`parse` and `show` are mutual inverses: the display channel (§6.2) and input validation meet end to end, closing error chains such as `"age: {e}"`. The Float NaN is an exception to IEEE equality, not to `show`/`parse`: the round trip preserves NaN identity but does not promise identical payload bits, and acceptance cannot use `==`.

### 4.5 Naming

Conversion names are spelled out in full: `to_int`, `to_uint`, `to_float`. They follow the domain-word spelling rule; abbreviations belong to the machine interface. Size-family conversions between domain words and machine words (`to_i32`, `from_u8`, …) are specified in `11-ffi.md §3.1`: domain → machine integer is `Option` on range, domain → `F32`/`F64` is total with rounding, machine → domain is always total.

### 4.6 The bitwise family

Bitwise operations are functions on `UInt`, and on `UInt` only — bit patterns
belong to unsigned semantics, and `Int` converts (`to_uint`, then back).
There are no operators, ever (`02-syntax.md §7`), and no bitwise functions
on machine words (`11-ffi.md §3.1`): they hold and ferry, domain words
compute.

| Function | Signature | Semantics |
|---|---|---|
| `UInt.band` / `bor` / `bxor` | `(UInt, UInt) -> UInt` | total; bit pattern of the ring ℤ/2⁶⁴ |
| `UInt.bnot` | `(UInt) -> UInt` | total; complement |
| `UInt.shl` / `shr` | `(UInt, UInt) -> UInt` | total; **truncation is the semantics, not an overflow**; a shift count ≥ 64 yields `0` |

The family is defined on the ring ℤ/2⁶⁴: `shl` is a fixed-width shift, not
multiplication, so no result ever fails to fit and nothing panics. Hashing
and flag code are the intended customers:

```eko
swap16: (v: UInt) -> UInt = v.band(0xFF).shl(8).bor(v.shr(8).band(0xFF))
x := seed.shl(13).bxor(seed.shr(7))          // xorshift step: truncation is load-bearing
```

The wrapping family is the same shape of vocabulary: `Int.wrapping_add`,
`Int.wrapping_sub`, `Int.wrapping_mul`, `Int.wrapping_neg`, and the `UInt`
analogues compute on the ring ℤ/2⁶⁴ and never panic. Overflow panics by
default (`02-syntax.md §6`); wrapping is an explicit function call.

## 5. Time

### 5.1 The three types

```eko
Time:     type = Time Int        // wall clock point: nanoseconds since 1970-01-01T00:00:00Z
Mono:     type = Mono Int        // monotonic reading: arbitrary epoch, relative meaning only
Duration: type = Duration Int    // duration: signed, nanoseconds
```

All three are nominal single-variant types with an `Int` nanosecond payload. Time comes in three quantities with mutually exclusive semantics: a timestamp (a human-calendar instant), a measurement (order and length), and a duration (a difference). A bare `Int` leaves the unit implicit — milliseconds-versus-microseconds disputes are a real source of incidents — and sealing the unit into a nominal type rules it out. A wall clock can jump (a time correction may move it backwards); a monotonic clock cannot. The two kinds of "now" are typed apart, and differences and comparisons each stay in their own type.

All time arithmetic is a named function on values:

| Function | Signature | Semantics |
|---|---|---|
| `Time.sub` | `(Time, Time) -> Duration` | difference between timestamps |
| `Time.add` | `(Time, Duration) -> Time` | shift a timestamp by a duration |
| `Time.to_ns` | `(Time) -> Int` | bridge: nanoseconds since the epoch |
| `Mono.sub` | `(Mono, Mono) -> Duration` | monotonic difference |
| `Mono.add` | `(Mono, Duration) -> Mono` | shift a reading by a duration |
| `Mono.to_ns` | `(Mono) -> Int` | bridge: the raw reading |
| `Duration.add` | `(Duration, Duration) -> Duration` | sum |
| `Duration.mul` | `(Duration, Int) -> Duration` | scale; a negative argument flips the sign |
| `Duration.neg` | `(Duration) -> Duration` | negation |
| `Duration.secs` | `(Int) -> Duration` | construct from seconds (×10⁹) |
| `Duration.millis` | `(Int) -> Duration` | construct from milliseconds (×10⁶) |
| `Duration.to_ns` | `(Duration) -> Int` | signed nanoseconds |

- All of these are pure: arithmetic on values, not a touch of the world. `==` and `<` are derived by payload recursion over the single variant (§6.4, §6.5).
- **No operator sugar**: `t2 - t1` does not exist. Single-variant payloads are not exposed, and there is no anonymous ADT, so operators cannot reach them; every piece of time arithmetic is named. Unit-mixing incidents have nowhere to occur.
- Native nanosecond construction uses constructor application (`Duration 500`, `Time 0`, the same form as `Ok 3`); `to_ns` is its inverse.

### 5.2 Clock services

```eko
Clock.now:   (Clock) !-> Time         // wall clock: timestamp
Clock.mono:  (Clock) !-> Mono         // monotonic: measurement — order and length
Clock.sleep: (Clock, Duration) !-> _  // sleep; a negative duration returns immediately; granularity is host-dependent
```

- Reading the clock sets the world bit (06-effects.md §5): two readings can differ, which is an observation of the world. Sleeping also sets the bit: time passes, the process yields, and the world is changed. Both require `Clock`; bit and permission share one origin.
- **Measure with `mono`, timestamp with `now`**: a wall-clock difference can be negative or jump, while elapsed semantics hold only on the monotonic clock. `Time < Time` is reading order (usable for sorting); precedence is decided with `Mono < Mono`. The derived `<` provides both semantics, and the types keep misuse from meeting.
- A negative duration returns immediately (clamped to zero, recorded as such). Timeouts and cancellation belong to the concurrency rules (06-effects.md §8); no vocabulary is pre-empted here.

```eko
main: (out: Out, clock: Clock) ! =
    m0 = clock.mono()
    clock.sleep(Duration.millis(500))
    m1 = clock.mono()
    out.println("elapsed: {m1.sub(m0)}")       // 0.5s
    stamp = clock.now()
    out.println("at: {stamp}")                 // 2026-10-22T00:00:00Z
```

### 5.3 Frozen clocks

```eko
Clock.frozen: (Time) -> Clock        // pure
```

`frozen(t)` creates a clock with constant readings: `now` always returns `t`, `mono` always returns one fixed reading, and `sleep` returns immediately (time does not flow). It belongs to the same family as `Out.null`, a pure derivation (06-effects.md §2); test determinism comes from it (09-library.md §6).

The bit is unchanged: `now()` on a frozen clock still sets the world bit. The bit is decided statically by built-in criteria and does not inspect values — the bit belongs to the signature and does not depend on the value (06-effects.md §5). Determinism is a property of the values; the bit remains a property of the signature.

### 5.4 Display

| Type | `show` | `inspect` |
|---|---|---|
| `Time` | `2026-10-22T01:29:00.123Z` (UTC; fractional seconds as needed, at most nine digits) | `Time 1792632540123000000` |
| `Duration` | `0.5s` (decimal seconds, trailing zeros removed) | `Duration 500000000` |
| `Mono` | `Mono 89347123456` (no human form — the epoch is meaningless) | same |

- `Time.show` formats ISO 8601 in UTC, and `Duration.show` prints decimal seconds; both are prelude overrides of the §6.2 machinery (the declaring module is the prelude). `inspect` is always the constructor form, so what you see can be written.
- UTC is the only zone; a local timezone would undermine determinism, and formatting and localization stay library domain. `Time.parse: (String) -> Result Time ParseError` accepts the ISO 8601 UTC forms that `Time.show` produces; `Duration.parse: (String) -> Result Duration ParseError` accepts decimal seconds. Both hold the round-trip property of §4.4.

### 5.5 Domain and overflow

- The payload is Int64 nanoseconds, a span of about ±292 years: the wall-clock domain is roughly 1677-09-21 to 2262-04-11, and `Duration` has the same span. Arithmetic outside the domain panics under the existing arithmetic rules (02-syntax.md §6); a panic does not set the world bit (06-effects.md §5).
- Leap seconds are not represented: the POSIX epoch maps directly, a day is always 86400 seconds, and the alignment policy belongs to the host domain, recorded as such.

**Exclusions.** The following are excluded from the time surface:

- A single type merging wall clock and monotonic readings (two readings in one type; a known source of subtle errors).
- Time operator sugar. The payload is not exposed; named arithmetic is the cost and the guarantee of unit safety.
- A local timezone or calendar family (`Date`, `DateTime`): determinism comes first.
- A construction right for `Clock` derivations. `frozen` is a pure combinator, not freedom to build clocks (capabilities are opaque, 06-effects.md §2); for the same reason there is no `Clock.frozen_mono`.

## 6. Derived services

### 6.1 show and inspect

A single overridable channel would lose structural truth. Eko has two channels:

| Channel | Name | Semantics | Overridable | Used by |
|---|---|---|---|---|
| Display | `show` | for humans | ✓ (namespace declaration) | **interpolation `"{e}"`**, logging |
| Structural | `inspect` | faithful literal form | ✗ always derived | diagnostics, tests, debugging |

Both channels are available as free functions with no type annotation (bare `show(t)`, 05-names.md §11) and as type namespace members (`t.show()`, `Money.show(m)`).

### 6.2 Default show and overrides

The default derivation is `inspect` minus the **top-level String quotes**. The top-level bare form is forced by interpolation: `"hi {name}"` must produce `hi wd`, and `Out.println` accepts only `String`, so interpolation is the primary display channel. A `String` inside a composite keeps its quotes (`{name: "wd"}` disambiguates).

An override uses the existing machinery and adds no syntax:

```eko
Money: type = [cents: Int, cur: String]

Money.show: (m: Money) -> String = "{m.cur} {m.cents}"

m: Money = {cents: 12500, cur: "USD"}
"{m}"          // USD 12500           override takes effect
inspect(m)     // {cents: 12500, cur: "USD"}   structure is always true
```

- An override is a namespace declaration, and ownership belongs to the declaring module (05-names.md §8); orphan overrides do not exist. A type without an override takes the default at zero cost.
- An override makes the output of `"{m}"` depend on the type declaration — the one place where interpolation requires non-local reasoning. The cost is recorded explicitly, and the structural channel is always present as a fallback.

### 6.3 inspect forms

What you see can be written, with no exception:

| Type | `inspect` | `show` (not overridden) |
|---|---|---|
| `Int`/`UInt` | `3` | same |
| `Float` | shortest round trip: `0.5`, `3.14159`; `NaN`, `Inf` | same |
| `Bool` | `True`, `False` | same |
| `String` | `"wd"` (quoted) | `wd` (top-level bare) |
| `Char` | `'a'` (quoted) | `a` (top-level bare) |
| `Bytes` | `Bytes.from_hex("4142")` (§3.5) | same |
| List / tuple | `{1, 2, 3}`, `{"first", 2}` | same |
| Dictionary / record | `{"1": 1}`, `{name: "wd", age: 2}` | same |
| ADT | constructor application form: `Man`, `Some 3`, `Err "…"` | same |
| `_` (unit) | `_` | same |
| Capability | `<cap>` (06-effects.md §2) | same |
| Function | `<fn>` | same |

Empty containers are `{}`. Record field order is declaration order; dictionary order is insertion order, and iteration follows it. The time types are specified in §5.4.

### 6.4 Equality

- Equality is structural and deep: records field by field; ADTs when the constructors match and payloads compare recursively; containers element by element. `Option` and `Result` follow the ADT rule.
- There are two exceptions: capabilities have no `==` (06-effects.md §2), and functions have no `==` (comparing closures is meaningless).
- `Float` follows IEEE: `NaN == NaN` is `False`. A total-order comparison does not enter the operator.
- Both sides of `==` have the same type: `Int.eq: (Int, Int) -> Bool` locks the signature. `1 == 1.0` and `age == n` (`UInt` versus `Int`) are type errors; conversion is explicit (`Int.to_float(x) == y`, §4.2).

### 6.5 Ordering

- `<` is derived **only for ADTs**: variants compare by declaration order, and payloads compare recursively in order (`Sex: Man | Woman` implies `Man < Woman`). A declaration list is naturally an order, so derivation is faithful.
- Records have **no `<`**: a field set has no magnitude semantics, and imposing an order is arbitrary. Sort records with `sort_by` and an explicit comparator (09-library.md §5).
- `Bool` has **no `<`**: the order of `True | False` is enumeration convenience, not magnitude. Partition with `filter`. Derivation of `<` is deliberately suppressed for `Bool`.
- Builtins are hand-defined rather than derived: numbers, `Char` code-point order, `String` lexicographic order, and `Bytes` byte order.

### 6.6 Record update with `with`

```eko
p2       := p.with({ age: 37 })
invoice2 := invoice.with({ status: Paid, paid_at: now })
p3       := p.with({ address: { city: "cd" } })   // deep update: nested partial literal
p.with({ age: 37 }).notify(out)                   // direct chain; the call is atomic
```

- The argument is a **partial field literal**. It is legal only in the argument position of `with`, and it is checked layer by layer against the field types (a misspelling is an error). This is the single localized exception to "composite literals never stand alone" (04-types.md §2).
- The second parameter type of `Person.with` — `(Person, partial fields) -> Person` — is **inexpressible** in Eko's type syntax. This is the first compiler built-in service whose type cannot be expressed; the exception is explicit and localized.
- Old values are untouched (values are immutable). The vocabulary is separated: `with` for record update, `insert`/`remove` for dictionaries, and `append`/`merge` for the body of `++` (02-syntax.md §6).

### 6.7 Dispatch

`show`, `inspect`, `==`, `<`, `to_json`, and `from_json` are all compile-time generic dispatch (04-types.md §4): the bare form resolves by static type to that type's namespace, and the derived default is the dispatcher's default member. World bits do not participate; every one of these services is pure.

```eko
show(t)            ≡  t.show()          // dispatch, override or default
inspect(t)         ≡  t.inspect()       // always derived
a == b             ≡  Int.eq(a, b)      // same-type locked; resolves by static type
a < b              ≡  Int.lt(a, b)      // ADT or builtin
"{e}"              ≡  show(e) injected  // display channel
p.with({age: 37})                       // record update, a derived service
```

`to_json`/`from_json` follow the same dispatch and override model (§7.5).

## 7. JSON

### 7.1 The Json type

`Json` is a prelude ADT with seven variants.

```eko
Json: type =
    | Null
    | Bool Bool
    | Int Int
    | Float Float
    | Str String
    | Arr []Json
    | Obj ([String]Json)
```

- `Int Int` has the same payload form as `Time 0`; constructor application is the inspect form.
- `Obj` is a string-keyed dictionary that preserves insertion order. `show` follows that order, so JSON text is reproducible and configuration diffs are stable.
- Construction is check-only: constructors are applied by juxtaposition, and literals land by expected type (`Obj {"a": Int 1}`).
- `Int` and `Float` are separate variants: `42` goes in and comes out as `Int` — round-trip fidelity takes precedence over the surface uniformity of JSON's single number type.

### 7.2 Parsing and showing

```eko
Json.parse: (String) -> Result Json ParseError     // input validation; payload carries offset and context
Json.show:  (Json) -> String                       // minimal form: no whitespace
```

- `parse` is a member of the §4 parse family: `ParseError` is reused, and the taxonomy placement is input validation, with a reason.
- `Json.show` uses the override machinery (the declaring module is the prelude). Interpolation `"{j}"` produces JSON text directly; the structural channel `inspect(j)` is always the constructor form, so the dual channels each hold their place (§6.1).
- Number text: a `Float` uses the shortest round trip — `3.5` prints `3.5`, `3.0` prints `3.0`; `3` prints `3` and parses back as `Int 3`. The variant decides the spelling.
- String escapes follow the JSON specification (`\uXXXX`). Surrogate pairs are parsed as pairs; lone surrogates are rejected — `String` is always valid UTF-8, and the entrance enforces it (§2.2).
- `parse(show(j)) == Ok j`.
- Non-finite floats render as themselves — `NaN`, `Inf`, `-Inf` — following the
  Float display vocabulary (§6.3), and `Json.parse` accepts those tokens back, so
  the round-trip promise holds, judged by NaN equivalence where applicable (§4.4).
  These spellings are an Eko-side extension of strict JSON text; converting them
  for strict external consumers is a library concern.
- Pretty printing (`indent`) is library domain.

### 7.3 Derived mapping

Each data type derives `to_json: (t) -> Json` and `from_json: (Json) -> Result t ParseError` by default; the exceptions are listed in the table below. The mapping is the following isomorphism:

| Eko type | JSON form | Example |
|---|---|---|
| Record | `Obj` (field names as keys) | `{name: "wd"}` → `{"name": "wd"}` |
| List / tuple / `Set` | `Arr` (insertion order) | `{1, 2}` → `[1, 2]` |
| `String` / `Char` | `Str` | `'a'` → `"a"` |
| `Int` / `Float` | number | `42` → `42`; `3.5` → `3.5` |
| `Bool` | `true` / `false` | builtin mapping |
| `_` (unit) | `null` | `Result _ IoError` → `{"Ok": null}` |
| `Bytes` | `Str` (lowercase hex) | `"hi".to_bytes()` → `"6869"` |
| Dictionary (string-keyed) | `Obj` | `{"a": 1}` → `{"a": 1}` |
| ADT | external tagging (§7.4) | `Man` → `"Man"`; `Ok 3` → `{"Ok": 3}` |
| Non-string-keyed dictionary | ✗ compile error | convert explicitly |
| Capability / function | ✗ compile error | things without `==` do not enter the isomorphism |

Every value whose type participates in the isomorphism satisfies the specification-level round-trip property `from_json(to_json(v)) == Ok v`. A missing field, an extra key, an unknown variant, or a payload type mismatch yields `Err`.

- `Bool` is special-cased: `true`/`false` are JSON booleans, while a user-declared enum (`Yes | No`) still goes through external tagging as strings. The built-in `Bool` mapping is hand-defined (§6.5); a declared type does not inherit it.
- `Bytes` maps to a lowercase hex string (the same basis as `to_hex`/`from_hex`, §3.2). The field type is known, so decoding is unambiguous, and the round trip is faithful.
- An override takes precedence over the isomorphism: once `Money.to_json` is declared, it shadows the default (§7.5).

### 7.4 External tagging

```eko
Sex:    type = Man | Woman            Man → "Man"
Result: type t e = Ok t | Err e       Ok 3 → {"Ok": 3}; Err "x" → {"Err": "x"}
Tree:   type t = Leaf t | Node [Tree t, Tree t, t]
        Node {Leaf 1, Leaf 2, 3} → {"Node": [{"Leaf": 1}, {"Leaf": 2}, 3]}
```

- A zero-payload variant maps to a string. A payload-bearing variant maps to a single-key object: a single payload is embedded directly, and multiple payloads (a tuple payload) become an array under the key.
- This is the externally-tagged form. It is the simplest for CLI configuration: zero-payload enums read and write directly.
- `from_json`: an unknown variant or key, a missing field, or a payload type mismatch yields `Err` (with an `Invalid "unknown variant …"` payload where applicable); constructor fields are checked, so a misspelling is a compile error (04-types.md §3).

### 7.5 Overrides and generics

- **Override**: the declaring module writes `Money.to_json: (Money) -> Json = …` in the type namespace, shadowing the default; `from_json` has the same right. Ownership belongs to the declaring module, and orphan overrides do not exist — the same machine as `show` (§6.2).
- **Generics**: `to_json` and `from_json` join the type-variable capability set (04-types.md §8). A generic function may call them directly on `t`; at instantiation, the concrete derivation or override applies. A capability, a function, or a non-string-keyed dictionary at an instantiation site is an error — the same rule as `==`.
- Dispatch is compile-time generic dispatch (§6.7), with zero runtime reflection.

```eko
load_config: (s: String) -> Result Config String =
    j <- Json.parse(s).map_err((e): "config: {e}").try()
    Config.from_json(j).map_err((e): "config: {e}")
```

**Exclusions.** The following are not part of the JSON surface:

- A format-string DSL or path queries (jq-like). These are library domain.
- Default pretty printing. `show` is minimal; `indent` is library domain.
- Silent non-string-keyed dictionary isomorphism. It is a compile error with an explicit conversion; no silent distortion.
- YAML or TOML in the prelude. They are library domain; JSON is the only built-in serialization surface.
- Runtime reflection. Derivation is compile-time expansion with zero reflection surface.
