# Standard Library, Testing, and Diagnostics

This chapter completes the language's surface without extending it. File-system
operations, formatting, floating-point mathematics, text, and collection
utilities are functions in the existing type namespaces (`05-names.md §2`).
`Set` is an ordinary std generic type with no added notation. A test is a
binding in the existing annotation slot (`04-types.md §1`); assertions are
implicitly opened std free functions (`05-names.md §11`); deterministic
tests are assembled from pure derived capabilities (`06-world.md §2`).
Diagnostics are compile-time messages with stable symbolic codes. No new
syntax, binding form, literal, or format-string language is introduced
anywhere in this chapter: every addition reuses machinery specified elsewhere.
Sections 1–5 specify the library surface, section 6 the testing contract, and
section 7 the diagnostic vocabulary.

## 1. File system operations

The `Fs` capability gains seven operations. All of them set the world bit
(`06-world.md §5`) and appear in the standard library bit assignments
(`06-world.md §9`).

| Service | Signature | Semantics |
|---|---|---|
| `Fs.list_dir` | `(Fs, String) !-> Result []String IoError` | name list in deterministic lexicographic order; not a directory → `Err NotFile`; excludes `.` and `..` |
| `Fs.mkdir` | `(Fs, String) !-> Result _ IoError` | one level; already exists → `Err Exists` |
| `Fs.mkdir_all` | `(Fs, String) !-> Result _ IoError` | creates the whole chain; already existing and a directory → `Ok` |
| `Fs.rm` | `(Fs, String) !-> Result _ IoError` | removes a file; a directory → `Err NotFile` |
| `Fs.rm_dir` | `(Fs, String) !-> Result _ IoError` | removes an empty directory; non-empty or not a directory → `Err (Other …)` |
| `Fs.rename` | `(Fs, String, String) !-> Result _ IoError` | rename or move; cross-device and similar failures → `Err (Other …)` |
| `Fs.stat` | `(Fs, String) !-> Result FileInfo IoError` | metadata, below; symbolic-link semantics are host domain |
| `Fs.realpath` | `(Fs, String) !-> Result String IoError` | resolves `.`/`..` and symbolic links through the file system; a relative input is resolved against the process working directory |

```eko
FileInfo: type = [size: UInt, mtime: Time, is_dir: Bool]
```

- **Error mapping reuses the closed `IoError`** (`06-world.md §3`). Details
  such as a non-empty directory, removal of a non-directory, and cross-device
  moves are reported as `Other String`. The closed union gains no variant, so
  existing exhaustive matches remain exhaustive.
- **`list_dir` yields names, not paths.** Join paths with `Path.join(dir, name)`
  (§4); the separator follows the host.
- **`size` for a directory is a platform-defined value**.
  `mtime` is a wall-clock point (`08-builtins.md §5`); its `show` form is
  ISO 8601.
- **Attenuation applies unchanged.** `Fs.root` and `Fs.read_only` restrict
  this surface exactly as they restrict the rest of `Fs`; the authorization
  layer is not modified.

```eko
list_sources: (fs: Fs) !-> Result []String IoError =
    names <- fs.list_dir("src").try()
    Ok names.map((name): "src/{name}")
```

## 2. Formatting functions

`show` handles display; functions handle formatting. A format-string language
is permanently excluded.

| Service | Signature | Semantics |
|---|---|---|
| `Int.to_hex` / `to_oct` / `to_bin` | `(Int) -> String` | shortest lowercase for positive values; negative values in 64-bit two's-complement form; the `UInt` family is the unsigned mirror |
| `Int.parse_hex` / `parse_oct` / `parse_bin` | `(String) -> Result Int ParseError` | input validation; no prefix; accepts `±`; case-insensitive; the `UInt` family mirrors these |
| `Float.to_fixed` | `(Float, UInt) -> String` | fixed point with `n` fractional digits; nearest decimal rounding, ties away from zero; NaN and the infinities are rendered as themselves |
| `Float.to_exp` | `(Float, UInt) -> String` | scientific notation with `n` mantissa digits; the same rounding |
| `String.pad_left` / `pad_right` | `(String, UInt, Char) -> String` | pads only, never truncates; a string already at the requested width is returned unchanged |

- **Interpolation and `show` are unchanged.** Both remain shortest round-trip
  (`08-builtins.md §4`). Formatting is a function invoked explicitly, not a
  second interpolation channel.
- **Hexadecimal literals and fixed-width hex.** `0x`/`0b` literals are
  specified in `02-syntax.md §4.1`; `to_hex` is a function. Fixed-width hex
  lives on machine words: `T.to_hex_pad: (T, UInt) -> String` zero-pads to
  at least `n` digits and never truncates — `pad_left`'s principle
  (`11-ffi.md §3.1`).
- **`parse_hex` and its siblings join the parse family** and share
  `ParseError` (`08-builtins.md §4`).
- **`to_fixed` and `to_exp` are total functions.** Formatting never fails:
  there is no `Result` and no domain error, and NaN and the infinities print
  as themselves. Failed conversion belongs to the parse family, which returns
  `Result … ParseError` (`08-builtins.md §4`).

```eko
Int.to_hex(255)               // "ff"
Int.to_hex(-1)                // "ffffffffffffffff"
Int.parse_hex("ff")           // Ok 255
Float.to_fixed(3.14159, 2)    // "3.14"
"7".pad_left(3, '0')          // "007"
```

## 3. Float mathematics

Out of domain is a value, not an error: NaN and the infinities follow the
hardware, and predicates accompany them. All functions in this section are
pure.

| Service | Signature | Semantics |
|---|---|---|
| `Float.sqrt` / `exp` / `log` / `log2` / `log10` | `(Float) -> Float` | `sqrt(-1) = NaN`; `log(0) = -Inf`; `log(-1) = NaN` |
| `Float.sin` / `cos` / `tan` / `asin` / `acos` / `atan` | `(Float) -> Float` | radians; `asin(2) = NaN` |
| `Float.atan2` | `(Float, Float) -> Float` | quadrant-correct |
| `Float.pow` | `(Float, Float) -> Float` | IEEE: `pow(0, 0) = 1`, `pow(1, Inf) = 1` |
| `Float.is_nan` / `is_inf` / `is_finite` | `(Float) -> Bool` | predicates; `is_inf` does not distinguish signs |
| `Float.abs` / `floor` / `ceil` / `trunc` / `round` | `(Float) -> Float` | `trunc` toward zero (the same base as `to_int`); `round` half to even |
| `Int.abs` | `(Int) -> Int` | `abs(Int.min)` panics (the overflow doctrine, `02-syntax.md §6`) |

- **This is the same axis as the IEEE rule for `Float ==`**
  (`08-builtins.md §6`). NaN is a value, distinguished by predicates rather
  than by a type layer. The checked gate sits at the conversion boundary:
  `Float.to_int` and `Float.to_uint` return `None` for NaN, the infinities, and
  out-of-range values (`08-builtins.md §4`). Inside the value domain
  operations are free; beyond it conversion is checked.
- **Integer overflow still panics.** `Int.abs` inherits the arithmetic rule:
  the result `abs(Int.min)` is not representable, so the call panics. No
  wrapping form is provided.
- **libm bit differences.** An implementation documents
  its maximum error; determinism is claimed per platform, not across
  platforms.
- **bignum, complex numbers, and matrices are library domain.**

```eko
Float.sqrt(-1.0).is_nan()     // True — out of domain is a value
Float.log(0.0).is_inf()       // True
```

## 4. Text utilities

| Service | Signature | Semantics |
|---|---|---|
| `String.contains` | `(String, String) -> Bool` | substring presence; the empty string is contained in every string |
| `String.starts_with` / `ends_with` | `(String, String) -> Bool` | prefix and suffix |
| `String.replace` | `(String, String, String) -> String` | **all occurrences** |
| `String.replace_first` | `(String, String, String) -> String` | first occurrence only |
| `String.slice` | `(String, Int, Int) -> Option String` | start plus length, Unicode scalar domain; out of range → `None`; O(n) explicit cost |
| `String.trim` / `trim_start` / `trim_end` | `(String) -> String` | Unicode White_Space |
| `List.slice` | `([]t, Int, Int) -> Option []t` | start plus length; out of range → `None` |

- **A slice is a domain check, so it returns `Option`**, by the same judgment
  as `get` (`05-names.md §4`). The prohibition is on index syntax; `slice` is
  a function and is unaffected.
- **A slice never clamps.** The requested range must lie entirely inside the
  value; a partially out-of-range request yields `None`. This is the
  contrast with `take`/`drop`, which truncate (§5.1).
- **Text is measured in Unicode scalar values**, so slicing costs O(n), and
  the cost is explicit in the name's contract (`07-runtime.md §7`).
- **Unicode case mapping and regular expressions are library domain.**
  `to_upper`/`to_lower` require the full Unicode tables plus context (the
  Turkish dotted and dotless i); an ASCII-only version is three lines of user
  code. Regular expressions are the same kind of library concern.

```eko
"hi wd".contains("wd")        // True
"a-b-c".replace("-", "+")     // "a+b+c"
"hello".slice(1, 3)           // Some "ell"
"  x ".trim()                 // "x"
```

### 4.1 Path

Paths are strings, and the path vocabulary is pure and lexical:

| Function | Signature | Semantics |
|---|---|---|
| `Path.join` | `(String, String) -> String` | joins the two components with the host separator |
| `Path.normalize` | `(String) -> String` | collapses separators and `.`, resolves `..` lexically; never touches the file system |
| `Path.is_absolute` | `(String) -> Bool` | decided by the platform rule |

`Path.normalize` is syntax, not semantics: it cannot know whether a
component is a symbolic link. Resolution against the working directory and
through symbolic links is world state and lives under `Fs.realpath` (§1).

## 5. Collection utilities

Collection utilities are functions, not syntax. A container with a new
surface does not acquire a new notation.

### 5.1 List

| Service | Signature | Semantics |
|---|---|---|
| `List.take` / `drop` | `([]t, Int) -> []t` | truncating and total: `n` beyond the length takes or drops everything; `n < 0` counts as zero |
| `List.take_while` / `drop_while` | `([]t, (t) ?-> Bool) ?-> []t` | predicate dispatchers, the same shape as `filter` |
| `List.join` | `([]t, String) -> String` | concatenates derived `show` forms with the separator; an element type without `show` is a compile error (the same judgment as interpolation) |
| `List.sort` | `([]t) -> []t` | derived default ordering via `<`; a record has no `<`, so the instantiation is a compile error (the same judgment as `==`) |
| `List.sort_by` | `([]t, (t, t) ?-> Bool) ?-> []t` | explicit comparator (less-than); stable |

- **Stability is a specification-level commitment.** Elements that compare
  equal keep their relative order, and the cost is O(n log n).
- **The comparator has signature `(t, t) -> Bool`.** The callback may touch
  the world: `?` passes through, exactly as for `filter`
  (`06-world.md §5`).
- `List.slice` is specified in §4.

```eko
xs.take_while((n): n < 3)          // ? dispatcher
xs.sort_by((a, b): a.age < b.age)  // stable; O(n log n)
{"a", "b"}.join(", ")              // "a, b"
```

### 5.2 Set

```eko
Set.of:        ([]t) -> Set t
Set.to_list:   (Set t) -> []t                 // insertion order (same as Dict)
Set.contains:  (Set t, t) -> Bool
Set.insert:    (Set t, t) -> Set t            // already present: the original set
Set.remove:    (Set t, t) -> Set t            // absent: the original set
Set.length:    (Set t) -> Int
Set.union:     (Set t, Set t) -> Set t        // left order first, then right-set new elements in right order
Set.intersect: (Set t, Set t) -> Set t        // preserves left order
Set.diff:      (Set t, Set t) -> Set t        // preserves left order
```

- **No notation and no literal.** The four composite forms are unchanged;
  construction goes through `Set.of({1, 2, 3})`, and the empty set is
  `Set.of({})` with an expected type. `Set.empty: () -> Set t` cannot be
  expressed (the ban on result-only generic variables, `04-types.md §8`), so
  no word is provided for it.
- **`Set.of` deduplicates keeping the first occurrence.** The order is
  insertion order, so results are deterministic and reproducible.
- **`Set` is a nominal std generic type**, written `Set t` and applied by
  juxtaposition (`04-types.md §7`). `==` is derived; `<` does not exist,
  because structural equality carries no ordering — the same judgment as for
  `Dict`.
- **`insert` and `remove` never mutate the receiver**; a no-op returns the
  original set.
- **The element domain is the key domain** specified in §5.3: the element
  type `t` must have `==`.

```eko
Set.of({1, 2, 3}).contains(2)   // True; no notation
empty: Set Int = Set.of({})     // the expected type supplies the element type
```

### 5.3 Dict key domains

- **The key domain is the data types that have `==`** (`08-builtins.md §6`).
  Capabilities and functions are excluded.
- **Structural equality is key equality.** Lists, records, tuples, ADTs,
  `Bytes`, `Set`, and dictionaries themselves are all admissible keys. The
  type `[[]String]Int` is therefore well formed (`04-types.md §2`).
- **Float keys.** Since `NaN != NaN`, a NaN key always
  misses. `0.0` and `-0.0` compare equal and are the same key — the IEEE
  doctrine extended to key equality.
- **The element domain of `Set` is the same domain.**

```eko
grid: [[]String]Int = {}        // lists as keys; the key domain is settled
```

## 6. Testing

Testing adds one annotation-slot word, two standard-library functions, and a family of
pure capability constructors. It adds no test framework, no test file kind,
and no second compiler mode.

### 6.1 The `test` slot

```eko
// person.eko
parse_rejects: test =
    case _parse_sex("x"):
        Err msg -> assert_eq(msg, "unknown sex: x")
        Ok sex  -> panic("unexpected: {sex}")

parse_smoke: test = assert_eq(_parse_sex("man"), Ok Man)
```

The annotation-slot vocabulary thus has three words:

| Slot word | Binds | Enters the interface | Compile condition |
|---|---|---|---|
| (empty) | a value | yes | always |
| `type` | a type | yes | always |
| `test` | a test | no | test root |

- **`test` and `main` are the same family**: host-contract bindings. `main`
  exists only in the entry module (`05-names.md §5`); `test` is allowed in any
  module. A module may hold both, and the two contracts compile
  independently.
- **Only at module top level, and only through `=`.** A test is declared
  once; `:=` and `<-` do not apply.
- **A test block is an ordinary block** (`02-syntax.md §1`): arbitrary
  expressions and bindings, with its value discarded. It has no arrow and
  therefore no bit to reconcile; world-touching calls inside it are evaluated
  as usual. Determinism comes from feeding derived capabilities (§6.5), not
  from exempting the bit.
- **Test names are snake_case** (`04-types.md §3`) and the report name is
  `module.test_name`. A duplicate within a module is a compile error, in
  keeping with name conflicts being errors rather than silent shadowing
  (`05-names.md §7.2`).
- **Slot words are not reserved words.** Slot words are snake_case and are
  layered against PascalCase type names (`04-types.md §3`), so a module named
  `test` and a local binding named `test` remain legal. A slot word lives
  only in the annotation position after `:`.

### 6.2 Location: in the module under test

There is no test module kind. A module containing `test` bindings is an
ordinary module; `test` is merely an additional class of top-level binding.
Tests live in the same file as the code they test, with three direct
consequences:

- **The private surface is directly testable.** A `test` binding shares the
  module scope with top-level bindings, so `_parse_sex` is callable as it
  stands. A private function is visible inside its module anyway; a
  same-module test uses it unchanged. Nothing must be made public for tests,
  and the interface never lies on account of tests.
- **Test helpers stay private.** `_mk_person` lives in the module under test
  behind the `_` prefix and causes no export pollution. Test bindings are
  absent from non-test compilation, and unused warnings are counted over all
  compilation roots, test roots included (§6.6), so such helpers raise no
  `W-unused`.
- **A cross-file test is an ordinary module plus `import`.** To test from
  another file, create an ordinary module and import the target; only the
  public surface is reachable, and the target's private functions cannot be
  tested there. Private names are tested in their own module only — the same
  discipline as production code.

The excluded alternative is a test-file suffix such as `_test.eko`: a suffix
convention creates a second-class module whose private surface is
unreachable, forcing the target to be made public for tests. The same file is
the same truth. The pain of a large file is solved by splitting modules
(`05-names.md §5`), not by splitting tests.

### 6.3 Assertions

```eko
assert:    (c: Bool) -> _ = if c: _ else: panic("assert failed")
assert_eq: (a: t, b: t) -> _ =
    if a == b: _ else: panic("assert_eq failed\n  left:  {inspect(a)}\n  right: {inspect(b)}")
```

- **Both are std free functions in the implicitly opened scope**
  (`05-names.md §11`); both are pure (panic does not set the world bit),
  always enabled, and require zero configuration.
- **`assert_eq` locks both arguments to the same type.** `==` and `inspect`
  are both compile-time generic dispatch (`08-builtins.md §6`). Capabilities
  and functions have no `==`, so asserting on them is a compile error
  (`E-no-eq`).
- **Failure messages go through `inspect`**, the structural channel, which is
  never overridable; this discharges the promise recorded for diagnostics,
  tests, and debugging.
- **A failure is a panic.** The host running the tests captures the panic per
  test and records that test as failed. Tests are panic's first customer, not
  an exception system; in normal compilation a panic still aborts the program.
- **Custom failure messages compose existing vocabulary**:
  `if cond: panic(...)`.
- **`assert_ne` mirrors `assert_eq`**: the same type lock, the same `inspect`
  channel; it panics when the values are equal.

### 6.4 Table-driven tests

A parameterized test is a list plus `each` — the existing dispatcher, which is
transparent in the bit.

The table literal carries an annotation, because a composite literal never
stands alone (`04-types.md §2`). The `each` callback takes one parameter, so
tuple elements are not destructured automatically; destructuring happens on a
binding line (`03-control.md §5`).

```eko
parse_corners: test =
    rows: [][String, Result Sex String] =
        {{"man", Ok Man}, {"woman", Ok Woman}, {"x", Err "unknown sex: x"}}
    rows.each((row):
        (s, want) = row                    // tuple split on the binding line
        assert_eq(_parse_sex(s), want))
```

The table is the parameter and `each` is the driver. The first failure panics
and stops, and that test is recorded as failed. Multi-parameter callbacks such
as `(p, i):` (`each_with_index`) are parameter groups in their own right
(`04-types.md §2`), not destructuring; tuple splitting is always explicit — on
a binding line or in a `case` pattern.

### 6.5 Capabilities and determinism

Code under test consumes capabilities; the test feeds it derived capabilities.
A mock is a derivation, not a framework.

| Derivation | Signature | Behavior |
|---|---|---|
| `Out.null` | `() -> Out` | swallows every write |
| `Out.tee` | `(Out, (String) !-> _) -> Out` | one write, one report: assertions ride along with the stream |
| `In.empty` | `() -> In` | reads are immediately EOF (`read_line` → `Ok None`) |
| `In.from_lines` | `([]String) -> In` | reads the lines in order, EOF when exhausted |
| `Fs.null` | `() -> Fs` | every path → `Err NotFound` |
| `Env.frozen` | `([String]String) -> Env` | reads the fixed table |
| `Args.frozen` | `([]String) -> Args` | reads the fixed value (`06-world.md §2`) |
| `Clock.frozen` | `(Time) -> Clock` | reads the fixed value; `sleep` returns immediately (`08-builtins.md §5`) |
| `Rand.with_seed` | `(Int) -> Rand` | the seed fixes the order; the sequence itself is an implementation detail and is not promised across versions — assert properties, not specific values |

`Out.tee`'s signature is fixed here. Construction is pure; the observer fires
once per write, and its bit is absorbed by the `!` of the write builtin (the
same OR absorption as `Fs.with_file`, `06-world.md §5`). A pure observer flows
in by adaptation.

```eko
greet_lines: test =
    out = Out.tee(Out.null(), (s): assert_eq(s, "hi wd"))
    greet(out, "wd")
```

- **Tests cannot obtain real capabilities.** Construction right belongs to the
  host; `main`'s parameter table is the only entrance (`06-world.md §2`),
  and a test has no parameter table. Derived combinators are the test's entire
  world, which makes the capability layer structurally deterministic.
  Integration tests against real I/O are host domain.
- **The embedding contract is discharged with zero host magic.** All
  derivations complete in the standard library layer.
- **Bits are unchanged.** What is fed is a real capability value, and the
  arrows of the functions under test do not change. Tests relax no check:
  same compiler, same rules.

### 6.6 Roots and running

- **A test root is a module containing `test` bindings.** The test run is
  initiated by the host or tool; command names are the tooling domain. The
  runner collects by root, compiles and executes root by root, and captures
  panic per test. The dependency closure is the ordinary DAG
  (`05-names.md §10`). Panic capture is a process boundary: the runner reads
  the panic block from the child's standard error and parses it per `§7.6`.
- **`test` bindings do not enter the interface.** Separate compilation is
  unaffected, and interface extraction skips them.
- **A module without tests is an ordinary dependency** under test compilation
  and produces no test surface. There are no ambient tests, matching the
  absence of ambient I/O.
- **Execution order and parallelism are host domain.** Tests share no state
  (values are immutable, top level is pure), so parallelism is safe by
  construction — a direct dividend of top-level purity (`06-world.md §7`).

## 7. Diagnostics

A diagnostic is a compile-time message. Runtime panic output is not a
diagnostic, but the CLI host's panic format is normative (`§7.6`).

### 7.1 Messages

Diagnostic messages are always English. The reasons are toolchain convention
and ecosystem affinity: error text stays searchable and citable, and message
language is independent of the language a document is written in.

The style rules are normative:

- Start lowercase.
- No trailing period.
- State the fact; do not pass judgment.
- Put repair suggestions on a hint line.
- Enclose names in backticks.

Individual entries are frozen word by word as the vocabulary grows. Message
wording may change over time; the code and its meaning may not.

### 7.2 Symbolic codes

The form is `E-` plus a kebab-case semantic word, for example
`E-nominal-mismatch`. Warnings carry the `W-` prefix. Codes are greppable.

A published code is never reused and never redefined. This is why
documentation, issues, and tests cite the code rather than the message: a
message can be reworded, while the code is a stable anchor. A numeric code is
rejected for the opposite reason — it would require a lookup table and could
not be grepped.

The first batch of codes:

| Code | Meaning | Source |
|---|---|---|
| `E-nominal-mismatch` | nominal does not flow into nominal | `04-types.md §5` |
| `E-shape-mismatch` | shape does not adapt (not the same shape) | `04-types.md §5` |
| `E-exhaust` | missing arm of an exhaustive match | `03-control.md §3` |
| `E-world-bit` | bit reconciliation failure (over- or under-declaration) | `06-world.md §5` |
| `E-open-bit-unfed` | an open bit that no feeder supplies | `06-world.md §5` |
| `E-open-bit-position` | a `?` in a position where no binding is generalized (result type, container element, record field) | `06-world.md §5.2` |
| `E-phantom` | phantom parameter / result-only variable | `04-types.md §7`, `§8` |
| `E-name-case` | naming-case violation | `04-types.md §3` |
| `E-name-conflict` | name conflict (same level, constructor clash, or duplicate module stems among imports) | `05-names.md §6`, `§7.2` |
| `E-namespace-owner` | a type-namespace member declared outside the owning module | `05-names.md §8` |
| `E-cycle-import` | circular dependency | `05-names.md §10` |
| `E-no-eq` | equality, keying, or assertion over a type whose `eq` derivation halts (a capability or function payload) | `08-builtins.md §6.4` |
| `E-json-shape` | no default JSON isomorphic form (for example a non-string-keyed dictionary) | `08-builtins.md §7` |
| `E-instantiation-depth` | generic instantiation expansion does not terminate | `07-runtime.md §6` |
| `E-literal-overflow` | a numeric literal outside the range of its target type | `02-syntax.md §4.1` |
| `E-tab` | a tab character used as whitespace | `02-syntax.md §1.1` |

The vocabulary is open to growth; adding an entry freezes it. This table is
the first batch.

### 7.3 Warnings

The warning list is closed and contains exactly four members:

| Code | Meaning | Source |
|---|---|---|
| `W-unreachable` | code after a `Never` call | `04-types.md §10` |
| `W-unused` | unused binding or unused import, counted over all compilation roots | §6.6 |
| `W-redundant-import` | an explicit `import std`: the implicit import (05-names.md §11) is always present | `05-names.md §11` |
| `W-deprecated` | use of a member whose documentation carries `/// @deprecated`; the interface diff between compiler versions is the authority (12-toolchain.md §4) | `12-toolchain.md §5` |

- **The closure is a contract.** Users perform zero-warning engineering
  against this list; an opened gap breaks the contract. A new warning requires
  amending this specification, not implementation discretion.
- **Warnings are not suppressible.** There is no `#[allow]`, no pragma, and no
  comment switch. Silencing a warning means fixing the code: for `W-unused`,
  rename the binding with a leading underscore (`_x`) or delete it; for
  `W-unreachable`, delete the code or restructure the control flow.
- **Parameters do not participate in `W-unused`.** Callbacks receive
  parameters positionally, and surplus positions are normal
  (`each_with_index((p, i): …)` is existing usage).

### 7.4 Structure and style

```text
E-nominal-mismatch: `Person` does not flow into `Human` (both nominal)
  hint: convert via a shared anonymous shape, or rebuild field by field
```

- **One message line (code plus statement), then indented detail lines**
  (hint, names involved). The position format (`file:line:column`) is
  implementation domain.
- **Diagnostics have three severities: error, warning, and none.**
  Bit diagnostics are always errors (`06-world.md §5`); this restates the
  rule rather than narrowing it.
- **Panic messages are runtime output** and are not part of the vocabulary
  specified here.

### 7.5 Exclusions

Each item below is excluded.

Standard library:

- A format DSL (printf-style or format strings) is excluded. Interpolation
  plus named functions already cover the need, and names can be grepped.
- `Set` notation and literals are excluded. The four composite forms are
  unchanged, and `{a, b}` already belongs to lists and tuples.
- Unicode case mapping and regular expressions are excluded from the standard library;
  they are library domain.
- bignum, complex numbers, and matrices are excluded from the standard library; they
  are library domain.
- Unstable sorting is not an option. Stability is the only commitment;
  quicksort-style internals are implementation freedom and are not observable.

Testing:

- No test-file suffix and no test module kind. A suffix convention creates a
  second-class module and forces the private surface to be published.
- No attribute annotations (`#[test]`, decorators). The annotation slot
  already exists, with `type` as the precedent; an attribute layer would be a
  second concept for one idea.
- No `describe`/`it` and no nested suites. Nesting is narrative sugar; a
  table plus a pure `_helper` function is equivalent and adds no surface.
- No `setup`/`teardown` hooks. Resources go through existing machinery such
  as `Fs.with_file`, and construction goes through pure functions; hooks are
  implicit control flow.
- No mock framework. A mock is a derived capability; a framework would be a
  second vocabulary for the capability system.
- No parameterized-test syntax. The table is the parameter and `each` is the
  driver.
- No test bit exemption and no relaxed checking. Same compiler, same rules;
  determinism comes from values, not from a lie.

Diagnostics:

- No numeric error codes. They impose a lookup table and cannot be grepped.
- No suppressible warnings (no allow attribute, pragma, or comment switch).
  The warning list is a contract, and suppression would be a lie.
- No open-ended warning list. New warnings go through an amendment, which
  keeps the contract stable.
- Runtime panic as a *diagnostic* is excluded — panic output is not a
  compile-time message; its CLI format is normative in `§7.6`.

### 7.6 Panic output (CLI host)

Panic output is the CLI host's surface, and its format is normative: the
test runner parses it (`§6.6`) and users cite it. The `P-` prefix is the
runtime sibling of `E-` and `W-` under the code discipline of `§7.2` — a
published code is never reused and never redefined, and documentation and
tests cite the code, not the message.

- **Channel.** Standard error, written by the host. Panic output never
  flows through `Out`: `panic` is pure and holds no capability
  (`06-world.md §3.3`); abort-reporting is host behavior, not world
  contact.
- **First line.** One rigid line: `panic [P-code]: message`. The message
  follows `§7.1` style; a user-supplied message (`panic`, `expect`) is
  printed verbatim and may span lines — frame lines are recognized by
  their leading `at`.
- **Backtrace.** Always printed, innermost frame first:

  ```
    at module.function (file:line:column)
  ```

  Function names are fully qualified (`05-names.md §7`). Frames eliminated
  by self tail calls do not appear (`07-runtime.md §8.3`). How frames are
  captured is implementation domain; the format is not.
- **Standard origins.**

  | Code | Origin | Message |
  |---|---|---|
  | `P-panic` | `panic(msg)` | the argument, verbatim |
  | `P-unwrap` | `Option.unwrap` / `Result.unwrap` | `Option.unwrap` on `None` / `Result.unwrap` on `Err` |
  | `P-expect` | `Option.expect` / `Result.expect` | the argument, verbatim |
  | `P-assert` | `assert` on `False` | assertion failed |
  | `P-assert-eq` | `assert_eq` mismatch | assertion failed, with `left:` and `right:` inspect lines |
  | `P-div` | division by zero | division by zero |
  | `P-overflow` | arithmetic overflow | arithmetic overflow |

- **Termination.** The process exits with code `1` (`06-world.md §2.1`).
- **Abort class.** Stack overflow and memory exhaustion are aborts, not
  panics (`07-runtime.md §8.4`): a best-effort single line
  `abort: reason` on standard error and a platform abort exit —
  distinguishable from panic's exit code.

```
panic [P-assert-eq]: assertion failed
  left:  Person {name: "ada", age: 36}
  right: Person {name: "grace", age: 41}
  at person.make_person (person.eko:14:9)
  at std.assert_eq (…)
```
