# Design Record — Types and Generics

Non-normative. Ruling clauses: 7.5, 7.6. Rejected alternatives, prior art, and
rationale.

## 1. Declarations

`type` lives in the annotation slot, so a type declaration has the same head
as every binding: the line starts with a name (`Name ':'`). The right-hand
side is a **declaration form only** — variant list or bracketed
record/tuple — which is the no-aliases ruling: naming never renames an
existing identity, so `type` cannot smuggle in a synonym and nominal identity
is never inherited by spelling.

## 2. Nominality

**Identity is bought explicitly; an anonymous form is a view.** Same-shaped
declarations never interconvert (`Celsius` vs `Fahrenheit`), and a nominal
value flows one-directionally into a same-shape anonymous form at exactly
three positions — argument, operand, expected type — never back. Same-shape
is exact, not a width relation: adding a field to `Person` breaks every
same-shape anonymous signature, and it should.

## 3. Composites

`[...]` at type position, `{...}` at value position, zero overlap
(`Clause 6.6`). A composite literal never stands alone: the expected type
resolves list-vs-tuple and dictionary-vs-record, the same machinery that types
lambda parameters. The single localized exception is `with`'s partial literal,
legal only in that argument position and descending through record field
values only (`Clauses 6.6.3, 9.8.6`). `Set` is prelude `Set t` with no
notation and no literal. A tuple has at least two elements; `(Int)` is a
parenthesized type, `()` the zero-parameter function type, and the unit is
`_`.

## 4. Generic application

Juxtaposition without commas: `Value Int Float`, composite arguments
parenthesized, the chain stopping at `,` and `->` (`Clause 7.6.4`). The
governing convention is *application by adjacency, enumeration by commas* —
`[k]v` has no comma, `[a, b]` does.

Type variables are lowercase. Case doctrine puts PascalCase on the identity
layer and bindings in snake_case; `Tree T` vs `Tree t` is unreadable at one
character's distance in unseparated adjacency; and the uppercase scheme had to
confiscate single-letter uppercase type names. The recorded cost: in a
right-hand side and in patterns, case no longer signals the domain — position
settles the parse.

Binders are listed after `type` and filled positionally. Collecting variables
by first occurrence fails silently: reordering variants would flip the meaning
of positional arguments at distant use sites.

## 5. Dangling variables

A declaration binder must occur in the right-hand side (phantom ban), or
`==`, `show`, and conversion semantics would depend on a position no value can
distinguish. A function annotation's variable must occur in at least one
parameter type (result-only ban) — `Set.empty: () -> Set t` is why no such
word exists (`Clause 9.10.3`). The open-bit prohibition (`Clause 7.9.3`) is
the same rule on the bit axis. Polymorphic recursion comes free: the type
comes from the annotation, not from inference.

## 6. Recursive types

A right-hand side may self-refer, and **every recursive position is a
pointer** (`Clause 8.5.5`): size is finite and opening recursion needed zero
new well-formedness checks. A recursive type with no base case is legal and
uninhabited. Derived services terminate by the acyclicity theorem
(`Clause 8.6.1`).

## 7. Constructors

Constructors are anchored, not layered: same-named constructors of different
types coexist, anchored by the expected type in value position or the
scrutinee type in pattern position, and an unanchored reference is legal only
when exactly one visible constructor of that name is non-generic
(`Clause 7.4.6`). Application is check-only — the instantiation comes from the
expected type.

A bare generic constructor has no self-anchoring equality: `Leaf 1 == Leaf 2`
is an error; with one side a variable the instance is determined (cross-
seeding); single-variant unions self-anchor, so `Red == Green` stays legal.
An unanchored generic constructor never instantiates bottom-up from its
payload: `x := Some 1` is `E-undetermined` and wants an annotation
(`Clause 7.6.4`).

## 8. `Never`

`Never` is the dual of `_`: `_` is the unit type with one value, `Never` the
bottom type with none. It resolves as vocabulary in every file, cannot be
declared, has no namespace, and a member access on it types as `Never` with no
evaluation. The empty right-hand side is removed from the grammar: a type
declaration always carries a variant list, a record, or a tuple.

Two consequences. Exhaustiveness belongs to patterns, never to divergence
(`control.md` §5): a `Never` arm's pattern covers like any other, and a case
whose arms all diverge is exhaustive with type `Never`, like `Clause 7.7.4`. And
`Never` has no services at all: every operation on a never-returning
expression returns `Never`.

## 9. Range

`Range: type t = _Range [start: t, end: t, closed: Bool]` is a std ADT row
(Annex C.2), not an intrinsic. `..` and `..=` are its only public
construction (`Clause 6.5.6`); the `_`-prefixed constructor spelling is
module-private under the one variant-visibility split (`Clause 7.3.3`). The
endpoint domain — `Int`, `UInt`, or `Char`, both endpoints one type — is the
operators' compile-time contract (`Clause 6.5.6`), so `Range Bool` is a
writable type with no writable value. Inside, `Range` is a real ADT, so `eq`,
ordering, and `show` come free; `inspect` is compiler-owned as `0..9` /
`0..=9`, the writable operator spellings (`Clause 9.8.3`).

## 10. Rejected surface

| Area | Rejected | Reason (clause) |
|---|---|---|
| Declaration | aliases (`Name: type = <expr>`) | naming never renames an identity (`6.9.5`) |
| Nominality | row polymorphism, width subtyping | same-shape exactness is the contract (`7.5.5`) |
| Nominality | automatic conjunction for mixed operands | operand type is fixed by the left operand; reopens the temperature accident (`7.5.5`) |
| Nominality | shape adaptation in containers and function types | shapes carry identity; bit adaptation is exempt because bits are erased (`7.5.5`) |
| Nominality | anonymous ADTs | no union form outside a right-hand side |
| Nominality | `Name(x)` conversion functions | conversion is a literal at the expected position or a rebuild (`7.5.2`) |
| Composite | value-position `(...)` tuples | parentheses are parameter groups, calls, grouping |
| Composite | `Set` notation or literal | `{a, b}` is already lists and tuples; four-form closure |
| Composite | zero-element tuple | `()` is the zero-parameter function type; the unit is `_` |
| Generic | `Value<Int, Float>` | operator ambiguity, `>>` nesting, surface break |
| Generic | `Value[Int, Float]` | collides with bracketed type forms (`6.6`) |
| Generic | `Value(Int, Float)` | collides with call syntax |
| Generic | `Value Int, Float` | `(Value Int, Float) -> Int` can only parse as two parameters |
| Generic | prefix binder `Value u t: type` | breaks the `Name ':'` declaration head |
| Generic | uppercase implicit variables | a misspelled type name silently becomes a variable (`7.6.4`) |
| Generic | ML `'a` | a new lexical class; recorded as a fallback |
| Generic | variables by first occurrence | reordering variants flips distant positional arguments |
| Constructor | visibility layering (std beneath user) | forces a winner where the anchor should decide (`7.4.6`) |
| Constructor | payload-driven inference | a second source of instantiation truth; nullary constructors never qualify |
| `Never` | std row + name-granted bottomness (Swift) | name magic in the checker, an import-layer anomaly, a one-customer grammar production |
| `Never` | every zero-variant type is bottom (Lean) | a family of incompatible bottoms with no customer |
| Range | placeholder opacity (the `File` route) | trades free services for a checker table |
| Range | public constructor | makes `Range Bool` a legal value; demotes operators from gateway |
| Range | `Until`/`Through` variants | the flag leaks into the surface; Rust and Swift split the type instead |
