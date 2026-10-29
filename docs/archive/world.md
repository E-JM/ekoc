# Design Record — Capabilities and the World Bit

Non-normative. Ruling clauses: 7.9, 8.4, 8.9, 9.12. Rejected alternatives,
prior art, and rationale.

## 1. The dividing line

**The capability carries the authorization granularity; the world bit is the
projection of capability *use* onto arrows.** Both use one vocabulary
(capability names), so the bit adds no second vocabulary. The whole
effect-system family is rejected, not merely unchosen (`Clause 7.9`):

| Rejected | Reason |
|---|---|
| Label sets | 1:1 mirroring of capabilities; two vocabularies, one phenomenon |
| Row polymorphism / row algebra | one bit per arrow leaves nothing to extend |
| Algebraic effects and handlers | world I/O cannot be handled away; native continuations are expensive |
| IO monad | loses direct style |
| User-defined labels | the slope back to algebraic effects |
| Exceptions or panic in the bit | errors are values; partiality is deliberately untracked |
| Anything after `!` or `?` | the cost lives in semantics and migration, not notation |

An early draft carried a named bit variable (`E`), cancelled because
OR-absorption makes several bit variables behave identically. The markers were
then joined to their arrows as single tokens `!->`/`?->` (`Clause 6.2.4`).

## 2. Bit semantics

**Only calls contribute.** Holding a capability, storing it, passing a
function value — none of it touches the world, which is why the pure factory
returning a `!->` closure is legal (`mk_logger`). **Exact match at annotated
bindings**: a missing and a surplus `!` are both errors; the subtyping
`(a) -> b <: (a) !-> b` exists only at adaptation positions. **Panic is
pure** — the bit governs world contact only; partiality, blocking, and
cancellation are non-promises stated beside the theorem (`Clause 7.9.7`).
**`?` is for dispatchers only**: `with_file` is already `!->`, and OR absorbs
everything.

## 3. Failure

EOF is not an error: `In.read_line: (In) !-> Result (Option String) IoError`
yields three exhaustive cases with no sentinel, so end of input and failure
stay distinguishable. `IoError` variants earn their names by being handled in
distinct branches; the union is closed because adding a variant breaks every
exhaustive match against it. Detail failures travel as `Other String`; the
runtime retries `EINTR`, so `Interrupted` never surfaces.

## 4. Handles

Staleness is decided by the language-level generation counter, not by the
host's descriptor reuse: double-close and use-after-close are checked errors
(`Err Closed`), never undefined behavior. Leaks are allowed — without
`close`, the OS reclaims at exit, and no finalizer is promised
(`Clause 8.6`). **No `Rights` type: the combinators are the rights.** A rights
datum nobody matches on is dead weight; `Fs.root` and `Fs.read_only` are pure,
composable, usable above any `->` boundary, and only ever narrow.

## 5. Args and `main`

`main`'s parameter list is the authorization protocol, so a data parameter
would create a second kind of parameter and split it; worse, a pure
`main: ()` would be a program that has "seen argv". `Args.all` yields the
argument vector without the program name, which belongs to the launcher.
`main` returns `_` only: `0`, `1`, and `2` are already taken by normal
termination, panic, and compile failure; a chosen code is `Proc.exit`.

## 6. Concurrency

`Conc.scope` is an open-bit dispatcher on a par with `List.map`. Values are
immutable and there is no inter-task communication, so a task's result is
determined at `spawn` and scheduling order cannot change it; cancellation has
explicit sources only. Pure tasks therefore have no data races and are
deterministic. `join`/`cancel` observe program-internal state and are
therefore pure; `join: (Task b) -> Option b` with `None` meaning cancelled.
Channels and timeouts are excluded — communication introduces observable
scheduling — hence no deadlock in the language. The honest boundary:
cancellation applies at world-call boundaries, so a non-terminating pure task
hangs scope exit.

## 7. Unit is `_`

`_` at type position only ever means Unit, never "please infer"
(`Clause 7.5.4`). The motivation was a real hole: `-> ()` parses as "returns a
zero-parameter function", so a unit return could not be written explicitly at
all. `-> _` fills the explicit form and the elided sugar round-trips — stated
plainly, because Rust, TypeScript, and Scala readers import the opposite
reading.

## 8. Soundness

The theorem and sketch live in `Clause 7.9.7`: the bit originates only in an
extern call or in a call of a world-touching function value, and every such
value closes transitively over an extern or over a bundle unwrapped from a
capability (`Clause 10.8`, real bundles are host-constructed). Capabilities
are unforgeable; a closure's bit is on its arrow; subtyping is
one-directional. Corollary: free top-level purity — the top level holds no
capability values. The non-promises are normative: no `!` does not mean no
panic, no non-termination, no blocking, no cancellation immunity.

## 9. Where the capability surface lives

The capability types, their members, and the derivation combinators are
declared by the capability module, `cap` — compiler-embedded Eko source, part
of the language definition (`Clause 9.1.1`). The forging channel is a compiler-
injected pure `wrap`/`unwrap` pair per derivable capability, visible only
inside that module (`Clause 10.8`). `std` and its companions hold no
capability content; `Proc.exit` and the four concurrency members are
compiler-provided declarations.

`cap` names itself; the framing that keeps `std` honest is "capabilities
belong to the language; the standard library is `std` and its companions".
## 10. Rejected surface

| Area | Rejected | Reason |
|---|---|---|
| Placement | members as std source over reserved externs | the forging surface sat in a swappable unit, and the module that "just uses" capabilities held the privilege |
| Placement | implementation-language intrinsics | world bits become compiler assertions; the zero-grant proof of `Out.null` becomes unverifiable; every member change is compiler surgery |
| Name | `prelude` | prior art means "names opened by default" |
| Name | `core` | Rust's core is the no-OS subset — inverted here; kept unspent for a future subset |
| Name | `lang` | names the locus, not the content |
| Delivery | `--std` override (formerly `11.1`) | would have replaced only the utility library, not paying for its compatibility machinery |
