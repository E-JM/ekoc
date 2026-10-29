# Design Record — Bindings, Functions, and Control Flow

Non-normative. Ruling clauses: 6.3, 6.7, 6.8, 7.7, 8.2, 8.3, 8.8. Rejected
alternatives and rationale.

## 1. Binding grammar

Every declaration line starts with a name or pattern; markers land on slots —
the annotation slot (`type`/`test`), the arrow markers (`!->`/`?->`), and the
binding operators (`Clause 6.3`). `=` binds initially and may carry an
annotation; `:=` rebinds the same name and never does; `<-` binds from the
trailing callback's parameter.

`:=` and `<-` end in a continuation: only `:`, `=`, and a `case` arm's `->`
open a block (`Clause 6.2.3`), so a multi-line rebinding is a continued
expression (`Clause 6.2.4`). `p <- f(a) rest` desugars to `f(a, (p): rest)` —
the prelude's existing trailing-callback shape (`with_file`, `List.each`,
`Result.try`), so `<-` adds no calling convention. The arrow follows the data:
`pattern -> value`, `call <- pattern`. `.try()` at each link is the visible
propagation point — the postfix operator would be a second, invisible channel.

## 2. Lambdas

A lambda is `(params): body`: no type annotations, no bit. Both are inferred
from context and body; annotations live on named bindings' arrows, where the
exact-match rule checks them (`Clause 7.9.2` rule 3). A lambda is check-only,
hence `:=` never generalizes (`Clause 7.6.3`). Capture is a reference to a
snapshot — O(1), no copy, no write-back (`Clause 8.2.3`). Holding is quiet,
calling is loud: a factory building a `!->` closure without calling it is
pure (`mk_logger`, `Clause 7.9.2` rule 1), and copying a capability is
delegation.

## 3. Branching

`case` is the only primitive; `if` is sugar over `case`-on-`Bool`
(`Clauses 6.7.1–6.7.4`), earning its place as the two-arm Bool idiom. Guards
are `PATTERN if GUARD ->` and close with an unguarded arm, which is where
exhaustiveness closes.

## 4. Patterns

The grammar is compositional — variables, `_`, literals, constructors, tuples,
records, lists, or-patterns, nested without depth limit (`Clause 6.8`). Record
patterns match a subset, with field names **checked**: a misspelling is a
compile error, so a subset cannot silently under-bind.

## 5. Never

`Never <: t` adapts a diverging expression into any value position; Eko's
subtyping is exactly that plus bit adaptation (`Clause 7.7.4`). Exhaustiveness
is decided by patterns, never by divergence: an arm is always `PATTERN ->
expression` and a bare `panic(...)` is not an arm, so an unhandled `Green`
cannot compile. Unreachable code after a `Never` call is legal and warns
(`Annex B.4`); only the bit domain is error-always. Adaptation applies at
value positions only — `(a) -> Never` does not flow where `(a) -> Int` is
expected.

## 6. Iteration

`while`, `for`, `do`, `break`, `continue`, `loop` are never part of Eko
(`Clause 8.8`): values are immutable, so a loop variable has nowhere to take
root, and `each`/`fold` give the body's two jobs ("for each" and "how to
combine") separate names — greppable, bit-transparent, replaceable one at a
time.

| Idiom | Stack guarantee |
|---|---|
| Built-in combinators | native iteration, stack-safe at any length (normative) |
| Self tail recursion | always compiled to a jump (normative) |
| Everything else | none; stack depth follows data depth |

Cross-function tail calls are best-effort and explicitly not promised
(`Clause 8.8.4`); a promise the specification cannot cash devalues the word.
Stack overflow is an abort, not a catchable panic.

| Loop habit | Eko vocabulary |
|---|---|
| counting `for` | `(a..=b).each/map/fold` |
| traversing `for` | `xs.each/map/...` |
| `while` | self tail recursion + guarded `if`, accumulator as parameter |
| `break` | `find_by`/`any`/`all` |
| `continue` | `filter` pre-selection or a guarded branch |
| infinite loop | `-> Never` recursion |

## 7. Rejected surface

| Area | Rejected | Reason (clause) |
|---|---|---|
| Binding | dedicated `?` propagation | second, invisible channel; `.try()` names what happens on its line |
| Lambda | parameter annotations | duplicate the annotation slot |
| Lambda | block-level self-reference | use before value exists is an error; lifting to module top is mechanical (`8.6.1`) |
| Branching | `match`/`switch` | two spellings for one mechanism |
| Branching | fallthrough | arms are `pattern -> expression` |
| Branching | truthiness | conditions are `Bool`; `True`/`False` are the only constructors |
| Branching | multi-scrutinee primitive | `case a, b:` is sugar over a tuple case (`6.7.2`) |
| Pattern | dictionary patterns | a key is a value, not structure; `Dict.get -> Option` is the branch |
| Pattern | as-patterns `x as p` | whole and part is two bindings |
| Pattern | repeated binders in one arm | `E-name-conflict`; `_` binds nothing and repeats freely |
| Pattern | patterns in parameter position | splitting a tuple value is always explicit |
| `Never` | divergence as an arm | hollow compile-time exhaustiveness |
| `Never` | `Never` inside function types | not needed; lambda literals already infer it |
| Iteration | generators / `yield` | its own ruling, not backfilled through loop's absence |
| Iteration | promised inter-function TCO | backends differ (`8.8.4`) |
| Iteration | `break`/`continue` vocabulary | early termination is combinator semantics |
