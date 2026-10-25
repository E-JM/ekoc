# 11. Implementation Requirements

This clause states requirements on compilers, hosts, and toolchains. It is
normative for implementations, not for the meaning of programs. No rule in this
clause changes the meaning of a program, adds a requirement to a program, or
alters the conformance of a program under Clause 2.2. The meaning of a program
is fixed by Clauses 5 through 10; this clause fixes what an implementation MUST
provide and how it MUST behave toward tools and other implementations.

## 11.1 Distribution

A compiler release MUST contain:

- the compiler binary;
- the standard library source `lib/std.eko`;
- the companion sources `lib/std/*.eko` (Clause 9.1.3).

The capability module (Clause 9.1.1) is embedded in the compiler binary; it is
not a release file.

The release MUST NOT contain derived artifacts: no interface files and no
precompiled objects.

The standard library is one module named `std` plus the companions listed above.
Its sources MUST be valid for every target. The release matrix is one compiler
binary per host platform plus the library sources. Platform variance resides in
the compiler and in the extern surface it provides (Clause 10.8); `std.eko` is
target-independent.

The compiler MUST embed the content hash of the `std.eko` with which it was
released. If the `lib/std.eko` presented to the compiler has a different content
hash, the compiler MUST refuse it with a diagnostic.

Neither surface has an override: the standard library and the capability
module are fixed to the compiler release, and the content-hash refusal above
is the whole rule.

## 11.2 Derived Cache

The interface of `std` is derived from source on first use. It MUST be cached
per machine and keyed by the compiler build identifier and the content hash of
`std.eko`. The interface of the capability module is keyed by the compiler
build identifier alone. The default cache directory is `~/.cache/eko/`. An implementation MAY choose
another directory; the choice is implementation-defined and MUST be documented
(Clause 2.5).

The cache is disposable. Deleting it MUST change nothing except the running
time of the next build.

The derived interface MUST be target-independent. It MUST carry:

- signatures;
- visibility;
- opened sets;
- namespace tables;
- the bodies of generic functions, from which downstream monomorphization
  expands instances (Clause 8.7.1).

Object code for standard-library functions MUST be generated lazily, per target,
and only for the functions a program references. It MUST be cached by content
hash and target.

## 11.3 Tooling Services

The toolchain MUST provide the following commands:

- `eko where std` prints the path of the standard library source in the
  distribution;
- `eko where std <stem>` prints the path of the named companion module
  (Clause 9.1.3);
- `eko doc <name>`, where `<name>` is a module stem, a type or namespace, or a
  qualified member, lists the signature, the world bit, the documentation
  comment, and the origin (Clause 5.4.2);
- `eko doc -src <member>` prints the definition source of a member: the
  embedded source for a capability-module member, the file path and span for a
  standard-library or companion member.

The compiler MUST be built as a library. Parsing, resolution, and checking MUST
be one implementation, shared by the command line and by language tooling. A
second parser MUST NOT exist.

The semantic query API MUST support:

- resolving an occurrence to its definition;
- enumerating the references of a definition;
- returning the signature and the world bit at a position;
- listing the members of a type namespace with their origin: capability-module
  source, compiler-provided, standard-library source, companion source, or
  compiler-generated.

Go-to-definition MUST land in source. A capability-module member lands in the
embedded source, read-only; `eko doc -src` is the text channel to it. A
compiler-generated member MUST be reported as `derived` and linked to the
service rules of Clause 9.8, not to a file.

Documentation comments (`///`) MUST be carried in the semantic index together
with the span of the binding they document (Clause 5.4.2). Extraction and
rendering belong to the documentation toolchain.

## 11.4 Version Discipline

The standard library and the capability module are versioned with the compiler
(Clause 9.1.4).

The interface diff between two compiler versions, over both surfaces, is the
library changelog and the compatibility verdict. The diff MUST report:

- members added;
- members removed;
- signature changes;
- world-bit changes.

Because signature overloading does not exist (Clause 6.10), any signature
change is breaking. Removing a member is breaking. Changing a world bit is
breaking.

Adding a member to a type namespace MUST be reported as breaking by default.
Resolution station 2 precedes station 3 (Clause 7.4.3), so a new namespace
member can capture receiver calls that previously resolved to a function of the
calling module.

## 11.5 Documentation

The signature inventory in Annex C MUST be generated: the standard-library
parts from `std.eko` and the companion sources, and the capability surface
(C.9) from the capability module's extracted interface, which the compiler MUST
emit. It MUST be verified against this document in continuous integration.
Annex C is normative; generation keeps it consistent with the shipped surface.

Deprecation is a documentation-level convention: a `/// @deprecated` line on a
binding plus the `W-deprecated` warning (Annex B.4). The compiler MUST issue the
warning at uses of a binding so documented. The language surface gains no syntax
or declaration for deprecation.

## 11.6 Build Graph and Separate Compilation

The dependency graph is acyclic, and modules have no initialization semantics
(Clause 7.2.4). Together these properties make the build graph a DAG cache;
incremental compilation follows from it.

An interface MUST be extracted automatically from the implementation. It MUST
include the exported types, signatures, and visibility (Clause 7.3.2). A
hand-written interface layer (`.mli`, `.d.ts`, or equivalent) MUST NOT be
required.

Separate compilation MUST proceed against the interface. A dependent module MUST
NOT require the source of a dependency.

Generic bodies MUST travel with the interface (Clause 8.7.1). Instantiation is
expanded at the use site. Identical instances MUST be deduplicated at link time.

A module MAY be compiled in any topological order of the dependency graph; the
order is unobservable (Clause 7.2.4).
