# 9. Standard Library

## 9.1 Delivery Model

### 9.1.1 Delivery Surfaces

The compiler release carries three surfaces.

The **capability module** is compiler-embedded Eko source. It declares the
members of the capability namespaces and the types of their signatures:
`File`, `OpenMode`, `FileInfo`, `IoError`, `Scope`, and `Task t`. It is part
of the language definition, it is versioned with the compiler, it has no
import path, and no tool replaces it. Its channel to the operating system is
the injected primitives of Clause 10.8. It carries one ordinary import,
`std/time`, for the time types in its declarations (Clause 9.1.3).

The **standard library root** is source-delivered: an ordinary Eko module
named `std`, whose source is `lib/std.eko`. It is versioned with the compiler
and is target-independent. It declares the types whose bare names are part of
the language vocabulary and their namespace members, except the
compiler-supplied rows stated below. It declares no free function other than
the assert family of Clause 9.14.3 and the members of the non-type namespaces
`Path` (Clause 9.11.2) and `C` (Clause 10.3.2). The standard library root
contains no capability content: no capability type, no member of a capability
namespace, no world-touching declaration. It declares none of the time
family: `Time`, `Mono`, and `Duration` belong to the companion `std/time`
(Clause 9.1.3).

The compiler supplies the placeholder type rows for `Set` and `Range` and the
intrinsic member rows of the `Dict`, `Set`, `Range`, and `Path` namespaces.
`Set` and `Range` therefore have no source declaration. `Range` has no
constructor and no pattern form; the range operators are its only construction
(Clause 6.5.6). The compiler also supplies the two time parsers beneath the
companion (Clause 9.7.4). It generates the default `show` and `inspect`,
structural `eq`, derived `lt`, `with`, `to_json`, `from_json` (Clause 9.8),
and the memory operations of Clause 8.6. These services attach to every type
namespace as defaults. The `panic` intrinsic (Clause 9.2.3) is likewise
compiler-generated; it attaches to String's namespace, the type of its
parameter. Every other service outside these compiler-supplied rows is
ordinary Eko source.

The **companions** are compiler-embedded Eko source under `lib/std/`, versioned
with the compiler and target-independent. A companion is delivered under the
reserved `std/` path and imported as an ordinary module (Clause 9.1.3).
`lib/std/time.eko` is the shipped companion: it declares `Time`, `Mono`, and
`Duration` and their source members. `ParseError` remains in the root, because
`Json.parse` uses it (Clause 9.6.3). A companion contains no capability
content: no capability type, no member of a capability namespace, no
world-touching declaration. Every standard-library type namespace takes
universal members only. Ownership of a type namespace belongs to the module
that declares the type (Clause 7.4.5). The capability module performs every
world contact of the shipped surface through the injected primitives of Clause
10.8; the standard library and its companions perform none and declare no
world-touching free function.

### 9.1.2 Implicit Import

Every source file is compiled as if it began with `import std.{assert,
assert_eq, assert_ne}`: an ordinary selective import (Clause 7.2.3) and a
constant of the language definition, independent of the file's contents.
Adding or removing a name is a language revision, not a library update. An
explicit `import std` — bare, or with a list whose names are all opened
already — is redundant and reports W-redundant-import (Annex B). The stem
`std`, the path prefix `std/`, and the stems of the shipped companions
(`time` today; Clause 9.1.3) are reserved. A user module MUST NOT be named
`std` or a reserved companion stem (Clause 7.2.2).

The implicit import opens no time name: `Time`, `Mono`, and `Duration` enter
scope only through `std/time` (Clause 9.7.1).

The capability module's names resolve in every file without import: its
namespaces through the type namespaces, as `List.map` does, and its types
(`File`, `OpenMode`, `FileInfo`, `IoError`, `Scope`, `Task t`, and the
capability types) as bare names. No import path names that module (Clause
9.1.1), and its type names have no `std.` spelling: `std.File` is
`E-unbound-name`, because the standard library carries no capability content.
The capability module is itself an ordinary importer of `std/time` (Clause
9.1.3).

`show`, `inspect`, and `panic` are not opened names: the first two are
compiler-generated services attached to every type namespace (Clause 9.8.1)
and `panic` is the service of String's namespace (Clause 9.2.3); their
free-call forms are the receiver forms of Clause 6.5.2. Type-attached
functions such as `List.map` resolve through the type namespace and are not
opened. An opened name and a file's top-level binding occupy the same layer: a
same-name collision is an error (Clause 7.4.6), while local shadowing remains
legal. The qualified spelling `std.assert` is legal and denotes the same
binding.

### 9.1.3 Companion Modules

A companion module is an ordinary module under the reserved `std/` path. The
line `import std/time` opens the companion's public types into unqualified
scope and makes its constructors visible for anchoring, as any import does
(Clause 7.2.3), and opens the module stem `time` for members: the types are
named `Time`, `Mono`, and `Duration`, constructor forms are `Time 0` and
`Duration 500`, and their members are `time.Duration.secs`. The qualified
spellings `time.Time`, `time.Mono`, and `time.Duration` are legal alternatives
(Clause 7.4.4). A receiver call on a companion value resolves through the type
namespace and requires no import (Clause 7.4.3). The stem enters a file's name
scope only on an import in that file. The path prefix `std/` is reserved, and
the stems of the shipped companions are reserved (`time` today). The capability
module's `import std/time` makes the companion part of every compilation's
module closure; it does not put the stem or its types into another file's
scope, so a file still writes `import std/time` to name them. No other name is
reserved. A companion follows the ordinary module rules of Clause 7.2: one file
is one module, interfaces are extracted from source, imports are read-only
grants, and the dependency graph is acyclic. A companion declares no free name
that the implicit import could open. The admission condition is common utility:
the unit is worth versioning with the compiler and is not needed by most files.

`std/time` is the first companion. It declares `Time`, `Mono`, and `Duration`
and their source members (Clause 9.7.1); `ParseError` remains in the root. It
is compiler-embedded and versioned with the compiler (Clause 9.1.4).

### 9.1.4 Versioning

The standard library root, its companions, and the capability module are
versioned with the compiler. The interface diff between two compiler
versions, over all shipped surfaces and covering members added or removed and
signature or world-bit changes, is the library changelog and the compatibility
verdict. The language has no signature overloading, so any change to an
existing signature is breaking.
Adding a member to a type namespace is breaking by default: resolution station
2 precedes station 3 (Clause 7.4.3), so a new member can capture receiver
calls that previously resolved to the calling module's own function. The
opened free-name set is closed across versions. A deprecated member carries
`/// @deprecated` in its documentation and reports `W-deprecated` at use sites
(Clause 11.5 and Annex B). Deprecation adds no language surface.

## 9.2 Failure Taxonomy

Every failure in the standard library is one of three forms. The
classification is stated once here; the rest of this clause and Clauses 7 and
8 cross-reference it.

| Form | Condition | Reason carried |
|---|---|---|
| `Result t e` | input validation: external data is untrusted and the caller distinguishes causes | yes, payload `e` |
| `Option t` | domain check: whether a value lies in a domain, not why | no |
| `panic` | invariant violation: a program assumption is false | message |

A recoverable failure caused by data outside the program is a `Result`. A
question of membership or presence within an already-trusted value is an
`Option`. A violated invariant is a panic. The three forms are exhaustive; a
library operation MUST NOT invent a fourth failure channel. End of input is a
normal outcome and is never reported as a failure (Clause 9.2.4).

### 9.2.1 Result

```eko
Result: type t e = Ok t | Err e
```

`Result` is a built-in standard-library ADT, sealed against extension, with
constructors available to programs. A recoverable failure MUST be represented
as `Result`, and the `Err` payload MUST carry a reason sufficient for the
caller to branch. A standard-library error type is a closed union and matches
over it are exhaustive; `IoError` (Clause 9.12.2) and `ParseError` (Clause
9.6.3) are such types. The combinators are open-bit dispatchers:

```eko
Result.try: (Result t e, (t) ?-> Result u e) ?-> Result u e
```

The remaining signatures are Annex C.7.

`try` applies a `Result`-returning callback to an `Ok` payload and propagates
`Err`. `map` applies a callback to an `Ok` payload and wraps the result in
`Ok`. `map_err` applies a callback to an `Err` payload and wraps the result in
`Err`. `traverse` applies a `Result`-returning callback to every element and
collects the successes; an `Err` yields `Err`. A pure callback makes the call
pure; a world-touching callback makes it world-touching (Clause 7.9). The
inverted binding `<-` appends a last-position callback and requires the
explicit `.try()` at each propagation point. The error type MUST agree at
every link, and `map_err` is the conversion point (Clause 8.2.4). `unwrap_or`
supplies a default. `unwrap` and `expect` assert that the value is `Ok`; both
panic on `Err` and both are pure (Clause 9.2.3). `is_ok` and `is_err` are
total predicates over the constructor. The remaining `Result` members are
listed in Annex C.

### 9.2.2 Option

```eko
Option: type t = Some t | None
```

`None` answers a domain question negatively and carries no reason. A domain
check MUST be represented as `Option`; it MUST NOT be converted to `Result`
without an explicit reason at the conversion site. Every index-based accessor
yields `Option`. `List.get` is O(1) and `String.at` is O(n) (Clause 8.10);
out-of-range access yields `None` and never panics. The same rule covers
`String.slice`, `List.slice`, `Bytes.get`, `Char.from_code`,
`Bytes.from_ints`, `Env.get`, and `Dict.get`. Conversions that can fail on
range yield `Option` (Clause 9.6.2). Consumption is explicit. A `case`
branches on `Some` and `None`; `unwrap_or` supplies a default; `to_result`
converts `None` to `Err` with a caller-supplied reason; `expect` and `unwrap`
assert an invariant and panic on `None`. `Option.get` is the list-nested
accessor and passes `None` through; payload-dispatched accessors do not exist.
`Option.map`, `Option.filter`, `Option.and_then`, and `Option.or_else` dispatch
on the payload and are open-bit dispatchers (Clause 7.9.6); `Option.is_some` and
`Option.is_none` are total predicates.
`Option` also represents a cancelled task join (Clause 8.9).

### 9.2.3 Panic

A panic is the failure form for an invariant violation. It is not recoverable,
it is not a value, and it MUST NOT be used for input validation or for a
domain check.

```eko
panic: (String) -> Never
```

`panic` is pure: aborting is not world contact, so the call sets no world bit
(Clause 7.9). It aborts the process without unwinding (Clause 8.4.2). A panic
has no handler and no catch form.

`panic` is compiler-generated (Clause 9.1.1), a service of String's namespace,
the type of its parameter; `panic(m)` and `m.panic()` are the same call
(Clause 6.5.2). It is not an opened name and is never std source. The standard
panic origins are `panic`; `Option.unwrap`, `Result.unwrap`, `Option.expect`,
and `Result.expect`; the assert family; division by zero; arithmetic overflow;
`Int.abs` on `Int.min`; `String.split` with an empty separator; `Rand.int`
with a zero bound; and time arithmetic outside the `Int` nanosecond domain.
Panic output, the `P-` code set, and the exit code are specified in Annex B.
Panic output is runtime output, not a diagnostic.

### 9.2.4 EOF

End of input is not an error. A line read at end of input returns `Ok None`; a
sequential read that reaches end of input returns `Ok` with the available
prefix. A line read has type `Result (Option String) IoError`, so `Ok (Some
line)`, `Ok None`, and `Err e` are matched exhaustively. An empty line is `Ok
(Some "")` and is distinct from end of input. `File.read_bytes` reports a
short read as `Ok` with a short `Bytes` and never reports `InvalidData`
(Clause 9.5.4). An implementation MUST NOT report end of input as `Err`, and
MUST NOT use an empty string, a sentinel value, or a distinguished byte
sequence as the end-of-input signal.
