# 10. Native Interface

## 10.3 C Vocabulary

### 10.3.1 Machine Words

The machine words are `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`,
`F32`, and `F64`. They are the ABI lexicon.

A machine word holds, compares, shows, and ferries a value. It does not compute:
no arithmetic function and no bitwise function is defined on a machine word.
Computation is performed on the domain words `Int`, `UInt`, and `Float`;
conversion is explicit at every boundary.

The services of a machine word are:

- `==`, `!=`, `<`, `<=`, `>`, and `>=` between two values of the same
  machine-word type;
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
