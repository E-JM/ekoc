# Design Record — Representation and Execution

Non-normative. Rulings: `07-runtime.md`. Rejected alternatives, prior art,
and rationale.

## 1. The governing principle

**Names hold references; operations see structure.** Binding, argument
passing, capture, and insertion copy an O(1) reference, never a value, and
no operation can observe that two names designate the same object. Value
semantics (`==`, `show`/`inspect`, patterns defined over structure, never
addresses) is what makes sharing undetectable, and undetectable sharing is
what makes the optimization surface of `§5` fully open.

## 2. The acyclicity theorem

Strict evaluation + immutable values + snapshot captures imply the heap
graph is a DAG: number each heap object by construction-completion moment,
and every edge points from a later object to an earlier one. The three
channels a cycle could come through, each closed by an existing ruling:

| Channel | Closed by |
|---|---|
| Mutable write-back (construct, then assign a field backwards) | Values are immutable; field assignment does not exist |
| Lazy suspension (allocate a placeholder, fill later) | Evaluation is strict; the right-hand side is evaluated before the name is bound |
| Capture into an unfinished binding (a closure environment pointing back) | Captures are snapshots; module-top self-reference is a compile-time name fact resolved through static slots, not an edge written during construction |

Four consequences: reference counting is complete (no cycle collector
exists); weak/unowned references have no application; derived services
terminate; cross-task sharing is only a matter of counting.

## 3. Memory strategy: the rejected option space

The full table that was weighed for `07-runtime.md §4`:

| Rejected | Reason |
|---|---|
| Tracing GC (generational / concurrent) | Fails the determinism promise; root scanning and pauses turn memory into a runtime–host negotiation; under the acyclicity theorem, tracing is redundant infrastructure |
| Ownership / borrowing | Free value flow and free closure capture are Eko's temperament; borrowing makes signatures carry a lifetime domain — a per-binding mental tax |
| RC + cycle collector (ORC) | Medicine for cycles that structurally cannot exist |
| Explicit regions / arenas | Closure escape makes region inference unreliable; regions remain implementation- and host-side techniques, never language features |
| Manual management | Memory safety is the floor of the language, not a capability — the floor is free and never explicit |

Reference counting is emitted by code generation — three operations
(allocate, retain, release), no runtime service — which keeps the embedding
contract plain: a host that holds a value holds a reference; no root
registration, no pause negotiation. The descriptive baseline (per-object
count; atomic counts for cross-task and boundary-crossing values, skipped
for task-private; elision where exclusive access is proven) follows the
established ARC route; elision may only reclaim *earlier*, never later.
Streaming boundedness (`xs.each` releases as it scans) is an engineering
dividend recorded as such, not a per-element contract.

## 4. Monomorphization versus uniform boxing

Generics are expanded per instance; layout follows the instance — `[]Float`
elements sit in 8-byte slots while `[]Person` elements are references, so
numeric containers pay no boxing tax. The rejected alternative, uniform
boxing with a runtime dictionary, would reintroduce exactly the runtime
dispatch surface that compile-time type identity removes. The two recorded
costs: machine-code size grows with the number of instances (deduplication
at link time is permanent work), and a non-terminating instantiation
expansion (polymorphic recursion) is a compile error
(`E-instantiation-depth`) — type checking stays free; only the expansion
is paid.

## 5. Recursive positions: the one hard layout rule

Every recursive position is a pointer — the only hard layout constraint.
It buys finite size with zero new well-formedness checks (a base-case-less
recursive type is legal and uninhabited) and termination of the derived
services. Everything else about layout is implementation domain: the
specification fixes shape and magnitude; the implementation fixes bytes and
timing.

## 6. Optimization freedom and the empty observation surface

Unboxing, stack allocation via escape analysis, count elision, and shared
instantiation are all licensed by the fact that an Eko program *cannot*
observe representation: no identity comparison, no address-of, no
finalizers, no memory-footprint vocabulary, no `gc()`. Each excluded
observation is also a guarantee — the exclusions list of `§9` is the
soundness proof of `§5`. The single explicit exception is the `ctype`
freeze (`11-ffi.md §3.3`), scoped to types that asked for it.

## 7. Iteration guarantees

The stack-guarantee ladder (combinators stack-safe; self tail calls always
a jump; everything else unguaranteed) is a *honesty* ruling as much as a
performance one: the specification promises only what every conforming
backend can deliver, and cross-function tail calls are explicitly
best-effort because backends differ. One observable cost is recorded:
frames eliminated by self tail calls do not appear in panic backtraces.
Stack overflow and memory exhaustion are process-level aborts — the same
class — with best-effort diagnostics.
