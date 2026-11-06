# src/check

Clauses 7.5-7.9 — types, generics, world bits

ek_check is a pure entry point, (input, arena, diag) -> output; it
never aborts.

The input is the resolve result. ek_check checks every module of
the closure, root first: a compilation is the whole program, so an
imported module's bodies are checked in the same pass, under their
own file. Imported surfaces supply the signatures references need;
the AST behind an imported signature (symbol->type, symbol->decl)
stays alive in the arena and is readable.

PLACEHOLDER STD BRIDGE (resolve.h): a reference resolve answered
with EK_SYM_EXTERNAL has an unknown-but-fixed type. Each external
reference gets a fresh inference variable at its use site (fresh per
reference, so occurrence-wise monomorphic); unification binds these
variables freely, and no diagnostic ever arises from their opacity.
Member namespaces are complete in the checker, so a member link a
known receiver does not carry is E-unbound-member, not a bridge; a
link on an open receiver type stays opaque.

Output: one row per typed AST node, sorted by node address, so lower
can binary search by node. `type` is a closed snapshot: bound
inference variables are substituted away; a remaining EK_T_IVAR is
one the bridge marked opaque — a placeholder std reference, an
unknown member reading, an injected pair's slot, or a recovery
after an earlier diagnostic. A plain inference variable never
survives: one that no argument and no expected type determines is
rejected with E-undetermined during the module's pass. `reading` is
the selected receiver reading for a member link, EK_RD_NONE
otherwise.

`bit` is the node's world classification. A CALL row carries the bit
of the call itself: pure through `->`, world-touching through
`!->`, and through `?->` the instantiation from the feeder
arguments; inside the body of a declaration whose signature contains
the open bit, a call through a `?` arrow carries that bit. A LAMBDA
row carries the bit derived from its body. An annotated binding row
(LET/REBIND/NAMESPACE over a function annotation) carries the
declared arrow's bit, open included. Every other row carries the OR
of the call bits in its subtree, lambda interiors excluded: only
calls contribute, so a reference to a function value is pure. PURE
where nothing contributes.

Test bindings have no arrow and nothing to reconcile; their rows
carry derived bits all the same, and no bit diagnostic fires inside
them.

OPACITY RULE for the placeholder-std bridge: a call's bit is fixed
only through known arrows ("through `->` it is pure; through `!->`
it is world-touching"); an unknown callee is not covered. A call
through an unknown callee contributes world-touching when its
receiver has a capability type other than `Conc`, which sets no bit,
and the member is not one of the pure derivation combinators
(Out.null/tee/err, In.empty/from_lines, Fs.null/root, Env/Args/
Clock.frozen, Rand.with_seed, Task.join/cancel), or when an argument
is a lambda whose body is world-touching (the open-bit
instantiation applied at opaque dispatchers); it contributes pure
otherwise. The derived services (the equality pair, the ordering
family, show, inspect, with, and the JSON pair) are pure, so a call
naming one contributes no bit whether or not the receiver carries
it. No diagnostic ever arises from the opacity itself: a call
through an unknown callee type is never rejected, it only feeds the
OR that an annotated binding reconciles against.
