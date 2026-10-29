# Design Record — The Standard Library and Operators

Non-normative. Ruling clauses: 7.2.2, 7.4.5, 9.1, 9.8.4, 9.8.7, 9.15, 11;
Annex C. Rejected alternatives and rationale for the library's name, delivery,
and operator doctrine.

## 1. The name

`std` — the concept the users already speak, shortest at the few positions
where the prefix is written, and it survives every growth scenario as the
reserved stem and path root. Rejected: `prelude` (Haskell association),
`eko` (would share a name with the compiler in tool output), `core` (reserved
for a possible future core/std split). The decision is not load-bearing — one
reserved stem either way — but it accretes qualified spellings and generated
documentation, so it was made early and frozen (`Clause 7.2.2`).

## 2. Delivery

Source only; the derived interface is a per-machine build cache keyed by
content hash, in the same family as `.hi`/rmeta — disposable, unaudited.
Rust's sysroot and GHC's base ship precompiled and pay cold-start proportional
to library size; Eko's library is one module of a few thousand lines, so a
shipped artifact buys nothing and costs a second file to keep in sync,
diluting the claim "the language definition is this source file". Object code
is lazy and per target, so nothing platform-specific enters the distribution.
The content-hash lock refuses accidental tampering.

## 3. Implicit import

One constant line: `import std.{assert, assert_eq, assert_ne}`. Companions
stay explicit, and a companion's stem enters scope only on import, so nothing
is reserved beyond the path prefix. The injected list is a language constant —
adding or removing a name is a language revision, not a library update. A
manual `import std` or a list confined to the injected names warns
(`W-redundant-import`) rather than erroring.

## 4. One vocabulary module

Per-type splitting maximizes the wrong cost: flat stems mean each module burns
a name users want (`Clause 7.2.1`), while the vocabulary is needed by every
file anyway.
Facade splitting fails structurally — forwarding is a function, not a
re-export (`Clause 6.10`), and there are no type aliases, so a facade cannot
carry types. Growth is governed by admission: the vocabulary test for `std`,
the common-utility test for companions. A `std` type's namespace is welded to
`std` by ownership (`Clause 7.4.5`), so convenience members live as free
functions in companions (`xs.listx.windowed(n)`) — the ugly spelling is the
visible price of namespace pollution.

## 5. `Fs.with_file`

Receivers-everywhere (`Clause 7.4.3`) decides: a capability as first parameter
makes the function a namespace member, and the `Clause 9.12.2` signature table
is corrected to `Fs.with_file`; the generic-namespace precedent (`Option.get`)
shows members with type variables are ordinary. The free-function spelling was
a transcription error.

## 6. Operators: the value layer only

The doctrine shrank three times — all types → builtins and ADTs → the value
layer (`Int`, `UInt`, `Float`, machine words, `Bool`, `Char`, `String`,
`Bytes`). Two arguments:

- **The name trap.** `a == b ≡ a.eq(b)` is load-bearing documentation. If
  users could declare `T.eq` where the operator exists, the identity would
  become conditional and `a == b` could disagree with `a.eq(b)`. Where the
  operator exists the member names are compiler-owned; where it does not they
  are ordinary overridable members.
- **Uniformity.** An operator is honest where its meaning is canonical and
  compiler-owned: machine values and text. A record's equality is a domain
  decision, and a domain decision is a visible call. This also fixed the
  earlier asymmetry where `==` covered records and `<` did not.

`++` keeps its wider domain (`[]t`, `[k]v`, `String`, `Bytes`): those types
carry dedicated literal and type syntax, so the boundary is a grammar fact.
`Bool` joins the value layer — `b == True` occurs — but has no ordering.

## 7. Equality is derived

Opt-in equality fails on mechanics: `Set t`, `Dict`, `Option.eq`, and
`assert_eq` need `t`'s equality, so the type system would carry a constraint
the surface cannot express — the inexpressible-signature exception that
`with` holds alone (`Clause 9.8.6`) would become a family, and every tested
type would pay a declaration tax. Derivation materializes on demand, composes
with declarations at every level, and halts at capabilities and functions
recursively; `ne` is synthesized from `eq`. One sentence holds it: derivation
gives the mechanics, declaration gives the semantics, the operator gives the
value layer.

## 8. JSON: the pair is frozen

`to_json` yields a `Json` value and rendering is one fixed function, so no
per-type code can emit malformed JSON — the conformance duty lives in the
`Json` type. The round-trip law decides the rest: the derived pair is an
involution, a one-sided override breaks it silently and a two-sided one cannot
be checked for it. Representation choices are protocol decisions and live as
named codec pairs in the protocol's module. `show` remains the one overridable
service, with `inspect` as the frozen structural guarantee beneath it; the
free-call identity `show(t) ≡ t.show()` is `Clause 6.5.2`.

## 9. Interfaces and diffs

The interface artifact must carry generic bodies (`Clause 11.6`), or
downstream monomorphization has nothing to expand. The interface diff between
compiler versions is the compatibility verdict; its station-3 shadowing flag
exists because ordered resolution (`Clause 7.4.3`) lets a new namespace member
capture receiver calls that previously resolved to the calling module's own
function. Annex C is generated from source and verified in CI, so the
normative inventory cannot drift from the shipped library.

## 10. Rejected surface

| Area | Rejected | Reason (clause) |
|---|---|---|
| Name | `prelude`, `eko`, `core` | prior-art meaning, tool-output collision, reserved subset |
| Delivery | shipped derived artifacts | a second file to drift; derivation costs milliseconds |
| Delivery | `--std` override | replaced only the utility library once capabilities left std |
| Import | demand-driven (`import std/{module}`) | desugaring becomes a function of usage, not of surface text |
| Import | all-at-once type injection | opens the union of all type names and burns every common stem |
| Layout | per-type modules | burns names users want; the vocabulary is needed by every file |
| Layout | facade modules | forwarding is not re-export; no aliases to carry types |
| Spelling | free `with_file` | a capability first parameter makes it a member (`7.4.3`) |
| Operator | operators beyond the value layer | users could declare `T.eq`, making the identity conditional |
| Operator | record `==` with `<` absent | one rule for both comparison families |
| Equality | opt-in `T.eq` | needs an inexpressible constraint; a per-type declaration tax |
| Equality | explicit constraint syntax | against the closed-union philosophy (`6.10`) |
| JSON | overridable `to_json`/`from_json` | breaks the involution |
| Import | compiler-owned assert family | writable Eko belongs in the library, not the compiler |
