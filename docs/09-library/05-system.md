# 9. Standard Library

## 9.12 File System and Handles

Every member and every type of this clause is declared by the capability
module (Clause 9.1.1).

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

The five variants cover the open surface. There is no flags integer and no
separate rights datum.

### 9.12.2 File Operations

`Fs.open` yields an opaque `File` handle; a program cannot construct one, only
obtain it from `Fs.open` or `Fs.with_file`. `File.read` reads sequentially at
most the requested number of characters and rejects non-UTF-8 content with
`Err InvalidData`; `File.read_bytes` is the raw-byte counterpart and never
reports `InvalidData` (Clause 9.5.4). `File.read_line` reads one line without
its terminating `'\n'` and reports end of input as `Ok None` (Clause 9.2.4).
`File.write` and `File.write_bytes` write at the current position. `File.seek`
takes a non-negative absolute byte position (`UInt`) from the start; a
position past end of file yields `Err InvalidData`. Every operation in this
subsection is world-touching and sets the bit (Clause 7.9).

The `Fs` namespace provides whole-path mirrors — `Fs.read`, `Fs.write`,
`Fs.append`, `Fs.read_bytes`, `Fs.append_bytes`, `Fs.exists` — where the text
mirrors retain the UTF-8 gate, the byte mirrors do not, and `Fs.exists` yields
`Bool`. `IoError` is the closed error union of the file-system surface,
carrying the reason required by Clause 9.2.1:

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

`NotFound` reports an absent path, `Permission` denied access, `Exists` an
exclusive creation over an existing path, and `NotFile` a non-regular file
where a regular file is required. `Closed` reports a stale handle (Clause
9.12.3); `InvalidData` carries a content detail, including non-UTF-8 text; and
`Other` carries a system message for a failure without a dedicated variant,
such as a non-empty directory passed to `rm_dir` or a cross-device rename.
Interrupted system calls are retried internally, so `Interrupted` never
surfaces.

### 9.12.3 Generations

A handle carries a generation. `File.close` increments the generation of the
slot, and every older copy of the handle becomes stale. Any operation on a
stale handle, including a second `close`, yields `Err Closed`. Leaks are
permitted. Without `close`, the operating system reclaims the resource at
process exit. No finalizer is promised (Clause 8.6). Staleness is decided by
the language-level generation, not by the number the operating system assigns
to a descriptor; descriptor reuse by the host cannot make a stale handle
valid.

### 9.12.4 Fs.with_file

```eko
Fs.with_file: (Fs, String, OpenMode, (File) !-> a) !-> Result a IoError
```

`Fs.with_file` opens the path, applies the callback, and closes the handle on
both the value path and the error path. A panic is an abort: the operating
system reclaims the resource and no unwinding occurs. The callback type is
`(File) !-> a` and has no open bit. `Fs.with_file` is world-touching, so `OR`
absorbs the callback's bit (Clause 7.9.5); a pure callback flows in by bit
adaptation and nothing is generalized. A fallible callback produces a nested
result: the callback's `Result` is the payload of the outer `Result`, which
covers opening and closing the channel. A single error channel is obtained by
handling the callback's failure and ending in `_`, or by flattening the two
levels with a `<-` chain (Clause 8.2.4).

### 9.12.5 Attenuation

There is no `Rights` type. The attenuation combinators are `Fs.root` and
`Fs.read_only`.

`Fs.root` restricts a file-system capability to a subtree and does not check
existence. `Fs.read_only` withdraws write rights. Both are pure and set no bit
(Clause 7.9). They compose, and they only narrow the grant:
`Fs.read_only(Fs.root(fs, "/tmp/work"))`.

### 9.12.6 Directory Operations

`Fs.list_dir` yields the entry names in deterministic lexicographic order,
excluding `.` and `..`. A path that is not a directory yields `Err NotFile`.
The result contains names, not paths; join with `Path.join(dir, name)` (Clause
9.11.2). `Fs.mkdir` creates one level; an existing path yields `Err Exists`.
`Fs.mkdir_all` creates the whole chain; an already existing directory yields
`Ok`. `Fs.rm` removes a file; a directory yields `Err NotFile`. `Fs.rm_dir`
removes an empty directory; a non-empty directory or a non-directory yields
`Err` with `Other`. `Fs.rename` renames or moves; a cross-device move and
similar failures yield `Err` with `Other`. `Fs.stat` yields `FileInfo` (Clause
9.12.7). `Fs.realpath` resolves `.`, `..`, and symbolic links through the file
system; a relative input is resolved against the process working directory.
The result of reading platform metadata such as symbolic links is
implementation-defined (Clause 2.5). All operations in this subsection set the
world bit (Clause 7.9). Error mapping uses the closed `IoError` of Clause
9.12.2 and adds no variant, so existing exhaustive matches remain exhaustive.

### 9.12.7 FileInfo

```eko
FileInfo: type = [size: UInt, mtime: Time, is_dir: Bool]
```

`size` is the file size in bytes. For a directory, `size` is a
platform-defined value. `mtime` is a wall-clock time (Clause 9.7.1); its
display form is ISO 8601 UTC (Clause 9.7.4). `is_dir` reports whether the path
is a directory.

## 9.13 Formatting and Mathematics

### 9.13.1 Formatting

Formatting is performed by named functions. `show` and string interpolation
remain the shortest round-trip display channel; there is no format-string
language and no second interpolation channel. `Int.to_hex`, `Int.to_oct`, and
`Int.to_bin` yield the shortest lowercase form for non-negative values. A
negative value is rendered in 64-bit two's-complement form. The `UInt` family
is the unsigned mirror. `Int.parse_hex`, `Int.parse_oct`, and `Int.parse_bin`
are input validation and return `Result Int ParseError`. They accept no
prefix, accept an optional sign, and are case-insensitive. The `UInt` family
mirrors them and shares `ParseError`. `Float.to_fixed` renders fixed point
with a requested number of fractional digits. `Float.to_exp` renders
scientific notation with a requested number of mantissa digits. Both use
nearest decimal rounding with ties away from zero. Both are total: `NaN` and
the infinities render as themselves, and formatting never returns `Result`.
`String.pad_left` and `String.pad_right` pad to a requested width with the
given `Char` and never truncate; the width counts Unicode scalar values, and a
string already at the requested width is returned unchanged. Fixed
width hexadecimal is `T.to_hex_pad` on machine words; it zero-pads to at least
the requested number of digits and never truncates.

The complete formatting inventory is listed in Annex C.

### 9.13.2 Float Mathematics

`Float.sqrt`, `Float.exp`, `Float.log`, `Float.log2`, and `Float.log10` are
total functions. Out-of-domain input is a value, not an error: `sqrt(-1)` is
`NaN`, `log(0)` is `-Inf`, and `log(-1)` is `NaN`. `Float.sin`, `Float.cos`,
`Float.tan`, `Float.asin`, `Float.acos`, and `Float.atan` operate in radians;
`asin(2)` is `NaN`. `Float.atan2` is quadrant-correct. `Float.pow` follows
IEEE 754; `pow(0, 0)` is `1` and `pow(1, Inf)` is `1`. `Float.is_nan`,
`Float.is_inf`, and `Float.is_finite` are predicates. `Float.is_inf` does not
distinguish signs. `Float.abs`, `Float.floor`, `Float.ceil`, `Float.trunc`,
and `Float.round` are total; `trunc` rounds toward zero and `round` rounds
half to even. `Int.abs` follows the arithmetic rule and panics on `Int.min`,
whose absolute value is not representable. No wrapping form exists. The
checked gate sits at the conversion boundary: `Float.to_int` and
`Float.to_uint` return `None` for `NaN`, the infinities, and out-of-range
values (Clause 9.6.2). Inside the value domain, operations are total. The
maximum error bound of the floating-point library functions is
implementation-defined (Clause 2.5). Determinism is claimed per platform, not
across platforms. Bignum arithmetic, complex numbers, and matrices are not
part of the standard library.

## 9.14 Testing

### 9.14.1 The test Slot

A `test` binding binds a test, does not enter the module interface, and is
compiled only for a test root. The binding MUST appear at module top level and
MUST use `=`. Rebinding and inverted binding do not apply. A test body is an
ordinary block or a single expression on the same line. Its value is
discarded. It has no arrow and no world bit to reconcile; world-touching calls
inside it are evaluated under Clause 7.9 without exemption. Test names are
snake_case. A duplicate test name within a module is a compile error
(`E-name-conflict`, Annex B). The report name is `module.test_name`. The slot
word `test` is not a reserved word. It is recognized only in the annotation
position after `:`; a module named `test` and a local binding named `test`
remain legal.

### 9.14.2 Location

There is no test module kind. A module containing `test` bindings is an
ordinary module, and `test` is an additional class of top-level binding. Tests
reside in the module under test. A test shares the module scope with top-level
bindings, so a private function is directly callable. No binding is made
public for testing, and the interface is unaffected. A test helper is a
private top-level binding with a leading `_`. Test bindings are absent from
non-test compilation; a helper used only by tests raises no warning. A
cross-file test is an ordinary module plus `import`. Only the public surface
of the target is reachable there; private names are tested in their own
module. A test-file suffix is not part of the language.

### 9.14.3 Assertions

The three signatures are Annex C.1.

`assert` panics when its argument is `False`. `assert_eq` panics when the
arguments are not equal; `assert_ne` panics when they are equal. All three are
std source, opened by the injected selective import of Clause 9.1.2. They are
pure, always enabled, and require no configuration. A failure is an ordinary
`panic` (`P-panic`, Annex B.5); the message text is std source, and no
dedicated panic code exists for the family. `assert_eq` and `assert_ne` lock
both arguments to the same type. `==`, `inspect`, and the structural
derivation are compile-time generic dispatch (Clause 9.8.7). Capabilities and
functions have no `==`, so an assertion over them is a compile error
(`E-no-eq`, Annex B). A failure message uses `inspect`, the structural
channel, which is never overridable. A user-supplied failure message is
written with `panic` and an explicit message. Assertion failure is a panic.
The host running the tests captures the panic per test and records that test
as failed; outside a test run, a panic aborts the program (Clause 8.4.2).
Panic output follows Annex B.

### 9.14.4 Table-Driven Tests

A table-driven test is a list of rows together with the `each` dispatcher. The
table literal carries an annotation; a composite literal does not stand alone
(Clause 6.6.2). The `each` callback takes one parameter, so tuple elements are
not destructured automatically; destructuring occurs on a binding line (Clause
6.3.2).

```eko
// context: `Sex` and `_parse_sex` are declared by the module under test.
parse_corners: test =
    rows: [][String, Result Sex String] =
        {{"man", Ok Man}, {"woman", Ok Woman}, {"x", Err "unknown sex: x"}}
    rows.each((row):
        (s, want) = row
        assert_eq(_parse_sex(s), want))
```

The first failure panics and stops the test; that test is recorded as failed.
A multi-parameter callback such as `(p, i):` is a parameter group, not
destructuring (Clause 6.4.3). Tuple splitting is explicit, on a binding line
or in a `case` pattern (Clause 6.8).

### 9.14.5 Determinism

Code under test consumes capabilities; a test supplies derived capabilities.
Mocking is performed by capability derivation; no separate framework exists.
The derivation combinators of this subsection are members of the capability
namespaces and are declared by the capability module (Clause 9.1.1), except
`In.from_lines` and `Rand.with_seed`: their sequences advance across calls,
which has no stateless expression, so both are compiler-provided declarations
over injected extern symbols (Clause 10.8).

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

`Out.tee` has signature `(Out, (String) !-> _) -> Out`. Construction is pure.
The observer fires once per write, and its bit is absorbed by the `!` of the
bundle's write closure (Clause 7.9.5). A pure observer flows in by adaptation.
Tests cannot obtain real capabilities. Construction right belongs to the host,
and `main`'s parameter table is the only entrance (Clause 7.9.1); a test has
no parameter table. Derived combinators are the test's entire world.
Integration tests against real input and output belong to the host. The world
bits of the functions under test are unchanged. A test feeds a real capability
value; it relaxes no check and uses the same compiler and the same rules as
ordinary code.

### 9.14.6 Roots and Execution

A test root is a module containing at least one `test` binding. The runner
collects tests by root, compiles and executes root by root, and captures panic
per test. The dependency closure is the ordinary acyclic module graph (Clause
7.2.4). Panic capture is a process boundary: the runner reads the panic block
from the child's standard error and parses it under Annex B. `test` bindings
do not enter the interface, so separate compilation is unaffected. A module
without tests is an ordinary dependency under test compilation and produces no
test surface. There are no ambient tests. `W-unused` is counted over all
compilation roots, test roots included; parameters do not participate (Annex
B). The scheduling order of tests is implementation-defined (Clause 2.5).
Parallelism is host domain. Tests share no state: values are immutable and the
top level is pure, so parallel execution is permitted.

## 9.15 Library Exclusions

The following are not part of the standard library:

- a format-string or printf-style DSL;
- regular expressions;
- Unicode case mapping;
- an encoding family other than UTF-8, including UTF-16, latin-1, and GBK;
- byte literals, index syntax, mutable `Bytes`, and builder or resize
  vocabulary;
- digest or CRC functions;
- bignum arithmetic, complex numbers, and matrices;
- YAML and TOML;
- pretty printing as a default or as a `show` form;
- a local timezone, `Date`, or `DateTime`;
- `Set` notation and `Set` literals;
- a payload-dispatched accessor, or a compound word such as `get_or`:
  `Option.get` is an ordinary function, and `get` composes with `unwrap_or`;
- a `Rights` type or a rights datum;
- an FFI capability;
- world-touching free functions and ambient input or output;
- a test framework, a test-file suffix, a test module kind, a second compiler
  mode, attribute annotations, `describe`/`it`, nested suites, setup or
  teardown hooks, a mock framework, parameterized-test syntax, a test bit
  exemption, and relaxed checking;
- unstable sorting as an option;
- runtime reflection over types.

Additions to the library inventory are specification changes (Clause 9.1.4).
