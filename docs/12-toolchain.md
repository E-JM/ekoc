# Chapter 12 — Toolchain Contract

This chapter fixes what a compiler release contains, what the compiler
derives from it, and what it exposes to tools. It is normative for the
toolchain, not for programs: no rule here changes what a program means.

## 1. Distribution

A compiler release contains the compiler binary, the standard library
source `lib/std.eko`, and the companion sources `lib/std/*.eko`
(05-names.md §11). Nothing derived is shipped: no interface files, no
precompiled objects. The standard library is one module, and its source is
valid for every target; the release matrix is one binary per host platform
plus the library sources. Platform variance lives in the compiler and in
the extern surface it provides (11-ffi.md §8); `std.eko` itself is
target-independent.

The compiler embeds the content hash of the `std.eko` it was released
with. A mismatching `lib/std.eko` is refused with a diagnostic; deliberate
replacement goes through the explicit override `eko build --std <path>`,
which compiles against a modified standard library without rebuilding the
compiler and keys its cache by the override's own content hash.

## 2. Derived cache

The interface of `std` is derived from source on first use and cached per
machine, keyed by the compiler build id and the content hash of `std.eko`
(default cache directory: `~/.cache/eko/`). The cache is disposable:
deleting it changes nothing but the next build's running time.

The derived interface is target-independent and carries the signatures,
visibility, opened sets, namespace tables, and the bodies of generic
functions — generic bodies travel with the interface (05-names.md §10), or
downstream monomorphization has nothing to expand. Object code for std
functions is generated lazily per target, only for the functions a program
references, and is cached by content hash and target.

## 3. Tooling services

- `eko where std` prints the path of the standard library source in the
  distribution; `eko where std <stem>` prints a companion's path.
- The compiler is built as a library: parsing, resolution, and checking
  are one implementation shared by the command line and by language
  tooling. There is no second parser.
- The semantic query API resolves an occurrence to its definition,
  enumerates references, returns the signature and world bit at a
  position, and lists the members of a type's namespace with their origin:
  std source, companion source, or compiler-generated. Go-to-definition
  lands in source; a compiler-generated member is reported as `derived`
  and links to the service rules (08-builtins.md §6), not to a file.
- `///` documentation comments are carried in the semantic index with the
  span of the binding they document (02-syntax.md §3); extraction and
  rendering belong to the documentation toolchain.

## 4. Version discipline

The standard library is versioned with the compiler. The interface diff
between two compiler versions — members added or removed, signature and
world-bit changes — is the library changelog and the compatibility
verdict: with no signature overloading (05-names.md §12), any signature
change is breaking. Adding a member to a type namespace is flagged as
breaking by default, because resolution station 2 precedes station 3
(05-names.md §2): a new namespace member can capture receiver calls that
previously resolved to the calling module's own function.

## 5. Documentation

The signature inventory of chapter 10 is generated from `std.eko` and the
companion sources and is verified against this manual in CI; the chapter
is normative, and the generation keeps it truthful. Deprecation is a
documentation-level convention: a `/// @deprecated` line plus the
`W-deprecated` warning (09-library.md §7.3). The language surface gains
nothing.
