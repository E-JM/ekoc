# Design Record — Representation and Execution

Non-normative. Ruling clauses: 8.5–8.8, 8.10. Rejected alternatives and
rationale.

## 1. The governing principle

**Names hold references; operations see structure.** Binding, argument
passing, capture, and insertion copy an O(1) reference, never a value, and no
operation observes that two names designate one object. Structural value
semantics (`==`, `show`/`inspect`, patterns) is what makes sharing
undetectable, and undetectable sharing is what keeps the optimization surface
of `Clause 8.10` fully open.

## 2. Acyclicity

Strict evaluation + immutability + snapshot capture imply a DAG: number each
object by construction-completion moment, and every edge points from a later
object to an earlier one. The three channels a cycle could use, each closed:

| Channel | Closed by |
|---|---|
| Mutable write-back | values are immutable; field assignment does not exist |
| Lazy suspension | evaluation is strict; the right-hand side is evaluated before binding |
| Capture into an unfinished binding | captures are snapshots; module-top self-reference is a static name fact, not an edge |

Consequences: reference counting is complete (no cycle collector exists),
weak references have no application, derived services terminate, cross-task
sharing is only counting.

## 3. Memory strategy

Reference counting is emitted by code generation — allocate, retain, release,
no runtime service — so a host that holds a value holds a reference: no root
registration, no pause negotiation. Counts are atomic only across tasks and
boundaries; elision may reclaim *earlier*, never later. Streaming
boundedness (`xs.each` releases as it scans) is an engineering dividend, not a
per-element contract.

## 4. Monomorphization

Generics expand per instance and layout follows the instance: `[]Float`
elements sit in 8-byte slots, `[]Person` elements are references, so numeric
containers pay no boxing tax. Two recorded costs: code size grows with
instance count (link-time deduplication is permanent work), and a
non-terminating expansion is `E-instantiation-depth`.

## 5. Recursive positions

Every recursive position is a pointer — the only hard layout constraint. It
buys finite size with no new well-formedness checks (a base-case-less
recursive type is legal and uninhabited) and termination of derived services.
Everything else about layout is implementation domain: the specification fixes
shape and magnitude, the implementation fixes bytes and timing.

## 6. Optimization freedom

Unboxing, stack allocation via escape analysis, count elision, and shared
instantiation are licensed by one fact: an Eko program cannot observe
representation — no identity comparison, no address-of, no finalizers, no
`gc()`. Each excluded observation is a guarantee, and `Clause 8.11`'s
exclusions list is the soundness proof of `Clause 8.10`. The single scoped
exception is the `ctype` freeze (`Clause 10.3.3`).

## 7. Iteration guarantees

The stack-guarantee ladder (combinators stack-safe, self tail calls always a
jump, everything else unguaranteed) is an honesty ruling first: promise only
what every conforming backend delivers (`Clauses 8.8.3, 8.8.4`). One cost is
visible — frames eliminated by self tail calls are absent from panic
backtraces. Stack overflow and memory exhaustion are process-level aborts with
best-effort diagnostics.

## 8. Rejected surface

| Area | Rejected | Reason (`8.6`) |
|---|---|---|
| Representation | uniform boxing with a runtime dictionary | a runtime dispatch surface compile-time type identity removes |
| Memory | tracing GC | fails the determinism promise; redundant under acyclicity |
| Memory | ownership / borrowing | lifetimes in signatures tax every binding; kills free capture |
| Memory | RC + cycle collector | medicine for cycles that cannot exist |
| Memory | regions / arenas | closure escape makes inference unreliable |
| Memory | manual management | memory safety is the floor, not a capability |
