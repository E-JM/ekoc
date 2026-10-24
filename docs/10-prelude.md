# Chapter 10 — The Prelude

This chapter is the complete inventory of the prelude: every type, free
function, and namespace member, with signatures. Semantics that are specified
elsewhere are referenced, not repeated; signatures here are normative. The
prelude's delivery model is specified in 05-names.md §11: an ordinary Eko
module named `prelude`, shipped as source with the compiler, implicitly
imported. Only the derived services, `panic`, and the memory operations are
compiler-generated; everything else is prelude source code. The world bit of
every signature follows 06-effects.md §9.

## 1. Free functions

| Function | Signature | Notes |
|---|---|---|
| `show` | `(t) -> String` | display channel; dispatches to the type namespace; compiler-generated default, overridable (08-builtins.md §6) |
| `inspect` | `(t) -> String` | structural channel; always derived (08-builtins.md §6.3) |
| `panic` | `(msg: String) -> Never` | compiler intrinsic; aborts the process; pure (06-effects.md §3) |
| `assert` | `(c: Bool) -> _` | prelude source; panics on `False` (09-library.md §6.3) |
| `assert_eq` | `(a: t, b: t) -> _` | prelude source; same-type; failure message via `inspect` |
| `assert_ne` | `(a: t, b: t) -> _` | prelude source; same-type; panics when equal (09-library.md §6.3) |

## 2. Operator namespaces

Every operator is sugar for a namespace function (02-syntax.md §6.1). The
complete table:

| Expression | Desugars to (by operand type) |
|---|---|
| `a + b`, `a - b`, `a * b` | `Int.add/sub/mul`, `UInt.add/sub/mul`, `Float.add/sub/mul` |
| `a / b` | `Int.quot`, `UInt.quot`, `Float.div` |
| `a % b` | `Int.rem`, `UInt.rem`, `Float.rem` |
| `-a` | `Int.neg`, `Float.neg` (no `UInt.neg`; unsigned values are non-negative) |
| `a == b`, `a != b` | `T.eq`, `T.ne` — `T` ∈ {`Int`, `UInt`, `Float`, `Bool`, `Char`, `String`, `Bytes`} and derived `eq` for ADTs and records |
| `a < b`, `a <= b`, `a > b`, `a >= b` | `T.lt/le/gt/ge` for the same hand-defined types; derived `<` for ADTs (records and `Bool` have none) |
| `a && b`, `a \|\| b` | sugar for `if`; there is no function (02-syntax.md §6.3) |
| `b.not()` | `Bool.not` |
| `xs ++ ys` | `List.append`, `String.append`, `Bytes.append`, `Dict.merge` (right side wins) |

Named arithmetic that has no operator: `Int.div`/`Int.mod` (floor family),
`Int.abs` (panics on `Int.min`), `Int.pow: (Int, UInt) -> Int`,
`Float.abs/floor/ceil/trunc/round`, `Float.pow`, `Float.sqrt/exp/log/log2/log10`,
trigonometry and predicates (09-library.md §3). Range constants: `Int.min`,
`Int.max`, `UInt.max`.

## 3. Types

`Bool`, `Char`, `String`, `Bytes`, `Int`, `UInt`, `Float`, `Range t`
(endpoints `Int`, `UInt`, or `Char`), `Option t`, `Result t e`, `Set t`,
`Time`, `Mono`, `Duration`, `Json`, `ParseError`, `IoError`, `Never`,
`File` (the handle type), `Scope` and `Task t` (06-effects.md §8), and the
capabilities `Out`, `In`, `Fs`, `Env`, `Args`, `Clock`, `Rand`, `Proc`, `Conc`. The machine
words `I8 I16 I32 I64 U8 U16 U32 U64 F32 F64` and the `C` namespace
(`C.Ptr.Void`, `C.Size`, `C.get_errno`, `C.In`; `11-ffi.md §3.2`). There
is no FFI capability (`11-ffi.md §1.2`).
Declarations of `Bool`, `Option`, `Result`, `ParseError`, `IoError`, `Json`,
`Time`, `Mono`, `Duration`, and `Never` are as given in 08-builtins.md and
06-effects.md §3.

## 4. List namespace (`[]t`)

| Function | Signature |
|---|---|
| `List.length` | `([]t) -> Int` |
| `List.get` | `([]t, Int) -> Option t` |
| `List.reverse` | `([]t) -> []t` |
| `List.append` | `([]t, []t) -> []t` |
| `List.map` | `([]t, (t) ?-> u) ?-> []u` |
| `List.filter` | `([]t, (t) ?-> Bool) ?-> []t` |
| `List.fold` | `([]t, u, (u, t) ?-> u) ?-> u` |
| `List.each` | `([]t, (t) ?-> _) ?` |
| `List.each_with_index` | `([]t, (t, Int) ?-> _) ?` |
| `List.any` | `([]t, (t) ?-> Bool) ?-> Bool` |
| `List.all` | `([]t, (t) ?-> Bool) ?-> Bool` |
| `List.find_by` | `([]t, (t) ?-> Bool) ?-> Option t` |
| `List.sort` | `([]t) -> []t` (derived `<`; instantiation of a record is an error) |
| `List.sort_by` | `([]t, (t, t) ?-> Bool) ?-> []t` (stable; an inconsistent comparator terminates with an implementation-defined order) |
| `List.zip_with` | `([]t, []u, (t, u) ?-> v) ?-> []v` |
| `List.take` / `List.drop` | `([]t, Int) -> []t` (truncating; 09-library.md §5.1) |
| `List.take_while` / `List.drop_while` | `([]t, (t) ?-> Bool) ?-> []t` |
| `List.join` | `([]t, String) -> String` (derived `show`; the same judgment as interpolation) |
| `List.slice` | `([]t, Int, Int) -> Option []t` |

## 5. Dict namespace (`[k]v`)

| Function | Signature |
|---|---|
| `Dict.length` | `([k]v) -> Int` |
| `Dict.get` | `([k]v, k) -> Option v` (O(1)) |
| `Dict.insert` | `([k]v, k, v) -> [k]v` |
| `Dict.remove` | `([k]v, k) -> [k]v` (absent key: the original) |
| `Dict.merge` | `([k]v, [k]v) -> [k]v` (right side wins; body of `++`) |
| `Dict.map` | `([k]v, (k, v) ?-> u) ?-> [k]u` |
| `Dict.filter` | `([k]v, (k, v) ?-> Bool) ?-> [k]v` |
| `Dict.fold` | `([k]v, u, (u, k, v) ?-> u) ?-> u` |
| `Dict.each` | `([k]v, (k, v) ?-> _) ?` |

Key domain: data types with `==` (09-library.md §5.3). Iteration follows
insertion order.

## 6. Set namespace

`Set.of`, `Set.to_list`, `Set.contains`, `Set.insert`, `Set.remove`,
`Set.length`, `Set.union`, `Set.intersect`, `Set.diff` — signatures and
semantics in 09-library.md §5.2. No notation, no literal.

## 7. String namespace

| Function | Signature | Notes |
|---|---|---|
| `String.length` | `(String) -> Int` | counts scalar values |
| `String.at` | `(String, Int) -> Option Char` | O(n), explicit cost |
| `String.chars` | `(String) -> []Char` | bridge to `List` |
| `String.from_char` | `(Char) -> String` | |
| `String.from_chars` | `([]Char) -> String` | |
| `String.append` | `(String, String) -> String` | body of `++`; O(n) |
| `String.lines` | `(String) -> []String` | see below |
| `String.split` | `(String, String) -> []String` | see below |
| `String.contains` | `(String, String) -> Bool` | |
| `String.starts_with` / `String.ends_with` | `(String, String) -> Bool` | |
| `String.replace` | `(String, String, String) -> String` | all occurrences |
| `String.replace_first` | `(String, String, String) -> String` | first occurrence only |
| `String.slice` | `(String, Int, Int) -> Option String` | scalar domain; never clamps |
| `String.trim` / `trim_start` / `trim_end` | `(String) -> String` | Unicode White_Space |
| `String.to_bytes` | `(String) -> Bytes` | UTF-8 encoding; total |

**`String.split(s, sep)`** splits on every occurrence of `sep` and keeps
empty fields, whether leading, trailing, or between adjacent separators:

```eko
"a,b".split(",")     // {"a", "b"}
",,x,".split(",")    // {"", "", "x", ""}
```

The separator must be non-empty; an empty separator is a panic (invariant
violation). **`String.lines(s)`** splits on `'\n'` and drops the final empty
element when `s` ends with `'\n'`: `"a\nb"` → `{"a", "b"}`; `"a\nb\n"` →
`{"a", "b"}`; `"\n"` → `{""}`; `""` → `{}`.

**Path** — `join: (String, String) -> String`, `normalize: (String) -> String`,
`is_absolute: (String) -> Bool`; pure and lexical (09-library.md §4.1).
Resolution through the file system is `Fs.realpath` (§10).

## 8. Char, Bytes, Option, Result, Range

| Namespace | Members |
|---|---|
| `Char` | `to_code: (Char) -> Int`, `from_code: (Int) -> Option Char` |
| `Bytes` | `length`, `get`, `append`, `to_ints`, `from_ints`, `to_hex`, `from_hex`, `Bytes.to_string` — signatures in 08-builtins.md §3.2 |
| `Option` | `map`, `and_then`, `or_else`, `filter`, `is_some: (Option t) -> Bool`, `is_none: (Option t) -> Bool`, `unwrap: (Option t) -> t`, `unwrap_or: (Option t, t) -> t`, `expect: (Option t, String) -> t`, `get: (Option ([]t), Int) -> Option t` (05-names.md §4.3), `to_result: (Option t, e) -> Result t e` |
| `Result` | `try`, `map`, `map_err`, `traverse` (03-control.md §6.1), `is_ok: (Result t e) -> Bool`, `is_err: (Result t e) -> Bool`, `unwrap: (Result t e) -> t`, `unwrap_or: (Result t e, t) -> t`, `expect: (Result t e, String) -> t` |
| `Range` | `to_list: (Range t) -> []t`, `length: (Range t) -> Int`, `each`, `each_with_index`, `map`, `fold` — same callback shapes as the `List` members |

`Option.unwrap` panics on `None`; `Option.expect` panics with its message;
both are pure (06-effects.md §3). `Option.to_result` maps `None` to
`Err e` and is the bridge used by `<-` chains (03-control.md §6.2).

## 9. Int, UInt, Float, machine words

- Conversions and the parse family: 08-builtins.md §4 (including
  `Int.parse_hex` and siblings, 09-library.md §2).
- **`Float.parse` grammar.** Optional sign; decimal digits with optional
  fraction and exponent (`1`, `1.5`, `1.5e3`, `.5` is not accepted — a
  leading digit is required, `0.5` is); plus the spellings `NaN`, `Inf`,
  `-Inf`, `+Inf`, which the round-trip property requires (08-builtins.md
  §4.4). `Int.parse` and `UInt.parse` accept an optional sign and decimal
  digits only.
- `Float.to_fixed` / `to_exp` (09-library.md §2), the mathematics surface
  (09-library.md §3), and the constants `Int.min`, `Int.max`, `UInt.max`.
- **The bitwise family** — `UInt.band/bor/bxor/bnot/shl/shr`, no
  operators, ℤ/2⁶⁴ total semantics: 08-builtins.md §4.6.
- **The wrapping family** — `Int.wrapping_add/sub/mul/neg` and the `UInt`
  analogues, ℤ/2⁶⁴ total semantics, never panic: 08-builtins.md §4.6.
- **Machine words** — `==`, `<`, `show`, `to_hex_pad`; conversions
  `to_i8`…`from_f64` per 11-ffi.md §3.1 (`Option` on range into machine
  integers, total rounding into `F32`/`F64`, always total widening out);
  no arithmetic, no bitwise.

## 10. Capability namespaces

Signatures marked `!` set the world bit; derivations are pure.

The `Out.unwrap`/`Out.wrap` pairs (and the per-capability analogues) are
prelude-private `Pure` externs to reserved host symbols; they are
implementation of the derivation surface, not public inventory
(`11-ffi.md §8`).

**Out** — `print: (Out, String) !` (no newline), `println: (Out, String) !`,
`tee: (Out, (String) !-> _) -> Out`, `null: () -> Out`.

**In** — `read_line: (In) !-> Result (Option String) IoError` (EOF is not an
error), `empty: () -> In`, `from_lines: ([]String) -> In`.

**Fs** — `open: (Fs, String, OpenMode) !-> Result File IoError`;
`with_file: (Fs, String, OpenMode, (File) !-> a) !-> Result a IoError`
(closes on both paths; 06-effects.md §4.4);
`exists: (Fs, String) !-> Bool`;
whole-path mirrors `read: (Fs, String) !-> Result String IoError`,
`write: (Fs, String, String) !-> Result _ IoError`,
`append: (Fs, String, String) !-> Result _ IoError`,
`read_bytes: (Fs, String) !-> Result Bytes IoError`,
`append_bytes: (Fs, String, Bytes) !-> Result _ IoError`;
the directory surface `list_dir`, `mkdir`, `mkdir_all`, `rm`, `rm_dir`,
`rename`, `stat`, `realpath` (09-library.md §1); derivations `null`, `root`,
`read_only` (06-effects.md §4.5).

**File** — `read`, `read_bytes`, `read_line`, `seek`, `write`, `write_bytes`, `flush`,
`close` (06-effects.md §4.2). Operations on a stale handle yield `Err Closed`.

**Env** — `get: (Env, String) !-> Option String` (unset variable → `None`;
presence is a domain check), `frozen: ([String]String) -> Env`.

**Args** — `all: (Args) !-> []String` (argument vector without the program
name), `frozen: ([]String) -> Args` (06-effects.md §2.5).

**Clock** — `now: (Clock) !-> Time`, `mono: (Clock) !-> Mono`,
`sleep: (Clock, Duration) !-> _`, `frozen: (Time) -> Clock`
(08-builtins.md §5).

**Rand** — `int: (Rand, UInt) !-> UInt` (uniform over `0..n` exclusive of
`n`; `n == 0` panics — the bound is an invariant), `with_seed: (Int) -> Rand`
(seed fixes the order; the sequence itself is an implementation detail).

**Proc** — `exit: (Proc, Int) !-> Never` (abrupt termination with the given
exit code; 06-effects.md §2).

**Conc** — `scope: (Conc, (Scope) ?-> a) ?-> a`, `spawn: (Scope, () ?-> b) ?-> Task b`,
`join: (Task b) -> Option b`, `cancel: (Task b) -> _` (06-effects.md §8).

## 11. Time, Mono, Duration, Json

`Time.sub/add/to_ns`, `Mono.sub/add/to_ns`, `Duration.add/mul/neg/secs/millis/to_ns`
— 08-builtins.md §5.1. `Time.parse` and `Duration.parse` —
08-builtins.md §5.4. `Json.parse`, `Json.show`, and the derived
`to_json`/`from_json` — 08-builtins.md §7.

## 12. What the prelude does not contain

The prelude contains no world-touching free function (every such function
hangs under a capability namespace), no format-string language, no regular
expressions, no Unicode case mapping, and no encoding
family beyond UTF-8 (09-library.md §7). Additions to this inventory are
specification changes: the prelude is versioned with the compiler, and its
opened names are part of its interface.
