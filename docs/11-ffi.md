# Chapter 11 — FFI and Native Interop

Eko reaches native code through one mechanism: the `extern` declaration.
There is no second vocabulary. The standard library reaches the operating system
through the same lexicon (§8), the embedding host binds through the same
symbols (§7), and user bindings declare C functions through the same form
(§2). The vocabulary that lives in other chapters — machine words
(`02-syntax.md §5`), hexadecimal and binary literals
(`02-syntax.md §4.1`), the bitwise family (`08-builtins.md §4.6`),
`Bytes↔[]U8` zero copy (`08-builtins.md §3.2`), and fixed-width hex
(`09-library.md §2`) — is specified in its home chapters and
cross-referenced here.

## 1. Model

### 1.1 One lexicon

`extern` is the single channel to native code. No compiler intrinsic is
added for it: the standard library's operating-system surface is expressed with the
declarations of §2 and the primitives of §8, and the compiler-provided
services are the three of `05-names.md §11` — derived services, `panic`,
and the memory operations.

### 1.2 Authorization is linking

There is no FFI capability. The authorization manifest (`06-world.md §2`)
governs the **operating-system surface**: std operations ride
capability values, and the manifest is that surface's whole extent. The
**native track** is authorized where it is declared and linked: every
extern is a visible declaration in a module interface, statically greppable,
and every symbol resolves at link time. A host or sandbox that refuses
native code refuses extern declarations at compile time or reserved the
symbol set at link time; there is no runtime grant to subvert and none to
forget. `--sandbox` therefore restricts the capability grant *and* rejects
extern declarations.

The enforcement granularity of both tracks is the same — whole-surface —
and the difference is visibility: capabilities appear in `main`'s
parameter list; externs appear in module interfaces.

### 1.3 Honesty

An extern call is world-touching by default: its arrow is `!->`
(`06-world.md §5.1` rule 7). The one exception is spelled with the
`Pure` word (§2.2). A binding that is not declared `Pure` and cannot touch
the world still writes `!` — an over-declared bit is as culpable as a dead
annotation, and the honest fix is the guarantee word, not a shrug.

### 1.4 The audit basis

World soundness (`06-world.md §7`) is stated on two tracks:

- the **capability track** is structural, as sketched there;
- the **native track** is audited: a `Pure` declaration is a manually
  audited contract (§2.2), and a lying `Pure` belongs to the trusted
  computing base — the same class as a lying C shim (§3.6).

The theorem holds structurally modulo audited `Pure` declarations and
audited link inputs; this is the honest form of the guarantee.

## 2. extern declarations

### 2.1 Form

```
name: signature = extern "symbol"          // world-touching: the arrow is !->
name: signature = extern "symbol" Pure     // audited pure: the arrow is ->
```

```eko
_sqrt: (F64) -> F64 = extern "sqrt" Pure
_crc32: (crc: U32, buf: Bytes, len: C.Size) -> U32 = extern "crc32" Pure
_qsort: (base: C.Ptr.Void, n: C.Size, size: C.Size,
         compar: (C.Ptr.Void, C.Ptr.Void) -> Int) ! = extern "qsort"
```

- The symbol is a bare C symbol. Library binding belongs to the build
  layer, not the declaration (§6).
- A declaration may be public or `_`-private (`05-names.md §5`); the
  recommended binding shape is a private extern plus a hand-written
  wrapper (§2.4).
- `extern` is a declaration primitive, not a call: it takes no bracketed
  argument list, and its tail carries word attributes.
- **A callback's bit is its own.** In `_qsort` the comparator is pure
  computation, so `->` is the honest spelling; `!->` on a callback is for
  callbacks that themselves touch the world, and a pure function also adapts
  into a `!->` parameter (`06-world.md §5.1` rule 3).

### 2.2 The `Pure` word

Choosing `Pure` asserts two guarantees, and both are load-bearing:

1. **No world contact.** The target performs no I/O, raises no signals,
   reads no clock, draws no entropy. Program-internal state is not world
   (`06-world.md §8`): reading a C library's own thread-local or static
   state sets no bit.
2. **Reentrancy and thread safety.** The target may be called concurrently
   from two tasks. Internal static state that breaks this (the `strtok`
   shape) voids the word: the determinism argument of `06-world.md §8`
   depends on it, and that argument is not part of the bit system — it is
   the easily missed half of the guarantee.

`Pure` is declarable in any module. Interfaces disclose `Pure`
declarations automatically (`05-names.md §10`), so the audit surface is
visible downstream. The compiler does not read C headers and cannot verify
either guarantee; the word is the audit, and lying with it is a trusted
computing base failure.

### 2.3 Signature rules

- Every parameter and result type must be in the marshalling whitelist
  (§3.5); anything else is a compile error (`E-extern-type`).
- **No type variables.** An extern signature is monomorphic; C symbols
  have no template semantics, and generic wrapping is ordinary library
  code over the binding.
- **No variadics.** A signature is complete; `printf`-shaped functions
  have no declaration.

### 2.4 Binding conventions

The recommended shape is a two-layer module: private `_extern` declarations
below, an idiomatic public wrapper above.

```eko
StatResult: ctype = [ok: I32, size: I64, mode: U32, err: I32]
_stat_raw: (path: String) !-> StatResult = extern "_eko_stat"

stat_size: (fs: Fs, path: String) !-> Result Int String =
    if fs.exists(path).not():
        Err "no such file: {path}"
    else:
        r := _stat_raw(path)
        if r.ok == 0:
            Err "stat: errno {C.get_errno().to_int()}"
        else:
            Ok r.size.to_int()
```

- **Extern ferries; the wrapper decides.** Error codes, errno values, and
  NULL sentinels are data; mapping them into `Result` or `Option` and
  declaring an error ADT is library-layer work. No automatic error
  mapping exists.
- **A named capability must be used.** A wrapper whose signature names a
  capability must actually use it — precheck, scope a path
  (`Fs.root`), or attenuate — or drop the parameter: a decorative
  capability is a dead annotation (`06-world.md §5.1` rule 3 in
  spirit). Bindings with no operating-system surface (`libm`) name no
  capability. A used capability parameter restores manifest visibility
  for the operating-system track and is honest guidance for the native
  one.

## 3. The C vocabulary

### 3.1 Machine words

`I8 I16 I32 I64 U8 U16 U32 U64 F32 F64` are ABI lexicon. They **hold,
compare, show, and ferry**; they do not compute — no arithmetic and no
bitwise functions exist on them. Compute on domain words and convert;
the conversions are explicit at every boundary, and that price is
recorded, not apologized for.

- Services: `==` and `<` (same type), `show` (full numeric form),
  `to_hex_pad` (`09-library.md §2`). Machine words are data types with
  `==` and join dictionary key domains.
- Conversions: domain → machine integer returns `Option` (out of range
  is a domain check); domain → `F32`/`F64` is total with rounding,
  C-aligned; machine → domain is always total (widening). Domain arithmetic
  overflow panics (`02-syntax.md §6.2`) — one overflow rule in the
  language, zero exceptions.

### 3.2 The `C` namespace

`C` is the standard library namespace for machine-interface vocabulary that has no
eko-side twin:

| Member | Shape | Semantics |
|---|---|---|
| `C.Ptr.Void` | opaque | a pointer C handed you, handed back; no dereference, no arithmetic; typed wrapping is ordinary records (`SqLite: type = [raw: C.Ptr.Void]`) |
| `C.Size` | `size_t` ferry | compare and convert (`to_uint`, total); no arithmetic |
| `C.get_errno` | `() -> I32` | reads the C thread-local errno; program-internal state, hence pure (`06-world.md §8`); a standard-library `Pure` extern to a host symbol (§8) |
| `C.In T` | borrow | §3.4 |

### 3.3 `ctype` — layout records

`ctype` is a word in the annotation slot, beside `type` and `test`. It
declares a record whose memory layout is pinned to C.

```eko
Div: type  = [quot: Int, rem: Int]        // ordinary record: layout free (07-runtime.md §5)
DivC: ctype = [quot: I32, rem: I32]       // layout record: mirrors C's div_t exactly
```

- **The field predicate is checked at the declaration line**: every field
  is a machine word or another `ctype` record; anything else — `Int`,
  `String`, a list — is a compile error (`E-ctype-field`) at that line.
  The widths are then exactly what the C header says, field for field.
- Declaration order is C order; C alignment and padding apply.
- **The freeze is carried by the declaration itself.** A `ctype`'s
  representation is a contract; unboxing and reordering are off for it.
  This is the one explicit exception to representation freedom
  (`07-runtime.md §5`), and it is scoped to types that asked for it —
  ordinary records keep the full optimization surface.
- **Crossing is by value only** — parameters and results. Partial
  re-declaration is impossible: a `ctype` mirrors the complete C struct
  (`div_t`: yes; `struct stat`: no), and the rest goes through a shim
  (§3.6).
- Everything else about them is ordinary: they are immutable heap values,
  `==` and `show` are derived, `with` applies.

### 3.4 `C.In` — the audited read borrow

Where C takes a pointer to caller-built data and only reads it, the
parameter is spelled `C.In T` with `T` a `ctype` record:

```eko
_bind: (fd: I32, addr: C.In SockAddrIn, len: C.Size) !-> I32 = extern "bind"
```

- **Semantics**: copy-in, valid for the call, the value unchanged. The
  program has no address vocabulary (`07-runtime.md §9`); the compiler
  takes the address of its own temporary — the same shape a bare `String`
  parameter has (§3.5).
- **The audit**: choosing `C.In` asserts that C reads during the call,
  retains nothing, writes nothing. Same audit class as `Pure`
  (§2.2); a lying borrow is a trusted computing base failure.
- Out-parameters — C *writing* through the pointer — have no spelling at
  all: that is reference semantics, and it is not in the language
  (§3.6). A bare `String` or `Bytes` parameter is the same copy-in read
  borrow without the explicit marker.

### 3.5 The marshalling whitelist

| Eko type | C shape | Direction and copy |
|---|---|---|
| `Bool` | `int`-sized truth value | by value, both ways |
| `Int` / `UInt` / `Float` | `int64_t` / `uint64_t` / `double` | by value |
| machine words | the exact-width type | by value |
| `String` | `const char *` | parameter: UTF-8 copy, NUL-terminated, call-scoped; a returned `char *` is copied to a fresh `String`, and must be valid UTF-8 — a violation is a panic: the declaration asserted a string |
| `Bytes` | `const void *` | parameter: pointer to payload, call-scoped read |
| `ctype` record | the mirrored struct | by value, in and out |
| `C.In T` | pointer to `T` | copy-in read borrow (§3.4) |
| `C.Ptr.Void` | `void *` | opaque round-trip |
| `C.Size` | `size_t` | by value |
| closure | function pointer + context | synchronous callback (§5) |

A signature is checked against this table; a type outside it — `Char`, a
list, a dictionary, an ordinary record, a capability, a type variable, a
variadic tail — is rejected (`E-extern-type`).

### 3.6 Shims

A binding may carry C code — a shim — compiled and linked beside the
eko module. Shims share the extern symbol space: link-time name
resolution, one symbol vocabulary, no registration step. A shim is C:
its audit class is the C library's, no better. Out-parameters are shim
work — the shim calls the real function and returns a layout record by
value:

```c
StatResult _eko_stat(const char *path) {      /* the extern names this symbol */
    struct stat st; int r = stat(path, &st);
    return (StatResult){ .ok = r == 0, .size = st.st_size,
                         .mode = st.st_mode, .err = errno };
}
```

## 4. Failure at the boundary

| C behavior | Eko behavior |
|---|---|
| error code / `errno` / NULL sentinel | data — the extern returns it; `Result`/`Option` wrapping is library-layer; `C.get_errno` reads errno, pure |
| segfault / `SIGABRT` | process abort, the panic mechanism (`06-world.md §3.3`); not catchable |
| `longjmp` / C++ exception crossing | abort; taming — `setjmp` capture before the crossing — is shim work |
| panic inside a callback | abort propagates into C; C does not unwind |
| `EINTR` | the host may retry internally; user externs promise nothing |

The choice between `Result`, `Option`, and `panic` follows the taxonomy
(`08-builtins.md §4.1`): external failure with a reason is `Result` at
the wrapper; an invariant violation the wrapper itself asserts may panic.
The extern mechanism chooses nothing.

## 5. Callbacks

- **Synchronous only.** Where C takes a function pointer (and a context),
  an Eko closure is passed; the compiler generates a trampoline whose
  lifetime is the call. Callback parameter and result types follow the
  whitelist (§3.5), and the closure's arrow carries its world bit as
  usual — the bit lives on arrows and does not infect data
  (`06-world.md §5.1` rule 4).
- **No async registration.** There is no vocabulary that makes a closure
  outlive its call. This is an absence of vocabulary, not a semantic
  invariant: a callback handle would be new vocabulary and would break no
  promise made here.
- Reentrancy (C → eko → C) is free; the stack behaves, and self tail
  calls remain jumps on the eko side.

## 6. Linking and symbols

- The declaration names the symbol; the build layer names the libraries
  (CLI `-l` flags, host link configuration). Library names are
  platform-dependent, conditional compilation does not exist
  (`05-names.md §12`), so a library name has no place inside a
  declaration.
- Platform variance — symbol decoration, library sets — is host and
  library business.
- **The reserved prefix.** Symbols beginning `eko_` are the host's:
  `eko_retain`, `eko_release`, the veneer entries (§7), and the
  `eko_<cap>_wrap` / `eko_<cap>_unwrap` pairs (§8). An extern declaration
  naming a reserved symbol is a compile error (`E-reserved-symbol`) —
  the lexical rule "the word is the audit" cannot stop a link-time
  symbol, so the symbol space itself draws the line.
- Static versus dynamic linking is implementation domain.

## 7. The embedding ABI

### 7.1 The veneer

The embedding contract is a thin, opaque veneer: **function signatures
are normative, layouts are not.** `eko_value` is opaque throughout.

| Entry | Purpose |
|---|---|
| `eko_eval_file(path)` | compile and load a module |
| `eko_get_export(module, name)` | fetch an exported binding |
| `eko_call(fn, args, n)` | call with an argument vector |
| `eko_retain(v)` / `eko_release(v)` | hold and drop a value (§7.2) |
| `eko_string_new(cstr)` / `eko_as_string(v)` | string bridge |
| `eko_int_new` / `eko_as_int` / `eko_float_new` / `eko_as_float` / `eko_bool_new` / `eko_as_bool` / `eko_unit_new` | scalar bridge |
| `eko_closure_new(fn, ctx)` | wrap a C function as an eko closure value |
| `eko_cap_out(write, ctx)` … | per-capability bundle constructors (§8) |
| `eko_on_panic(observer)` | best-effort notification at abort |

A conformance host links against the veneer and nothing else. The fast
path — calling the compiler's generated C directly — exists, is faster,
and is a compiler detail, not contract (`06-world.md §2.4` keeps its
word for the fast path).

### 7.2 Holding values

A host that holds a value holds a reference: `eko_retain` on receipt,
`eko_release` on disposal. There is no handle table and no root
registration (`07-runtime.md §4`). Reclamation is deterministic and
promised to the host (`07-runtime.md §4.1`).

### 7.3 Threads

Any thread may call into Eko. The protocol is three sentences:

1. The language has no global mutable state — there is nothing to race on.
2. A value that crosses a boundary is shared, and shared means atomically
   counted (`07-runtime.md §4.2`); embedding boundaries and Conc captures
   are judged alike.
3. Thread safety of a capability's operations is the bundle author's
   guarantee — the same audit class as `Pure`.

Conc scheduling belongs to the host bundle: the language fixes the
semantics (`06-world.md §8`), the CLI default bundle is
compiler-emitted, and an embedding host supplies its own.

### 7.4 Application surfaces

An embedding host does not extend the manifest. It calls an exported
entry point directly and passes application surfaces as ordinary record
values — the closure-record idiom (`05-names.md §12`):

```eko
EditorServices: type = [paste: () !-> String, insert: (String) !]
init: (out: Out, svc: EditorServices) ! = …
```

The manifest stays a pure operating-system surface; the audit surface
moves to the entry signature, statically greppable. From the script's
side, writing for a host is ordinary application development — there is
no embedding vocabulary.

### 7.5 Panic

Panic aborts the process (`06-world.md §3.3`). The veneer observer is
best-effort notification, not recovery.

## 8. The standard library channel

The standard library is delivered as source (`05-names.md §11`) and reaches the
operating system through this chapter's own lexicon. A capability is
internally a bundle of world-touching closures: the host constructs real
bundles, and the standard library composes derived ones (`06-world.md §2.3`)
from two primitives per capability:

```eko
_OutB: type = [write: (String) !]                         // private bundle type
Out.unwrap: (Out) -> _OutB  = extern "eko_out_unwrap" Pure
Out.wrap:   (_OutB) -> Out   = extern "eko_out_wrap"   Pure

Out.println: (o: Out, s: String) ! = o.unwrap().write(s)  // one line per operation
Out.null: () -> Out = Out.wrap({write: (s): _})           // derivation is composition
Out.tee: (o: Out, obs: (String) !-> _) -> Out =           // constructs only — pure
    b := o.unwrap()
    Out.wrap({write: (s): b.write(s); obs(s)})
```

- **Bit accounting**: `println` calls a `!` closure and is `!`; `tee` and
  `null` only construct and are pure (`06-world.md §5.1` rule 1 — the
  `mk_logger` shape).
- **The pairs are protected by the reserved prefix** (§6), not by
  secrecy: `wrap` is the forging surface, and no user declaration can
  name it.
- **The standard library never calls libc directly.** Operating-system code lives
  in the host-constructed bundles; the standard library's extern surface is
  exactly the wrap/unwrap pairs — one lexicon, and no intrinsics.

## 9. Exclusions

- **No FFI capability.** Honesty rides the world bit, authorization rides
  the link layer (§1.2).
- **No address-of.** `&` for Eko values does not exist in any form; a
  borrow is `C.In` (a copy), and out-parameters are shim work (§3.6).
  `07-runtime.md §9` is untouched.
- **No reference semantics at the boundary.** Nothing an extern does can
  change an Eko value; results are returned.
- **No partial struct mirroring.** A `ctype` is the complete C struct, or
  the interface goes through a shim.
- **No bitwise operators — ever.** The functions are the vocabulary
  (`08-builtins.md §4.6`); `&` and `|` return to the or-pattern reserve
  (`02-syntax.md §7`).
- **No machine-word arithmetic.** Machine words hold and ferry; domain
  words compute (`§3.1`).
- **No variadic externs.** A signature is complete (§2.3).
- **No library names in declarations.** Platform-conditional linking is
  host business (§6).
- **No async callback registration**; the ban is an absence of vocabulary,
  not a semantic invariant (§5).
