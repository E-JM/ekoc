# 7. Static Semantics
This clause specifies the static semantics of Eko: the conditions under which a program is well formed. It defines program and module structure,
name spaces, resolution, visibility, type identity, generic instantiation, pattern typing, exhaustiveness, call and operator typing,
capabilities, and the world bit.

A program that violates a rule of this clause is rejected by an error diagnostic (Clause 2.4). Clause 7.10 enumerates the categories of required
rejection; Annex B owns the diagnostic codes. Evaluation is Clause 8. The failure taxonomy is Clause 9.2 and is only cross-referenced here.
## 7.1 Program Structure
A program is a set of modules checked from one or more roots. Static checking applies to every module in the compilation unit.
### 7.1.1 Compilation Units
A **compilation unit** is one source file and the modules it imports, transitively (Clause 4.1). For an executable program the checking root is
the entry module (Clause 7.1.3); for test compilation the roots are the modules containing a `test` binding (Clause 9.14.6).

The modules reachable from the roots by import edges form the program. A module outside that closure is not part of the compilation unit, and its
contents MUST NOT cause the program to be rejected.

The capability module (Clause 9.1.1) is part of every compilation unit without being imported: its names resolve per Clause 9.1.2, and it is checked
once per compiler build, not per unit (Clause 11.2).

Every module in the closure MUST satisfy Clauses 5 and 6 and this clause. A violation in any reachable module is an error. Top-level bindings are
checked without dependence on source order, except for the scope rules of Clause 7.4.2.
### 7.1.2 Top-Level Bindings
The top level holds imports and bindings. The binding forms are the value binding, the `type` declaration, the `ctype` declaration, the `test`
declaration, and the `extern` declaration (Clause 6.3.1).

A value binding MAY omit its annotation; its type then comes from the right-hand side, and a composite literal with no expected type requires an
annotation (Clause 7.5.3). A `type` right-hand side MUST be a declaration form (Clause 7.5.2). A `ctype` right-hand side MUST satisfy the field
predicate (Clause 10.3.3). An `extern` signature MUST satisfy Clause 7.8.1. A `test` right-hand side is a block and is checked only for a test
root (Clause 9.14.1).

Every top-level name MUST have the case class of its name space (Clause 5.5.2). A leading `_` makes the binding private (Clause 7.3.1). Top-level
bindings are mutually visible throughout their module, including their own right-hand sides (Clause 7.4.2).

Top-level initialization is pure. No capability value is in scope at top level, so a world-touching call is not typeable in an initializer
(Clause 7.9.7).
### 7.1.3 The Entry Point
An executable program has exactly one entry module: the module containing `main`. `main` MUST be declared at top level and MUST be public. A
binding named `main` or `_main` in any other module is an error; `_main` in the entry module is an error.

The result type of `main` MUST be `_`. Every parameter MUST have a capability type (Clause 7.9.1), and a capability type MUST NOT occur twice in
the list. A `main` with no parameters is legal.

The parameter list of `main` is the authorization manifest. The host supplies exactly these capabilities by position and type; a missing or
surplus item is a startup failure (Clause 10.1.2).

A test root has no `main` requirement; its roots are its `test` bindings (Clause 9.14.6).
## 7.2 Modules and Imports
A module is a compile-time namespace. It holds types and free functions and has no state, no initialization semantics, and no runtime footprint.
### 7.2.1 File Is Module
One source file is one module. The file has extension `.eko` and a stem that is a snake_case identifier containing no dot (Clause 5.5.2). The
stem is the module's name in code.

Directories are not modules and are never named in code. They organize files; the hierarchy creates no namespace nesting, and modules under one
directory are mutually independent. A file name MUST NOT begin with `_`.

A path resolves to exactly one file relative to the project root (Clause 7.2.2). Two files with the same stem in different directories are
distinct modules with the same name; importing both into one compilation unit is a name conflict (Clause 7.4.6). `csv.eko` and a directory `csv/`
MAY coexist, defining different modules.
### 7.2.2 Import Paths
An import declaration uses `import` and appears before the other top-level bindings of the file, one module per line (Clause 6.2.1). A path is a
single segment or a `/`-separated sequence of identifier segments. `..`, absolute paths, and leading or trailing separators do not exist. `/`
appears only in imports; in an expression it is division (Clause 6.5.4).

All paths resolve against the project root: the directory containing the entry module, or the common ancestor of the test roots under test
compilation (Clause 9.14.6). The path `std` and every path under `std/` are reserved; they never resolve against the project root, and a user
module MUST NOT be named `std` (Clause 9.1.2).

The final segment's stem is the module's name in code. Importing `csv/lex` opens module `lex` only, not module `csv`. A path that resolves to no
file is an error.
### 7.2.3 Import Effects
An import has exactly three effects: it adds the target module to the dependency graph, it opens the target module's types and constructors
into unqualified scope, and, when the import carries a selective list — `import m.{f, g}` — it opens the listed free names of `m` into the
importing file's top layer. An opened free name follows the import rules of Clause 7.4.6: a file top-level binding of the same name is
E-name-conflict, a local binding shadows it legally, and two imports that open the same name are E-name-conflict at the declaration point.

Functions outside a selective list stay module-qualified at the expression head, as in `person.parse(l)`, or use the receiver form with a
module segment, as in `x.person.parse()` (Clause 7.4.4). Types and constructors are opened; the bare spelling is the normal form, and the
qualified spelling is legal at every name position (Clause 7.4.4).

There is no implicit visibility and no transitive re-export. An import is a read-only grant: it adds nothing to, removes nothing from, and
overrides nothing in the imported interface or namespaces (Clause 7.4.5).

An unused import produces W-unused. An explicit `import std` is redundant and produces W-redundant-import (Clause 7.2.5; Annex B).
### 7.2.4 Dependency Graph
The dependency graph has one vertex per module and one directed edge per import. It MUST be acyclic; a cycle is E-cycle-import (Annex B).

Names within a module MAY reference each other freely, including recursive and mutually recursive functions. Cross-module recursion is not
available; modules are merged, or function values are passed as parameters.

Modules have no initialization semantics. Top-level bindings are pure, and their evaluation order is unobservable (Clause 7.1.2). Separate
compilation checks an importing module against interfaces (Clause 7.3.2); acyclicity and top-level purity make the build graph a DAG (Clause
11.6).
### 7.2.5 Standard Library Import
The standard library is the module `std`, delivered as source and versioned with the compiler; the capability module is compiler-embedded
(Clause 9.1.1). Every compilation unit is checked as if it began with `import std`; the injection is unconditional (Clause 9.1.2). An explicit
`import std` is redundant and reports W-redundant-import (Annex B).

The implicit import opens the public types of `std` into unqualified scope: `Bool`, `Char`, `String`, `Bytes`, the numeric
types, `Option`, `Result`, `Range`, `Never`, the error types, and the time types. The names of the capability module — the capability types,
`File`, `OpenMode`, `FileInfo`, `IoError`, `Scope`, and `Task t` — resolve in every file without import (Clause 9.1.2). Opened type
names — std's and the capability module's — occupy a fallback layer: an explicit import is chosen and may conflict (Clause 7.4.6), but this
import is unconditional with no opt-out, so it reserves no name; any type declaration of the same spelling shadows one, and the qualified
spelling `std.T` denotes the opened type. Constructors are not opened as names: a constructor reference is anchored by the expected type in
value position (Clause 7.5.2) or by the scrutinee type in pattern position (Clause 7.7.1), and the anchor's type supplies its constructor set.

The free names opened by the implicit import are exactly `assert`, `assert_eq`, and `assert_ne`; the
injected selective import is a constant of the language definition (Clause 9.1.2). An opened free name
follows the import rules of Clause 7.4.6: a file top-level binding of the same name is E-name-conflict,
and a local binding shadows it legally. `std.assert` is the qualified spelling (Clause 7.4.4).

A companion module is imported by its `std/` path, for example `import std/math`; the import opens the final segment's stem (Clause 9.1.3).
### 7.2.6 Module Exclusions

The following module-system mechanisms do not exist. Their absence is normative.

- **Signature overloading.** One name denotes one binding with one signature (Clause 7.4.6). Ad-hoc polymorphism is a closed union with `case`
  (Clause 7.6.6).
- **Functors and module-level signatures.** Generics already parameterize types and functions (Clause 7.6).
- **Modules as values.** Where a value is wanted, a record of closures is used. A module never enters value position.
- **Opening functions into unqualified scope.** An import opens types and constructors only (Clause 7.2.3). Functions are reached through a
  module name or a type namespace.
- **Re-export.** An import is read-only, and forwarding is a function (Clause 7.3.2).
- **`/` paths outside `import`.** In expressions, `/` is division only (Clause 6.5.4).
- **Conditional compilation.** Platform variance belongs to the host and to ordinary code, not to the module surface.
- **Initialization blocks and module-level side effects.** A module body is a set of pure bindings; there is no ambient I/O and no
  initialization order to observe (Clause 7.1.2).
- **Cross-module extension of a type namespace.** Ownership belongs to the declaring module (Clause 7.4.5).

## 7.3 Visibility and Interfaces
### 7.3.1 Public and Private
Every top-level binding is public by default. A top-level binding whose name begins with `_` is private: it is excluded from the module's
interface and is not visible outside the declaring module. The prefix is not part of the name's case class (Clause 5.5.2); a private type is
spelled `_Person`, and a private function `_parse`.

`_` applies to module top-level bindings only. A local binding spelled `_x` is legal and semantically neutral; it has no export meaning and does
not participate in W-unused (Annex B).

The `_` family means the unit type at type position, the unit value at value position, the wildcard in pattern or parameter position, a discard
binding in `_ := e`, and a private-name prefix in a top-level identifier (Clauses 7.5.4, 7.7.1). A private function, type, constructor, or
namespace is inaccessible outside its module; `person._secret` is an error. `main` MUST be public; `_main` is an error (Clause 7.1.3).
### 7.3.2 Interface Extraction
The interface of a module consists of its exported types, the signatures of its exported functions, and the visibility of both. It is extracted
from the implementation; there is no hand-written interface declaration.

The interface MUST contain what is required to check importing modules: declared types with their binders, function signatures, visibility, the
`Pure` status of extern declarations (Clause 10.2.2), and the layout declaration of a public `ctype` (Clause 10.3.3).

Generic bodies travel with the interface, so a downstream instantiation expands downstream (Clauses 7.6.4, 8.7.1).

Test bindings do not enter the interface (Clause 9.14.1). Derived services are namespace members, not interface declarations (Clause 9.8.7), and
tooling marks them as derived (Clause 11.3). Separate compilation proceeds against the interface; an import does not change it (Clause 7.4.5).
### 7.3.3 Visibility of Types in Signatures
A public function's parameter and result types MUST be public. A private type MUST NOT occur in the signature of a public function; a private
type lives only in private signatures.

The field and payload types of a public `type` or `ctype` declaration are exported surface and MUST be public. A value of a public type can flow
to an importing module, so the name of every field and payload type MUST be spellable there; the rule is the same one that governs a public
function signature.

Variant visibility follows the type and MUST NOT be split. A public ADT has all its variants public, and a private ADT has all its variants
private.
## 7.4 Names and Resolution
Names are resolved statically. There is no dynamic name lookup and no runtime namespace.
### 7.4.1 Name Spaces
| Name space | Members | Case class |
|---|---|---|
| Identity | declared types, constructors, type namespaces | PascalCase |
| Binding | value bindings, parameters, function names, type variables | snake_case |
| Module | module stems, import paths | snake_case |
| Field | record fields, per record type | checked against the declaration |

A name's spelling determines its space: PascalCase carries identity, and snake_case is a binding or a module path (Clause 5.5.2). A name in the
wrong case class is E-name-case (Clause 5.5.2).

A constructor and a type MAY share a spelling. `Age: type = Age UInt` declares the type `Age` and the constructor `Age`; type position denotes
the type, while value and pattern positions denote the constructor.

In type position, a bare lowercase identifier (after any privacy prefix) is a type variable of the enclosing annotation or declaration, a
bare PascalCase identifier is a declared type, and a dotted name is a module path (Clauses 5.5.2, 6.9.1, 7.4.4). In expression position, a lowercase name is a binding or a module, and
a PascalCase name is a constructor; a namespace member needs a receiver or a qualified head (Clause 7.4.3).

Field names belong to their record; they do not form a global namespace. `_` is positional (Clause 7.3.1). Reserved and contextual words follow
Clause 5.6.
### 7.4.2 Scopes
Scopes are lexical and nest: the import scope, the module top level, each block, each lambda body, and each `case` arm with its guard.

| Position | Lookup order |
|---|---|
| Expression head | parameter, local binding, file top-level binding, imported module |
| Type position | type variable of the enclosing annotation or declaration, declared type in unqualified scope, module-qualified type |
| Pattern constructor | visible constructor, unqualified or module-qualified |
| Receiver | the member resolution stations of Clause 7.4.3; lexical bindings are not consulted |

A binding in an inner scope shadows a binding of the same name in an outer scope. An import and a file top-level binding share one layer, so
their collision is E-name-conflict (Clause 7.4.6). A local binding MAY shadow an imported module (Clause 7.4.4).

`:=` introduces a fresh binding that shadows an earlier binding of the name; it does not mutate the earlier value (Clause 8.2.2). A closure
captures the value held at capture time (Clause 8.2.3).

A top-level binding is visible throughout its module, including its own right-hand side. A local binding is visible from its binding line to the
end of the enclosing block; a block-level self-reference is an error (Clause 8.2.1).

A pattern binding is visible in its arm's guard and result (Clause 7.7.1), and a lambda parameter in its body. A type variable's scope is the
whole annotation (Clause 7.6.1); a declaration's binders are local to its right-hand side (Clause 7.6.2). A name MUST be bound in an enclosing
scope or imported; an unbound name is an error.
### 7.4.3 Member Resolution
A call in receiver form, `x.f(args)`, resolves `f` at four stations in order. The first station with a hit resolves the member; stations are not
merged.

| Station | Searches | Form |
|---|---|---|
| 1. Record field | the field `f` of the record type of `x` | `x.f`; `x.f(args)` when the field is a function value |
| 2. Type namespace | the namespace of the static type of `x` | `x.f(args)` |
| 3. Current module | top-level free functions of the current module whose first parameter type is the static type of `x` | `x.f(args)` |
| 4. Module path | public free functions of the named module whose first parameter type is the static type of `x` | `x.Mod.f(args)` |

The namespace of a type is named after the type's head: `[]t` maps to `List`, `[k]v` to `Dict`, `Option t` to `Option`, and `Result t e` to
`Result`. A declared type uses its own name; an anonymous tuple or record type has no namespace.

Station 2 includes the derived service members of Clause 9.8.7. When the static type is a type variable, station 2 resolves only the operations
of Clause 7.6.6, and the concrete service is selected at instantiation (Clause 8.7.2).

Station 3 is the only station at which a module can define a receiver-form function on a foreign type. A top-level function of a different module
is not found there and requires station 4. Station 4 applies when the name is a multi-segment qualified name; the module segment names an
imported module or the current module.

The receiver position never consults lexical bindings. Station 2 precedes station 3, so a namespace member resolves over an equally named
function in the current module; this is ordered resolution, not a name conflict (Clause 7.4.6).

If no station has a hit, the call is rejected, and the diagnostic MUST list the readings available at the receiver position, including a module
reading when the name also denotes a module. If a name is both a field and a function reachable at a later station, station 1 wins for field
access; for a call whose callee is not a function value, the diagnostic MUST report both readings, and the author selects the function by a
fully qualified call (Clause 7.4.4).

A bare `x.f` that is not a call is a field access. A function member requires a call. Resolution depends only on the static type of the receiver;
there is no dynamic dispatch. The receiver is the member's first parameter, and the call is typed as Clause 7.8.2.
### 7.4.4 Qualified Names
A qualified name has the form `Mod.f` at the expression head, `x.Mod.f(args)` at a receiver link, `Mod.Type` in type position, or `Mod.Ctor` at
constructor value or pattern position. The prefix is the declaring module's name, not an import path.

| Position | Form | Denotes |
|---|---|---|
| Expression head | `Mod.f(args)` | free function `f` of module `Mod` |
| Receiver link | `x.Mod.f(args)` | `Mod.f(x, args)` (Clause 6.5.2) |
| Type position | `Mod.Type` | type `Type` declared in `Mod` |
| Constructor | `Mod.Ctor` | constructor `Ctor` declared in `Mod` |

The bare spelling is the normal form for types and constructors. The qualified spelling is legal at every name position and is the disambiguation
device for same-named types and constructors (Clause 7.4.6).

A module's own name is in scope inside itself. The prefix MUST denote an imported module or the current module. A local binding at the expression
head shadows a module of the same name; the receiver form is unaffected (Clause 7.4.2).

A qualified name resolves only public names (Clause 7.3.1) and does not bypass namespace ownership (Clause 7.4.5). `/` never appears in a
qualified name; module paths occur only in imports (Clause 7.2.2).

A fully qualified name MAY appear at four positions in a chain:

| Position | Form | Basis |
|---|---|---|
| Chain head | `Mod.f(x)` | the direct identity |
| Argument | `.map(Mod.f)` | an ordinary name reference |
| Element-level link | `.traverse((l): Mod.f(l.split(",")))` | a direct form inside a callback |
| Accumulated-value link | `.Mod.f()` | the extended identity |

A qualified name in argument position is a name reference, not a call (Clause 6.5.3). `a.Mod.f(b)` is identical to `Mod.f(a, b)` (Clause 6.5.2);
its typing is Clause 7.8.2.
### 7.4.5 Type Namespace Ownership
A type namespace belongs to the module that declares the type. It is keyed by the declaring module, not by the calling file, and it travels with
the type's visibility.

A top-level binding whose head is `TypeName.name` declares a member of `TypeName`'s namespace. It is legal only in the module that declares
`TypeName`; anywhere else it is E-namespace-owner (Annex B). A qualified spelling does not bypass the rule: `compiler.Compiler.temp_fn` outside
`compiler` targets the same foreign type and is the same error.

An import is a read-only grant. Nothing written in an importing module adds to, removes from, or overrides the imported namespace or interface. A
module that wants a service on a foreign type declares a free function: reachable at the receiver through station 3 inside its module, and
elsewhere through `temp.temp_fn(c)` or `c.temp.temp_fn()` (Clauses 7.4.3, 7.4.4).

Same-named types MAY coexist. When two visible types share a name, from two imports or from an import beside a local declaration, the unqualified
spelling is an error at its use site, and each is written module-qualified. A module's own name is in scope inside itself (Clause 7.4.4).

A namespace is reachable through values. Station 2 resolves through the static type of the receiver whenever the type is in the compilation unit,
independently of the calling file's imports; imports govern names, not the receiver form (Clause 7.4.3).

Derived services attach to every type namespace as defaults. An override is a namespace declaration and obeys ownership: it is legal only in the
module that declares the type (Clause 9.8.2).
### 7.4.6 Name Conflicts
A name conflict is a collision within one layer. The following are conflicts:

- two top-level bindings of one module with the same name — a namespace binding is keyed by its full head, so `Int.add` and `Float.add` coexist, and only
  the same head conflicts;
- an import and a file top-level binding with the same name;
- two imported modules with the same stem, judged over the compilation unit's import closure and not over one file's import list (Clause 7.2.1);
- a type variable with the same name as an imported module;
- two parameters of `main` with the same capability type (Clause 7.1.3);
- a repeated binding name within one pattern (Clause 7.7.1).

Every such conflict is E-name-conflict (Annex B).

Shadowing is not a conflict: an inner binding shadows an outer one, `:=` shadows an earlier binding, and a local binding MAY shadow an imported
module (Clause 7.4.2). An opened standard-library or capability type name is shadowable by any type declaration of the same spelling
(Clause 7.2.5); an opened free name is not.

Two functions with the same name in different modules coexist and are selected by a qualified name (Clause 7.4.4). Two types with the same name
coexist; an unqualified use is an error at the use site, and a qualified spelling selects one (Clause 7.4.5). Constructors never conflict by
visibility: same-named constructors of different types coexist, and a reference is anchored by the expected type or the scrutinee type
(Clauses 7.5.2, 7.7.1). An unanchored reference is legal only when exactly one constructor of that name is visible and it is non-generic
(self-anchoring, Clause 7.6); otherwise it is an error, and an annotation anchors it. A qualified constructor selects one.

Signature overloading does not exist. One name denotes one binding in its layer, with one signature; the same name with different signatures is
not admitted (Clause 6.10).

A conflict is reported even when the conflicting name is never used, except for same-named types and constructors, whose unqualified use is the
point of rejection.
## 7.5 Types
A type is a compile-time identity that classifies values (Clause 4.4). Types exist only at compile time; no expression denotes a type and no test
inspects one (Clause 8.11).
### 7.5.1 Type Identity
A declared type is nominal: its identity is its declaration, identified by the declaring module and the declared name. Two declarations are
different types even when their names and right-hand sides coincide.

An anonymous composite type is identified structurally: a list by its element type, a dictionary by its key and value types, a tuple by its
components in order, and a record by its field names and field types.

A function type is identified by its parameter types, its result type, and its arrow form. `(a) -> b`, `(a) !-> b`, and `(a) ?-> b` are three
different types. The world bit is part of identity and is erased at run time (Clause 7.9.3).

A type variable is identified by its binder (Clause 7.6.1). An instantiated type is identified by its declaration and its type arguments,
compared componentwise. `_` is the unit type and `Never` is the bottom type; each is equal only to itself (Clause 7.5.4).

Instantiation does not weaken identity: `Pair Int String` and `Pair Int Float` are different nominal types (Clause 7.6.4). Identity is
independent of representation (Clause 8.5) and of the path by which a type is imported.

The maximum nesting depth of a type is implementation-defined (Clause 2.5); a type that exceeds it is rejected with a diagnostic (Annex B).
### 7.5.2 Nominal Types
Every `type` declaration introduces a new nominal type. The right-hand side MUST be a declaration form: a variant list, or a bracketed record or
tuple type (Clause 6.9.5). A composite type expression such as `[]Int` is not a declaration form, and there are no type aliases.

A variant list declares an algebraic data type. A variant name is a PascalCase constructor; a payload follows by juxtaposition, and a
multi-component payload is a tuple type. A zero-variant list is legal. A constructor applied by juxtaposition is check-only: it requires an
expected type; its instantiation is taken from that type (Clause 7.6.4). A constructor is never written with parentheses (Clause 6.3.5).
Its visibility follows the type (Clause 7.3.3), and its name MAY equal the type name.

A bracketed right-hand side declares a record or a tuple-shaped nominal type. Field access does not apply to a tuple-shaped nominal type; its
values are taken apart by patterns (Clause 7.7.1).

A single-variant declaration with one payload is the newtype form. It admits no implicit conversion in either direction; the wrapped type is
reachable by pattern matching.

Two nominal types with the same shape are mutually incompatible. Conversion is explicit: a literal or a field-by-field rebuild. A nominal value
MAY flow into a same-shape anonymous type by shape adaptation (Clause 7.5.5); the reverse flow does not exist.

A user-declared zero-variant type has no values and the same derived-service behavior as `Never`, but bottom adaptation remains a property of
`Never` alone (Clause 7.5.4).

A `ctype` declaration introduces a nominal record type whose layout is pinned to C. Its field predicate is checked at the declaration line
(Clause 10.3.3).
### 7.5.3 Composite Types
| Form | Type syntax | Constraint |
|---|---|---|
| List | `[]t` | one element type |
| Dictionary | `[k]v` | key and value types |
| Tuple | `[a, b, ...]` | at least two components |
| Record | `[f: a, g: b, ...]` | declared field names |

List and dictionary are distinguished inside the brackets: an empty bracket pair is a list, and a single type is a dictionary. These are the only
forms that place a type after `]`.

A tuple has at least two components. A one-element bracket `[Int]` is not a tuple and is not a valid type. Parentheses are context-sensitive:
in a complete-type position `(Int)` is the unit-returning function type `(Int) -> _`, while in a type-atom position it groups a type; a
zero-element `()` is the parameter group of a zero-parameter function type (Clause 6.9.3). The unit value is `_`, not a zero-element tuple. Parentheses disambiguate nesting: `[]([String]Int)` is a list of
dictionaries, and `[[]String]Int` is a dictionary whose keys are lists.

A composite literal is resolved by its expected type:

| Inside braces | Literal form | Resolved by |
|---|---|---|
| comma items, no colon | list or tuple | expected type |
| colon pairs | dictionary or record | expected type |

A composite literal never stands alone. Where no expected type exists, the literal requires an annotation (Clause 6.6.2); an empty literal
follows the same rule. A partial record literal is legal only in the argument position of `with` (Clause 9.8.6); anywhere else it is rejected
(Clause 6.6.3). A full record literal MUST name every field exactly once and no field outside the record type; list elements, dictionary keys
and values, and tuple components MUST have the expected types.

A composite type is invariant in its component types. Shape adaptation does not enter a component (Clause 7.5.5). The exceptions at a
collection-element position are `Never` adaptation (Clause 7.5.4) and bit adaptation (Clause 7.9.5). `Set` has no composite notation and no
literal (Clause 9.10.3).
### 7.5.4 Unit Type and Never
`_` at type position is the unit type: exactly one value, the unit value `_`. It never means inference. An annotation has no inference holes.

`()` at type position is the zero-parameter function type and abbreviates `() -> _`. A unit result is written `-> _`; `-> ()` is not a unit
result. The elided arrow `(a)` abbreviates `(a) -> _` (Clause 6.9.3).

`Never` is a standard-library zero-variant ADT: no constructors and no values. It is distinct from the unit type.

`Never` is the bottom type. A `Never` expression adapts to any value type at the following positions: an argument, an operand, an expected type,
a collection element, a `case` arm, and a declared return position. The adaptation applies at value positions only. It does not apply inside a
function type: `(a) -> Never` does not adapt to `(a) -> Int`; a lambda wrapper is the explicit form. A `[]Never` value has only the empty list.

A function MAY declare any arrow with result `Never`. `panic` is pure and has type `(String) -> Never`; `Proc.exit` is world-touching and has
type `(Proc, Int) !-> Never` (Clause 9.2.3). A declared result type `t` does not promise termination, and divergence neither sets nor clears the
world bit (Clause 7.9.2).

`Never` marks a call that does not return. `<-` is not applicable to a `Never`-returning call that has no callback parameter (Clause 7.8.1). A
`case` arm whose body is `Never` adapts to the case type, but its pattern still counts for exhaustiveness (Clause 7.7.4).

Unreachable code after a call of type `Never` is legal and produces W-unreachable (Clause 8.4.4).

The language has exactly two subtyping relations: bit adaptation (Clause 7.9.5) and `Never` adaptation. Shape adaptation (Clause 7.5.5) is a
separate one-way adaptation, not subtyping.
### 7.5.5 Shape Adaptation
A nominal value MAY flow into a same-shape anonymous type. Same shape is exact structural equality between the anonymous type and the nominal
type's representation.

For a record, the field names and field types MUST match exactly. For a tuple, the arity and component types MUST match exactly. At a recursive
position the comparison is by nominal name; cycles do not enter the comparison (Clause 7.5.6).

Adaptation is one-directional: nominal to anonymous. A nominal value never flows into a different nominal type, and an anonymous value never
flows into a nominal type. Conversion to a nominal type is a literal or a field-by-field rebuild.

Adaptation applies at three positions: an argument, an operand, and an expected type.

Same shape is an exact match, not a width relation: an anonymous type with fewer fields does not accept a nominal value with more, and row
polymorphism does not exist. There is no automatic conjunction; operand typing is fixed by the left operand, the right operand MUST adapt, and
two nominal operands do not search for a common anonymous type.

Shape adaptation does not enter container elements or the interior of a function type. An explicit map re-establishes the element shape. Bit
adaptation is not subject to this restriction (Clause 7.9.5).

Anonymous ADTs do not exist, so shape adaptation is available for records and tuples only. A tuple shape extends to a generic instantiation by
the shape of its right-hand side: for `p: Pair Int String`, `q: [Int, String] = p` is legal. An algebraic shape does not extend.

A failed nominal-to-nominal flow is E-nominal-mismatch; a failed same-shape flow is E-shape-mismatch (Annex B). Shape adaptation creates no
supertype and does not compose: an adapted anonymous value cannot flow into a nominal type.
```eko
Human: type = [name: String, age: Int]
Wide: type = [name: String, age: Int, city: String]

view: (p: [name: String, age: Int]) -> String = p.name
h: Human = {name: "ada", age: 36}
w: Wide = {name: "ada", age: 36, city: "x"}

view(h)
q: [name: String, age: Int] = h
view(w)   // error: `Wide` does not have the same shape as the parameter
```
### 7.5.6 Recursive Types
A type declaration's right-hand side MAY refer to the name being declared. Self-recursion and mutual recursion within one module are equally
open. Cross-module type recursion is closed by the prohibition on cyclic imports (Clause 7.2.4).

A recursive position is indirect: a self-reference is represented by a reference, so a recursive type has finite size. Opening recursion requires
no additional check. A recursive type with no base case is legal and has zero inhabitants. Inhabitance is not checked; `[]Never` needs no
inhabitance analysis.

`show` and `eq` recurse structurally, and the derived `lt` of an ADT does so; termination follows from acyclicity of the heap graph
(Clause 8.6.1). Same-shape comparison at a recursive position compares by nominal name, and cycles do not enter the judgment (Clause 7.5.5).

An anonymous recursive type does not exist: a recursive reference requires a declared name. A recursive generic reference MUST be fully applied;
`Tree t` in its own right-hand side is well formed, while a bare `Tree` is an arity error (Clause 7.6.2).
```eko
Nat: type = Zero | Succ Nat
Tree: type t = Leaf t | Node [Tree t, Tree t, t]
Person: type = [name: String, friends: []Person]

Expr: type = Num Int | Add [Expr, Expr] | Block []Stmt
Stmt: type = Let [String, Expr] | Ret Expr

Loop: type = Wrap Loop
```
### 7.5.7 Type Equality
The judgment `t = u` holds when the two types have the same identity.

| Type form | Equality condition |
|---|---|
| nominal `T a1 ... an` | same declaration and equal arguments in order |
| list `[]t` | equal element types |
| dictionary `[k]v` | equal key and value types |
| tuple `[t1, ..., tn]` | same arity and equal components in order |
| record `[f1: t1, ...]` | same field names with equal field types |
| function type | equal parameter types, equal result type, same arrow form |
| type variable | same binder |
| `_` | both `_` |
| `Never` | both `Never` |

Type equality is symmetric and transitive. It is decidable by nominal lookup and structural comparison. A record type's field order is not part
of equality; declaration order remains the representation order (Clause 8.5.2). Two anonymous record types with the same field names and types
are therefore equal regardless of the order in which the fields are written.

An expression of type `t` conforms to an expected type `u` when `t = u`, or when one of the one-way adaptations applies at the position: `Never`
adaptation (Clause 7.5.4), shape adaptation (Clause 7.5.5), or bit adaptation (Clause 7.9.5). No other conversion is applied, and an annotation
is checked exactly.

A function type's arrow form is part of equality. An open arrow `?->` is equal only to an open arrow with the same parameter and result types.
The maximum nesting depth of a checked type is implementation-defined (Clause 2.5).
## 7.6 Generics
Generics are parametric. Type variables are generalized at annotated bindings and in type declarations, and instantiated at use sites. There are
no bounds, traits, or constraint solving.
### 7.6.1 Type Variables
A type variable is a lowercase identifier in type position. In a function annotation, an unbound lowercase name is a type variable of that
function; its scope is the whole annotation. In a type declaration's right-hand side, the declared binders are the type variables (Clause 7.6.2).
Variables are fresh per declaration.

A type variable MUST NOT have the same name as an imported module; the same-layer conflict is E-name-conflict (Clause 7.4.6).

A variable MAY appear in a parameter type, a result type, a bracketed shape, an application argument, and a nested function type. A variable MUST
NOT head an application: `t Int` is an error; a variable denotes a type, not a type constructor (Clause 6.9.4). A variable MUST NOT
appear at value position, in an extern signature (Clause 10.2.3), or in a `ctype` field (Clause 10.3.3). A variable MAY stand for any type; it
has no bounds.
### 7.6.2 Type Binders
A `type` declaration lists its type variables after `type` and before `=` (Clause 6.3.5). The list is ordered and positional: in `Pair: type t u
= [t, u]`, `t` receives the first argument and `u` the second. A declaration with no binders is monomorphic.

A binder MUST occur in the right-hand side. A binder that occurs nowhere is phantom and is E-phantom (Annex B).

Binder order is independent of variant order. In `Value: type t u = First [u, t] | Second t`, `t` is still the first argument and `u` the second;
the payload order does not reorder the binders. Every binder is in scope throughout the right-hand side and not outside the declaration.

A reference to the declaration inside its own right-hand side MUST be fully applied. A generic type constructor is not a first-class type;
partial application does not exist.

An application of a generic type MUST supply exactly one argument per binder. Too few or too many arguments is an error. An argument is an
arrow-free type atom; a composite argument is parenthesized (Clause 6.9.4). Application is by juxtaposition with no commas, and the chain stops
at a comma or an arrow (Clause 6.9.4).

The phantom check sees through `?` positions: a type variable that occurs under an open arrow counts as a use (Clause 7.9.6).
```eko
Pair:  type t u = [t, u]
Value: type t u =
    |First [u, t]
    |Second t
Tree:  type t = Leaf t | Node [Tree t, Tree t, t]

p: Pair Int String = {1, "a"}
v: Value Int Float = First {2.5, 1}
```
A function type is not a declaration form (Clause 6.9.5). A named function type appears in an annotation or a parameter type, not as the
right-hand side of `type`. The `?` phantom rule applies to those positions:

```eko
apply: (f: (t) ?-> _, x: t) ?-> _ = f(x)
```

Here `t` occurs under an open arrow, so it is not phantom; the `?` is fed by the parameter `f` (Clause 7.9.6).

A declared type applied to arguments is a nominal type whose identity includes the arguments (Clause 7.5.1). The open bit runs on the same
machinery (Clause 7.9.6).
### 7.6.3 Function Generalization
The generalization point is the annotated binding, at top level and at block level alike. In a function annotation, every unbound lowercase name
is a type variable of that function, and generalization binds those variables over the annotation.

An unannotated binding is monomorphic; its type is the type of its right-hand side. `:=` does not generalize, and a lambda is always check-only,
so a polymorphic binding MUST use an annotated `=`.

A polymorphic recursive or mutually recursive function is admitted. The annotation supplies the type, and each recursive call MAY use any
instantiation. A body may call itself or a mutually recursive partner at a different instantiation.
A type variable is rigid within its declaration. It is not a unification variable: nothing refines or narrows it after generalization, and one
variable never unifies with another. Instantiation substitutes a variable exactly, at the call site (Clause 7.6.4); no variance, constraint, or
partial order attaches to a variable.
Generalization has no explicit quantifier syntax; the annotation's variables are implicitly generalized. The open bit is generalized at the same
point (Clause 7.9.6). There are no inference holes: `_` in type position is the unit type, never a placeholder (Clause 7.5.4). A binding whose
right-hand side is a lambda MUST carry an annotation; a lambda is always check-only and `:=` carries none.
### 7.6.4 Instantiation
Instantiation replaces each type variable by a concrete type at a use site. Type arguments are never written; the replacement is determined by
unification with the argument types and by the expected type. A variable is bound literally to an argument type; no bounds or coercions apply.

Lambda arguments are checked last. In `List.map({1, 2, 3}, (x): x * 2)`, the list argument fixes `t = Int` before the lambda is checked;
otherwise the lambda parameter would be undetermined. A user-written generic function behaves identically to a standard-library one at the call
site.

The parameter-binding rule (Clause 7.6.5) guarantees that every variable is determined by the parameters or by the expected type. An undetermined
variable is an error.

Constructor application by juxtaposition is check-only and takes its instantiation from the expected type. A bare generic constructor has no
self-anchoring type: `Leaf 1.eq(Leaf 2)` has no unique instantiation and is an error. When one side has a known type, the instance is determined.
A constructor of a non-generic ADT has a unique type and is self-anchoring — legal only when it is the sole visible constructor of its name;
with several, an annotation anchors the reference (Clause 7.4.6).
```eko
Tree: type t = Leaf t | Node [Tree t, Tree t, t]

x: Tree Int = Leaf 1
y := x.eq(Leaf 1)          // legal: the receiver fixes `Tree Int`
z := Leaf 1.eq(Leaf 2)     // error: no unique union anchors the instantiation
```
Different instantiations are different nominal types. `Pair Int String` and `Pair Int Float` do not interchange. A literal lands at the expected
instantiation: `p: Pair Int String = {1, "a"}`.

Instantiation is expanded by monomorphization (Clause 8.7.1). Derived services resolve at the instantiation site (Clause 8.7.2). An expansion
that does not terminate is rejected with E-instantiation-depth (Clause 8.7.3; Annex B). An instantiation MUST be complete; partial application is
an error (Clause 7.6.2).
### 7.6.5 Parameter-Binding Rule
Every type variable of a function annotation MUST occur in at least one parameter type. A variable that occurs only in the result is a
declaration-time error with code E-phantom (Annex B). An occurrence anywhere inside a parameter type satisfies the rule, including inside a
nested function type.
```eko
f: (g: () -> t) -> t = g()            // `t` occurs in parameter `g`
empty: () -> []t = {}                 // error: `t` occurs only in the result
read: (n: Int) -> Option t = None     // error: `t` occurs only in the result
```
The rule is symmetric with the phantom binder ban (Clause 7.6.2): both reject a variable that no value can determine at the declaration line. The
result-only prohibition for open bits is the same rule (Clause 7.9.6). The replacement is an empty list with an expected type, `ys: []Int = {}`.
### 7.6.6 Operations on Type Variables
A value whose type is a type variable supports exactly the following operations.

| Operation | Form | Basis |
|---|---|---|
| equality | `==`, `!=`, `T.eq` | structural derivation (Clause 9.8.4) |
| display | `show`, interpolation | derived `show` (Clause 9.8.1) |
| JSON | `to_json`, `from_json` | derived mapping (Clause 9.9.3) |
| binding, passing, storing, destructuring | language forms | representation (Clause 8.5) |

Ordering is not available. `<` and `T.lt` are not defined on a type variable; sorting requires an explicit comparator (Clause 9.10.1). There are
no bounds, traits, or constraint solving; the table is closed.

At an instantiation, each operation resolves to the concrete derived service or declaration for the substituted type (Clause 8.7.2). An
instantiation with a capability or a function where `eq`, `show`, or the JSON pair is required is rejected by that service's rule (Clauses 9.8.4,
9.9.5).

A type variable does not support arithmetic, concatenation, or indexing. A value of type `t` MAY be bound, passed, stored, captured, and
destructured; these are representation operations (Clause 8.5).
## 7.7 Patterns and Exhaustiveness
A pattern is checked against the type of the scrutinee. The pattern grammar is Clause 6.8; this clause gives the typing and exhaustiveness rules.
### 7.7.1 Pattern Typing
| Pattern | Scrutinee type | Binding |
|---|---|---|
| variable `x` | any `T` | `x: T` |
| wildcard `_` | any `T` | none |
| literal | value-layer `T` | none; literal placed at `T` |
| zero-payload constructor | nominal ADT declaring it | none |
| constructor with payload | nominal ADT; payload type | payload pattern at the payload type |
| tuple `{P, Q, ...}` | tuple of matching arity | component types |
| record `{f}` or `{f: P}` | record with field `f` | field type |
| list `{}`, `{h, ..t}`, `{.., P}`, `{_, ..}` | list type | element and list types |
| or `P \| Q` | same `T` | same names at the same types |

A constructor pattern's constructor MUST belong to the scrutinee's nominal ADT and MUST be visible at the pattern position; a module-qualified
constructor is legal (Clause 7.4.4). A multi-component payload has a tuple type, and the payload pattern has the corresponding tuple shape
(Clause 6.3.5).

A record pattern matches a subset of fields. Every field named in the pattern MUST be a field of the record type; a misspelling is an error, and
unwritten fields are unconstrained. A tuple pattern MUST have the scrutinee's arity. A list pattern is checked against the list element type:
`{}` is the empty list, a rest binds the remaining list, and a fixed-length pattern binds elements. The expected type decides whether a brace
pattern is a tuple pattern or a list pattern, as it decides a composite literal (Clause 6.8.1).

A pattern binder's type is taken from the instantiated scrutinee type, and a constructor payload type is the instantiated payload (Clause 7.6.4).
Within one pattern, a binding name MUST occur at most once; `_` is exempt, and the rule applies separately to each or-alternative (Clause 6.8.6).
Every alternative of an or-pattern MUST bind the same set of names at the same types (Clause 6.8.3).

A literal pattern's literal is placed at the scrutinee type (Clause 5.7.4); an out-of-range numeric literal is E-literal-overflow (Annex B). A
literal pattern is a `String`, an integer, a float, a negative numeric literal, or a zero-payload constructor (Clause 6.8.5); an interpolated
string is not a pattern. A value binding admits only irrefutable patterns: `_`, a variable, a tuple or group pattern of irrefutable patterns, and a
record pattern whose subpatterns are irrefutable. Constructor, literal, list, and or-patterns occur only in `case` arms; a refutable pattern in a
value binding is E-refutable-binding (Annex B). Parameter positions admit an identifier, `_`, or a tuple parameter group, and no other pattern form.
Record,
constructor, list, literal, and or-patterns are rejected there; `<-` inherits the restriction (Clause 7.8.1).

Dictionary patterns, as-patterns, and conjunctive patterns do not exist; a program MUST NOT use them (Clause 6.8.7). A `case` over several scrutinees has one pattern
per scrutinee; each pattern is checked against its scrutinee's type, and the arm matches when every pattern matches (Clause 6.7.2). The arm's patterns bind together
(Clause 6.8.6); a name repeated across them is a name conflict.
### 7.7.2 Exhaustiveness
A `case` MUST be exhaustive. A non-exhaustive `case` is rejected with E-exhaust (Annex B). A `case` with several scrutinees is checked over
the product of their types: an arm covers a combination when each component pattern covers its scrutinee, and the arms must cover every
combination. An arm whose patterns are all `_` or variables covers the whole product.

Coverage is judged by constructors at every depth, not by inhabitance. A wildcard `_` or a variable pattern covers every value of its type.

For an algebraic scrutinee, every constructor MUST be covered. An arm covers a constructor when the pattern's head is that constructor and its
payload pattern covers the payload type. A variable or `_` payload covers the whole payload; a constructor payload is covered recursively; tuple,
record, and list payloads follow the rules below. A final `_` arm makes the `case` exhaustive.

For a tuple scrutinee, a tuple pattern of the scrutinee's arity covers it when each component pattern covers its component type.

For a record scrutinee, a record pattern covers it when every field it names is covered; unwritten fields are unconstrained. A record pattern
alone can therefore cover a record type.

For a list scrutinee, `{}` covers the empty list. A suffix-rest pattern `{h, ..t}` or `{_, ..}` covers every non-empty list. A fixed-length
pattern covers lists of its arity when its element patterns cover the element type. A prefix-rest pattern covers non-empty lists when its
last-element pattern covers the element type. The empty list and the non-empty lists MUST both be covered.

For every other scrutinee type, the `case` MUST end with a `_` arm; a literal pattern alone does not cover the type.

An or-pattern covers what its alternatives cover; each alternative is considered. Exhaustiveness is checked against the instantiated union:
`Result t Never` still requires an `Err` arm; inhabitance is not analyzed. A diverging arm does not discharge exhaustiveness (Clause
7.7.4).

Tuple, record, and list scrutinees are judged by shape; every other non-ADT scrutinee requires a final `_` arm. This is the rule that the
pattern forms of Clause 6.8 and the conformance suite exercise.
### 7.7.3 Guards and Exhaustiveness
An arm MAY carry a guard: a pattern, `if`, a `Bool` expression, `->`, and a result (Clause 6.7.3). The pattern binds before the guard is checked.
The guard MUST have type `Bool` and MAY use the pattern's bindings. If the pattern matches and the guard is false, matching continues with the
next arm.

A `case` containing a guarded arm MUST end with an unguarded arm. Exhaustiveness is judged on the unguarded arms only; a guarded arm does not
contribute coverage.

A guard occurs only at arm level (Clause 6.7.3). Its world bit is ORed into the enclosing context like any other expression (Clause 7.9.4). A
`case` with no guards follows Clause 7.7.2.
### 7.7.4 Never and Arm Typing
Every arm of a `case` MUST have the same type. A `Never` arm adapts to the type of the other arms (Clause 7.5.4); if every arm is `Never`, the
`case` is `Never`. The scrutinee and all guards contribute their bits to the enclosing context; `case` has no separate bit rule (Clause 7.9.4).

An arm is always a pattern followed by `->` and an expression. A bare diverging expression is not an arm (Clause 6.7.1); it is a syntax error. A
`Never` arm does not discharge exhaustiveness: its pattern MUST still cover its constructor, or the case MUST end with `_` (Clause 7.7.2).

The `if` expression requires a `Bool` condition. Its two branches MUST have the same type. An omitted `else` is `else: _`, so a then-branch
without an `else` MUST have type `_` or `Never` (Clause 6.7.4). `if` is sugar for a `case` over `Bool`; its condition and both branches
contribute bits by OR (Clause 7.9.4).

A declared return position is an adaptation position for `Never`:
```eko
die: (msg: String) -> Never = panic(msg)

value: Int = if True: 1 else: die("no")
```
Unreachable code after a call of type `Never` is legal and produces W-unreachable, not an error (Clause 8.4.4). A `Never` result does not set the
world bit; `panic` and `Option.expect` are pure (Clause 7.9.2).
## 7.8 Calls and Operators
A call is checked against the callee's function type. An operator is a call in notation form (Clause 6.5.4).
### 7.8.1 Call Typing
A call `f(a1, ..., an)` requires `f` to have a function type with exactly `n` parameters. There are no optional parameters, default arguments, or
variadic parameters; arity MUST match exactly.

Each argument is checked against the corresponding parameter type at an argument position. The argument position admits shape adaptation (Clause
7.5.5), `Never` adaptation (Clause 7.5.4), and bit adaptation (Clause 7.9.5).

The type of the call is the callee's result type. The expected type at the call site MAY determine type variables of the callee (Clause 7.6.4).
Type arguments are never written; the callee's variables are instantiated by unification with the argument types and the expected type.

Lambda arguments are checked last (Clause 7.6.4). A callee that is not a function type is an error. A call through a value of function type is a
call; holding the value sets no bit (Clause 7.9.2). The world bit of a call is the bit of the callee's arrow: `->` contributes pure, `!->`
contributes world-touching, and `?->` contributes its instantiation (Clause 7.9.4). A call in receiver form is resolved by Clause 7.4.3 and typed by
Clause 7.8.2.

An inverted binding `p <- f(a...)` is the call `f(a..., (p): rest)` (Clause 6.3.4). The bound pattern's type is the callback's parameter type,
and the admitted pattern forms are those of a parameter position (Clause 7.7.1). The zero-argument form `<- f()` passes a zero-parameter
callback. The call MUST have a last callback parameter; `<-` is not applicable to a `Never`-returning call without one.

An `extern` declaration is checked as a binding of its declared function type. Its signature MUST satisfy Clause 10.2.3: every parameter and
result type is in the marshalling whitelist, the signature is monomorphic,
and variadic parameters are absent. The declaration's own arrow MUST
be `!->` or `!`, or `->` when it carries `Pure` (Clause 10.2.1). A callback parameter carries its own bit. A `C.In T` parameter requires `T` to
be a `ctype` record (Clause 10.3.4).

A violation of the whitelist, monomorphism, or variadic rule is E-extern-type. A violation of the arrow rule is
E-extern-arrow (Clause 10.2.3). A call to an extern contributes the bit of its declared arrow (Clause 7.9.2). Signature overloading does not exist; a callee name denotes
one binding (Clause 7.4.6).
### 7.8.2 UFCS Typing
The identity `a.f(b, c) ≡ f(a, b, c)` is specified in Clause 6.5.2. This clause specifies its typing.

Resolution selects `f` by the stations of Clause 7.4.3. The receiver supplies the first argument, and the remaining arguments map left to right.
The call is then typed exactly as the direct call.

The receiver and the direct first argument occupy the same argument position; shape, `Never`, and bit adaptation apply to the receiver as to any
argument. A qualified receiver call `a.Mod.f(b)` is identical to `Mod.f(a, b)`; module qualification affects resolution only. An operator is the
same call: `a + b` is `Int.add(a, b)`, hence `a.add(b)` (Clause 7.8.3).

A receiver chain types each link independently. The receiver of a link is the result of the previous link, and the static type fixes the
namespace station (Clause 7.4.3). A fully qualified name in argument position, as in `.map(String.trim)`, is a name reference, not a call (Clause
6.5.3). A bare `x.f` without a call is a field access (Clause 7.4.3). There is no dynamic dispatch; the static type of the receiver selects the
member (Clause 7.5.1). A member call's result is the member's result type, and the first parameter is not repeated in the argument list.
### 7.8.3 Operator Typing
The operator set is closed; no user-defined operator exists (Clause 6.10). Each operator has the precedence and associativity of Clause 6.5.5.

An operator is notation for a type-namespace member (Clause 6.5.4). Typing resolves the member in the namespace of the operand type; overloading
is namespace resolution, not a second dispatch mechanism. Both operands MUST have the same type, and no implicit conversion is applied.

| Operators | Operand type | Result |
|---|---|---|
| `+ - * / %`, unary `-` | numeric domain | same numeric type |
| `== !=` | value layer | `Bool` |
| `< <= > >=` | value layer with order | `Bool` |
| `&& \|\|` | `Bool` | `Bool` |
| `++` | list, `String`, `Bytes`, dictionary | same type |
| `.. ..=` | `Int`, `UInt`, `Char` | `Range t` |

`==` and `!=` are defined on exactly the value layer: `Int`, `UInt`, `Float`, the machine words, `Bool`, `Char`, `String`, and `Bytes`. There
`a == b` is identical to `a.eq(b)`; `eq` and `ne` are compiler-owned (Clause 9.8.4). For every other type the operator does not exist; equality
is the member `T.eq`, called as `x.eq(y)` or `T.eq(x, y)`, and `T.ne` is its negation. Both operands MUST have the same type: `1 == 1.0` and
`UInt` versus `Int` are errors. A value whose static type is a type variable MAY use `==` and `!=`; the operation is the derived `eq` selected
at instantiation (Clause 7.6.6).

`T.eq` is derived structurally on demand. Records and tuples compare field by field in declaration order with short-circuit; ADTs compare
constructors and then payloads recursively; containers compare element by element; each component compares through its own `eq`. A declaration
`T.eq` in the declaring module replaces the derivation (Clause 7.4.5). Derivation halts at capabilities and functions, recursively: a type with
such a field has no `eq`, and equality, keying, or assertion over it is E-no-eq (Clause 9.8.4). `Float.eq` follows IEEE 754; `NaN == NaN` is
`False`.

A bare generic constructor has no self-anchoring equality; the instance is determined when one operand has a known type (Clause 7.6.4). A
constructor of a non-generic ADT is self-anchoring.

`<`, `<=`, `>`, and `>=` follow the shape of equality on the value layer with an order: the numeric types, the machine words, `Char` by code
point, `String` lexicographically, and `Bytes` by byte order. Operands MUST have the same type and the result is `Bool`; `Bool` has no ordering.
An ADT derives `T.lt` by declaration order with recursive payloads; the member backs no operator, and the call is `a.lt(b)`. Records have no
derived ordering; use `sort_by` with an explicit comparator (Clause 9.10.1), and a hand-written `T.lt` is an ordinary member.

Chained comparisons are an error (Clause 6.5.5); `not` is a function and `!` is not a logical operator (Clause 6.5.4). Range endpoints MUST have
the same type, `Int`, `UInt`, or `Char`; the result is `Range t`. A `Float` endpoint and an unbounded range do not exist (Clause 6.5.6).
### 7.8.4 Arithmetic
Arithmetic comprises the binary operators `+`, `-`, `*`, `/`, `%` and unary `-` (Clause 6.5.4). Operands are the domain numeric types `Int`,
`UInt`, and `Float`; the machine words have no arithmetic (Clause 10.3.1). Both operands MUST have the same type, and the result has that type.
Unary `-` follows the operand type's namespace. No implicit numeric conversion is applied; `1 / 2.0` is an error. A numeric literal is placed by
the expected type (Clause 5.7.4), and a value outside the target range is E-literal-overflow (Clause 5.7.1).

`/` and `%` truncate toward zero and are paired: `-7 / 2` is `-3`, and `-7 % 2` is `-1`. The floor family is `Int.div` and `Int.mod` (Annex C). `UInt` values are non-negative, so the distinction vanishes. `Float` `%` uses the same truncation basis.

Overflow and division by zero panic at run time (Clause 8.4.2); `Int.min / -1` overflows. This is not a static error, and a panic sets no world
bit (Clause 7.9.2). Wrapping arithmetic is the explicit `Int.wrapping_*` and `UInt.wrapping_*` family (Clause 9.6.6). Bitwise operations are
functions of the `UInt` family (Clause 9.6.5). `Int.pow` and `Float.pow` are functions; no exponentiation operator exists (Clause 6.10).
Conversions between numeric types are explicit and return `Option` where the target range is narrower (Clause 9.6.2).
### 7.8.5 Logical Operators
`&&` and `||` are sugar for `if` (Clause 6.5.4). Both operands MUST have type `Bool`, and the result is `Bool`. They are lazy: the right operand
is evaluated only when the result requires it (Clause 8.1.2). There is no truthiness coercion; `True` and `False` are the only `Bool`
constructors.

The world bit is transparent: the operands contribute their bits by OR to the enclosing context (Clause 7.9.4). `not` is an ordinary function,
written `b.not()` or `Bool.not(b)`; prefix `!` is the world-bit marker, not negation. `&&` and `||` associate to the left (Clause 6.5.5). `&` and
`|` are not logical operators; `|` in pattern position is the or-pattern separator (Clause 6.8.3). A condition of `if` and a guard MUST be
`Bool` (Clause 6.7.4).
### 7.8.6 Concatenation
`++` is defined on `[]t`, `String`, `Bytes`, and `[k]v`; both operands MUST have the same type, and the result has that type. For a list, both
operands MUST be lists with the same element type; `++` appends and does not wrap elements, as in `prefix ++ {x} ++ suffix`. For a dictionary,
both operands MUST have the same key and value types; `++` merges, and the right operand wins on a duplicate key (Clause 9.10.2). For `String`
and `Bytes`, `++` concatenates. Characters do not concatenate with strings: `'a' ++ "bc"` is an error. Tuples and records do not support `++`; a
partial record literal is legal only with `with` (Clause 9.8.6).

`++` has the same precedence as `+` and `-` and associates to the left (Clause 6.5.5). No implicit conversion of elements or keys is applied.
## 7.9 Capabilities and the World Bit
A capability value carries authority for a class of world contact. The world bit records whether a call may use a capability (Clause 4.5).
### 7.9.1 Capability Types
Capability types are built-in nominal opaque types. They are unforgeable and copyable. A program MUST NOT fabricate a real capability;
construction of a real capability belongs to the host and to the parameters of `main` (Clause 7.1.3). A program MAY construct a derived
capability purely (Clause 9.14.5).

| Capability | Surface | Grant | Sets bit |
|---|---|---|---|
| `Out` | session write | birth-granted | yes |
| `In` | session read | birth-granted | yes |
| `Fs` | file system | birth-granted | yes |
| `Env` | environment variables | birth-granted | yes |
| `Args` | startup arguments | birth-granted | yes |
| `Clock` | time | opt-in | yes |
| `Rand` | entropy | opt-in | yes |
| `Proc` | process control | opt-in | yes |
| `Conc` | parallel resources | opt-in | no |

Birth-granted capabilities are supplied by the host at startup. Opt-in capabilities are requested explicitly by naming them in `main`'s parameter
list. A capability type MUST NOT occur twice in that list; the list is the whole operating-system surface, and no real capability outside it is
reachable (Clause 10.1.2). There is no ambient I/O: no global `print`, `println`, `input`, or `File.*` binding exists, and world contact is
authorized through `main`'s parameters or a pure derivation (Clause 9.14.5). The members of the capability namespaces are declared by the
capability module (Clause 9.1.1).

A capability value MAY be copied, passed, returned, stored, and captured. Copying is delegation. Holding or capturing a capability sets no bit
(Clause 7.9.2).

A capability has no `eq`; equality, keying, or assertion over it is E-no-eq (Clause 9.8.4). `show` prints `<cap>` (Clause 9.8.3).

A capability is necessary but not sufficient for world contact; bit assignment is per operation (Clause 7.9.2). `Conc` authorizes parallel
resources and sets no bit; `Task.join` and `Task.cancel` are pure (Clause 8.9.2). Derived capabilities are constructed by pure combinators and
set no bit at construction or holding (Clauses 9.12.5, 9.14.5); the attenuation combinators only narrow the grant. A derivation constructs a
capability value, never authority: the grant of a derived capability is contained in the grants of the capabilities it consumes, and a
zero-input derivation yields a zero-grant value. There is no capability for native code; the native track is authorized by declaration and
linking (Clause 10.1.2).
### 7.9.2 The World Bit
A function type carries a world bit: pure (`->`), world-touching (`!->`), or open (`?->`) (Clause 7.9.3).

The bit of a body is the OR of the bits of all call positions in the body. Only calls contribute; referencing a function, storing a capability,
passing a function value, and capturing a capability set no bit.

| Left | Right | Result |
|---|---|---|
| pure | pure | pure |
| pure | world-touching | world-touching |
| world-touching | any | world-touching |
| open | pure | open |
| open | world-touching | world-touching |
| open | open | open |

A call that observes or changes the world sets the bit; a pure derivation does not. `Out.println` and `Clock.now` set the bit; `Out.tee`
and `Fs.root` are pure.

At an annotated binding, the binding's own bit is the top-level arrow of the annotation when the annotation is a function type, and pure otherwise. The
own bit MUST equal the OR of the call bits of the body; a missing `!` and a surplus `!` are both errors with code E-world-bit (Annex B).
Independently, the body is checked against the annotation by type identity; bit adaptation applies only at the positions of Clause 7.9.5, never at a
declaration, and never in the reverse direction. A pure function value is therefore bound under its exact type and adapts at its use positions.

The bit lives on the arrow and does not infect data. `logged := List.map(xs, greeter)` requires an enclosing `!`, but `logged: []String` is an
ordinary value; there is no world-touching list type.

`?` appears only on a dispatcher that is pure if its callback is pure. A higher-order function that is itself world-touching uses `!` and no `?`:
`Fs.with_file` is `!->`, and its callback is written `(File) !-> a`.

All `?` in one signature are the same open bit. OR absorbs on the bit axis, so several bit variables would be indistinguishable; `?` is anonymous
and no bit variables exist.

An `extern` declaration sets the bit. Its arrow is `!->` unless it carries `Pure`, which makes it `->` (Clauses 10.2.1, 10.2.2).

The bit is part of type equality (Clause 7.5.1) and is fully erased at run time. It has no calling-convention consequence. The bit of a call
through `?->` is its instantiation (Clause 7.9.6); through `->` it is pure; through `!->` it is world-touching. `panic`, `Option.expect`,
`Option.unwrap`, division by zero, and overflow are pure (Clause 9.2.3). Bit diagnostics are always errors, never warnings (Annex B).
### 7.9.3 Arrow Forms
| Form | Meaning |
|---|---|
| `(a) -> b` | pure function from `a` to `b` |
| `(a) !-> b` | world-touching function from `a` to `b` |
| `(a) ?-> b` | open function from `a` to `b` |
| `(a)` | `(a) -> _` |
| `(a) !` | `(a) !-> _` |
| `(a) ?` | `(a) ?-> _` |

`()` is the zero-parameter function type and abbreviates `() -> _`. A unit result is written `-> _` (Clause 6.9.3).

`!` and `?` are written joined to the arrow as `!->` and `?->`. A lone marker occurs only in the elided forms at end of line (Clause 6.9.3).
Arrows nest without parentheses: `(Out) -> (String) !-> String` is pure and returns a world-touching function; `(Out) !-> (String) -> String` is
world-touching and takes a pure function; `(Out) !-> (String) !-> String` is world-touching in both positions. Nothing follows `!` or `?`: no
label, variable, or annotation (Clause 6.10).

The arrow form is part of the function type's identity (Clause 7.5.1). `?` is an anonymous parameter and is instantiated at the call site (Clause
7.9.6). An extern declaration's own arrow MUST be `!->` or `!`, or `->` with `Pure`; an open arrow is not admitted (Clause 10.2.1).
### 7.9.4 Bit Inference
A lambda carries no bit annotation. It is checked against an expected function type, which supplies the parameter and result types; its bit is
the OR of the calls in its body. A pure lambda adapts to a world-touching function type at an adaptation position, while a world-touching lambda
does not adapt to a pure position (Clause 7.9.5). A lambda passed as the callback argument of an open arrow instantiates the open bit at its own
body bit. The bit of a named binding's body is the OR of the calls in that body. The declared bit MUST equal the body bit, and a mismatch in
either direction is E-world-bit (Clause 7.9.2).

An inverted binding `<-` has no separate bit rule. The remaining block is the callback body; its bit is inferred as for any lambda, and the
dispatcher instantiates its open bit from it (Clause 6.3.4). A dispatcher that is itself world-touching has no open bit; its `!` absorbs the
callback's bit by OR.

`if` and `case` are bit-transparent: the condition or scrutinee, the guards, and every branch or arm body contribute their bits by OR (Clause
8.3). `&&` and `||` are bit-transparent (Clause 7.8.5).

Holding is quiet. A function value MAY be stored, passed, or returned without setting the bit; only calls contribute. A pure factory may
therefore return a `!` closure:
```eko
mk_logger: (out: Out) -> (Int) !-> Int =
    (n): Out.println(out, show(n)); n + 1
```
A named binding with an elided arrow, `(a) !` or `(a) ?`, follows the same rules with `_` as the result type.
### 7.9.5 Bit Adaptation
A pure function type adapts to a world-touching function type with the same parameters and result. `(a) -> b` is accepted where `(a) !-> b` is
expected; the reverse does not hold.

Bit adaptation applies at four positions: an argument, an operand, an expected type, and a collection element. At an annotated binding, bit
adaptation does not apply; the declared bit and the body bit MUST be equal (Clause 7.9.2). Adaptation is for values in positions, not for
declarations.

Bit adaptation is one-directional and is not a general subtyping relation. It does not create an ordering among function types; only this single
step is admitted. Adaptation is judged on the whole function type at the position. A `?->` parameter is instantiated from the argument's bit: a
`->` argument gives a pure instantiation, and a `!->` argument gives a world-touching one (Clause 7.9.6).

Bit adaptation is not subject to the position discipline of shape adaptation; collection elements are adaptation positions. It does not infect
data; a value of type `[]String` remains an ordinary value.
```eko
apply: (f: (Int) !-> Int, n: Int) !-> Int = f(n)
inc: (Int) -> Int = (n): n + 1

use: (n: Int) !-> Int = apply(inc, n)
```
A world-touching function value does not adapt to a pure position. A pure callback adapts into a `!->` parameter such as the callback of
`Fs.with_file` (Clause 9.12.4) or the observer of `Out.tee` (Clause 9.14.5).
### 7.9.6 Open Bits
`?` is an anonymous parameter on the bit axis. It is generalized at the annotated binding and instantiated at the call site from the bits of the
arguments. The mechanism is the same as for a type variable (Clause 7.6), with two differences: the open bit is anonymous, and its
instantiation is a bit. All `?` in one signature are the same open bit; there are no bit variables, and nothing follows `?`.

The open bit is allowed in exactly two positions: the annotated binding's own arrow, and arrows inside that signature's parameter types. It MUST
NOT appear in a result type, nested in a result type, in a container element, or in a record field. Each forbidden occurrence is
E-open-bit-position (Annex B).

An own `?` arrow MUST be fed by a parameter: at least one parameter type MUST contain a `?` arrow. If no parameter feeds the open bit, the
declaration is rejected with E-open-bit-unfed (Annex B). The rule is judged after expansion of the parameter types. When a declaration is rejected
with E-open-bit-unfed, bit reconciliation of that declaration (Clause 7.9.2) is skipped: one defect yields one diagnostic.

When a parameter type contains `?`, the corresponding argument supplies the instantiation: a `->` argument instantiates the bit pure, and a `!->`
argument instantiates it world-touching. With several such parameters, their bits are ORed. Lambda arguments are checked last, so the callback's
bit is known when the open bit is instantiated (Clause 7.6.4). A `?` in the result is rejected by the position rule, whether or not a parameter
feeds another open bit; this mirrors the result-only variable rule (Clause 7.6.5). A type variable under a `?` arrow counts as a use
(Clause 7.6.2), and an open arrow MUST NOT appear on an extern declaration (Clause 10.2.1).
A dispatcher signature has the form `List.map: ([]t, (t) ?-> u) ?-> []u`.
```eko
inc: (n: Int) -> Int = n + 1
xs: []Int = {1, 2, 3}

ys: []Int = List.map(xs, inc)   // the argument is pure, so this call is pure
```
A `?` arrow with no feeder:
```eko
count: (xs: []Int) ?-> Int = List.length(xs)
// error: E-open-bit-unfed: no parameter contains `?`
```
A world-touching body under an open declaration, and a pure body under an open declaration:
```eko
loud_apply: (out: Out, f: (Int) ?-> Int, n: Int) ?-> Int =
    Out.println(out, "call")
    f(n)
// error: E-world-bit: the body bit is world-touching, not open

twice_fast: (f: (Int) ?-> Int, n: Int) -> Int = f(f(n))
// error: E-world-bit: the body bit is open, not pure
```
A `?` in a forbidden position:
```eko
handlers: [](Int) ?-> Bool = {(n): n == 0, (n): n < 0}
// error: E-open-bit-position: `?` is in a container element
```
### 7.9.7 Soundness
**World soundness.** If `f: (a) -> b` in safe Eko, then calling `f` does not touch the world, modulo audited `Pure` declarations on the native
track (Clause 10.1.4).

The argument is structural.

1. The bit originates only in an extern call or in a call of a world-touching function value, and every world-touching function value closes,
   transitively, over an extern declaration or over a bundle unwrapped from a capability; a real bundle is host-constructed (Clause 10.8). An
   extern is a visible declaration resolved at the link layer; `Pure` is an audited exception.
2. Capabilities cannot be forged. A real capability is constructed only by `main`'s parameters or by the host; a pure derivation yields a
   restricted or deterministic substitute (Clauses 7.9.1, 9.14.5).
3. A closure's bit is on its arrow type, and its body is checked against that annotation (Clause 7.9.2).
4. Bit adaptation is one-directional: a pure function may flow into a world-touching position, never the reverse (Clause 7.9.5).

**Corollary (top-level purity).** Top-level initialization is pure. No capability value is in scope at top level, so a bit-setting call is not
typeable there (Clause 7.1.2).

The absence of `!` does not promise freedom from panic, termination, blocking, or cancellation. A pure function MAY panic (Clause 8.4.2), diverge
(Clause 8.3.5), block, or be cancelled (Clause 8.9.4). The bit governs world contact only; partiality is an independent dimension and is not
tracked.

`Pure` is a trusted-base assertion; a false `Pure` is outside the language guarantee (Clause 2.6).
### 7.9.8 Terminology

The terms of this layer are fixed as follows.

| Term | Spelling | Meaning |
|---|---|---|
| unit-returning function type | `(t)` | a parameter group with the arrow elided; the result is `_` |
| world marker | `!` | the arrow definitively touches the world |
| open marker | `?` | the bit is a parameter, instantiated from callbacks |
| world bit | bit | the honesty axis as a whole |
| world-touching arrow | `!->` | an arrow that may touch the world |
| open arrow | `?->` | an arrow whose bit is a parameter |

The vocabulary of effect systems is not used in this document or in diagnostics. In particular, "effect system", "effect row", and "effect
handler" do not name any Eko mechanism. The ordinary English phrase "takes effect" is not affected.

Use "world bit", "sets the world bit", "open bit", "bit annotation", "world-touching", and "touches the world" consistently.

### 7.9.9 Exclusions

The following mechanisms do not exist. Their absence is normative.

- **No label sets.** A label set that mirrors capabilities would duplicate the capability value and require permanent synchronization.
  Capability names are the only authority vocabulary.
- **No row polymorphism, row algebra, or row extension.** There is one bit per arrow and nothing to extend.
- **No algebraic effects and no handlers.** World contact is not handled away; errors are values and partiality is not tracked.
- **No IO monad.** The direct style is preserved, and higher-order functions are not made monadic.
- **No exceptions.** Recoverable failure is `Result`; invariant violation is `panic` (Clause 9.2).
- **No user-defined labels.** Labels would be a path to a second authority vocabulary.
- **No `Rights` type.** Attenuation is the pure combinators of Clause 9.12.5.
- **No ambient I/O.** Authorization flows only from `main` (Clause 7.1.3).
- **No FFI capability.** Native code is authorized at the link layer (Clause 10.1.2).
- **Bit diagnostics are errors, never warnings.** A missing or surplus `!` is rejected (Clause 7.9.2).

## 7.10 Static Errors
A conforming compiler MUST reject every program that violates a static rule and MUST NOT accept it (Clause 2.2). The table lists the categories
of required rejection and the clause that owns each. Diagnostic codes are listed in Annex B. Where Annex B defines a code for a condition, the
diagnostic MUST use that code; where none is defined, the diagnostic MUST identify the violated clause (Clause 2.7).

| Category | Required rejection | Owning clause |
|---|---|---|
| Lexical and syntactic | invalid text, layout, token, or literal; a construct outside the grammar | Clauses 5, 6 |
| Declarations and naming | a non-declaration `type` right-hand side; a bad `ctype` field; a wrong case class; a reserved word as identifier | Clauses 6.9.5, 10.3.3, 5.5.2 |
| Program structure | no entry module; a non-public or `_main` entry; an invalid `main` signature | Clause 7.1.3 |
| Modules | an unresolved import path; a user module under `std/`; an import cycle | Clauses 7.2.2, 7.2.4 |
| Visibility and names | a private name outside its module; a private type in an exported signature; split variants; an unbound name; a failed station; a foreign namespace member; a same-layer conflict | Clauses 7.3.1–7.4.6 |
| Types | a nominal mismatch; a failed shape adaptation; a literal without expected type; an unknown field or wrong arity; a `Never` adaptation inside a function type; `-> ()`; a non-conforming annotation | Clauses 7.5.2–7.5.7 |
| Generics | a phantom binder; an arity mismatch; partial application; a variable heading an application; a result-only or undetermined variable; a non-terminating instantiation | Clauses 7.6.1, 7.6.2, 7.6.5, 8.7.3 |
| Patterns and exhaustiveness | an unknown field; a duplicate or mismatched binder; a forbidden parameter pattern; a non-exhaustive `case`; a guarded case without a final unguarded arm | Clauses 7.7.1–7.7.3 |
| Calls and operators | a non-function callee; an arity or argument mismatch; a `<-` without a callback; mismatched operands; equality, ordering, or `++` outside its domain; a chained comparison; a `Float` range | Clauses 7.8.1–7.8.6 |
| Arithmetic | an implicit numeric conversion; a literal outside its target range | Clauses 7.8.4, 5.7.1 |
| Capabilities and world bit | a non-capability or duplicated `main` parameter; a capability construction; a missing or surplus `!`; an unfed or forbidden `?` | Clauses 7.9.1, 7.9.2, 7.9.6 |
| Extern | a signature outside the whitelist; a type variable or variadic in an extern; an open arrow; a reserved symbol | Clauses 10.2.3, 10.3.5, 10.6 |
| Derived service and test | equality, keying, or assertion over a type whose `eq` halts; a JSON instantiation without a default form; a duplicate or misplaced test | Clauses 9.8.4, 9.9.3, 9.14.1 |

Each row is an error diagnostic; none is a warning. The warning vocabulary is closed by Annex B.4 and does not reject a program.

Warnings do not change typing. W-unreachable, W-unused, W-redundant-import, and W-deprecated are specified in Annex B.4.

Bit violations are always errors (Clause 7.9.2). A program that produces an error diagnostic is not a conforming program (Clause 2.2). An
implementation MAY report additional warnings, but it MUST NOT reject a conforming program (Clause 2.4). Rejection at link time or run time is
not a substitute for a required static rejection.
