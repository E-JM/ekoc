# Capabilities and I/O

Eko's I/O layer is built from four orthogonal mechanisms. Each answers exactly one
question, and none of them re-describes a phenomenon that another already covers.

| Axis | Mechanism | Question answered |
|---|---|---|
| Authority | capability values | who is allowed to do it |
| Honesty | the world bit (`!` / `?`) | whether this call touches the world |
| Failure | `Result` values | what to do when it fails |
| Resource | generation handles + `with_*` | what a misused resource does (checked error, never UB) |

## 1. Model

A capability value carries authorization granularity: it names a surface of the
world (`Out`, `Fs`, `Clock`, …) and is the only thing that grants access to that
surface. The world bit is that same fact projected onto arrows: it records whether
a call uses a capability, not which one. Consequently the bit never names an
authority, and the design needs no second vocabulary: capability names are the
only vocabulary there is.

This is the dividing line between Eko and label/row-based designs. Labels mirroring
capabilities (`console` ↔ `Out`, `fs` ↔ `Fs`) would be a permanent synchronization
burden for information the type of the receiver already carries. Label sets, row
algebra, and handlers are therefore rejected outright (§10).

Because the four axes are independent, each is stated and checked on its own terms:
the arrow says whether, the capability value says who, the `Result` says what happens
on failure, and the handle's generation says what happens after misuse. No axis is
expressed in the vocabulary of another.

```eko
// authority comes from the value; honesty comes from the arrow
main: (out: Out, fs: Fs) ! =
    case with_file(fs, "log.txt", Append, (f):
        case File.write(f, "started\n"):
            Ok _  -> _
            Err e -> Out.println(out, "write: {e}")):
        Ok _  -> Out.println(out, "logged")
        Err e -> Out.println(out, "open: {e}")
```

## 2. Capabilities

### 2.1 The capability set

Capabilities are built-in types. They are opaque, unforgeable, and copyable.

```
Out  In  Fs  Env  Args              // birth-granted (§2.2)
Clock  Rand  Proc  Conc            // opt-in (§2.2)
```

Native code needs no capability: the FFI track is declared in module
interfaces and authorized at the link layer (`11-ffi.md §1.2`).

- **Construction belongs to the host and to entry points only.** A program cannot
  fabricate a capability; it can only receive one, or derive one purely from a
  capability it already holds (§2.3).
- **No ambient I/O.** The global `print` / `println` / `input` / `File.*` bindings
  do not exist. All I/O flows down from `main`'s parameters.
- **`main`'s parameter list is the authorization manifest.** Every parameter is
  either a capability type or the list is empty; the host grants item by item.
  `main: ()` — a pure program — is always legal. No capability type may appear
  twice in the list: the grant is identified by type, so two parameters of the
  same capability type are a compile error.
- Capabilities are copyable, passable, and capturable by closures. Copying is
  delegation. There is no `==` on capabilities; `show` prints `<cap>`.
- `main` returns `_`, and there is no other return form. An `Int`-returning `main`
  would create an exit-code ambiguity for no gain — the codes `0`, `1`, and `2`
  are already taken by normal termination, panic, and compile-time failure — so
  the option does not exist. Under the CLI the exit code is `0` for normal
  termination and nonzero for a panic. A chosen exit code is
  `Proc.exit(proc, n)` — abrupt, `!-> Never` (`04-types.md §10`).

### 2.2 Birth-granted and opt-in

The governing axis: **birth-granted is the surface the host supplies at startup;
opt-in is the source a program asks the world for.**

| Capability | Surface | Phase | CLI default | World contact |
|---|---|---|---|---|
| `Out` | session write | initial | yes | sets bit: `print` / `println` |
| `In` | session read | initial | yes | sets bit: `read_line` |
| `Fs` | file system | initial | yes | sets bit: `open` / `read` / … |
| `Env` | environment variables | initial | yes | sets bit: `get` |
| `Args` | startup arguments | initial | yes | sets bit: `all` (§2.5) |
| `Clock` | time | initial | no, explicit | sets bit: `now` / `mono` / `sleep` |
| `Rand` | entropy | initial | no, explicit | sets bit: `int` |
| `Proc` | process control | initial | no, explicit | sets bit: `exit` |
| `Conc` | parallel resources | initial | no, explicit | **sets no bit at all** (§8) |

- **Birth-granted** (`Out`, `In`, `Fs`, `Env`, `Args`): stdio, the file system, the
  environment, the argument vector — the context a process carries from startup.
  The CLI default grant is exactly this layer. The default surface is not "the
  commonly used one"; it is "the one you were born with".
- **Opt-in** (everything else): observing or disturbing the world outside the
  startup configuration — clock and entropy (non-deterministic sources), process
  control (abrupt exit), parallelism (resources). Native code rides the
  link-authorized extern track instead (`11-ffi.md §1.2`). These are
  requested explicitly and are off by default.
- **A capability is necessary but not sufficient for world contact.** `Conc` sets no
  bit anywhere (§8: `scope` / `spawn` are `?` dispatchers, `join` / `cancel` are
  pure) — a resource capability authorizes "you may", it does not guarantee "this
  touches the world". Bit assignment is decided per builtin by §5.1 rule 2, never by
  capability name wholesale.

The CLI grants the five birth-granted capabilities `Out`, `In`, `Fs`, `Env`, `Args`
by default, so script ergonomics do not regress; the sandbox is off unless asked for,
and `--sandbox` restricts the grant.

Capabilities that are not in a program's parameter list cannot be reached at all —
the list is the whole **operating-system** surface, not a hint. The native track
is the second surface: it is declared in module interfaces and authorized at
the link layer (`11-ffi.md §1.2`).

### 2.3 Derived capabilities

Derivation is a **pure** operation: it reads and rewrites authorization metadata and
sets no bit, neither at construction nor at holding (§5.1 rule 1). Derived
capabilities are the entire mock surface of the language; no framework is involved
(`09-library.md §6`).

| Derivation | Signature | Behavior |
|---|---|---|
| `Out.null` | `() -> Out` | swallows every write |
| `Out.tee` | `(Out, (String) !-> _) -> Out` | writes once, reports once — assertions ride the stream |
| `In.empty` | `() -> In` | every read is EOF (`read_line` → `Ok None`) |
| `In.from_lines` | `([]String) -> In` | yields the given lines in order, then EOF |
| `Fs.null` | `() -> Fs` | every path → `Err NotFound` |
| `Fs.root` | `(Fs, String) -> Fs` | restricts to a subtree, no existence check (§4.5) |
| `Fs.read_only` | `(Fs) -> Fs` | withdraws write rights (§4.5) |
| `Env.frozen` | `([String]String) -> Env` | always reads the given table |
| `Clock.frozen` | `(Time) -> Clock` | fixed reading, `sleep` returns at once (`08-builtins.md §5`) |
| `Rand.with_seed` | `(Int) -> Rand` | seeded order; the sequence itself is an implementation detail |
| `Args.frozen` | `([]String) -> Args` | always reads the given argument vector |

- Every capability has at least one **deterministic** constructor, so no capability
  is a source of untestable nondeterminism. This closes the symmetry of the
  derivation surface and is what makes the embedding contract's third promise
  (§2.4) real rather than aspirational.
- `Out.tee`'s observer is triggered by a write builtin; the observer's bit is
  absorbed by that builtin's `!` in the same way as `with_file`'s callback (§5.1
  rule 5), so a pure observer flows in by adaptation.
- Derivation is not a way to observe the world. If a decision depends on world state,
  call a separate bit-setting builtin instead (for example `Fs.exists`).

### 2.4 The embedding contract

The language level promises exactly three things. The host-side native API is a
compiler detail and no part of this contract.

1. **`main`'s parameter type list is the authorization protocol.** The host supplies
   capabilities by position and type-checks each item; a missing or surplus item is a
   startup failure.
2. **Capabilities are opaque.** Inside the language one only ever sees `<cap>`;
   construction authority lies with the host alone.
3. **Tests need no host.** Derivations such as `Out.null` are implemented in the
   prelude layer; no host magic is required to obtain them.

### 2.5 Args

The argument vector is data of the host's world, so reading it observes the world.
The front door is a capability, not a magic parameter.

```
Args.all:    (Args) !-> []String       // the only door: the whole vector, read in one go
Args.frozen: ([]String) -> Args        // pure derivation: always reads fixed values
```

```eko
main: (out: Out, args: Args) ! =
    case args.all():
        {cmd, ..rest} -> dispatch(out, cmd, rest)
        {}            -> usage(out)
```

- **Why not `main: (argv: []String)`.** The parameter list is the authorization
  protocol (§2.4: item-by-item type checking, missing or surplus items fail startup).
  A data parameter would create a second kind of parameter and split the protocol in
  two. Worse, the pure `main: ()` would then be a program that has "seen argv" — the
  no-ambient principle has to hold without exception. `Args` uses the existing
  machine: declaring it is requesting it, the host supplies it by position, and an
  embedding host may synthesize its own `Args` freely.
- `all` yields the **argument vector, without the program name** (`argv[0]`): a tool
  wants arguments, while the program name belongs to the launcher's narrative and
  has no meaning inside an embedding host.
- **There is no attenuation combinator.** `Fs` attenuation is the continued grant of
  a channel (subtree, read-only); what `Args` yields is data, after which the channel
  is useless. Subcommand forwarding is the list pattern shown above; everything
  downstream is pure computation over `[]String`.
- Flag parsing (`--help` and friends) is a user-domain library over `[]String` and
  does not enter the language.

## 3. Failure

### 3.1 Result and panic

```
Result: type t e = Ok t | Err e
```

`Result` is built-in and sealed against extension, but it is **constructible** —
that is its only difference from `Option`: the currency of error handling must be
mintable.

- Recoverable failure is always `Result`. `panic` is reserved for invariant
  violations.
- **EOF is not an error.** `In.read_line: (In) !-> Result (Option String) IoError`,
  so the three cases `Ok Some` / `Ok None` / `Err` are matched exhaustively. End of
  input and failure stay distinguishable without a sentinel.
- `Result`'s combinators (`try`, `map`, `map_err`, `traverse`) are `?` dispatchers
  (§9); the `<-` inverted binding keeps unwrap chains flat (`03-control.md §6`).

### 3.2 IoError

```
IoError: type =
    | NotFound              // path does not exist
    | Permission            // permission denied
    | Exists                // exclusive creation hit an existing path (OpenMode.New)
    | NotFile               // a directory or other non-regular file
    | Closed                // handle already closed (generation check, §4)
    | InvalidData String    // invalid content (e.g. not UTF-8); payload is the detail
    | Other String          // anything else; payload is the system message
```

- `IoError` is a **closed** union, matched exhaustively. **Its variants are
  fixed**: adding one breaks every exhaustive `case` written against the union,
  and each variant earns its place by being handled in a distinct branch.
- Detail failures (a non-empty directory passed to `rm_dir`, a cross-device rename)
  travel through `Other String`; that is what the payload is for.
- The runtime retries `EINTR` internally, so `Interrupted` never surfaces.
- The scope is console, file system, and handles.

### 3.3 panic surface

- `panic(msg) -> Never` is **pure**: aborting is not world contact, so the call sets
  no bit and may appear in a `->` function.
- `Option.expect` and `Option.unwrap` are pure for the same reason.
- Division by zero and integer overflow panic without setting the bit.
- Indexing out of bounds is no longer a panic at all — indexing is a domain check
  (`05-names.md §4`).
- Panic output and exit code for the CLI host are normative
  (`09-library.md §7.6`).

## 4. File handles

### 4.1 OpenMode

```
OpenMode: type = Read | Write | Append | New | Update
//  Read    open an existing file for reading; absent → Err NotFound
//  Write   create or truncate
//  Append  create or append
//  New     create only if absent (exclusive); present → Err Exists
//  Update  read and write, no truncation (in-place rewrite)
```

The five variants cover the whole open surface; there is no flags integer and no
separate rights datum (§4.5).

### 4.2 Operations

```
Fs.open:          (Fs, String, OpenMode) !-> Result File IoError

File.read:        (File, Int) !-> Result String IoError   // sequential, at most n chars; non-UTF-8 → Err InvalidData
File.read_bytes:  (File, Int) !-> Result Bytes IoError    // sequential, at most n bytes; never InvalidData
File.read_line:   (File) !-> Result (Option String) IoError  // one line without its '\n'; EOF is not an error
File.seek:        (File, UInt) !-> Result _ IoError      // absolute position from the start; past EOF → Err InvalidData
File.write:       (File, String) !-> Result _ IoError     // write at the current position
File.write_bytes: (File, Bytes) !-> Result _ IoError      // bytes at the current position
File.flush:       (File) !-> Result _ IoError
File.close:       (File) !-> Result _ IoError             // stale handle → Err Closed
with_file:        (Fs, String, OpenMode, (File) !-> a) !-> Result a IoError
```

- The text gate and the byte gate are separate. `File.read` rejects invalid UTF-8
  with `Err InvalidData`; raw bytes go through `File.read_bytes`, which never reports
  `InvalidData` — any byte sequence is legal, and a short read simply means EOF.
- All of these set the bit: they observe or change the world.
- Convenience mirrors over whole paths (`Fs.read`, `Fs.write`, `Fs.append`,
  `Fs.read_bytes`, `Fs.append_bytes`, `Fs.exists`) and the directory surface live in
  `09-library.md §1`.

### 4.3 Generation checks

1. **Generation check.** `close` increments the slot's generation; every older copy
   of the handle becomes stale. Any operation on a stale handle — including a second
   `close` — yields `Err Closed`. Double-close and use-after-close are checked
   errors, not UB.
2. **Leaks are allowed.** Without `close`, the OS reclaims at process exit. No
   finalizer is promised (`07-runtime.md §4`).

Neither rule depends on the host's file descriptor reuse, because staleness is
decided by the language-level generation, not by the number the OS handed back.

The escape example shows what the generation buys — a handle that outlives its
`with_file` is inert and sayable:

```
sneaky: (fs: Fs) !-> Result File IoError =
    with_file(fs, "x", Read, (f): f)        // the returned handle is already closed
// any later File.read on it → Err Closed: checked, not UB
```

### 4.4 with_file

- `with_file` closes on **both** paths: the value path and the error path.
- A panic is an abort: the OS reclaims, and no unwinding is needed.
- The callback type is `(File) !-> a` and has **no open bit**. `with_file` is itself
  world-touching, and `OR` absorbs everything (§5.1 rule 5), so a pure callback flows
  in by adaptation and there is nothing to generalize.

```eko
// a fallible callback nests: with_file yields Result a IoError with a = Result _ IoError
append_line: (fs: Fs, line: String) !-> Result (Result _ IoError) IoError =
    with_file(fs, "log.txt", Append, (f): File.write(f, "{line}\n"))
```

The inner `Result` is the callback's; the outer covers opening and closing the
channel. To stay on a single error channel, either handle the callback's own failure
and end in `_` (as in §1), or flatten the two levels with a `<-` chain
(`03-control.md §6`).

### 4.5 Attenuation

There is **no `Rights` type. The combinators are the rights**: a rights datum that
nobody would ever match on is dead weight, whereas two pure combinators compose and
cover every attenuation scenario.

```
Fs.root:      (Fs, String) -> Fs    // pure: restricts to a subtree, does not check existence
Fs.read_only: (Fs) -> Fs            // pure: withdraws write rights
// read_only(Fs.root(fs, "/tmp"))
```

```eko
sandboxed: (fs: Fs) !-> Result []String IoError =
    Fs.list_dir(Fs.read_only(Fs.root(fs, "/tmp/work")), ".")
```

Because both combinators are pure, attenuation can happen anywhere, including above
a `->` boundary, and it never widens what was granted: derived capabilities only ever
narrow.

## 5. The world bit

### 5.1 Semantics

1. **The bit of a body is the OR of all call positions in that body; only calls
   contribute.** Holding sets nothing: referencing a world-touching function, storing
   a capability value, or passing a function value all set no bit.
2. **Builtin criterion, in one sentence: a builtin call that observes or changes the
   world sets the bit; a pure derivation does not.** `Out.println` and `Clock.now` set
   it; `Out.tee` and `Fs.root` are pure.
3. **Exact match.** At an annotated binding, the declared bit must equal the bit of
   the body — a missing `!` and a surplus `!` are both compile errors (a dead
   annotation is as culpable as dead code). The subtyping `(a) -> b <: (a) !-> b`
   applies only at **adaptation positions**: arguments, operands, expected type,
   collection elements. It never runs the other way.
4. **The bit lives on the arrow; it does not infect data.**
   `logged := List.map(xs, greeter)` requires an enclosing `!` for the call, but
   `logged : []String` is an ordinary value — there is no "world-touching list".
5. **`?` appears only on dispatchers that are "pure if the callback is pure"**
   (iterators such as `map` / `each` / `fold`). A higher-order function that is itself
   world-touching needs no `?`: `with_file` is already `!->`, `OR` absorbs
   everything, and the callback is written directly as `(File) !-> a`.
6. **All `?` in one signature are the same open bit.** `OR` absorbs on the bit axis,
   so several bit variables would behave identically; `?` is therefore anonymous, and
   since signatures contain no bit variables, names have no disambiguation work to do.
7. **`extern` sets the bit.** An extern declaration's arrow is `!->` unless it
   carries the `Pure` word — an audited two-part contract: no world contact,
   reentrant and thread-safe (`11-ffi.md §2.2`).

### 5.2 Arrow syntax

```
(a) -> b                    // pure: guaranteed not to touch the world (may panic or diverge, §7)
(a) !-> b                   // definitively world-touching: may touch the world
(a) !                       // elided-arrow form of a unit return (≡ (a) !-> _)
(a) ?-> b                   // open bit: pure if the callback is pure, world-touching otherwise
(a) ?                       // elided-arrow form of an open unit return (≡ (a) ?-> _)
([]t, (t) ?-> u) ?-> []u    // dispatcher: every ? in the signature is the same open bit
```

Read it in one line: **`?` is an anonymous parameter on the bit axis, running on the
same machine as the type variable `t`** (§6) — the open bit is fed by a parameter and
instantiated at the call site from the arguments. The three arrows are then "two bits
plus one parameter": `->` and `!->` are the two bits, and `?->` is their transparent
form.

`!` / `?` are written **joined to their arrow as a single token** `!->` / `?->`,
attached to that arrow, with no binding rules. Nesting needs no parentheses in either
direction:

```
(Out) -> (String) !-> String      // inner world-touching, outer pure
(Out) !-> (String) -> String      // outer world-touching, inner pure
(Out) !-> (String) !-> String     // both world-touching
```

- **The tokens are self-delimiting, so no layout rule is needed.** `!->` / `?->` are
  lexed by longest match; `!=` remains the expression comparison operator, and there
  is no ambiguity in type position. A lone `!` or `?` occurs only in the
  elided-arrow forms (`(a) !` ≡ `(a) !-> _`, `(a) ?` ≡ `(a) ?-> _`).
- **Grammar:** `ParenGroup ('!' | '?')? ('->' Type)?`. A marker may appear only after
  a parameter group that could be followed by an arrow. The layout algorithm changes
  in no way: joined tokens never wrap, and a lone marker occurs only in the
  elided-arrow form, that is, at end of line (`02-syntax.md §1`).
- **The bit is fully erased**, with zero runtime cost. It is part of type equality and
  of nothing else; it has no calling-convention consequence.
- **The open bit is anonymous; there are no bit variables.** All `?` in a signature
  are one open bit (rule 6). The open bit is generalized at the annotation,
  instantiated at the call site from the argument bits, and collected when lambda
  arguments are checked last.
- **The open bit must be fed by a parameter.** A signature's own arrow may be `?->`
  only if at least one parameter type contains a `?` arrow — the same shape as the
  result-only prohibition on type variables, decided after alias expansion. A `?` in
  the result or in the body with no parameter to feed it is a compile error.
- **`?` is allowed in exactly two kinds of position:** the annotated binding's own
  arrow, and arrows inside that signature's parameter types. Return types (including
  types nested in a return type), container elements, and record fields are all
  forbidden — no binding is generalized there, so an open bit could never be
  instantiated and would be false precision. Every other position has only `->` and
  `!->`.

### 5.3 Unit and `_`

The unit type and the unit value are both spelled `_`.

- **Type position:** `_` is Unit (`Result _ IoError`, `Option _`). `()` then has
  exactly one meaning left — a zero-parameter function type (abbreviation of
  `() -> _`).
- **Value position:** `_` is the unit value: `Err e -> _`, and `Ok _` constructs a
  `Result _ IoError`. `()` has no value-position meaning; it appears only in call
  syntax `f()`.
- **Pattern position:** `_` is the wildcard. Unit has exactly one member, so the
  wildcard and the unit literal pattern have the same extension — `Ok _` behaves
  identically under either reading, and no collision exists.
- **Display** is `_` as well: what can be written is what is displayed; display does
  not get a second spelling.

`_` in type position **only ever** means Unit; it never means "please infer". Eko
annotations have no inference holes, and any document introducing `_` must say so
plainly to prevent misreadings imported from Rust, TypeScript, or Scala.

A lambda parameter may be `_`: parameter position is a binding position, and
`_` binds nothing there — the wildcard (`03-control.md §5.6`). `(_, i)` discards
the first parameter of a group, and `<-` inherits the form (`_ <- f().try()` is
the chain form of a `Result _ e` link). The discard binding form `_ := e` is
unchanged and is distinguished from the value `_` by the `:=` syntax.

At type position `()` is the zero-parameter function type, so a unit return cannot
be spelled `-> ()`. `-> _` supplies the explicit form, and the elided form's sugar
round-trips:

```
foo: (a: Int) -> _  ≡  foo: (a: Int) = ...
(a) !-> _           // explicit form of a world-touching unit return
```

### 5.4 Terminology

The word "effect" is banned from all documentation and all diagnostics.

| Term | Spelling | Meaning |
|---|---|---|
| unit-returning function type | `(t)` | a parameter group with the arrow elided: the result is `_` |
| world marker | `!` | the arrow definitively touches the world |
| open marker | `?` | the bit is a parameter, instantiated from callbacks |
| world bit | bit | the honesty axis as a whole |
| world-touching arrow / open arrow | `!->` / `?->` | the two non-pure arrows |

Use "world bit", "sets the world bit", "open bit", "bit annotation", "world-touching",
and "touches the world" consistently. There is no "effect system", no "effect row",
and no "effect handler" in this language.

### 5.5 Open-bit discipline

There is only one legal shape: **a parameter feeds the bit, and the body passes it
through.** The teaching line is: writing `->` and `!->` needs no justification;
writing `?` does — the callback's bit must actually be passed out. In the prelude `?`
belongs to dispatchers only (§9); in user code it should be a rare word.

```eko
apply_twice: (f: (Int) ?-> Int, n: Int) ?-> Int = f(f(n))
// correct: the body's bit is open (two calls to f, OR is idempotent) = the declared bit

inc: (n: Int) -> Int = n + 1
a := apply_twice(inc, 5)    // pure argument ⟹ pure instantiation, no enclosing ! required
b := apply_twice((n): Out.println(out, show(n)); n + 1, 5)
                            // world-touching argument ⟹ ! instantiation, enclosing ! required
```

All four mismatch modes below are errors; there is no "mark it `?` to be safe".

```eko
count: (xs: []Int) ?-> Int = List.length(xs)
// wrong: no parameter contains ?, so nothing feeds the open bit; the body's bit is
// pure ≠ open. Fix: (xs: []Int) -> Int
```

```eko
loud_apply: (out: Out, f: (Int) ?-> Int, n: Int) ?-> Int =
    Out.println(out, "call"); f(n)
// wrong: body bit = open ∨ ! = ! (! absorbs under OR).
// Fix: (out: Out, f: (Int) !-> Int, n: Int) !-> (rule 5; a pure callback arrives by adaptation)
```

```eko
print_all: (out: Out, xs: []String) -> _ =
    List.each(xs, (s): Out.println(out, s))
// wrong: each is instantiated at !, so the body bit is ! ≠ pure; and touching the world
// requires holding a capability value (§2).
// Fix: print_all: (out: Out, xs: []String) ! = ...
```

```eko
twice_fast: (f: (Int) ?-> Int, n: Int) -> Int = f(f(n))
// wrong: body bit = open ≠ pure. Fix: (f: (Int) -> Int, n: Int) -> Int
```

The result-only prohibition and alias laundering are the same rule, judged after
expansion:

```eko
double: (xs: []Int) ?-> []Int = List.map(xs, (n): n * 2)
// wrong: no parameter contains ?; map is instantiated pure, so the body bit is pure.
// Fix: (xs: []Int) -> []Int

Cb: type = (Int) ?-> _
c: Cb = (n): n + 1                      // wrong: expanded it is the binding's own ? arrow, nothing feeds it
run: (f: Cb, n: Int) ?-> Int = f(n)     // correct: Cb sits in a parameter position and can be fed
```

Positions follow §5.2's second rule:

```eko
handlers: [](Int) ?-> Bool = {isZero, isNegative}
// wrong: ? sits in a container element position; no binding is generalized here.
// All-pure → [](Int) -> Bool; mixed → [](Int) !-> Bool (pure elements arrive by adaptation)

make: (seed: Int) !-> (Int) ?-> Int
// wrong: ? is inside the return type. A returned function's bit is fixed at creation;
// what should be open is the caller's binding, not the result
```

By contrast, holding sets no bit and the `?` discipline does not infect anything:

```eko
mk_logger: (out: Out) -> (Int) !-> Int =
    (n): Out.println(out, show(n)); n + 1
// correct: it only constructs and never calls, so the factory is pure (rule 1);
// a pure function returning a ! closure is legal everywhere
```

## 6. Integration with generics

Open bits and type variables are one machine (`04-types.md §7`, `§8`). Everything
about generics holds unchanged, variable spelling included (type variables are
lowercase). Three increments apply:

1. **Position kinds.** `?` follows the positional rules of §5.2: the binding's own
   arrow and arrows in its parameter types, nowhere else.
2. **Same generalization and instantiation.** `?` is generalized at the annotation
   and instantiated at the call site, exactly as a type variable is, except that the
   instantiation is anonymous — the argument's bit decides it.

```eko
List.map: ([]t, (t) ?-> u) ?-> []u

List.map(xs, inc)                  // ? instantiated pure ⟹ the call is pure
List.map(xs, (n): Out.println(out, show(n)); n)   // ? instantiated ! ⟹ enclosing ! required
```

3. **The phantom check sees through `?` positions.** `Callback: type t = (t) ?-> _`
   is legal: `t` occurs in a `?`-position, which counts as a use, and the open bit
   appears through the `?->` on the right-hand side.

## 7. Soundness and non-promises

> **World soundness.** If `f : (a) -> b` (no `!`) in safe Eko, then calling `f` does
> not touch the world — structurally on the capability track, modulo audited
> `Pure` declarations on the native track (`11-ffi.md §1.4`).

Sketch:

1. Setting the bit originates only in a builtin call that requires a capability
   value, or in an extern call — the latter is visible as a declaration, resolved
   at the link layer, and `Pure` itself is an audited exception.
2. Capabilities cannot be forged: they are constructed only by `main`'s parameters,
   by the host, or purely derived from capabilities already held.
3. A closure's bit is on its arrow type, and its body is checked against that
   annotation (§5.1 rule 3).
4. Subtyping is one-directional: `(a) -> b` may flow into `(a) !-> b`, never the
   reverse (§5.1 rule 3).

**Corollary (free top-level purity).** Top-level initialization is pure by
construction: top level holds no capability values, so a bit-setting call is
structurally impossible there.

**Non-promises.** These must be stated alongside the theorem. The absence of `!` does
*not* mean:

- no panic — `panic`, `expect`, `unwrap`, division by zero, and overflow remain
  possible (§3.3);
- termination — a `->` function may diverge;
- no blocking — a pure function may block for reasons outside the world;
- immunity to cancellation — a pure task can be cancelled (§8).

The bit governs world contact only. Partiality is an independent dimension, and it is
deliberately not tracked.

## 8. Concurrency

```
Conc.scope:  (Conc, (Scope) ?-> a) ?-> a          // pure callback ⟹ the scope call is pure
Conc.spawn:  (Scope, () ?-> b) ?-> Task b         // a child's world bit joins the open bit
Conc.join:   (Task b) -> Option b                 // pure: None = cancelled
Conc.cancel: (Task b) -> _                        // pure: program-internal state
```

**Why `scope` has an open bit instead of a fixed `!->`.** Values are immutable and
the initial design has no communication between tasks, so a task's result is already
determined at the moment of `spawn`, and scheduling order cannot change any result. Cancellation has
explicit sources only (a parent's `cancel`, or scope exit), so program order
determines everything. Pure tasks therefore have no data races and are deterministic,
which makes `scope` a `?` dispatcher on a par with `List.map` — not a special case.

`join` and `cancel` observe program-internal state and do not touch the world; they are
pure, the mechanical consequence of "only observing the world sets the bit". Since
cancellation is not an error, `join` returns `Option b` with `None` meaning cancelled.

```eko
greet_all: (conc: Conc, out: Out) ! =
    conc.scope((sc):
        t = sc.spawn((): Out.println(out, "child is running"))
        Out.println(out, "parent is running")
        case t.join():
            Some _ -> Out.println(out, "child finished")
            None   -> Out.println(out, "child cancelled"))
```

`scope` is instantiated at `!` here because the spawned callback touches the world;
had the callbacks been pure, the whole expression would have been pure.

**Boundaries.** Cancellation is applied at world-call boundaries.
A pure task that never terminates hangs scope exit, consistent with the rule
that divergence is not tracked (§7). There are no channels, hence no deadlock
in the language. Channels and timeouts are excluded: communication introduces
observable scheduling and would break the determinism argument.

## 9. Prelude bit assignments

| Category | Signatures |
|---|---|
| sets `!` | `Out.print/println`, `In.read_line`, `Fs.read/write/append/read_bytes/append_bytes/exists/open/list_dir/mkdir/mkdir_all/rm/rm_dir/rename/stat/realpath`, `File.read/read_bytes/read_line/seek/write/write_bytes/flush/close`, `with_file`, `Env.get`, `Args.all`, `Clock.now/mono/sleep`, `Rand.int`, `Proc.exit` |
| derived (pure) | `Out.tee`, `Out.null`, `In.empty`, `In.from_lines`, `Fs.null`, `Fs.root`, `Fs.read_only`, `Env.frozen`, `Clock.frozen`, `Rand.with_seed`, `Args.frozen`, `show: (t) -> String`, `to_json/from_json` |
| pure, surprisingly | `panic(msg) -> Never` — aborting is not world contact |
| `?` dispatchers | every builtin that takes a callback: `List.map/filter/fold/each/any/all/find_by/sort_by/zip_with`, `Dict.map/filter/fold/each`, `Range.each/each_with_index/map/fold`, `Option.map/and_then/or_else/filter`, `Result.try/map/map_err/traverse` (the combinators behind `<-` inverted binding, `03-control.md §6`) |
| pure | `List.length/get/reverse/append/…`, `String.append/chars/from_chars/at/length/from_char`, `Char.to_code/from_code`, `Bytes.*`, `Int.*`, `Float.*`, the conversion family (`to_int/to_uint/to_float/parse`), `Option.unwrap/unwrap_or/expect/get/to_result`, `Dict.insert/remove/get/merge`, `Range.to_list/length` |

The contrast: `Option.expect` is pure while `Option.map` is a `?`
dispatcher — panicking sets no bit, callbacks do.

`show` and the JSON pair are pure: they derive text from a value without touching the
world. For the full surface of each namespace see `05-names.md §11` and
`09-library.md §1`.

## 10. Exclusions

The following are normative exclusions, not preferences.

- **No label sets.** A label set mirrors capabilities one-to-one (`console` ↔ `Out`,
  `fs` ↔ `Fs`) and would be a permanent synchronization burden describing a
  distinction the capability value already carries.
- **Nothing may follow `!` or `?`.** No labels, no bit variables, no annotations of
  any kind. Reserve syntax saves no cost — the cost lives in semantics and
  migration, not in the notation — and the mirroring of labels onto capabilities is a
  structural fact that every layer (concurrency, FFI, auditing, testing, optimization)
  answers better elsewhere. If a mechanism of this kind is ever needed, it gets its own
  syntax and does not parasitize this marker. **Nothing ever follows `!` or `?`.**
- **No row polymorphism, row algebra, or row extension.** The bit makes them
  unnecessary: there is one bit per arrow and nothing to extend.
- **No algebraic effects and no handlers.** World I/O cannot be "handled away";
  native continuations are expensive; handler support would be a different feature
  with a different semantics.
- **No IO monad.** It would lose the direct style and force every higher-order
  function to become monadic.
- **No exceptions and no panic in the bit.** Errors are values (`Result`); partiality
  is an independent dimension (§7).
- **No user-defined labels.** They are the slippery slope to algebraic effects.
- **No `Rights` type.** Attenuation is the pure combinators of §4.5.
- **Bit diagnostics are always errors, never warnings.** A world-bit mismatch is a
  compile error (missing and surplus `!` alike). General diagnostics may be warnings;
  the bit domain is exempt.
- **No ambient I/O.** Authorization flows only from `main`.
- **No FFI capability.** Native code is authorized at the link layer and marked
  by the world bit (`11-ffi.md §1.2`, `§9`).
- **The term "effect" is not used** — in documentation, in diagnostics, or in
  identifiers of the specification (§5.4).
