# Design Record — Names, Calls, and Modules

Non-normative. Ruling clauses: 7.2, 7.3, 7.4. Rejected alternatives, prior
art, and rationale.

## 1. UFCS

`a.f(b, c) ≡ f(a, b, c)` always and everywhere: every function is a free
function and `.` is its receiver form (`Clause 7.4.3`). No second dispatch
mechanism exists to learn or implement. Resolution is an ordered search —
record field, type namespace, current module, module path — first hit wins, and
the receiver position never consults lexical bindings. Stations are never
merged: a cross-station collision is resolved by spelling, and the diagnostic
lists the readings.

## 2. Structural access

Index syntax `x[i]` is banned for every structure (`Clause 9.10`): each
structure has one entry point, out-of-bounds is a domain check returning
`Option`, and the trust decision is made where the value is consumed.

| Family | Practice | Assessment |
|---|---|---|
| Rust, Swift, Go, C# | `[]` panics, `.get` returns `Option` | trust hides in the choice of syntax; Swift's "dictionary safe, array crashes" is the tension |
| Gleam, Elm | function-only access | **Eko follows** — one vocabulary, branch at consumption |
| JS, Kotlin | `?.[]` safe navigation | a postfix form the marker discipline excludes |
| Ruby, Julia | lenient defaults | silent defaults contradict honesty |

Consequence: out-of-bounds panic vanishes, and the panic surface shrinks to
arithmetic, `unwrap`/`expect`, `panic`, and `assert`. `Option.get` is an
ordinary prelude function in the `Option` namespace, so `m.get(i).get(j)`
resolves at station 2.

## 3. Modules and imports

One file is one module, named by its file stem; directories organize files,
are never modules, and are never named in code (`Clause 7.2.1`). Import paths
locate files; call sites use the single-segment stem. `import` opens **types**
into unqualified scope and declares the dependency edge; constructors are
visible for anchoring without forming a name layer (`Clause 7.4.6`), so
patterns write `Ok v ->` and annotations write `p: Person`. Functions stay
module-qualified, so provenance is visible at every call site; `x.Mod.f(args)`
fills the one position unreachable natively.

The symmetry with capabilities is deliberate: `main`'s parameter list is the
capability authorization manifest, the import list the *name* authorization
manifest. Ambient authority and ambient names are rejected alike.

## 4. Visibility

Public by default; a leading `_` makes a top-level binding module-private
(`Clause 7.3.1`): zero keywords, zero manifest, the interface extracted from
the implementation. The recorded price: the export surface grows without a
separate declaration, and changing visibility is a rename. `_` applies at
module top level only (a local `_x` is legal and exempt from `W-unused`);
exported signatures must reference visible types; variant visibility follows
the type, because exhaustiveness depends on the complete constructor set.

## 5. The prelude opening

The prelude is the only implicit module, injecting `assert`, `assert_eq`, and
`assert_ne` (`Clause 9.1.2`). `show`, `inspect`, and `panic` are
compiler-generated services attached to type namespaces; their free-call forms
are the identity of `Clause 6.5.2`, not opened names. The free spelling spans
all type namespaces at once — no single namespace could supply it — and
without the identity every call would read `x.show()`. The opened set is
closed and versioned with the compiler.

## 6. Rejected surface

| Area | Rejected | Reason (clause) |
|---|---|---|
| Access | index syntax `x[i]` | trust in syntax; two entry points per structure (`9.10`) |
| Access | payload dispatch inside `Option.get` | a second type-inexpressible built-in after `with` |
| Access | compound names like `get_or` | composition suffices |
| Import | `use` | Rust's `use` opens names into scope — the reverse semantics |
| Import | `dep` | package-management association; packages are tooling domain |
| Module | functors, module-level signatures | generics already own parameterization (`6.10`) |
| Module | modules as values | a record of closures is the value form |
| Module | `open` | does not fix the receiver position; loses provenance; adds a global collision surface |
| Module | re-export | forwarding is a function, not a renaming |
| Module | cyclic dependencies | the DAG is the foundation of build, caching, and incrementality |
| Module | conditional compilation | host and library business, not name-layer business |
| Module | module-level side effects | top-level purity is free; order is unobservable |
| Module | import aliases (`as`) | rename the local instead |
| Module | private modules (`_x.eko`) | module visibility *is* the import edge |
| Visibility | explicit export lists | double-writing; the interface is extracted, not declared (`7.3.1`) |
