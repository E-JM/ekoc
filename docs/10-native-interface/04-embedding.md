# 10. Native Interface

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
clause's primitives. The standard library and its companions declare no native
contact.

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
bundles. It carries one ordinary import, `std/time`, for the time types in
`FileInfo.mtime`, the `_ClockB` bundle, and `Clock.sleep`; it imports nothing
else (Clause 9.1.3). The compiler-provided declarations of the capability
namespaces are fifteen. `Proc.exit` and the four concurrency members — `Conc.scope`,
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
