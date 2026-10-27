# Design Record — Types and Generics

Non-normative. Rulings: Clauses 7.5, 7.6. Rejected alternatives, prior art,
and rationale.

## 1. Declarations: the annotation slot and no aliases

`type` lives in the annotation slot, so a type declaration has the same
grammatical shape as every other binding: the line starts with a name. The
same design act puts `!->`/`?->` on arrows and `=`/`:=`/`<-` on bindings —
markers land on slots, and the declaration head keeps the invariant
`Name ':'` common to all declaration forms.

The right-hand side is a **declaration form only** (variant list or bracketed
record/tuple). This is the no-aliases ruling: naming never renames an
existing identity, so `type` cannot smuggle in a synonym, and nominal
typicality stays honest — identity is bought explicitly, never inherited by
spelling.

## 2. Nominality and shape adaptation

The axis: **identity is bought explicitly — a declaration is an identity,
an anonymous form is a view.** The same philosophy as capabilities
(permissions are explicit; identity is explicit). Consequences:

- Same-shaped declarations never interconvert (`Celsius` vs `Fahrenheit`);
  the temperature accident is structurally impossible, and a newtype is
  airtight against every declared type.
- A nominal value flows one-directionally into a *same-shape* anonymous
  form at exactly three positions: argument, operand, expected type. The
  reverse never happens — an anonymous form does not impersonate a nominal.
- Same-shape is an exact match, not a width relation. Field-evolution
  brittleness is the contract: adding a field to `Person` breaks every
  same-shape anonymous signature, and it should.

**Rejected alternatives:**

| Rejected | Reason |
|---|---|
| Row polymorphism / width subtyping | Same-shape exactness is the contract; rows reintroduce silent structural drift (`Clause 7.5.5`) |
| Automatic conjunction for mixed operands | Operand typing is fixed by the left operand; searching for a common anonymous supertype would reopen the temperature accident from the equality side (`Clause 7.5.5`) |
| Shape adaptation inside containers and function types | Shapes carry identity, so they do not move freely; bit adaptation is exempt because bits are erased (`Clause 7.5.5`) |
| Anonymous ADTs | No union form outside a right-hand side; adaptation stays records-and-tuples only |
| `Name(x)` conversion functions and "type names in value position" | Explicit conversion is a literal at the expected position or a field-by-field rebuild (`Clause 7.5.2`) |

## 3. The four composites and two bracket pairs

`[...]` at type position, `{...}` at value position, zero overlap
(`Clause 6.6`). A composite literal never stands alone — the expected
type resolves list-vs-tuple and dictionary-vs-record, the same machinery
that types lambda parameters. The single localized exception is `with`'s
partial literal (`Clause 9.8.6`).

- Value-position tuples do not use `(...)`: parentheses are already taken
  by parameter groups, call syntax, and parenthesized expressions.
- `Set` is in the prelude as `Set t` with **no notation and no literal**:
  `{a, b}` is already given to lists and tuples, and a fifth composite form
  would break the four-form closure.
- A tuple has at least two elements; `(Int)` is a parenthesized type, `()`
  the zero-parameter function type, and the unit is `_` — never a zero
  element tuple.

## 4. Generic application: the notation decision

Application is **juxtaposition without commas**: `Value Int Float`, composite
arguments parenthesized, the chain stops at `,` and `->`
(`Clause 7.6.4`). The full option space that was weighed:

| Candidate | Verdict |
|---|---|
| `Value<Int, Float>` | Rejected: operator ambiguity, `>>` nesting damage, and a surface break in a language whose other applications are by adjacency |
| `Value[Int, Float]` | Rejected: fatal collision with the bracketed type forms of `Clause 6.6` |
| `Value(Int, Float)` | Rejected: collides with call syntax |
| `Value Int, Float` | Rejected — the decisive case: a function parameter list already separates parameters with commas, so `(Value Int, Float) -> Int` can only parse as two parameters; writing a single argument would need double parentheses in exactly the position where function types appear most often |
| Prefix binders `Value u t: type` | Rejected: breaks the `Name ':'` declaration-head invariant |
| Uppercase multi-letter implicit variables | Rejected: a misspelled type name silently becomes a variable (`Clause 7.6.4`) |
| ML apostrophe `'a` | Rejected: a new lexical class; recorded as a fallback, never used |

The winning rationale is internal consistency: the language's existing
convention is *application by adjacency, enumeration by commas* — `[k]v`
has no comma, `[a, b]` does.

### Type variables are lowercase

Lowercase variables close three breaches of the earlier uppercase scheme
(`Tree: type T`):

1. **Case doctrine.** PascalCase is the identity layer (declared types,
   constructors, namespaces); a variable is a binding, and bindings are
   snake_case everywhere else. The old scheme dressed a binding in the
   identity layer's clothes.
2. **Juxtaposition self-collision.** `Tree T` vs `Tree t` — one character
   of case distinguishing type from variable in unseparated adjacency is
   unreadable; lowercase separates at a glance.
3. **Name confiscation.** The old scheme had to ban single-letter uppercase
   type names to keep the lexical judgment working — confiscating a whole
   class of names. Lowercase confiscates nothing.

The cost is recorded: in a declaration's right-hand side and in patterns,
case no longer signals the domain — `Ok t` as payload and `Ok t` as pattern
look alike; position settles the parse, and human scanning pays.

### Explicit ordered binders

Binders are listed after `type` and filled positionally. The rejected
alternative — collecting variables by first occurrence in the right-hand
side — fails silently: reordering variants or fields would flip the meaning
of positional arguments at distant use sites, and a union carries no
positional skeleton of its own to prevent it.

## 5. Dangling variables: the two symmetric bans

- A declaration binder must occur in the right-hand side (phantom ban) —
  otherwise `==`, `show`, and conversion semantics would be entangled with
  a position no value can distinguish.
- A function annotation's variable must occur in at least one parameter
  type (result-only ban) — a type not determined by the parameters should
  be determined, and `Set.empty: () -> Set t` is exactly why no such word
  exists (`Clause 9.10.3`).

The open-bit result-only prohibition (`Clause 7.9.3`) is the same
rule on the bit axis: bits and types run on one machine. Polymorphic
recursion comes free because the type comes from the annotation rather
than inference, and a recursive call may use any instantiation.

## 6. Recursive types: open recursion, indirect positions

A right-hand side may self-refer; **every recursive position is a pointer**
(`Clause 8.5.5`), so size is always finite and opening recursion needed
zero new well-formedness checks. A recursive type with no base case is
legal and has zero inhabitants — inhabitance is not checked, the same
spirit as `Never` and `[]Never`. Derived services terminate on recursive
structure by the acyclicity theorem (`Clause 8.6.1`).

## 7. Constructors and equality anchoring

Constructor application is check-only — it needs an expected type, because
the instantiation comes from that type. A bare generic constructor has no
self-anchoring equality: `Leaf 1 == Leaf 2` is an error (no unique union to
anchor the instantiation); if one side is a variable the instance is
determined and the equality is legal — cross-seeding. Constructors of
single-variant unions self-anchor, so `Red == Green` stays legal.

**Ruling.** An unanchored generic constructor does not instantiate
bottom-up from its payload: `x := Some 1` stays `E-undetermined` and
wants an annotation. Instantiation has exactly one source, the expected
type — written as an annotation, supplied by an anchored position, or
determined by unification — and the payload is never a second one
(`Clause 7.6.4`). One source keeps the annotation discipline the generics
rest on; the undetermined-expectation path already reaches every real
inference context, composite elements and lambda results among them.
**Rejected: Rust-style payload-driven inference.** It is a second,
competing source of instantiation truth, and a `None`-style nullary
constructor could never instantiate bottom-up anyway — the accommodation
would cover half the constructors and split the rule.

## 8. `Never`: vocabulary, not a declaration

The bottom type needs two properties no declaration can express: it
adapts to every value type, and it marks a call that does not return.
An empty variant list can state *uninhabitedness*, but bottomness is a
fact of the type lattice and of evaluation, not of a constructor
enumeration. `std` carrying a `Never: type =` row therefore made the
library definition decorative — the checker recognized the type by its
`std` + `Never` name pair, the one declaration whose meaning lived in
its spelling.

**Ruling.** `Never` is a type of the language, the dual of `_`: as `_`
is the unit type with exactly one value, `Never` is the bottom type
with none. It resolves in every file as vocabulary, cannot be declared
by any module, has no namespace, and a member access on a `Never`
expression types as `Never` — the receiver never evaluates. The empty
right-hand side is removed from the grammar: a type declaration always
carries a variant list, a record, or a tuple.

**Rejected: the Swift model** (keep the std row, grant bottomness by
name). This was the standing design. It needs name magic in the
checker, an import-layer anomaly (an opened name that is not a name),
and an empty-RHS grammar production with exactly one legal customer.

**Rejected: the Lean model** (every zero-variant declaration is
bottom). Explosion from uninhabitedness is derivable and sound, but it
buys a family of mutually incompatible bottoms (`[]Void` vs `[]Never`,
`() -> Void` vs `() -> Never`) with no customer in an industrial
language — the `Result t Never` idiom needs one bottom, not many.
Rust `!`, Kotlin and Scala `Nothing`, and TypeScript `never` agree.

Two load-bearing consequences. First, exhaustiveness belongs to
patterns, never to divergence (record `control.md` §5): a `Never`
arm's pattern covers like any other arm's, and a case whose arms all
diverge is exhaustive with type `Never` — `Clause 7.7.4` says exactly
this. Second, the vacuous derived ordering over the zero-variant
declaration is gone with the declaration: `Never` has no services at
all, because every operation on a never-returning expression returns
`Never`.

## 9. Range: one private constructor

`Range t` is a std ADT whose declaration is a row in the standard
library file, not a compiler intrinsic. The row is
`Range: type t = _Range [start: t, end: t, closed: Bool]` (Annex C.2).
The `..` and `..=` operators are its only public construction
(`Clause 6.5.6`). The constructor spelling carries the privacy prefix,
and `Clause 7.3.3` carries the one split in variant visibility:
visibility follows the type; the one exception is a `_`-prefixed
constructor spelling, which is private to the declaring module.
`Range Int` is spellable everywhere; the one constructor spelling that
would build a range directly is not.

The endpoint domain — `Int`, `UInt`, or `Char`, both endpoints one type —
is the operators' compile-time contract (`Clause 6.5.6`). The private
constructor closes direct construction so the domain stays the operators'
alone: `Range Bool` is writable as a type and has no writable value.
Inside, `Range` is a real ADT, so `eq`, ordering, and `show` come free
over it. `inspect` is compiler-owned as `0..9` and `0..=9`, the writable
operator spellings, on the precedent of `NaN` and `Bytes.from_hex("4142")`
(`Clause 9.8.3`). What is shown is what can be written back, and the
private constructor leaves the operator as that form.

**Rejected alternatives:**

- **Placeholder opacity** — the `File` and machine-word route. It would
  give `Range` an opaque representation and hand-write every service as
  a compiler row. `Range` is a plain value type whose `eq`, ordering, and
  `show` come free over a real ADT; opacity would trade inspectable
  declaration data for a table the checker must special-case.
- **A public constructor** — Rust's `Range` struct precedent. The
  endpoint domain is a written contract of the type row in the inventory
  (Annex C.2), not only of the operator; a public constructor would make
  `Range Bool` a spelled, legal value and demote the operators from
  gateway to convenience.
- **Two variants, `Until` and `Through`.** The exclusive/inclusive flag
  would leak into the surface as representation — two constructor names
  for one boolean, a wider constructor set for one value — and buy
  nothing. Rust and Swift split the type instead (`RangeInclusive`,
  `ClosedRange`); the split is the same leak with a compiler-blessed
  spelling.
