# Names, Calls, and Modules

This chapter defines how names are written, resolved, and scoped. Sections 1–4 cover call-site naming and structural access: the receiver form of a call, the order in which `.` resolves a member, the chains that receiver form makes possible, and the single entry point each structure offers. Sections 5–11 cover the module system that supplies the outermost name layer: modules as files, imports, qualified names, namespace ownership, visibility, the dependency graph, and the prelude. Section 12 lists the exclusions.

## 1. Calls and UFCS

### 1.1 The identity

```eko
a.f(b, c)  ≡  f(a, b, c)
```

The two spellings are always available and always semantically identical. Every function in Eko is a free function; a function is attached to a type namespace or to a module, and `.` is its receiver form at the call site. The identity extends to qualified names:

```eko
a.Mod.f(b, c)  ≡  Mod.f(a, b, c)
```

In both cases the first parameter is the receiver, and there is no other notion of a method. Operator sugar obeys the same identity, since an operator is a type-namespace function (`a + b ≡ Int.add(a, b)`, hence `a.add(b)`; 02-syntax.md §6).

### 1.2 Receivers everywhere

Every prelude signature takes a capability, a handle, or data as its first parameter, and the receiver form fits that shape exactly:

```eko
out.println("hi")           // ≡ Out.println(out, "hi")
fs.open("data.csv", Read)   // ≡ Fs.open(fs, "data.csv", Read)
file.read(4096)             // ≡ File.read(file, 4096)
xs.map((n): n * 2)          // ≡ List.map(xs, (n): n * 2)
line.split(",")             // ≡ String.split(line, ",")
```

World-touching calls (`out`, `fs`, `file`), container traversal (`xs`), and pure transforms (`line`) share one shape. There is no separate method-call syntax and no second dispatch mechanism: a member call is a function call whose first argument is written to the left of the dot.

## 2. Member resolution

`x.f(args)` is resolved at the following stations, in order; the first hit wins. When `f` is a multi-segment qualified name, station 4 applies.

| Station | Searches | Example |
|---|---|---|
| 1. Record field | the field `f` of the record `x` | `p.name` |
| 2. Type namespace | the namespace of `typeof(x)` | `xs.map(f)`, `s.at(i)` |
| 3. Current module | free functions at the top level of the current file whose first parameter is `typeof(x)` | `p.audit()` inside the module that defines `audit` |
| 4. Module path | the qualified segment `x.Mod.f(args)` (§7.1) | `x.lex.tokenize()` |

Station 2 details: the namespace of a type is named after the type's head — `[]t → List`, `Option t → Option`, `Result t e → Result`; prelude types such as `String`, `Dict`, and `Bytes` use their own names. The namespace is owned by the module that declares the type and travels with the type (§8).

Station 3 makes services defined in the calling module available on any type without adding anything to the type's namespace; this is the only station at which a module can define receiver-form functions on a foreign type. A function defined at the top level of a different module is not found there and requires station 4.

The receiver position never consults lexical bindings (§7.2).

When a name is simultaneously a field and a function of the receiver's type, the compiler reports both and the author selects one by writing field access or a fully qualified call. A field name that collides with a module name uses the same mechanism: the field station wins, and if the chain then fails to resolve, the diagnostic lists the readings available at that point — including the module reading — so that the author can choose a spelling.

Resolution never merges stations: they are tried in order and the first station with a hit resolves. A function found in the receiver's type namespace (station 2) therefore takes precedence over an equally named function in the current module (station 3), and likewise for any later station; this is ordered resolution, not a name conflict. The compile-error rule for name conflicts applies within a layer (§11), not across resolution stations.

## 3. Chaining

A multi-step transformation is a receiver chain: each link's receiver is the previous link's value, and the chain stays linear across modules by attaching a module at the link with `.Mod.f()` (§7.1).

```eko
content.split("\n")
       .map(String.trim)
       .filter((l): l != "")
       .length()
```

Each link resolves independently by the station order of §2, so `.split`, `.map`, `.filter`, and `.length` resolve locally in whatever module the chain is written: a type namespace travels with the type, not with the module (§8). No link needs a module prefix unless the function lives under a module path. A fully qualified name may also be passed as an argument, as in `.map(String.trim)`: that is an ordinary name reference, not a call.

## 4. Structural access

### 4.1 One entry point per structure

Index syntax `x[i]` is prohibited for every structure; the bracket pair belongs to the type position. The two bracket pairs are disjoint — `[...]` for types and `{...}` for values.

Each structure offers exactly one entry point:

| Structure | Entry point | Form |
|---|---|---|
| Record | field | `p.name` (§2, station 1) |
| Tuple | pattern | `(a, b) = p`; a `case` arm |
| List, dictionary | `get` | `xs.get(i) -> Option t`; `d.get(k) -> Option v` |
| String | `at` | `s.at(i) -> Option Char` |

The name carries the cost: `get` is O(1), while `at` is O(n) with explicit cost (07-runtime.md §7). Every index-based entry point answers a presence question — is the index inside the domain? — and a presence question is a domain check, so the answer is `Option` and never a panic (three-way failure taxonomy, 01-overview.md §3). Out-of-bounds panic therefore does not exist in the language: no accessor can raise one, and the trust decision belongs to the consumer (§4.2).

### 4.2 Trust is declared at consumption

An accessor always yields `Option`. Declaring trust is a consumption-site decision, written explicitly and with a message:

```eko
v = xs.get(i).expect("i validated")                            // assertion: informed panic
v = xs.get(i).unwrap_or(0)                                     // default witness
age <- data.get(1).to_result("short row: missing name").try()  // into a Result chain
```

| Consumption | Function | Effect |
|---|---|---|
| Assertion | `expect(msg)`, `unwrap` | `expect` panics with `msg`; `unwrap` panics without one. Used only for invariants already verified |
| Default | `unwrap_or(d)` | Substitutes `d` for `None` |
| Bridge | `to_result(e)` | Converts to `Result`, with `None → Err e`, for statement-level access |

The panic surface shrinks accordingly to arithmetic (division by zero, overflow), `unwrap`/`expect`, `panic`, and `assert` (06-world.md §3). A dual track of bracket syntax with panics plus a separate `Option`-returning accessor would hide the trust declaration in the choice of syntax; here there is one access vocabulary, and the branch happens at consumption (`case`, `expect`, `unwrap_or`), exactly as for any other `Option`.

### 4.3 Nested access

| Scenario | Form |
|---|---|
| List nested access, expression position | `m.get(i).get(j)` — `Option.get`, with `None` passing through |
| Statement level (Result, host) | `<-` plus `to_result`: one line per layer, zero lambdas |
| Verified invariants (algorithm) | `m.get(i).expect("i").get(j).expect("j")`; or a flat index such as `grid.get(i * w + j)` |
| Default (table initialization) | `m.get(i).get(j).unwrap_or(0)` |
| Heterogeneous (JSON) | `case` navigation over variants — a nominal type is mandatory; `and_then` to pass through structures |

`Option.get` is an ordinary prelude function that any user could write; there is no compiler magic:

```eko
Option.get: (rows: Option ([]t), i: Int) -> Option t =
    case rows:
        Some xs -> xs.get(i)
        None    -> None
```

It lives in the `Option` namespace. In `m.get(i).get(j)` the receiver of the second link has type `Option ([]t)`, so station 2 looks it up in `Option` and finds it; placed in the `List` namespace it would be unreachable from the chain, and only the direct call would remain.

The main channel for nested access is `<-` together with `Option.to_result` (03-control.md §6):

```eko
row <- m.get(i).to_result("no row").try()
v   <- row.get(j).to_result("no col").try()
```

Each layer costs one line and no lambda is written. Dictionary payloads (`Option ([k]v)`) are traversed with `and_then`: a payload-dispatched variant of `Option.get` would collide by name (signature overloading, §12) and payload dispatch would be a compiler built-in, so neither exists. Heterogeneous navigation (JSON) goes through `case` over variants (08-builtins.md §7): descending through a dynamic shape requires a nominal type first.

## 5. Modules

A module is a compile-time namespace: it holds types and free functions, and it has no state, no initialization semantics, and no runtime footprint. The module system answers three questions and no others — where a name lives (unit), who can see whom (visibility), and how the build graph connects (dependency). The remaining agenda of traditional module systems is covered by existing orthogonal mechanisms: top-level purity removes initialization order and import side effects (06-world.md §7), capability values with capture replace module-level state (06-world.md §2), generics replace parameterized modules (04-types.md §7), and full erasure of world bits and types removes runtime linking (06-world.md §5).

### 5.1 File is module

One source file is one module. The extension is `.eko`, and the file stem — a snake_case identifier, containing no dot — is the module's name everywhere in code. Directories organize files: they are never modules and are never named in code.

```
person.eko        → person        (import person)
csv.eko           → csv           (import csv)
csv/lex.eko       → lex           (import csv/lex)
```

- The import path (`csv/lex`, §6) only locates the file; the module is identified in code by its single-segment name. The mapping is unique: a path resolves to exactly one file relative to the project root.
- `csv.eko` and a `csv/` directory may coexist — they are the two distinct modules `csv` and `lex`. Modules under one directory are mutually independent; folders create no namespace nesting because they are not nameable.
- To split a large unit, split it into submodules. A facade is optional: consumers may import `csv/lex` and call `lex.tokenize(l)` directly, or import `csv` and go through forwarding functions there. Forwarding is a function, not a re-export; it is honest and greppable.
- Renaming a module means renaming the file. Moving it changes only the `import` lines that locate it — call sites use the stem and do not change — which makes restructuring a tooling-friendly operation.
- Two modules with the same stem may exist in different folders; importing both into one file is a same-layer conflict (§6).
- A file name may not begin with `_`. Module visibility *is* the import edge: what no module imports is not visible, and there is no finer private-module layer; `_x.eko` would suggest a concept that does not exist.

### 5.2 Entry points

The program entry is the module that contains a `main` binding. The dependency graph is the transitive closure from that entry, and the host authorizes according to `main`'s parameter list — the authorization manifest — independently of the module graph (06-world.md §2). Three boundaries follow:

- `main` may be bound only in the entry module; a binding named `main` in a non-entry module is a compile error. The entry `main` must be public, and `_main` is the same error: `main` is a host contract, not an ordinary name.
- `main` returns `_` — there is no `Int`-returning form; exit codes belong to the host, and a chosen exit code is `Proc.exit` (06-world.md §2).
- Modules outside the entry closure do not participate in compilation; this is not an error. Test compilation takes the modules containing a `test` binding as roots (09-library.md §6).

## 6. import

The keyword is `import`. Imports are listed at the top of the file, before the other top-level bindings, one module per line:

```eko
import person
import stats
import csv/lex
```

An import names a module by a **path**: a single segment (`import compiler`)
locates `compiler.eko`; a `/`-separated path (`import csv/lex`) locates
`csv/lex.eko`. The `/` separator appears **only** in import statements — in
every expression it is the division operator (02-syntax.md §6). Path segments
are identifiers; `..` and absolute paths do not exist.

All paths resolve against the **project root**: the directory containing the
entry module and, under test compilation, the common ancestor of the test
roots (09-library.md §6.6).

The path locates the file; the module's name in code is the **final segment's
stem**. Importing `csv/lex` opens the types and constructors of module `lex`
— it does not import `csv` — and `lex` is the name used at call sites. Two
imports whose modules share a stem are a same-layer name conflict, a compile
error (`E-name-conflict`); rename one of the files.

An import does exactly two things:

1. **Declares a dependency**: the module enters the compilation graph.
2. **Opens scope**: the target module's types and constructors are brought into unqualified scope.

Functions from an imported module are always accessed by module-qualified name — `person.parse(l)`, `stats.summary(ps)`, `lex.tokenize(l)` — so that the provenance of every function is visible at the call site.

Types and constructors must be brought into unqualified scope, because the pattern position writes bare constructors (`Ok v ->`, `Man ->`) and type annotations write bare names (`p: Person`); neither spelling can carry a qualified prefix.

```eko
import person

p: Person = person.parse({"ada", "36", "man", "x"}).unwrap()
case p.sex:
    Man   -> "M"
    Woman -> "F"
```

There is no implicit visibility and no transitive re-export. An import list is a name authorization manifest, the exact counterpart of `main`'s parameter list as a capability authorization manifest: the language rejects ambient authority and ambient names alike.

## 7. Qualified names in chains

### 7.1 UFCS extension

```
x.Mod.f(args)  ≡  Mod.f(x, args)
```

The function name in the receiver position may be a fully qualified name. This is not new notation but the existing composition of `.`; the module name appears on the chain, so provenance is not lost. It fills the only position that is otherwise natively unreachable:

```eko
report <- persons
    .filter((p): p.age >= 18)
    .stats.summary()          // ≡ stats.summary(persons.filter(...))
    .show()                   // type-namespace station; travels with Summary
```

Without the extension, that position could only be written by splitting off a variable or by nesting an argument, and the pipeline would no longer be linear.

A fully qualified name can appear at four positions in a chain:

| Position | Form | Basis |
|---|---|---|
| Chain head | `Mod.f(x)` | The plain identity (§1.1) |
| Argument position | `.map(Mod.f)` | An ordinary name reference |
| Element-level link | `.traverse((l): Mod.f(l.split(",")))` | Direct form, as at the head |
| Accumulated-value link | `.Mod.f()` | The UFCS extension above |

### 7.2 Name layering

An import is the outermost lexical scope. Three syntactic positions have one rule each:

| Position | Rule |
|---|---|
| Expression head | **Binding first, module fallback**: `people` is looked up among bindings (parameter → local → file top level); if no binding exists and an import provides that module, it is a module path. An inner binding shadows the module — inner shadows outer, the general lexical rule. An import and a file top-level binding with the same name are an error, being in the same layer. |
| Receiver position | Station order: field → type namespace → current module → module path. Lexical bindings are never consulted. When a later resolution fails, the diagnostic lists the readings. |
| Type position | A dotted name can only be read as a module path, since the type position has no value semantics. The qualified form `a.Person` is the disambiguation device for same-named types in two modules. |

Case enforces the layering structurally: types and constructors are PascalCase, bindings and modules are snake_case, both checked at compile time (04-types.md §3). A PascalCase head is necessarily a namespace or a constructor; for a snake_case head, the head-position rule decides between binding and module.

Two escape cards write module intent out explicitly and are immune to shadowing: the direct form `Mod.f(x)`, where the head has no binding and is therefore a module, and the extended form `x.Mod.f()`, where the receiver position does not consult bindings. After a local shadowing, the module path is unreachable at the head position in that scope; renaming the local resolves it, and there is no alias mechanism.

```eko
import person
import stats

f: (rows: [][]String) -> []Person =
    person := rows.get(0).unwrap()   // local shadowing of the module: legal
    person.parse(row)                // error: []String has no `parse`; the diagnostic lists the module reading
    rows.stats.summary()             // correct: the receiver position does not consult bindings
```

When a field name collides with a module name, the field station wins and a downstream failure produces the diagnostic described in §2; `r.csv.split(",")` is such a case. Two modules providing a function of the same name are disambiguated by writing the module at the head, as in the direct form above.

## 8. Type namespace ownership

A type namespace belongs to the module that declares the type, and it travels with the type's visibility. After `import person`, `p.make()` resolves through station 2 in any module, as long as `Person` is declared in `person`: the namespace is keyed by the declaring module, not by the calling file. No other module may add functions to a foreign type's namespace. Cross-module services on a foreign type are module-level functions, reached by fully qualified name or by the extended receiver form (§7.1). A module's own functions that take a foreign type as their first parameter resolve through station 3 inside that module only.

**Declaring a member.** A top-level binding whose head is `TypeName.name` — `Money.show: (m: Money) -> String` (08-builtins.md §6.2) — declares a member of `TypeName`'s namespace. It is legal only in the module that declares `TypeName`; anywhere else it is a compile error (`E-namespace-owner`, 09-library.md §7.2), and a module-qualified spelling does not bypass the rule: `compiler.Compiler.temp_fn` written outside `compiler` targets the same foreign type and is the same error. An import is a **read-only grant** — nothing written in an importing module adds to, removes from, or overrides the imported module's namespace or interface. A module that wants a service on a foreign type declares a free function: inside the module it is reachable unprefixed at the receiver (station 3); elsewhere it carries the module name — `temp.temp_fn(c)` or `c.temp.temp_fn()` (§7.1).

**Same-named types coexist.** When two visible types share a name — from two imports, or an import beside a local declaration — the unqualified spelling is a compile error at its use site, and each is written module-qualified: `compiler.Compiler`, `temp.Compiler`. A module's own name is in scope as a module path inside itself, so self-qualification is always available. Types that do not collide are always written plain; the qualified form is legal everywhere but is idiomatic only for disambiguation.

Ownership settles orphan-instance problems in advance: whether a function belongs to a type's namespace is decided where the type is declared, never at a distant call site that happens to import both.

## 9. Visibility

Everything is public by default. A top-level binding whose name begins with `_` is module-private:

```eko
show_name:  (p: Person) -> String = …     // public
_show_name: (p: Person) -> String = …     // private: not in the export surface
```

The `_` family carries one meaning throughout — decoupled from the public name domain: type position `_` is Unit, the nameless type; value position `_` is discard; pattern position `_` is the wildcard; parameter position `_` is the wildcard parameter; `_ := e` is a discarded binding; and a leading `_` in an identifier keeps the name out of the module export surface.

Three companion rules:

1. **`_` applies to module top-level bindings only.** A local binding spelled `_x` is legal and semantically neutral, since locals have no export concept, and it does not participate in the `W-unused` warning (09-library.md §7) — the "don't care" sense of the family.
2. **Types in exported signatures must themselves be visible.** A public function whose parameters or result reference a private type `_Foo` is a compile error; otherwise a value could flow while its type name cannot be spelled, and the pattern position could not be written. A private type lives only in private signatures.
3. **Variant visibility follows the type and cannot be split.** A public ADT has all its variants public, because exhaustiveness depends on the complete set (03-control.md §5); a private type is private in all its variants.

The result is zero keywords, zero manifest, zero double-writing: public and private are visible at the call site, and a private function such as `person._secret` is an error from outside its module. The price is that the exported interface grows without a separate declaration and that changing visibility is a rename — the interface is extracted from the implementation (§10).

## 10. Dependency graph and interfaces

- **Modules have no initialization semantics.** A module body is a set of pure constants; it may be evaluated in any DAG order or folded to constants directly, and the order is unobservable (06-world.md §7).
- **Cyclic dependencies are prohibited** — a compile error, not a warning. Names within a module may reference each other freely, recursive functions included; cross-module mutual recursion is resolved by merging the modules, or by passing function values as parameters — the same shape as capability flow.
- **Interfaces are extracted automatically** from the implementation: types, signatures, and visibility. There is no hand-written `.mli`/`.d.ts` layer. Separate compilation proceeds against the interface; acyclicity plus top-level purity make the build graph a DAG cache, so incremental compilation follows.
- **Generic bodies travel with the interface** (07-runtime.md §6). Downstream instantiation expands downstream, and identical instances are deduplicated at link time.

## 11. Prelude

The prelude is the only implicit module: `List`, `String`, `Bytes`, `Set`, `Json`, `Result`, `Option`, the capability types, and the implicitly opened free functions are always available without an import. It is an ordinary Eko module named `prelude`, **delivered as source alongside the compiler** and versioned with it: apart from the compiler-provided services below, the prelude is ordinary Eko code — `assert` and `assert_eq` are prelude source, and so are the combinators and the capability derivations. The complete inventory is 10-prelude.md.

**Compiler-provided services are not library code.** The derived services (`show`/`inspect` defaults, `==`, `<`, `with`, `to_json`/`from_json`; 08-builtins.md §6), the `panic` intrinsic, and the memory operations (07-runtime.md §4) are generated by the compiler and cannot be written in Eko source.

The channel through which the prelude reaches the operating system is the `extern` lexicon of `11-ffi.md §8` — no intrinsic exists for it, and the compiler-provided services are the three above. A pure external is declared with the `Pure` guarantee word (`11-ffi.md §2.2`); the prelude's wrap/unwrap primitives are protected by the reserved symbol prefix (`11-ffi.md §6`).

The implicit scope opening supplies the half of the import rule that §6 leaves open: **the free functions `show`, `inspect`, `panic`, `assert`, `assert_eq`, and `assert_ne` are brought into unqualified scope with the prelude.** `show` and `inspect` are generic over every type, and the opened name spans all type namespaces at once — each namespace carries only its own type's member (`08-builtins.md §6.7`) — so no single namespace can supply the free spelling; without the opening, every call would read `prelude.show(n)` and the spelling floor of the language would collapse. Type-attached functions such as `List.map` still resolve through the type namespace, for which scope opening is meaningless.

The opened set is closed and versioned with the compiler, so its collision surface is bounded. Conflict rules are the same as for user modules: a file top-level binding and an opened name are in the same layer, so a same-name collision is an error, and local shadowing remains legal. User imports never open everything, so call-site provenance is unaffected.

## 12. Exclusions

- **Index syntax `x[i]` for any structure.** Two access vocabularies, and silent trust at the access site; the bracket pair belongs to the type position.
- **Signature overloading.** The same name with different signatures is hidden ad-hoc polymorphism and competes with generics: open polymorphism is parametric generics, ad-hoc polymorphism is a closed union with `case` (04-types.md §8).
- **Payload dispatch built into `Option.get`.** A payload-dispatched `get` would be a second type-inexpressible built-in after `with` (08-builtins.md §6).
- **Compound words such as `get_or`.** `get` and `unwrap_or` compose; no dedicated word is warranted.
- **Functors and module-level signatures.** Generics already own the parameterization vocabulary, and two vocabularies for one phenomenon are prohibited.
- **Modules as values.** Where a value is wanted, use a record of closures; modules never enter the value position.
- **`open`, or opening functions into unqualified scope.** This does not fix the receiver position — the stations still exclude opened names, so another rule would be needed — and it loses call-site provenance while introducing a global collision surface.
- **Re-export.** Forwarding is a function, and forwarding is not renaming; forwarding is honest and greppable.
- **`/` paths outside `import`.** Division is the only `/` in expressions; module paths are import-only (§6). Call sites name modules by their single stem.
- **Cyclic dependencies.** The DAG is the foundation of the build graph, of caching, and of incremental compilation.
- **Conditional compilation.** Multiple platforms are the business of the host and of ordinary code, not of the name domain.
- **Initialization blocks and module-level side effects.** Top-level purity is free; no back door is opened, and there is no ambient I/O.
- **Cross-module extension of a type namespace.** Ownership belongs to the declaring module (§8); monkey-patching is opposed to call-site provenance.
