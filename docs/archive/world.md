# Design Record — Capabilities and the World Bit

Non-normative. Rulings: Clauses 7.9, 8.4, 8.9, 9.12. Rejected alternatives, prior art,
and rationale.

## 1. The dividing line against label- and row-based designs

The core insight that separates Eko from effect-row designs: **the
capability already carries the authorization granularity; the world bit is
the projection of capability *use* onto arrows.** The two share one
vocabulary (capability names) — the bit introduces no second vocabulary. A
label set mirroring capabilities (`console` ↔ `Out`, `fs` ↔ `Fs`) would be
a permanent synchronization burden describing a distinction the capability
value already carries. This is why the whole effect-system family is
rejected, not merely not chosen (`Clause 7.9`):

| Rejected | Reason |
|---|---|
| Label sets | 1:1 mirroring of capabilities; two vocabularies, one phenomenon |
| Row polymorphism / row algebra / row extension | One bit per arrow leaves nothing to extend |
| Algebraic effects and handlers | World I/O cannot be "handled away"; native continuations are expensive; a different feature with different semantics |
| IO monad | Loses direct style; every higher-order function goes monadic |
| User-defined labels | The slippery slope back to algebraic effects |
| Exceptions or panic in the bit | Errors are values; partiality is an independent, deliberately untracked dimension |
| Anything after `!` or `?` (labels, bit variables) | Reserve syntax saves no cost — the cost lives in semantics and migration, not notation; a future mechanism gets its own syntax and does not parasitize this marker. **Nothing ever follows `!` or `?`.** |

History: an early draft carried a named bit variable (`E`); it was
cancelled in favor of the anonymous `?`, because OR-absorption makes
several bit variables behave identically — a variable with no
distinguishing work to do. The markers were then joined to their arrows as
single tokens `!->`/`?->` (longest-match lexing, no binding rules, no
layout rules — `Clause 6.2.4`).

## 2. Bit semantics: the seven rules and their reasons

- **Only calls contribute.** Holding a capability, storing it, passing a
  function value — none of it touches the world. This is what makes the
  pure factory returning a `!->` closure legal (`mk_logger`).
- **Exact match at annotated bindings.** A missing `!` and a surplus `!`
  are both errors: a dead annotation is as culpable as dead code. The
  one-directional subtyping `(a) -> b <: (a) !-> b` exists only at
  adaptation positions, never the other way.
- **Panic is pure.** Aborting is not world contact — the bit governs world
  contact only; partiality, blocking, and cancellation are non-promises
  stated beside the soundness theorem (`Clause 7.9.7`).
- **`?` is for dispatchers only** — "pure if the callback is pure". A
  higher-order function that is itself world-touching needs no `?`:
  `with_file` is already `!->`, and OR absorbs everything.

## 3. Failure: Result, EOF, and the closed IoError

- **EOF is not an error.** `In.read_line: (In) !-> Result (Option String)
  IoError` yields three exhaustive cases (`Ok Some`/`Ok None`/`Err`) with
  no sentinel value — end of input and failure stay distinguishable.
- **IoError variants earn their names** by being handled in distinct
  branches; the union is closed because adding a variant breaks every
  exhaustive match written against it. Detail failures travel as
  `Other String`. The runtime retries `EINTR` internally, so `Interrupted`
  never surfaces.

## 4. Handles: generations, not file-descriptor numbers

Staleness is decided by the language-level generation counter, not by the
host's descriptor reuse — double-close and use-after-close are checked
errors (`Err Closed`), never undefined behavior. Leaks are allowed: without
`close`, the OS reclaims at exit; no finalizer is promised, consistent with
reclamation being unobservable (`Clause 8.6`).

**No `Rights` type — the combinators are the rights.** A rights datum that
nobody would ever match on is dead weight; `Fs.root` and `Fs.read_only`
are pure, composable, usable above any `->` boundary, and only ever narrow.

## 5. Args and main

- **Why not `main: (argv: []String)`.** The parameter list is the
  authorization protocol (item-by-item type checking; a data parameter
  would create a second kind of parameter and split the protocol). Worse,
  the pure `main: ()` would then be a program that has "seen argv" — the
  no-ambient principle has to hold without exception. `Args.all` yields the
  argument vector *without* the program name: a tool wants arguments, and
  the program name belongs to the launcher's narrative.
- **`main` returns `_` only.** An `Int`-returning form would create an
  exit-code ambiguity for no gain — `0`, `1`, and `2` are already taken by
  normal termination, panic, and compile failure. A chosen exit code is
  `Proc.exit`.

## 6. Concurrency: the determinism argument

`Conc.scope` is an *open-bit dispatcher on a par with `List.map`*, not a
special case. The argument: values are immutable and there is no
inter-task communication, so a task's result is already determined at the
moment of `spawn`, and scheduling order cannot change any result.
Cancellation has explicit sources only (a parent's `cancel` or scope
exit), so program order determines everything. Pure tasks therefore have
no data races and are deterministic.

- `join`/`cancel` observe program-internal state — pure, the mechanical
  consequence of "only observing the world sets the bit". Cancellation is
  not an error, hence `join: (Task b) -> Option b` with `None` meaning
  cancelled.
- Channels and timeouts are excluded: communication introduces observable
  scheduling and would break the determinism argument. No channels, hence
  no deadlock in the language.
- The honest boundary: cancellation applies at world-call boundaries, so a
  non-terminating *pure* task hangs scope exit — consistent with divergence
  not being tracked.

## 7. Unit is `_`

`_` at type position only ever means Unit, never "please infer"
(`Clause 7.5.4`). The motivation was a real hole in an earlier
grammar: `-> ()` parsed as "returns a zero-parameter function", so a unit
return could not be written explicitly at all. `-> _` fills the explicit
form and the elided sugar round-trips. Documents must state this plainly to
prevent misreadings imported from Rust, TypeScript, or Scala, where `_` is
an inference hole.

## 8. World soundness

The theorem and its sketch live in `Clause 7.9.7`: the bit originates only in
an extern call or in a call of a world-touching function value, and every such
value closes, transitively, over an extern or over a bundle unwrapped from a
capability (real bundles are host-constructed, `Clause 10.8`); capabilities
are unforgeable; a closure's bit is on its
arrow; subtyping is one-directional. The corollary is free top-level purity
— the top level holds no capability values, so a bit-setting call is
structurally impossible there. The non-promises are normative: no `!` does
not mean no panic, termination, no blocking, or immunity to cancellation.

## 9. Where the capability surface lives

**Ruling:** the capability types, their members, and the derivation
combinators are declared by the capability module — compiler-embedded Eko
source, part of the language definition (`Clause 9.1.1`). The forging
channel is a compiler-injected pure `wrap`/`unwrap` pair per derivable
capability, visible only inside that module (`Clause 10.8`). `std` and its
companions contain no capability content; `Proc.exit` and the four
concurrency members (`Conc.scope`, `Scope.spawn`, `Task.join`,
`Task.cancel`) are compiler-provided declarations.

**Rejected: members as std source over reserved extern pairs** (the prior
design). It worked — the wall was reserved symbols plus private bindings —
but the forging surface sat inside a swappable unit (the `--std` override
carried it), the capability story was scattered across four clauses, and
the module that "just uses" capabilities held the privilege.

**Rejected: implementation-language intrinsics.** Rejected for three
reasons: world bits would become compiler assertions instead of inferred
facts; the zero-grant proof of `Out.null` (three auditable lines) would
become an unverifiable compiler fact; and every member change would be
compiler surgery instead of an Eko-source edit.

**Rejected names:** `prelude` (prior art means "names opened by default";
the literal meaning, "overture", is no guide); `core` (Rust's core is the
no-OS subset — exactly inverted here, and the name stays unspent for a
future strippable subset, if one ever exists); `lang` (names the locus,
not the content). `cap` chosen: the content names itself. `std` keeps its
name; the framing that makes it honest is "capabilities belong to the
language; the standard library is `std` and its companions".

**Also cancelled:** the `--std` override (formerly `Clause 11.1`). With the
capability layer compiler-fixed, an override would have replaced only the
utility library; the feature's remaining value did not pay for the
interface-compatibility machinery it required.
