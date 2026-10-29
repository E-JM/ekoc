# 7. Static Semantics

## 7.1 Program Structure

A program is a set of modules checked from one or more roots. Static checking
applies to every module in the compilation unit.

### 7.1.1 Compilation Units

A **compilation unit** is one source file and the modules it imports,
transitively (Clause 4.1). For an executable program the checking root is the
entry module (Clause 7.1.3); for test compilation the roots are the modules
containing a `test` binding (Clause 9.14.6).

The modules reachable from the roots by import edges form the program. A
module outside that closure is not part of the compilation unit, and its
contents MUST NOT cause the program to be rejected.

The capability module (Clause 9.1.1) is part of every compilation unit without
being imported: its names resolve per Clause 9.1.2, and it is checked once per
compiler build, not per unit (Clause 11.2).

Every module in the closure MUST satisfy Clauses 5 and 6 and this clause. A
violation in any reachable module is an error. Top-level bindings are checked
without dependence on source order, except for the scope rules of Clause
7.4.2.

### 7.1.2 Top-Level Bindings

The top level holds imports and bindings. The binding forms are the value
binding, the `type` declaration, the `ctype` declaration, the `test`
declaration, and the `extern` declaration (Clause 6.3.1).

A value binding MAY omit its annotation; its type then comes from the
right-hand side, and a composite literal with no expected type requires an
annotation (Clause 7.5.3). A `type` right-hand side MUST be a declaration form
(Clause 7.5.2). A `ctype` right-hand side MUST satisfy the field predicate
(Clause 10.3.3). An `extern` signature MUST satisfy Clause 7.8.1. A `test`
right-hand side is a block and is checked only for a test root (Clause
9.14.1).

Every top-level name MUST have the case class of its name space (Clause
5.5.2). A leading `_` makes the binding private (Clause 7.3.1). Top-level
bindings are mutually visible throughout their module, including their own
right-hand sides (Clause 7.4.2).

Top-level initialization is pure. No capability value is in scope at top
level, so a world-touching call is not typeable in an initializer (Clause
7.9.7).

### 7.1.3 The Entry Point

An executable program has exactly one entry module: the module containing
`main`. `main` MUST be declared at top level and MUST be public. A binding
named `main` or `_main` in any other module is an error; `_main` in the entry
module is an error.

The result type of `main` MUST be `_`. Every parameter MUST have a capability
type (Clause 7.9.1), and a capability type MUST NOT occur twice in the list. A
`main` with no parameters is legal.

The parameter list of `main` is the authorization manifest. The host supplies
exactly these capabilities by position and type; a missing or surplus item is
a startup failure (Clause 10.1.2).

A test root has no `main` requirement; its roots are its `test` bindings
(Clause 9.14.6).

## 7.2 Modules and Imports

A module is a compile-time namespace. It holds types and free functions and
has no state, no initialization semantics, and no runtime footprint.

### 7.2.1 File Is Module

One source file is one module. The file has extension `.eko` and a stem that
is a snake_case identifier containing no dot (Clause 5.5.2). The stem is the
module's name in code.

Directories are not modules and are never named in code. They organize files;
the hierarchy creates no namespace nesting, and modules under one directory
are mutually independent. A file name MUST NOT begin with `_`.

A path resolves to exactly one file relative to the project root (Clause
7.2.2). Two files with the same stem in different directories are distinct
modules with the same name; importing both into one compilation unit is a name
conflict (Clause 7.4.6). `csv.eko` and a directory `csv/` MAY coexist,
defining different modules.

### 7.2.2 Import Paths

An import declaration uses `import` and appears before the other top-level
bindings of the file, one module per line (Clause 6.2.1). A path is a single
segment or a `/`-separated sequence of identifier segments. `..`, absolute
paths, and leading or trailing separators do not exist. `/` appears only in
imports; in an expression it is division (Clause 6.5.4).

All paths resolve against the project root: the directory containing the entry
module, or the common ancestor of the test roots under test compilation
(Clause 9.14.6). The path `std` and every path under `std/` are reserved; they
never resolve against the project root. A user module MUST NOT be named `std`
(Clause 9.1.2), and a user module MUST NOT bear the stem of a shipped
companion (`time` today; Clause 9.1.3).

The final segment's stem is the module's name in code. Importing `csv/lex`
opens module `lex` only, not module `csv`. A path that resolves to no file is
an error.

### 7.2.3 Import Effects

An import has exactly three effects: it adds the target module to the
dependency graph, it opens the target module's types into unqualified scope
and makes its constructors visible for anchoring, and, when the import carries
a selective list — `import m.{f, g}` — it opens the listed free names of `m`
into the importing file's top layer. An opened free name follows the import
rules of Clause 7.4.6: a file top-level binding of the same name is
`E-name-conflict`, a local binding shadows it legally, and two imports that open
the same name are `E-name-conflict` at the declaration point.

Functions outside a selective list stay module-qualified at the expression
head, as in `person.parse(l)`, or use the receiver form with a module segment,
as in `x.person.parse()` (Clause 7.4.4). Types are opened; constructors are
visible for anchoring and form no name layer (Clause 7.4.6). The bare spelling
is the normal form for types and constructors, and the qualified spelling is
legal at every name position (Clause 7.4.4).

There is no implicit visibility and no transitive re-export. An import is a
read-only grant: it adds nothing to, removes nothing from, and overrides
nothing in the imported interface or namespaces (Clause 7.4.5).

An unused import produces W-unused. An explicit `import std` is redundant and
produces W-redundant-import (Clause 7.2.5; Annex B).

### 7.2.4 Dependency Graph

The dependency graph has one vertex per module and one directed edge per
import. It MUST be acyclic; a cycle is `E-cycle-import` (Annex B).

The compilation root obeys the same stem rules as an import. The root's file
is spelled by the host, so a same-stem import cannot be compared to the root
by file path: the stem is the root's identity, the import denotes the root,
and the resulting back edge is a cycle (Clause 7.2.1).

Names within a module MAY reference each other freely, including recursive and
mutually recursive functions. Cross-module recursion is not available; modules
are merged, or function values are passed as parameters.

Modules have no initialization semantics. Top-level bindings are pure, and
their evaluation order is unobservable (Clause 7.1.2). Separate compilation
checks an importing module against interfaces (Clause 7.3.2); acyclicity and
top-level purity make the build graph a DAG (Clause 11.6).

### 7.2.5 Standard Library Import

The standard library is the module `std`, delivered as source together with
its companions and versioned with the compiler; the capability module is
compiler-embedded (Clause 9.1.1). Every compilation unit is checked as if it
began with `import std.{assert, assert_eq, assert_ne}`; the injection is
unconditional (Clause 9.1.2). An explicit `import std`, or a list confined to
the injected names, is redundant and reports W-redundant-import (Annex B).

The implicit import opens the public types of `std` into unqualified scope:
`Bool`, `Char`, `String`, `Bytes`, the numeric types, `Option`, `Result`,
`Range`, and the error types. It opens no time name: `Time`, `Mono`, and
`Duration` belong to the companion (Clause 9.7.1). `Never` resolves in every
file too, but as vocabulary of the language rather than an opened name: it is
never imported, opened, or declared (Clause 7.5.4). The names of the capability
module — the capability types, `File`, `OpenMode`, `FileInfo`, `IoError`,
`Scope`, and `Task t` — resolve in every file without import (Clause 9.1.2).
Opened type names — std's and the capability module's — occupy a fallback
layer: an explicit import is chosen and may conflict (Clause 7.4.6), but this
import is unconditional with no opt-out, so it reserves no name; any type
declaration of the same spelling shadows one, and the qualified spelling
`std.T` denotes the opened type. Constructors are not opened as names: a
constructor reference is anchored by the expected type in value position
(Clause 7.5.2) or by the scrutinee type in pattern position (Clause 7.7.1),
and the anchor's type supplies its constructor set.

The free names opened by the implicit import are exactly `assert`,
`assert_eq`, and `assert_ne`; the injected selective import is a constant of
the language definition (Clause 9.1.2). An opened free name follows the import
rules of Clause 7.4.6: a file top-level binding of the same name is
`E-name-conflict`, and a local binding shadows it legally. `std.assert` is the
qualified spelling (Clause 7.4.4).

A companion module is imported by its `std/` path — `import std/time` is the
shipped companion (Clause 9.1.3). The import follows the general rule of
Clause 7.2.3: it opens the companion's public types and constructors
unqualified and opens the final segment's stem `time`. The types are named
`Time`, `Mono`, and `Duration`, a constructor form is `Time 0`, and their
members are `time.Duration.secs`; the qualified spellings `time.Time`,
`time.Mono`, and `time.Duration` are legal (Clause 7.4.4). Receiver calls on
time values resolve through the type namespace and require no import (Clause
7.4.3). The stems of shipped companions are reserved (Clause 7.2.2).

### 7.2.6 Module Exclusions

The following module-system mechanisms do not exist. Their absence is
normative.

- **Signature overloading.** One name denotes one binding with one signature
  (Clause 7.4.6). Ad-hoc polymorphism is a closed union with `case` (Clause
  7.6.6).
- **Functors and module-level signatures.** Generics already parameterize
  types and functions (Clause 7.6).
- **Modules as values.** Where a value is wanted, a record of closures is
  used. A module never enters value position.
- **Bulk opening of functions.** An import does not open a module's functions
  wholesale (Clause 7.2.3); only a selective list opens the named free
  functions. Every other function is reached through a module name or a type
  namespace.
- **Re-export.** An import is read-only, and forwarding is a function (Clause
  7.3.2).
- **`/` paths outside `import`.** In expressions, `/` is division only (Clause
  6.5.4).
- **Conditional compilation.** Platform variance belongs to the host and to
  ordinary code, not to the module surface.
- **Initialization blocks and module-level side effects.** A module body is a
  set of pure bindings; there is no ambient I/O and no initialization order to
  observe (Clause 7.1.2).
- **Cross-module extension of a type namespace.** Ownership belongs to the
  declaring module (Clause 7.4.5).

## 7.3 Visibility and Interfaces

### 7.3.1 Public and Private

Every top-level binding is public by default. A top-level binding whose name
begins with `_` is private: it is excluded from the module's interface and is
not visible outside the declaring module. The prefix is not part of the name's
case class (Clause 5.5.2); a private type is spelled `_Person`, and a private
function `_parse`.

`_` applies to module top-level bindings only. A local binding spelled `_x` is
legal and semantically neutral; it has no export meaning and does not
participate in W-unused (Annex B).

The `_` family means the unit type at type position, the unit value at value
position, the wildcard in pattern or parameter position, a discard binding in
`_ := e`, and a private-name prefix in a top-level identifier (Clauses 7.5.4,
7.7.1). A private function, type, constructor, or namespace is inaccessible
outside its module; `person._secret` is an error. `main` MUST be public;
`_main` is an error (Clause 7.1.3).

### 7.3.2 Interface Extraction

The interface of a module consists of its exported types, the signatures of
its exported functions, and the visibility of both. It is extracted from the
implementation; there is no hand-written interface declaration.

The interface MUST contain what is required to check importing modules:
declared types with their binders, function signatures, visibility, the `Pure`
status of extern declarations (Clause 10.2.2), and the layout declaration of a
public `ctype` (Clause 10.3.3).

Generic bodies travel with the interface, so a downstream instantiation
expands downstream (Clauses 7.6.4, 8.7.1).

Test bindings do not enter the interface (Clause 9.14.1). Derived services are
namespace members, not interface declarations (Clause 9.8.7), and tooling
marks them as derived (Clause 11.3). Separate compilation proceeds against the
interface; an import does not change it (Clause 7.4.5).

### 7.3.3 Visibility of Types in Signatures

The parameter and result types of a public function, extern declaration, or
namespace binding MUST be public. A private type MUST NOT occur in such a
signature; a private type lives only in private signatures.

The field and payload types of a public `type` or `ctype` declaration are
exported surface and MUST be public. A value of a public type can flow to an
importing module, so the name of every field and payload type MUST be
spellable there; the rule is the same one that governs a public function
signature.

Variant visibility follows the type. A public ADT has all its variants public,
and a private ADT has all its variants private, with one split: a constructor
spelling that carries the privacy prefix `_` is private to the declaring
module. The capability module closes the construction of `File` this way:
`_File` is private to the capability module, and a handle is obtained only
from `Fs.open` or `Fs.with_file` (Clauses 7.9.1 and 9.12.2).
