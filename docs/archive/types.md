# Design Record — Types and Generics

Non-normative. Rulings: `04-types.md`. Rejected alternatives, prior art,
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
| Row polymorphism / width subtyping | Same-shape exactness is the contract; rows reintroduce silent structural drift (`04-types.md §5.2`) |
| Automatic conjunction for mixed operands | Operand typing is fixed by the left operand; searching for a common anonymous supertype would reopen the temperature accident from the equality side (`§5.3`) |
| Shape adaptation inside containers and function types | Shapes carry identity, so they do not move freely; bit adaptation is exempt because bits are erased (`§5.3`) |
| Anonymous ADTs | No union form outside a right-hand side; adaptation stays records-and-tuples only |
| `Name(x)` conversion functions and "type names in value position" | Explicit conversion is a literal at the expected position or a field-by-field rebuild (`§5.1`) |

## 3. The four composites and two bracket pairs

`[...]` at type position, `{...}` at value position, zero overlap
(`04-types.md §2`). A composite literal never stands alone — the expected
type resolves list-vs-tuple and dictionary-vs-record, the same machinery
that types lambda parameters. The single localized exception is `with`'s
partial literal (`08-builtins.md §6.6`).

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
(`04-types.md §7.2`). The full option space that was weighed:

| Candidate | Verdict |
|---|---|
| `Value<Int, Float>` | Rejected: operator ambiguity, `>>` nesting damage, and a surface break in a language whose other applications are by adjacency |
| `Value[Int, Float]` | Rejected: fatal collision with the bracketed type forms of `§2` |
| `Value(Int, Float)` | Rejected: collides with call syntax |
| `Value Int, Float` | Rejected — the decisive case: a function parameter list already separates parameters with commas, so `(Value Int, Float) -> Int` can only parse as two parameters; writing a single argument would need double parentheses in exactly the position where function types appear most often |
| Prefix binders `Value u t: type` | Rejected: breaks the `Name ':'` declaration-head invariant |
| Uppercase multi-letter implicit variables | Rejected: a misspelled type name silently becomes a variable (`§7.2`) |
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
  exists (`09-library.md §5.2`).

The open-bit result-only prohibition (`06-world.md §5.2`) is the same
rule on the bit axis: bits and types run on one machine. Polymorphic
recursion comes free because the type comes from the annotation rather
than inference, and a recursive call may use any instantiation.

## 6. Recursive types: open recursion, indirect positions

A right-hand side may self-refer; **every recursive position is a pointer**
(`07-runtime.md §2`), so size is always finite and opening recursion needed
zero new well-formedness checks. A recursive type with no base case is
legal and has zero inhabitants — inhabitance is not checked, the same
spirit as `Never` and `[]Never`. Derived services terminate on recursive
structure by the acyclicity theorem (`07-runtime.md §3`).

## 7. Constructors and equality anchoring

Constructor application is check-only — it needs an expected type, because
the instantiation comes from that type. A bare generic constructor has no
self-anchoring equality: `Leaf 1 == Leaf 2` is an error (no unique union to
anchor the instantiation); if one side is a variable the instance is
determined and the equality is legal — cross-seeding. Constructors of
single-variant unions self-anchor, so `Red == Green` stays legal.
