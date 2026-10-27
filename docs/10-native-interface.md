# 10. Native Interface

This clause specifies the native interface: the `extern` declaration, the C
vocabulary admitted in extern signatures, the rules for linking and symbols,
the embedding ABI, and the channel through which the capability module reaches
the operating system. It also specifies the conditions under which a program
leaves the language guarantee (Clause 2.6). The world bit is specified in
Clause 7.9, value representation and memory management in Clauses 8.5 and 8.6,
and standard-library delivery in Clause 9.1; this clause does not restate those
rules.

## 10.1 Model

### 10.1.1 One Lexicon

`extern` is the single channel from Eko to native code. No compiler intrinsic
exists for a native call, and no second declaration form is added for the
standard library or for an embedding host. The injected capability primitives
of Clause 10.8 are compiler-provided and are not a program-reachable channel.

A program reaches native code through the declarations of Clause 10.2. The
capability module reaches the operating system through the injected primitives
of Clause 10.8. The compiler-provided services are exactly the derived services
(Clause 9.8), `panic` (Clause 8.4.2), and the memory operations (Clause 8.6);
none of them is a native-call mechanism.

Native-adjacent vocabulary that has a home elsewhere is specified there and is
not redefined here: hexadecimal and binary literals (Clause 5.7.1), the bitwise
family (Clause 9.6.5), zero-copy conversion between `Bytes` and `[]U8`
(Clause 9.5), and fixed-width hexadecimal output (Clause 9.13.1). Machine words
are specified in Clause 10.3.1.

### 10.1.2 Authorization

No FFI capability exists. Native authority is not represented by a value and is
not granted at run time.

The authorization manifest specified in Clause 7.9.1 governs the
operating-system surface: standard-library operations ride capability values,
and the manifest is the whole extent of that surface. The native track is
authorized where it is declared and linked:

- every extern declaration is a top-level declaration in a module and is
  statically discoverable;
- every native symbol resolves at link time (Clause 10.6).

A host or sandbox that refuses native code MUST refuse extern declarations at
compile time or MUST reserve the symbol set at link time. It MUST NOT rely on a
run-time grant. A host that provides the `--sandbox` mode MUST restrict the
capability grant and MUST reject extern declarations.

The enforcement granularity of both tracks is the whole surface. The difference
is visibility: capabilities appear in the parameter list of `main`
(Clause 7.1.3), and extern declarations appear in module source.

### 10.1.3 Honesty

The world bit is specified in Clause 7.9; an extern declaration obeys the
following rule.

An extern call is world-touching by default. Its arrow is `!->`, equivalently
`!` when the result is `_` (Clause 7.9.3). The single exception is the `Pure`
word (Clause 10.2.2), which changes the arrow to `->`.

A declaration that does not carry `Pure` MUST use a world-touching arrow,
whether or not its target performs world contact. An extern whose target
performs no world contact and whose declaration omits `Pure` carries an
over-declared bit; the correction is the `Pure` word. The compiler cannot
inspect the target, so the declaration is the only statement of the bit.

### 10.1.4 Trusted Base

World soundness (Clause 7.9.7) is stated on two tracks. The capability track is
structural. The native track is audited.

A `Pure` declaration is a manually audited contract. A false `Pure` declaration
belongs to the trusted computing base, as does a C shim whose code has undefined
behavior (Clause 10.3.6). The guarantee of Clause 7.9.7 holds modulo audited
`Pure` declarations and audited link inputs.

The compiler does not read C headers and cannot verify either part of the
`Pure` contract (Clause 10.2.2).

## 10.2 extern Declarations

### 10.2.1 Form

An extern declaration is a binding in the form specified in Clause 6.3.8:

```text
name: signature = extern "symbol"
name: signature = extern "symbol" Pure
```

The first form requires a world-touching arrow in `signature`; the second form
requires a pure arrow. `extern` and `Pure` are contextual words
(Clause 5.6.2).

The symbol is a bare C symbol. It does not name a library; library selection
belongs to the build layer (Clause 10.6). An extern declaration is a declaration
primitive, not a call: it takes no bracketed argument list, and its tail carries
word attributes.

A declaration MAY be public or private (Clause 7.3.1). The recommended binding
shape is a private extern declaration below a public wrapper (Clause 10.2.4).

A callback parameter carries its own bit. In `_qsort` below, the comparator is
pure computation and is written `->`; a callback that touches the world is
written `!->`. A pure function value adapts into a `!->` parameter position
(Clause 7.9.5).

```eko
_sqrt: (F64) -> F64 = extern "sqrt" Pure
_crc32: (crc: U32, buf: Bytes, len: C.Size) -> U32 = extern "crc32" Pure
_qsort: (base: C.Ptr.Void, n: C.Size, size: C.Size,
         compar: (C.Ptr.Void, C.Ptr.Void) -> Int) ! = extern "qsort"
```

The `_qsort` declaration is world-touching even though its comparator is pure:
the declaration omits `Pure`, so its arrow is `!`.

### 10.2.2 Pure

`Pure` asserts two guarantees:

1. **No world contact.** The target performs no input or output, raises no
   signal, reads no clock, and draws no entropy. Program-internal state is not
   world (Clause 4.5); reading a C library's own thread-local or static state
   sets no bit.
2. **Reentrancy and thread safety.** The target MUST be safe for concurrent
   calls from two tasks. Internal static state that violates this requirement,
   such as the state used by `strtok`, voids the word. The determinism
   requirement of Clause 8.9.3 depends on this guarantee; the guarantee is not
   part of the bit system.

`Pure` MAY be declared in any module. Interfaces disclose `Pure` declarations
(Clause 7.3.2), so the audit surface is visible to downstream modules.

The compiler does not read C headers and cannot verify either guarantee. The
word is the audit. A `Pure` declaration whose target violates either guarantee
is outside the language guarantee (Clause 2.6) and is a trusted-computing-base
failure (Clause 10.1.4).

### 10.2.3 Signature Rules

An extern signature is subject to the following rules.

1. Every parameter type and every result type MUST be permitted by the
   marshalling whitelist (Clause 10.3.5), in the direction recorded there. A
   type outside the whitelist is a compile error with code `E-extern-type`.
2. The signature MUST be monomorphic. Type variables MUST NOT appear in an
   extern signature. A C symbol has no template semantics; a generic wrapper is
   ordinary library code over a monomorphic binding.
3. The signature MUST be complete. Variadic parameters MUST NOT appear. A
   `printf`-shaped function has no extern declaration.
4. The declaration's own arrow MUST be `!->` or `!`, or `->` when the
   declaration carries `Pure`. An open arrow MUST NOT appear in an extern
   signature — neither as the declaration's own arrow nor inside a
   parameter type.

A violation of rule 1, rule 2, or rule 3 is reported as `E-extern-type`. A
violation of rule 4 is reported as `E-extern-arrow` (Annex B).

The compiler checks the declaration, not the native symbol. A declaration whose
signature does not match the native symbol is outside the language guarantee
(Clause 2.6).

```eko
_sum: (xs: []I64) !-> I64 = extern "sum64"
// error: E-extern-type: a list is not in the marshalling whitelist
```

```eko
_id: (x: t) !-> t = extern "id_any"
// error: E-extern-type: an extern signature is monomorphic
```

### 10.2.4 Binding Conventions

The recommended shape for a native binding is a two-layer module: private extern
declarations below, an idiomatic public wrapper above. The wrapper presents the
language's failure and capability vocabulary; the extern presents the C surface.

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

The extern ferries; the wrapper decides. Error codes, `errno` values, and NULL
sentinels are data. Mapping them into `Result` or `Option`, and declaring an
error ADT, is library-layer work. No automatic error mapping exists.

A wrapper whose signature names a capability MUST use that capability, by
prechecking, by scoping a path (for example with `Fs.root`, Clause 9.12.5), or
by attenuation. If the capability is not used, it MUST be removed from the
signature. An unused capability is a dead annotation (Clause 7.9.2).

A binding with no operating-system surface, such as a `libm` binding, names no
capability. A used capability parameter restores manifest visibility for the
operating-system track; for the native track it records the intended authority.

## 10.3 C Vocabulary

### 10.3.1 Machine Words

The machine words are `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`,
`F32`, and `F64`. They are the ABI lexicon.

A machine word holds, compares, shows, and ferries a value. It does not compute:
no arithmetic function and no bitwise function is defined on a machine word.
Computation is performed on the domain words `Int`, `UInt`, and `Float`;
conversion is explicit at every boundary.

The services of a machine word are:

- `==`, `!=`, `<`, `<=`, `>`, and `>=` between two values of the same machine-word type;
- `show`, in full numeric form;
- `to_hex_pad` (Clause 9.13.1).

Machine words are data types. They have structural equality and belong to the
dictionary key domains (Clause 9.10.4). Their representation is scalar
(Clause 8.5.1).

Conversions have the following forms:

- from a domain word to a machine integer: `Option`; an out-of-range value is a
  domain check; the conversion names are specified in Clause 9.6.2;
- from a domain word to `F32` or `F64`: total, with rounding aligned to C;
- from a machine word to the corresponding domain word: total, by widening.

Domain arithmetic overflow panics (Clause 7.8.4). The machine words have no
overflow rule because they have no arithmetic.

### 10.3.2 The C Namespace

`C` is the standard-library namespace for machine-interface vocabulary that has
no Eko-side counterpart. Its members are specified in the following table.

| Member | Shape | Semantics |
|---|---|---|
| `C.Ptr.Void` | opaque pointer | no dereference and no arithmetic; typed wrapping is an ordinary record, as in `SqLite: type = [raw: C.Ptr.Void]` |
| `C.Size` | `size_t` ferry | compare and convert to `UInt` (`to_uint`, total); no arithmetic |
| `C.get_errno` | `() -> I32` | reads the C thread-local `errno`, which is program-internal state and therefore pure (Clause 4.5); a standard-library `Pure` extern to a host symbol |
| `C.In T` | read borrow | Clause 10.3.4 |

`C.Ptr.Void` is not a general pointer type. A program cannot dereference it,
index it, add to it, or convert it to an integer. It is an opaque value that
crosses the boundary unchanged. A typed wrapper is an ordinary nominal record
whose only field is the `C.Ptr.Void`.

`C.Size` is a distinct type. It compares with `==` and `<` and converts to
`UInt`; it has no arithmetic. A value of type `C.Size` is permitted only in the
positions recorded in Clause 10.3.5.

`C.get_errno` is pure: the C `errno` is thread-local program state, not the
world. The standard library declares it as a `Pure` extern to a host symbol.

### 10.3.3 ctype

`ctype` is a word in the annotation slot, beside `type` and `test`
(Clause 6.3.6). It declares a record whose memory layout is pinned to a C
struct.

```eko
Div: type  = [quot: Int, rem: Int]
DivC: ctype = [quot: I32, rem: I32]
```

`Div` is an ordinary record and its layout is free (Clause 8.5). `DivC` mirrors
C's `div_t` exactly.

The field predicate is checked at the declaration line:

- every field type MUST be a machine word (Clause 10.3.1) or another `ctype`
  record;
- a field of any other type, including `Int`, `String`, a list, or a
  capability, is a compile error with code `E-ctype-field` at that line.

```eko
Stat: ctype = [size: I64, label: String]
// error: E-ctype-field: `String` is not a machine word or a ctype
```

The widths are exactly the widths of the C header, field for field. Declaration
order is C order. C alignment and padding apply.

The layout freeze is carried by the declaration. A `ctype`'s representation is a
contract; unboxing and field reordering are not permitted for it. This is the
one explicit exception to the representation freedom specified in Clause 8.5;
ordinary records retain that freedom.

A `ctype` is a nominal type (Clause 7.5.2). A bare `ctype` crosses the boundary
by value only, as a parameter or a result. Partial re-declaration is not
permitted: a `ctype` mirrors the complete C struct. A complete mirror is
possible for `div_t`; it is not possible for `struct stat`, whose interface goes
through a shim (Clause 10.3.6).

Apart from the layout freeze and the field predicate, a `ctype` is an ordinary
value type: it is immutable, it is a heap value (Clause 8.5.2), `==` and `show`
are derived (Clause 9.8), and `with` applies (Clause 9.8.6). The exact bytes
beyond the declared fields are implementation-defined (Clause 2.5).

### 10.3.4 C.In

Where C takes a pointer to caller-built data and only reads it during the call,
the parameter type is `C.In T`, with `T` a `ctype` record.

```eko
SockAddrIn: ctype = [family: U16, port: U16, addr: U32]

_bind: (fd: I32, addr: C.In SockAddrIn, len: C.Size) !-> I32 = extern "bind"
```

The value is copied in, is valid for the duration of the call, and is unchanged
by the call. A program has no address vocabulary (Clause 8.11). The compiler
takes the address of its own temporary; this is the same copy-in shape that a
bare `String` parameter has (Clause 10.3.5).

Choosing `C.In` asserts that the target reads during the call, retains no
pointer, and writes nothing through the pointer. This is the same audit class as
`Pure` (Clause 10.2.2). A false `C.In` declaration is outside the language
guarantee (Clause 2.6) and is a trusted-computing-base failure
(Clause 10.1.4).

An out-parameter, through which C writes, has no spelling. Such an interface is
reference semantics and is not part of the language; it is implemented by a shim
(Clause 10.3.6). A bare `String` or `Bytes` parameter is the same copy-in read
borrow without the explicit marker.

### 10.3.5 Marshalling Whitelist

An extern signature is checked against the following table. A type is permitted
only in the directions recorded for it.

| Eko type | C shape | Direction and copy |
|---|---|---|
| `Bool` | `int`-sized truth value | by value, both directions |
| `Int`, `UInt`, `Float` | `int64_t`, `uint64_t`, `double` | by value, both directions |
| machine words | the exact-width C type | by value, both directions |
| `String` | `const char *` | parameter: UTF-8 copy, NUL-terminated, valid for the call; result: copied into a fresh `String`, which MUST be valid UTF-8, else panic (Clause 8.4.2) |
| `Bytes` | `const void *` | parameter: pointer to the payload, read-only for the call |
| `ctype` record | the mirrored C struct | by value, both directions |
| `C.In T` | pointer to `T` | parameter: copy-in read borrow (Clause 10.3.4) |
| `C.Ptr.Void` | `void *` | both directions, opaque |
| `C.Size` | `size_t` | by value, both directions |
| `_` | `void` | result only; corresponds to C `void` |
| closure | function pointer and context | parameter: synchronous callback (Clause 10.5) |

A parameter or result type outside this table is a compile error with code
`E-extern-type`. The excluded types include `Char`, a list, a dictionary, an
ordinary record, a capability, a type variable, and a variadic tail. `Bytes`,
`C.In T`, and a closure are parameter-only; `_` is result-only.

The `_` row admits a unit result because the elided-arrow form `(a) !` denotes
a unit result (Clause 6.9.3); it corresponds to C `void`. No other unit
position is permitted in an extern signature.

A `String` result is a claim that the returned bytes are UTF-8. A violation
aborts with panic; the declaration asserted a string (Clause 8.4.2). A `String`
parameter is copied to a NUL-terminated UTF-8 buffer whose lifetime is the call;
a target that retains the pointer violates the boundary (Clause 2.6).

### 10.3.6 Shims

A shim is C source associated with a module that declares an extern; it is
compiled and linked beside the Eko module. Shims share the extern symbol space:
resolution is by symbol name at link time, there is one symbol vocabulary, and
there is no registration step. A shim is C code; its audit class is that of the
C library (Clause 2.6).

Out-parameters are shim work. The shim calls the real C function, receives the
written values in C storage, and returns a `ctype` record by value:

```c
StatResult _eko_stat(const char *path) {
    struct stat st;
    int r = stat(path, &st);
    return (StatResult){ .ok = r == 0, .size = st.st_size,
                         .mode = st.st_mode, .err = errno };
}
```

The extern declaration names `_eko_stat` and returns `StatResult`
(Clause 10.2.4). A shim whose C code has undefined behavior is outside the
language guarantee (Clause 2.6).

The mechanism by which a build attaches a shim to a module is
implementation-defined. The declaration is an ordinary extern, and the symbol
resolves at link time (Clause 10.6). No Eko syntax attaches a shim; the
language surface is the declaration alone.

## 10.4 Failure at the Boundary

The following table specifies the behavior at the boundary.

| C behavior | Eko behavior |
|---|---|
| error code, `errno`, or NULL sentinel | data; the extern returns it; `Result` or `Option` wrapping is library-layer work (Clause 10.2.4); `C.get_errno` reads `errno` and is pure (Clause 10.3.2) |
| segmentation fault or `SIGABRT` | process abort (Clause 8.4.3); not catchable |
| `longjmp` or a C++ exception crossing the boundary | abort; taming the crossing, by capturing with `setjmp` before it, is shim work (Clause 10.3.6) |
| panic inside a callback | abort propagates into C; C does not unwind |
| `EINTR` | the host MAY retry internally; a user extern promises nothing |

The choice between `Result`, `Option`, and panic follows the taxonomy of
Clause 9.2. External failure with a reason is `Result` at the wrapper. An
invariant violation that the wrapper itself asserts MAY panic. The extern
mechanism chooses none of them; it returns data or aborts.

## 10.5 Callbacks

Callbacks are synchronous only. Where C takes a function pointer and a context,
an Eko closure is passed. The compiler generates a trampoline whose lifetime is
the call. The callback's parameter and result types MUST follow the marshalling
whitelist (Clause 10.3.5). The closure's arrow carries its world bit as usual;
the bit lives on arrows and does not infect data (Clause 7.9.2).

A callback that outlives its call is undefined behavior (Clause 2.6). No
vocabulary makes a closure outlive its call. There is no asynchronous
registration form. The absence is an absence of vocabulary, not a semantic
invariant: a future callback registration form would be new vocabulary and would
not contradict any promise in this clause.

Reentrancy from C into Eko and back into C is permitted. Self tail calls remain
jumps on the Eko side (Clause 8.8.3).

## 10.6 Linking and Symbols

The declaration names the symbol. The build layer names the libraries, through
command-line `-l` flags or host link configuration. Library names are
platform-dependent. Conditional compilation does not exist, so a library name
has no place in a declaration.

Platform variance, including symbol decoration and library sets, is the business
of the host and the library.

Every extern symbol MUST resolve at link time. An unresolved symbol is a link
error.

Symbols beginning with `eko_` belong to the host: `eko_retain`, `eko_release`,
and the veneer entries (Clause 10.7.1). An extern declaration that names a
reserved symbol is a compile error with code `E-reserved-symbol`. The
restriction is on the symbol name, not on the Eko binding name.

Static linking and dynamic linking are implementation domain.

## 10.7 Embedding ABI

The embedding contract is a thin veneer. Function signatures are normative;
layouts are not. `eko_value` is opaque throughout.

### 10.7.1 Veneer

| Entry | Purpose |
|---|---|
| `eko_eval_file(path)` | compile and load a module |
| `eko_get_export(module, name)` | fetch an exported binding |
| `eko_call(fn, args, n)` | call with an argument vector |
| `eko_retain(v)`, `eko_release(v)` | hold and drop a value (Clause 10.7.2) |
| `eko_string_new(cstr)`, `eko_as_string(v)` | string bridge |
| `eko_int_new`, `eko_as_int`, `eko_float_new`, `eko_as_float`, `eko_bool_new`, `eko_as_bool`, `eko_unit_new` | scalar bridge |
| `eko_closure_new(fn, ctx)` | wrap a C function as an Eko closure value |
| `eko_cap_out(write, ctx)` and the corresponding constructor for each capability | per-capability bundle construction (Clause 10.8) |
| `eko_on_panic(observer)` | best-effort notification at abort (Clause 10.7.5) |

A conforming host links against the veneer and nothing else. A fast path that
calls the compiler's generated C directly exists. It is a compiler detail and is
not part of the contract.

### 10.7.2 Holding Values

A host that holds a value holds a reference. It calls `eko_retain` on receipt
and `eko_release` on disposal. There is no handle table and no root
registration.

Reclamation follows Clause 8.6. The embedding contract adds only the
retain/release protocol; it adds no pause negotiation and no finalization
obligation.

### 10.7.3 Threads

Any thread MAY call into Eko. The protocol has three requirements.

1. The language has no global mutable state; there is nothing to race on
   (Clause 8.9.3).
2. A value that crosses a boundary is shared, and a shared value is atomically
   counted (Clause 8.6.3). Embedding boundaries and `Conc` captures are judged
   alike.
3. Thread safety of a capability's operations is the bundle author's guarantee,
   in the same audit class as `Pure` (Clause 10.2.2).

No thread registration is required.

`Conc` scheduling belongs to the host bundle. The language fixes the semantics
(Clause 8.9); the command-line default bundle is compiler-emitted, and an
embedding host supplies its own.

### 10.7.4 Application Surfaces

An embedding host does not extend the authorization manifest (Clause 7.9.1). It
calls an exported entry point directly and passes application surfaces as
ordinary record values, using the closure-record idiom:

```eko
EditorServices: type = [paste: () !-> String, insert: (String) !]
init: (out: Out, svc: EditorServices) ! =
    out.println("ready")
    svc.insert("ready")
```

The manifest remains a pure operating-system surface. The audit surface moves to
the entry signature and is statically discoverable. Writing a program for an
embedding host is ordinary application development; no embedding vocabulary
appears on the Eko side.

### 10.7.5 Panic

Panic aborts the process (Clause 8.4.3). The veneer's `eko_on_panic` observer
provides best-effort notification at abort. It is not a recovery mechanism, and
a host MUST NOT rely on it for cleanup or continuation.

## 10.8 Capability Channel

The capability module (Clause 9.1.1) reaches the operating system through this
clause's primitives. The standard library declares no native contact.

A capability is internally a bundle of world-touching closures. The host
constructs the real bundles through the veneer (`eko_cap_out` and the
corresponding constructors, Clause 10.7.1). The capability module composes
derived bundles from two compiler-provided bindings per capability:

```eko
_OutB: type = [write: (String) !]
_out_unwrap: (Out) -> _OutB
_out_wrap: (_OutB) -> Out

Out.println: (o: Out, s: String) ! = _out_unwrap(o).write(s)
Out.null: () -> Out = _out_wrap({write: (s): _})
Out.tee: (o: Out, obs: (String) !-> _) -> Out =
    b := _out_unwrap(o)
    _out_wrap({write: (s): b.write(s); obs(s)})
```

The two bindings are compiler-provided: they are injected into the capability
module, they are pure, and they are visible nowhere else. No declaration form
exists for them; a module cannot name, import, or redeclare them. The runtime
implements them over its capability representation; no extern symbol names
them, and they set no bit.

The example above is ordinary Eko source inside the capability module, and the
bit accounting is ordinary inference (Clause 7.9): `Out.println` calls a `!`
closure and is `!`; `Out.null` and `Out.tee` construct values and are pure.
Every derivation remains auditable source (`eko doc -src`, Clause 11.3).

The bundle record is a private record of the capability module (`_OutB` above),
outside the public inventory (Annex C). A pair exists for each capability that
admits derivation: `Out`, `In`, `Fs`, `Env`, `Args`, `Clock`, and `Rand`.
`Proc` and `Conc` admit no derivation and have no pair.

The capability module declares no extern declaration; its only native contact
is the injected pair, and operating-system code lives in the host-constructed
bundles. The compiler-provided declarations of the capability namespaces are
fifteen. `Proc.exit` and the four concurrency members — `Conc.scope`,
`Scope.spawn`, `Task.join`, `Task.cancel` — are not source-expressible: an
open-bit dispatcher over a runtime scheduler and the observation of runtime
task values have no source form (Clauses 7.9.6, 8.9). The eight `File` members
reach the runtime's handle table directly; a handle admits no bundle pair,
so no source form carries them. `In.from_lines` and `Rand.with_seed` are
derivations whose sequences advance across calls; the language has no
stateless closure expression of an advancing sequence. Each of the fifteen
carries the signature and the bit of its Annex C.9 row, its body is an extern
symbol the compiler injects, and no declaration form names it.

## 10.9 Exclusions

Each of the following is prohibited.

- **No FFI capability.** Honesty is carried by the world bit (Clause 7.9), and
  authorization by the link layer (Clause 10.1.2).
- **No address-of.** `&` does not exist for Eko values in any form. A borrow is
  `C.In`, which copies (Clause 10.3.4); an out-parameter is shim work
  (Clause 10.3.6). The exclusions of Clause 8.11 are unchanged.
- **No reference semantics at the boundary.** No extern operation can change an
  Eko value. Results are returned.
- **No partial struct mirroring.** A `ctype` is the complete C struct, or the
  interface goes through a shim (Clause 10.3.3).
- **No bitwise operators.** The functions of Clause 9.6.5 are the vocabulary.
  `&` and `|` remain reserved pattern tokens (Clause 6.8.3).
- **No machine-word arithmetic.** Machine words hold and ferry; domain words
  compute (Clause 10.3.1).
- **No variadic externs.** A signature is complete (Clause 10.2.3).
- **No library names in declarations.** Platform-conditional linking is host
  business (Clause 10.6).
- **No asynchronous callback registration.** The prohibition is an absence of
  vocabulary, not a semantic invariant (Clause 10.5).
- **No compiler intrinsic for native calls.** The single channel is `extern`
  (Clause 10.1.1).
