# Chapter 1 — Overview

Eko is a small, statically typed, immutable language that compiles to
machine code. There is no virtual machine and no runtime system in the
traditional sense: type identity, effects, and generics are all resolved
at compile time, and memory is managed by reference counting emitted by
the compiler.

Eko's stated goals are **honest, concise, consistent**:

- **Honest** — costs and effects are visible: capabilities gate the
  operating-system surface, `extern` declarations gate the native track
  (11-ffi.md), the world bit marks world-touching calls, indexing syntax
  does not exist, and expensive operations say so in
  their names.
- **Concise** — one mechanism per problem: `case` is the only branching
  primitive, operators are sugar over named functions, modules are
  files, and the standard library adds surface without adding syntax.
- **Consistent** — a few rules apply everywhere: composite literals are
  resolved by expected type, failure follows a three-way taxonomy, and
  the `_` family means "not part of the public conversation" in every
  position it appears.

## 1. Design goals in outline

The language surface can be summarized in a few lines:

- **Values are immutable.** Bindings are `=` (initial), `:=` (rebind),
  and `<-` (inverted binding, which threads the rest of a block as a
  trailing callback). There is no mutation anywhere (03-control.md §1).
- **Functions are expressions.** Lambdas are written
  `(params): body`, carry no annotations, and capture by reference
  sharing (03-control.md §2).
- **`case` is the branching primitive**; `if` is sugar over
  `case`-on-`Bool` (03-control.md §3–4).
- **Types are nominal and declared in the annotation slot**:
  `Person: type = [name: String, age: UInt]`. Anonymous record, tuple,
  list, and dictionary shapes exist for structural views; nominal values
  flow into same-shape anonymous positions, never the reverse
  (04-types.md §5).
- **Generics are applied by juxtaposition** (`Result Int String`) and
  generalized implicitly at annotated bindings (04-types.md §7–8).
- **Every function is a free function**; method syntax is uniform
  function call syntax: `a.f(b) ≡ f(a, b)` (05-names.md §1).
- **All I/O flows from `main`'s parameter list** — the authorization
  manifest. Capabilities are opaque values; the world bit on arrows
  records world contact (06-effects.md §2, §5).
- **Memory is reference counting**; immutable values plus strict
  evaluation make the heap graph acyclic by construction, so counting
  is complete and no cycle collector exists (07-runtime.md §3–4).
- **There is no loop syntax.** Iteration is combinators and recursion;
  self tail calls are guaranteed to run in constant stack
  (07-runtime.md §8).

## 2. The four I/O mechanisms

The I/O layer is four orthogonal mechanisms; each solves exactly one
problem and none overlaps another (06-effects.md §1):

| Dimension | Mechanism | Question it answers |
|---|---|---|
| Authority | capability values | who is allowed |
| Honesty | the world bit (`!`/`?`) | does this call touch the world |
| Failure | `Result` values | what happens when it fails |
| Resource | generational handles + `with_*` | what happens when resources are misused (checked, never undefined behavior) |

The capability already carries the authorization granularity; the world
bit is the projection of capability *use* onto arrows. The two share a
single vocabulary — capability names — and the bit introduces no second
vocabulary.

## 3. Failure taxonomy

Failure has three natures, each with exactly one word
(08-builtins.md §4.1):

| Nature of failure | Word | Example |
|---|---|---|
| Input validation: external data is untrusted, the reason matters | `Result` | `UInt.parse`, file reads, JSON decoding |
| Domain check: asking whether, not why | `Option` | `xs.get(i)`, `Char.from_code`, `Float.to_int` |
| Invariant violation: the programmer's own mistake | `panic` | arithmetic overflow, `expect` on `None` |

Conversions never panic. Recoverable failure is always a `Result` value;
`panic` aborts the process and is never used for control flow
(06-effects.md §3).

## 4. A complete program

The manual's running example reads a CSV file of people and prints it
with line numbers. It exercises bindings, inverted bindings, `case`,
`with_file`, `Result` combinators, interpolation, and derived `show`:

```eko
Sex:    type = Man | Woman
Person: type = [name: String, age: UInt, sex: Sex, address: String]

parse_sex: (data: String) -> Result Sex String =
    case data:
        "man"   -> Ok Man
        "woman" -> Ok Woman
        _       -> Err "unknown sex: {data}"

make_person: (data: []String) -> Result Person String =
    name    <- data.get(0).to_result("short row: missing name").try()
    age_s   <- data.get(1).to_result("short row: missing age").try()
    age     <- UInt.parse(age_s).map_err((e): "age: {e}").try()
    sex_s   <- data.get(2).to_result("short row: missing sex").try()
    sex     <- parse_sex(sex_s).try()
    address <- data.get(3).to_result("short row: missing address").try()
    Ok {name: name, age: age, sex: sex, address: address}

main: (out: Out, fs: Fs) ! =
    case fs.with_file("data.csv", Read, (file):
        content <- file.read(4096).map_err((e): "io: {e}").try()
        persons <- content.split("\n")
            .traverse((l): l.split(",").make_person())
            .try()
        Ok persons.each_with_index((p, i): out.println("{i}: {p}"))):
        Ok _  -> _
        Err e -> out.println(e)
```

Reading it top to bottom:

- `main: (out: Out, fs: Fs) !` — the parameter list is the program's
  permission request; the host grants capabilities per parameter. The
  `!` says the body touches the world.
- `with_file` opens the file and guarantees closure on both the value
  and error paths; its callback's result becomes the success payload.
- `<-` threads each step's remaining block as a trailing callback;
  `.try()` and `.map_err(...)` move errors through the chain, which ends
  in `Ok ...` (03-control.md §6).
- `"{i}: {p}"` interpolates via the derived `show` service
  (08-builtins.md §6).
- Access to `data` is `get`-only and returns `Option`; the choice of how
  to handle absence (`to_result`, `expect`, `unwrap_or`) is made at the
  consumption site (05-names.md §4).

## 5. Language surface

| Area | Summary | Chapter |
|---|---|---|
| Layout | `:` opens indented blocks; free continuation inside brackets | 02 |
| Operators | closed set; sugar over namespace functions | 02 |
| Bindings | `=`, `:=`, `<-` | 03 |
| Lambdas | zero annotations, capture is delegation | 03 |
| Branching | `case` primitive, guards, `if` sugar | 03 |
| Patterns | records, or-patterns, list rest, multi-scrutinee | 03 |
| Types | ADTs, records, four composites, nominal + shape adaptation | 04 |
| Generics | binders after `type`, juxtaposed application, implicit variables | 04 |
| `Never` | bottom type; exhaustiveness comes from patterns | 04 |
| Calls | UFCS identity, four-station resolution | 05 |
| Access | `get`/`at` only; trust declared at consumption | 05 |
| Modules | file is module; `import` opens types; functions fully qualified | 05 |
| Effects | capabilities, world bit, open bit `?` | 06 |
| Handles | `OpenMode`, generation checks, `with_file`, attenuation | 06 |
| Runtime | boxed immutables, O(1) sharing, reference counting, monomorphization | 07 |
| Iteration | combinators, self tail recursion, TCO guarantee | 07 |
| Data types | `Bool`, `Char`/`String`, `Bytes`, numeric conversions, time | 08 |
| Services | `show`/`inspect`, `==`, `<`, `with`, JSON | 08 |
| Library | fs, formatting, math, text, collections | 09 |
| Tooling | `test` slots, assertions, diagnostics codes | 09 |
| FFI | `extern` + `Pure`, machine words, `C.*`, `ctype` and `C.In`, callbacks, linking, embedding veneer, the prelude channel | 11 |

## 6. Terminology

| Term | Meaning |
|---|---|
| capability | opaque, unforgeable, copyable value granting authority for a class of world contact (06-effects.md §2) |
| world bit | the `!`/`?` marker on an arrow recording whether calls may touch the world (06-effects.md §5) |
| world-touching | a call that observes or changes the world; sets the world bit |
| open bit (`?`) | an arrow whose bit is instantiated from callback arguments at the call site |
| dispatcher | a function with an open bit whose own purity follows from its callbacks (e.g. `List.map`) |
| inverted binding (`<-`) | binding form that passes the rest of the block as a trailing callback (03-control.md §1.3) |
| annotation slot | the position after `:` in a binding, occupied by `type`, `ctype`, `test`, or an arrow annotation |
| single-variant type | an ADT with one constructor and payload; the newtype form (04-types.md §1.4) |
| shape adaptation | one-directional flow of a nominal value into a same-shape anonymous position (04-types.md §5.2) |
| three-way taxonomy | failures are `Result` (validation), `Option` (domain check), or `panic` (invariant violation) |
| derived service | compiler-provided per-type service: `show`, `inspect`, `==`, `<`, `with`, `to_json`, `from_json` (08-builtins.md §6) |
| acyclicity theorem | immutability + strict evaluation + capture-by-snapshot imply the heap graph is a DAG (07-runtime.md §3) |
| monomorphization | generics are expanded per instance at compile time; no runtime dictionaries (07-runtime.md §6) |
| host | the environment that starts an Eko program and constructs capabilities (06-effects.md §2.4) |
| extern declaration | a binding whose right-hand side names a C symbol; `!->` unless `Pure` (11-ffi.md §2) |
| `Pure` | the audited guarantee word: no world contact, reentrant and thread-safe (11-ffi.md §2.2) |
| layout record | a `ctype` declaration whose field types and order pin a C layout (11-ffi.md §3.3) |
| veneer | the embedding ABI: normative opaque function signatures, layouts excluded (11-ffi.md §7) |
| prelude | the only implicitly imported module; core types and free functions (05-names.md §11) |
