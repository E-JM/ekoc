# Design Record — The Standard Library and Operators

Non-normative. Rulings: Clauses 7.2.2, 7.4.5, 9.1, 9.8.4, 9.8.7, and 9.15;
Annex C; Clause 11. Rejected alternatives and rationale, restructured from
the design session that named the library, fixed its delivery model, and
settled the operator and equality doctrine.

## 1. The name: `std`

`prelude` was rejected by the designer (Haskell association); `eko` was
rejected because the compiler and the module would share a name in tool
output; `core` was reserved for a possible future core/std split. `std`
matches the concept the designer already uses (the standard library), is
shortest at the few positions where the prefix is written, and survives
every growth scenario (as the reserved stem and as the reserved path
root). The decision is not load-bearing — one reserved stem either way —
but it accretes qualified spellings and generated documentation, so it was
made early and frozen.

## 2. Distribution: source only; the cache is derived per machine

Rust's sysroot and GHC's base ship precompiled: the tradition buys
cold-start time proportional to library size. Eko's library is one module
of a few thousand lines; derivation costs milliseconds, so a shipped
derived artifact buys nothing and costs a second file that must stay in
sync with the source, diluting the claim "the language definition is this
source file". Zig's model transfers exactly: source in the distribution,
cache on first use, keyed by content hash. The interface artifact is a
build cache in the same family as `.hi`/rmeta — nobody audits it, it is
disposable by definition. Object code is lazy and per target; nothing
platform-specific ever enters the distribution. The content-hash lock
refuses accidental tampering. The `--std` override that formerly admitted
deliberate experimentation was cancelled when the capability surface moved
into the compiler (`world.md` §9): an override would have replaced only
the utility library, and the compatibility machinery it required bought
too little.

## 3. Implicit import: a constant, not an analysis

The rejected general form was `import std/{module}` with the compiler
auto-importing what a file uses. Demand-driven injection makes desugaring
a function of usage analysis rather than of surface text — two identical
files could desugar differently, which dissolves the no-sugar form.
All-at-once injection of a per-type module tree opens the union of all
std type names ambiently and burns every common module stem (`string`,
`list`, `io`, …), because module names are flat file stems (Clause 7.2.1) and
directories are not nameable. The adopted rule injects one constant line,
`import std`; companions stay explicit; a companion's stem enters scope
only on import, so nothing is reserved beyond the path prefix. A manual
`import std` warns (`W-redundant-import`) rather than erroring: it is
harmless and almost always a misunderstanding.

## 4. One vocabulary module, not per-type modules

Per-type splitting maximizes exactly the wrong cost: flat stems mean each
module burns a name users want, and the vocabulary is needed by every
file anyway (`Option`, `Result`, `String` pervade patterns and
annotations), so splitting changes nothing about which names a file must
see. Facade splitting fails structurally: forwarding is a function, not a
re-export (Clause 6.10 exclusions), and there are no type aliases (Clause 7),
so a facade cannot carry types. Growth is governed by admission, not
layout — the vocabulary test for std, the common-utility test for
companions. Promotion into std costs every file an ambient name; demotion
breaks bare spellings; membership is conservative. A std type's namespace
is welded to std by ownership (Clause 7.4.5), so convenience members live as free
functions in companions (`xs.listx.windowed(n)`) — the ugly spelling is
the visible price of namespace pollution. Repo-side parts assembled into
one `std.eko` at release remain a zero-language-cost fallback, held in
reserve, not a planned mechanism; a fixed implicit set remains the
escalation if the vocabulary itself outgrows one file.

## 5. `Fs.with_file`, not `with_file`

Three spellings coexisted in the manual: a free function in the Clause 9.12.2
signature table, a receiver form in Clause 6's example, and a namespaced
form in drafts. The receivers-everywhere principle (Clause 7.4.3) decides: a
capability as first parameter makes the function a namespace member. The
free-function spelling was a transcription error; the generic-namespace
precedent (`Option.get`) shows members with type variables are ordinary.

## 6. The free-name set is closed

Six names: `show`, `inspect`, `panic` (compiler-generated) and `assert`,
`assert_eq`, `assert_ne` (std source). They cannot be namespace members:
`show` and `inspect` dispatch over every type and no single namespace can
supply the free spelling — first-class use (`.map(show)`) needs the name
of the dispatcher itself; the assert family is generic over `t` with no
anchor type. Moving the assert family into the compiler was rejected: it
is writable Eko source, and what is writable belongs in the library, not
the compiler. The set is closed permanently — the ambient name surface is
six functions plus the type vocabulary, in every future version. New std
functions require an anchor type; anchor-less utilities belong in
companions.

## 7. Operators: the value layer, only

The doctrine went through three rounds, each toward a smaller domain:
all types → builtins and ADTs → the value layer (`Int`, `UInt`, `Float`,
machine words, `Bool`, `Char`, `String`, `Bytes`). Two arguments drove the
shrinkage.

- **The name trap.** The identity `a == b ≡ a.eq(b)` is load-bearing
  documentation. If users could declare `T.eq` where the operator exists,
  the identity would become conditional, and `a == b` could disagree with
  `a.eq(b)` on the same type. Where the operator exists, the member names
  (`eq`, `ne`, `lt`, …) are compiler-owned; where it does not, they are
  ordinary overridable members. The trap, not taste, draws the line.
- **Uniformity.** An operator is honest exactly where its meaning is
  canonical and compiler-owned: machine values and text. A record's
  equality is a domain decision, and a domain decision is a visible
  function call. This also fixed an asymmetry in the earlier manual: `==`
  covered records while `<` did not; both comparison families now follow
  one rule.

`++` keeps its wider domain (`[]t`, `[k]v`, `String`, `Bytes`): those
types carry dedicated literal and type syntax, so the boundary is a fact
about the grammar, not a judgment. `Bool` joins the value layer: the
truth type is compiler-designated, and `b == True` occurs. `Bool` still
has no ordering — `True | False` is enumeration convenience, not
magnitude.

## 8. Equality: derived structure, not opt-in

Opt-in equality (declare `T.eq` or go without) was rejected on mechanics.
`Set t`, `Dict`, `Option.eq`, and `assert_eq` are generic over `t` and
need `t`'s equality, so the type system would have to carry a constraint
the surface cannot express: the inexpressible-signature exception that
`with` holds alone (Clause 9.8.6) would become a family, and every tested type
would pay a declaration tax. Explicit constraint syntax was rejected as a
language feature against the closed-union philosophy (Clause 6.10 exclusions).
Derivation was adopted with three sharpenings: it materializes on demand;
it composes with declarations at every level (a record whose field type
has a declared `eq` routes through the declaration); and it halts at
capabilities and functions recursively — the old top-level exceptions are
the base cases of one recursive rule, and the diagnostic names the
offending field. `ne` is synthesized from `eq`; a declaration replaces
the derivation entirely. One sentence holds the doctrine: derivation
gives the mechanics, declaration gives the semantics, the operator gives
the value layer.

## 9. JSON: the derived pair is frozen

The override model was withdrawn. Conformance was never the real risk:
`to_json` yields a `Json` value and rendering is one fixed function, so
no per-type code can emit malformed JSON — the conformance duty lives in
the `Json` type. The round-trip law is decisive: the derived pair is an
involution; a one-sided override breaks it silently, and a two-sided one
cannot be checked for it. Representation choices are protocol decisions
and live as named codec pairs in the protocol's module. One overridable
service remains — `show`, the display channel, with `inspect` as the
frozen structural guarantee beneath it.

## 10. Interfaces and diffs

The interface artifact must carry generic bodies (Clause 11.6:
generic bodies travel with the interface) or downstream monomorphization
has nothing to expand — a hard requirement easy to miss in
implementation. The interface diff between compiler versions is the
compatibility verdict, and its station-3 shadowing flag exists because
ordered resolution (Clause 7.4.3) lets a new namespace member capture receiver
calls that previously resolved to the calling module's own function:
evolution's blast radius is made explicit mechanically. Annex C is
generated from source and verified in CI so the normative inventory can
never drift from the shipped library.
