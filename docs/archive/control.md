# Design Record — Bindings, Functions, and Control Flow

Non-normative. Rulings: Clauses 6.3, 6.7, 6.8, 7.7, 8.2, 8.3, 8.8. Rejected alternatives, prior art,
and rationale.

## 1. Binding grammar: one grammar, three operators

Every declaration line starts with a name or pattern — the **line-start
invariant**. Markers land on slots: the annotation slot (`type`/`test`),
the arrow markers (`!->`/`?->`), and the binding operators (`=`, `:=`,
`<-`). One grammar, three operators:

- `=` initial binding, carries the optional annotation;
- `:=` rebinding, reuses the existing name, never carries an annotation;
- `<-` inverted binding, whose type comes from the trailing callback's
  parameter.

`:=` and `<-` end in a continuation, not a block opener: only `:`, `=`, and
a `case` arm's `->` open a block (`Clause 6.2.3`); a multi-line rebinding
right-hand side is a continued expression (`Clause 6.2.4`).

`<-` desugars to a trailing callback: `p <- f(a...) rest` is
`f(a..., (p): rest)`. The trailing-callback contract is the prelude's
existing shape (`with_file`, `List.each`, `Result.try`), so `<-` adds zero
new calling conventions — it is pure layout sugar for the position that
already exists. The arrow direction is the data direction: case arms write
`pattern -> value` (value flows to the result), `<-` writes
`call <- pattern` (value flows to the name). The explicit `.try()` at every
link is the visible propagation point — the role `?` plays in Rust and `use`
plays in Gleam — earned by the general callback-slot rule rather than a
dedicated operator; no line propagates an error silently.

**Rejected:** a dedicated error-propagation operator (`?`). A postfix
operator would be a second, invisible propagation channel; `.try()` says
what happens on its line, and `map_err` is the single conversion point.

## 2. Lambdas: zero annotations, capture as delegation

A lambda is `(params): body` with no annotations at all — neither types nor
bits. Three rulings carry this:

- Types and the bit are inferred from context and body; annotations live on
  *named* bindings' arrows, where the exact-match rule can check them
  (`Clause 7.9.2` rule 3). A lambda is always check-only — hence `:=`
  never generalizes (`Clause 7.6.3`).
- Capture is by reference to a snapshot: O(1), no copy, and a rebinding
  never writes back into a captured environment (`Clause 8.2.3`).
- Holding is quiet, calling is loud: a factory that constructs a `!->`
  closure without calling it is itself pure — the `mk_logger` shape
  (`Clause 7.9.2` rule 1). Copying a capability is delegation;
  capture itself sets no bit.

**Rejected:** parameter annotations on lambdas (would duplicate the
annotation slot); block-level self-reference (a name used before its value
exists is a compile error; lifting to module top level is a mechanical
transformation, `Clause 8.6.1`).

## 3. Branching: one primitive, one sugar

`case` is the only branching primitive; `if` is sugar over `case`-on-`Bool`
(`Clause 6.7.1–4`). A separate `match`/`switch` vocabulary never exists:
two spellings for one mechanism violates one-mechanism-per-problem, and
`if` earns its existence only as the two-arm Bool idiom, which is sugar,
not a second primitive. Guards are `PATTERN if GUARD ->` and close with an
unguarded arm — exhaustiveness closes where the guard-free arm sits.

**Rejected:** fallthrough (arms are `pattern -> expression`, nothing
sequences into the next arm); truthiness coercion (conditions and guards
are `Bool`, `True`/`False` the only constructors); multi-scrutinee as a
primitive (`case a, b:` is sugar over a tuple case, `Clause 6.7.2`).

## 4. Patterns: the compositional grammar and its two permanent bans

The pattern grammar is compositional — variables, `_`, literals,
constructors, tuples, records, lists, or-patterns, nested without depth
limit (`Clause 6.8`). Record patterns are *subset* matches with
**checked** field names: a misspelling is a compile error, so a subset
cannot silently under-bind.

| Banned forever | Reason |
|---|---|
| Dictionary patterns | A key is a value, not structure; `Dict.get -> Option` is already the natural branch — a pattern would hide a domain check inside syntax |
| As-patterns `x as p` | Wanting the whole and a part is two bindings; nest a `case` or take the field |
| Repeated binder names in one pattern or across the patterns of one arm (`(x, x)`, `case a, b: x, x -> ...`) | Compile error; one arm's patterns bind together; `_` is the exception (binds nothing, repeats freely) |
| Record/constructor/list patterns in parameter position | The parameter group is identifiers and tuple groups; `(p, i):` is a two-parameter group *shaped like* a tuple, not destructuring — splitting a tuple value is always explicit (binding line or `case`) |

## 5. Never: exhaustiveness belongs to patterns

`Never <: t` adapts a diverging expression into any value position; Eko's
entire subtyping is two relations — bit adaptation and the Never bottom
type (`Clause 7.7.4`). The load-bearing ruling: **exhaustiveness is done
by patterns, never by divergence**. An arm is always `PATTERN ->
expression`; a bare `panic(...)` is not an arm. If divergence could stand
in for a pattern, `Green` unhandled would still compile and the
compile-time guarantee would be hollow — a runtime crash wearing a
compile-time badge.

Unreachable code after a Never call is legal and *warns*: diagnostics have
three severities, and the warning channel exists (`Annex B.4`);
only the bit domain is error-always. Adaptation applies at value positions
only — inside function types, a named `(a) -> Never` does not flow where
`(a) -> Int` is expected; a lambda literal already reaches `u := Never` by
inference, so no function-type subtyping is needed.

## 6. Iteration: no loop syntax

`while`, `for`, `do`, `break`, `continue`, and `loop` are never part of
Eko (`Clause 8.8`). The argument in one line: control flow is machine
primitives (`case`/`if`/calls); iteration is library functions.

- Values are immutable, so a loop variable is the ceremony of mutable
  state — an increment has nowhere to take root, while an accumulator
  carried as a parameter has the same shape with zero assignment.
- A loop body does two jobs — "for each element" and "how to combine" —
  and `each` and `fold` give each job a name: greppable, bit-transparent
  (`?` dispatchers), replaceable one at a time. `while` is an implicit
  `goto`; combinators compose (`xs.filter(f).map(g)`), syntactic loops do
  not.

### Stack guarantees: promise only what every backend can deliver

| Idiom | Guarantee |
|---|---|
| Built-in combinators | native iteration, stack-safe at any length (normative) |
| Self tail recursion | always compiled to a jump (normative) |
| Everything else | no promise; stack depth follows data depth |

Cross-function tail calls are optimized best-effort and explicitly not
promised — backends differ in their tail-call guarantees, and a promise the
specification cannot cash devalues the word. Stack overflow is an abort,
not a catchable panic, same class as memory exhaustion.

### Loop habits and their replacements

| Habit | Eko vocabulary |
|---|---|
| counting `for` | `(a..=b).each/map/fold` |
| traversing `for` | `xs.each/map/...` |
| `while` | self tail recursion + guarded `if`, accumulator as parameter |
| `break` | `find_by`/`any`/`all` — first-hit termination is combinator semantics |
| `continue` | `filter` pre-selection or a guarded branch |
| infinite loop | `-> Never` recursion — a typed form whose return type says it never returns |

**Also rejected:** generators/`yield` — lazy sequences would be their own
ruling with their own semantics; they are not backfilled through loop's
absence. Promising TCO beyond self tail calls — see above. `break`/`continue`
as vocabulary — early termination is combinator semantics, not a control
primitive.
