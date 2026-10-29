# 10. Native Interface

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
