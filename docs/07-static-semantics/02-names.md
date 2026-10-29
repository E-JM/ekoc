# 7. Static Semantics

## 7.4 Names and Resolution

Names are resolved statically. There is no dynamic name lookup and no runtime
namespace.

### 7.4.1 Name Spaces

| Name space | Members | Case class |
|---|---|---|
| Identity | declared types, constructors, type namespaces | PascalCase |
| Binding | value bindings, parameters, function names, type variables | snake_case |
| Module | module stems, import paths | snake_case |
| Field | record fields, per record type | checked against the declaration |

A name's spelling determines its space: PascalCase carries identity, and
snake_case is a binding or a module path (Clause 5.5.2). A name in the wrong
case class is `E-name-case` (Clause 5.5.2).

A constructor and a type MAY share a spelling. `Age: type = Age UInt` declares
the type `Age` and the constructor `Age`; type position denotes the type,
while value and pattern positions denote the constructor.

In type position, a bare lowercase identifier (after any privacy prefix) is a
type variable of the enclosing annotation or declaration, a bare PascalCase
identifier is a declared type, and a dotted name is a module path (Clauses
5.5.2, 6.9.1, 7.4.4). In expression position, a lowercase name is a binding or
a module, and a PascalCase name is a constructor; a namespace member needs a
receiver or a qualified head (Clause 7.4.3).

Field names belong to their record; they do not form a global namespace. `_`
is positional (Clause 7.3.1). Reserved and contextual words follow Clause 5.6.

### 7.4.2 Scopes

Scopes are lexical and nest: the import scope, the module top level, each
block, each lambda body, and each `case` arm with its guard.

| Position | Lookup order |
|---|---|
| Expression head | parameter, local binding, file top-level binding, imported module |
| Type position | type variable of the enclosing annotation or declaration, declared type in unqualified scope, module-qualified type |
| Pattern constructor | visible constructor, unqualified or module-qualified |
| Receiver | the member resolution stations of Clause 7.4.3; lexical bindings are not consulted |

A binding in an inner scope shadows a binding of the same name in an outer
scope. An import and a file top-level binding share one layer, so their
collision is `E-name-conflict` (Clause 7.4.6). A local binding MAY shadow an
imported module (Clause 7.4.4).

`:=` introduces a fresh binding that shadows an earlier binding of the name;
it does not mutate the earlier value (Clause 8.2.2). A closure captures the
value held at capture time (Clause 8.2.3).

A top-level binding is visible throughout its module, including its own
right-hand side. A local binding is visible from its binding line to the end
of the enclosing block; a block-level self-reference is an error (Clause
8.2.1).

A pattern binding is visible in its arm's guard and result (Clause 7.7.1), and
a lambda parameter in its body. A type variable's scope is the whole
annotation (Clause 7.6.1); a declaration's binders are local to its right-hand
side (Clause 7.6.2). A name MUST be bound in an enclosing scope or imported;
an unbound name is an error.

### 7.4.3 Member Resolution

A call in receiver form, `x.f(args)`, resolves `f` at four stations in order.
The first station with a hit resolves the member; stations are not merged.

| Station | Searches | Form |
|---|---|---|
| 1. Record field | the field `f` of the record type of `x` | `x.f`; `x.f(args)` when the field is a function value |
| 2. Type namespace | the namespace of the static type of `x` | `x.f(args)` |
| 3. Current module | top-level free functions of the current module whose first parameter type is the static type of `x` | `x.f(args)` |
| 4. Module path | public free functions of the named module, or the namespace of a named type in it, whose first parameter type is the static type of `x` | `x.Mod.f(args)`, `x.Mod.Type.f(args)` |

The namespace of a type is named after the type's head: `[]t` maps to `List`,
`[k]v` to `Dict`, `Option t` to `Option`, and `Result t e` to `Result`. A
declared type uses its own name; an anonymous tuple or record type has no
namespace.

Station 2 includes the derived service members of Clause 9.8.7. When the
static type is a type variable, station 2 resolves only the operations of
Clause 7.6.6, and the concrete service is selected at instantiation (Clause
8.7.2).

Station 3 is the only station at which a module can define a receiver-form
function on a foreign type. A top-level function of a different module is not
found there and requires station 4. Station 4 applies when the name is a
multi-segment qualified name; the module segment names an imported module or
the current module.

The receiver position never consults lexical bindings. Station 2 precedes
station 3, so a namespace member resolves over an equally named function in
the current module; this is ordered resolution, not a name conflict (Clause
7.4.6).

If no station has a hit, the call is rejected, and the diagnostic MUST list
the readings available at the receiver position, including a module reading
when the name also denotes a module. If a name is both a field and a function
reachable at a later station, station 1 wins for field access; for a call
whose callee is not a function value, the diagnostic MUST report both
readings, and the author selects the function by a fully qualified call
(Clause 7.4.4).

A bare `x.f` that is not a call is a field access. A function member requires
a call. Resolution depends only on the static type of the receiver; there is
no dynamic dispatch. The receiver is the member's first parameter, and the
call is typed as Clause 7.8.2.

### 7.4.4 Qualified Names

A qualified name has the form `Mod.f` or `Mod.Type.f` at the expression head,
`x.Mod.f(args)` or `x.Mod.Type.f(args)` at a receiver link, `Mod.Type` in type
position, or `Mod.Ctor` at constructor value or pattern position. The module
prefix is exactly one segment, not an import path; a qualified name reference
with more than one head segment is `E-unbound-name` (Annex B).

| Position | Form | Denotes |
|---|---|---|
| Expression head | `Mod.f(args)` | free function `f` of module `Mod` |
| Expression head | `Mod.Type.f(args)` | namespace member `f` of type `Type` in `Mod` |
| Receiver link | `x.Mod.f(args)` | `Mod.f(x, args)` (Clause 6.5.2) |
| Receiver link | `x.Mod.Type.f(args)` | `Mod.Type.f(x, args)` |
| Type position | `Mod.Type` | type `Type` declared in `Mod` |
| Constructor | `Mod.Ctor` | constructor `Ctor` declared in `Mod` |

The bare spelling is the normal form for types and constructors. The qualified
spelling is legal at every name position and is the disambiguation device for
same-named types and constructors (Clause 7.4.6).

A module's own name is in scope inside itself. The prefix MUST denote an
imported module or the current module. A local binding at the expression head
shadows a module of the same name; the receiver form is unaffected (Clause
7.4.2).

A qualified name resolves only public names (Clause 7.3.1) and does not bypass
namespace ownership (Clause 7.4.5). `/` never appears in a qualified name;
module paths occur only in imports (Clause 7.2.2).

A fully qualified name MAY appear at four positions in a chain:

| Position | Form | Basis |
|---|---|---|
| Chain head | `Mod.f(x)` | the direct identity |
| Argument | `.map(Mod.f)` | an ordinary name reference |
| Element-level link | `.traverse((l): Mod.f(l.split(",")))` | a direct form inside a callback |
| Accumulated-value link | `.Mod.f()` | the extended identity |

A qualified name in argument position is a name reference, not a call (Clause
6.5.3). `a.Mod.f(b)` is identical to `Mod.f(a, b)` (Clause 6.5.2); its typing
is Clause 7.8.2.

### 7.4.5 Type Namespace Ownership

A type namespace belongs to the module that declares the type. It is keyed by
the declaring module, not by the calling file, and it travels with the type's
visibility.

A top-level binding whose head is `TypeName.name` declares a member of
`TypeName`'s namespace. It is legal only in the module that declares
`TypeName`; anywhere else it is `E-namespace-owner` (Annex B). A qualified
spelling does not bypass the rule: `compiler.Compiler.temp_fn` outside
`compiler` targets the same foreign type and is the same error.

An import is a read-only grant. Nothing written in an importing module adds
to, removes from, or overrides the imported namespace or interface. A module
that wants a service on a foreign type declares a free function: reachable at
the receiver through station 3 inside its module, and elsewhere through
`temp.temp_fn(c)` or `c.temp.temp_fn()` (Clauses 7.4.3, 7.4.4).

Same-named types MAY coexist. When two visible types share a name, from two
imports or from an import beside a local declaration, the unqualified spelling
is an error at its use site, and each is written module-qualified. A module's
own name is in scope inside itself (Clause 7.4.4).

A namespace is reachable through values. Station 2 resolves through the static
type of the receiver whenever the type is in the compilation unit,
independently of the calling file's imports; imports govern names, not the
receiver form (Clause 7.4.3).

Derived services attach to every type namespace as defaults. An override is a
namespace declaration and obeys ownership: it is legal only in the module that
declares the type (Clause 9.8.2).

### 7.4.6 Name Conflicts

A name conflict is a collision within one layer. The following are conflicts:

- two top-level bindings of one module with the same name — a namespace
  binding is keyed by its full head, so `Int.add` and `Float.add` coexist, and
  only the same head conflicts;
- an import and a file top-level binding with the same name;
- two imported modules with the same stem, judged over the compilation unit's
  import closure and not over one file's import list (Clause 7.2.1);
- a type variable with the same name as an imported module;
- two parameters of `main` with the same capability type (Clause 7.1.3);
- a repeated binding name within one pattern, including a name repeated across
  the patterns of one arm (Clauses 6.8.6, 7.7.1).

Every such conflict is `E-name-conflict` (Annex B).

Shadowing is not a conflict: an inner binding shadows an outer one, `:=`
shadows an earlier binding, and a local binding MAY shadow an imported module
(Clause 7.4.2). An opened standard-library or capability type name is
shadowable by any type declaration of the same spelling (Clause 7.2.5); an
opened free name is not.

Two functions with the same name in different modules coexist and are selected
by a qualified name (Clause 7.4.4). Two types with the same name coexist; an
unqualified use is an error at the use site, and a qualified spelling selects
one (Clause 7.4.5). Constructors never conflict by visibility: same-named
constructors of different types coexist, and a reference is anchored by the
expected type or the scrutinee type (Clauses 7.5.2, 7.7.1). An unanchored
reference is legal only when exactly one constructor of that name is visible
and it is non-generic (self-anchoring, Clause 7.6); otherwise it is an error
(`E-undetermined`), and an annotation anchors it. A qualified constructor
selects one.

A constructor name that resolves to no visible constructor is `E-unbound-name`
in value and pattern heads alike (Annex B).

Signature overloading does not exist. One name denotes one binding in its
layer, with one signature; the same name with different signatures is not
admitted (Clause 6.10).

A conflict is reported even when the conflicting name is never used, except
for same-named types, whose unqualified use is the point of rejection.
