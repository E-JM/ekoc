# Design Record — Names, Calls, and Modules

Non-normative. Rulings: `05-names.md`. Rejected alternatives, prior art,
and rationale.

## 1. UFCS: one identity, four stations

`a.f(b, c) ≡ f(a, b, c)` always and everywhere; every function is a free
function, and `.` is its receiver form (`05-names.md §1`). The prelude's
signatures — capabilities, handles, data as first parameters — are the
natural soil: `out.println(...)`, `fs.open(...)`, `xs.map(...)` share one
shape, and there is no second dispatch mechanism to learn or to implement.

Resolution is an ordered search — record field, type namespace, current
module, module path — first hit wins, and the receiver position never
consults lexical bindings (`§2`). This is *ordered resolution*, not name
conflict: stations are never merged, and a collision across stations is
resolved by spelling (field access or fully qualified call), with the
diagnostic listing the readings.

## 2. Structural access: index syntax and the prior-art map

Index syntax `x[i]` is banned for every structure; each structure has
exactly one entry point, and out-of-bounds is a domain check returning
`Option` — the trust decision is made at the consumption site
(`05-names.md §4`). The prior-art comparison, recorded for the decision:

| Family | Practice | Assessment |
|---|---|---|
| Rust, Swift, Go, C# | Dual track: `[]` panics, `.get` returns Option | Trust declaration hides in the *choice of syntax*; Swift's "dictionary safe, array crashes" is a sample of the internal tension |
| Gleam, Elm | Function-only access; drilling via `use`/`and_then` | **Eko follows this family** — one vocabulary, branch at consumption |
| JS, Kotlin | `?.[]` safe-navigation syntax | A postfix form the marker discipline already excludes, and both stand on null/undefined |
| Ruby, Julia | Lenient defaults | Silent defaults conflict with honesty |

Consequences: out-of-bounds panic vanishes from the language entirely, and
the panic surface shrinks to arithmetic, `unwrap`/`expect`, `panic`, and
`assert`. The old corpus example — `data[1]` panicking on a short CSV row
instead of returning an error — was the concrete failure mode that closed
the case.

`Option.get` (list drilling) is an ordinary prelude function, zero magic,
living in the `Option` namespace so the chain `m.get(i).get(j)` resolves at
station 2. Rejected: payload dispatch baked into `Option.get` (a second
type-inexpressible built-in after `with`, and signature overloading is
banned) and compound words like `get_or` (composition suffices; no word
earns its place).

## 3. Modules: file is module

One file is one module, named by its file stem; directories organize files
and are never modules and never named in code (`05-names.md §5`). This
structurally eliminates the two historical ambiguities of the family: the
Rust module tree (is a directory a module or a container?) and Go's
package-name-vs-directory mismatch. Import paths locate files; call sites
use the single-segment stem.

### The import keyword

`import` was chosen on the Gleam precedent — import plus
module-qualified function calls is the natural pair. Rejected: `use`
(Rust's `use` *opens names into scope*, the reverse semantics — the
collision would confuse exactly the readers it looks familiar to) and
`dep` (package-management association; packages are tooling domain, below
the name layer).

### What an import does, and does not, do

An import declares the dependency edge and opens **types and constructors**
into unqualified scope — because patterns write bare constructors
(`Ok v ->`) and annotations write bare names (`p: Person`), spellings that
cannot carry a prefix. Functions are always module-qualified so provenance
is visible at every call site. The UFCS extension `x.Mod.f(args)` fills the
one position otherwise unreachable natively, keeping the pipeline linear.

The symmetry with capabilities is deliberate: `main`'s parameter list is
the capability authorization manifest; the import list is the *name*
authorization manifest. Eko rejects ambient authority and ambient names
alike.

## 4. Visibility: the `_` prefix

Public by default; a leading `_` makes a top-level binding module-private
(`05-names.md §9`). The yield: zero keywords, zero manifest, zero
double-writing — the interface is extracted from the implementation. The
price, recorded: the export surface grows without a separate declaration,
and changing visibility is a rename.

Companion rulings: `_` applies to module top level only (a local `_x` is
legal, semantically neutral, and exempt from `W-unused`); exported
signatures must reference visible types (otherwise a value flows while its
type name cannot be spelled); variant visibility follows the type, because
exhaustiveness depends on the complete constructor set.

## 5. Rejected module-system alternatives

| Rejected | Reason |
|---|---|
| Functors / module-level signatures | Generics already own parameterization; two vocabularies for one phenomenon are prohibited (`§12`) |
| Modules as values | A record of closures is the value form (`mk_logger`); modules never enter value position |
| `open` — opening functions into scope | Does not fix the receiver position (the stations still exclude opened names, so yet another rule would be needed), loses call-site provenance, and adds a global collision surface |
| Re-export | Forwarding is a function, and forwarding is not renaming; honest and greppable |
| Cyclic dependencies | The DAG is the foundation of the build graph, caching, and incremental compilation; cross-module mutual recursion is resolved by merging modules or passing function values |
| Conditional compilation | Multiple platforms are host and library business, not name-layer business |
| Initialization blocks / module-level side effects | Top-level purity is free — module bodies are pure constants evaluable in any DAG order, the order unobservable |
| Alias mechanisms for imports | Local shadowing of a module name is resolved by renaming the local; no `as` escape hatch |
| Private modules (`_x.eko`) | Module visibility *is* the import edge; a finer layer would suggest a concept that does not exist |

## 6. The prelude opening

The prelude is the only implicit module, and the implicitly opened free
functions (`show`, `inspect`, `panic`, `assert`, `assert_eq`, `assert_ne`)
are brought into unqualified scope with it (`05-names.md §11`). The opened
name spans all type namespaces at once — each namespace carries only its
own type's member — so no single namespace can supply the free spelling;
without the opening, every call would read `prelude.show(n)` and the
spelling floor of the language would collapse. The opened set is closed and
versioned with the compiler, so its collision surface is bounded, and
conflict rules are the same as for user modules.
