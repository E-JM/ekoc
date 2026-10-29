# 7. Static Semantics

## 7.9 Capabilities and the World Bit

A capability value carries authority for a class of world contact. The world
bit records whether a call may use a capability (Clause 4.5).

### 7.9.1 Capability Types

Capability types are built-in nominal opaque types. They are unforgeable and
copyable. A program MUST NOT fabricate a real capability; construction of a
real capability belongs to the host and to the parameters of `main` (Clause
7.1.3). A program MAY construct a derived capability purely (Clause 9.14.5).

| Capability | Surface | Grant | Sets bit |
|---|---|---|---|
| `Out` | session write | birth-granted | yes |
| `In` | session read | birth-granted | yes |
| `Fs` | file system | birth-granted | yes |
| `Env` | environment variables | birth-granted | yes |
| `Args` | startup arguments | birth-granted | yes |
| `Clock` | time | opt-in | yes |
| `Rand` | entropy | opt-in | yes |
| `Proc` | process control | opt-in | yes |
| `Conc` | parallel resources | opt-in | no |

Birth-granted capabilities are supplied by the host at startup. Opt-in
capabilities are requested explicitly by naming them in `main`'s parameter
list. A capability type MUST NOT occur twice in that list; the list is the
whole operating-system surface, and no real capability outside it is reachable
(Clause 10.1.2). There is no ambient I/O: no global `print`, `println`,
`input`, or `File.*` binding exists, and world contact is authorized through
`main`'s parameters or a pure derivation (Clause 9.14.5). The members of the
capability namespaces are declared by the capability module (Clause 9.1.1).

A capability value MAY be copied, passed, returned, stored, and captured.
Copying is delegation. Holding or capturing a capability sets no bit (Clause
7.9.2).

A capability has no `eq`; equality, keying, or assertion over it is `E-no-eq`
(Clause 9.8.4). `show` prints `<cap>` (Clause 9.8.3).

A capability is necessary but not sufficient for world contact; bit assignment
is per operation (Clause 7.9.2). `Conc` authorizes parallel resources and sets
no bit; `Task.join` and `Task.cancel` are pure (Clause 8.9.2). Derived
capabilities are constructed by pure combinators and set no bit at
construction or holding (Clauses 9.12.5, 9.14.5); the attenuation combinators
only narrow the grant. A derivation constructs a capability value, never
authority: the grant of a derived capability is contained in the grants of the
capabilities it consumes, and a zero-input derivation yields a zero-grant
value. There is no capability for native code; the native track is authorized
by declaration and linking (Clause 10.1.2).

### 7.9.2 The World Bit

A function type carries a world bit: pure (`->`), world-touching (`!->`), or
open (`?->`) (Clause 7.9.3).

The bit of a body is the OR of the bits of all call positions in the body.
Only calls contribute; referencing a function, storing a capability, passing a
function value, and capturing a capability set no bit.

| Left | Right | Result |
|---|---|---|
| pure | pure | pure |
| pure | world-touching | world-touching |
| world-touching | any | world-touching |
| open | pure | open |
| open | world-touching | world-touching |
| open | open | open |

A call that observes or changes the world sets the bit; a pure derivation does
not. `Out.println` and `Clock.now` set the bit; `Out.tee` and `Fs.root` are
pure.

At an annotated binding, the binding's own bit is the top-level arrow of the
annotation when the annotation is a function type, and pure otherwise. The own
bit MUST equal the OR of the call bits of the body; a missing `!` and a
surplus `!` are both errors with code `E-world-bit` (Annex B). Independently,
the body is checked against the annotation by type identity; bit adaptation
applies only at the positions of Clause 7.9.5, never at a declaration, and
never in the reverse direction. A pure function value is therefore bound under
its exact type and adapts at its use positions.

The bit lives on the arrow and does not infect data. `logged := List.map(xs,
greeter)` requires an enclosing `!`, but `logged: []String` is an ordinary
value; there is no world-touching list type.

`?` appears only on a dispatcher that is pure if its callback is pure. A
higher-order function that is itself world-touching uses `!` and no `?`:
`Fs.with_file` is `!->`, and its callback is written `(File) !-> a`.

All `?` in one signature are the same open bit. OR absorbs on the bit axis, so
several bit variables would be indistinguishable; `?` is anonymous and no bit
variables exist.

An `extern` declaration sets the bit. Its arrow is `!->` unless it carries
`Pure`, which makes it `->` (Clauses 10.2.1, 10.2.2).

The bit is part of type equality (Clause 7.5.1) and is fully erased at run
time. It has no calling-convention consequence. The bit of a call through
`?->` is its instantiation (Clause 7.9.6); through `->` it is pure; through
`!->` it is world-touching. `panic`, `Option.expect`, `Option.unwrap`,
division by zero, and overflow are pure (Clause 9.2.3). Bit diagnostics are
always errors, never warnings (Annex B).

### 7.9.3 Arrow Forms

| Form | Meaning |
|---|---|
| `(a) -> b` | pure function from `a` to `b` |
| `(a) !-> b` | world-touching function from `a` to `b` |
| `(a) ?-> b` | open function from `a` to `b` |
| `(a)` | `(a) -> _` |
| `(a) !` | `(a) !-> _` |
| `(a) ?` | `(a) ?-> _` |

`()` is the zero-parameter function type and abbreviates `() -> _`. A unit
result is written `-> _` (Clause 6.9.3).

`!` and `?` are written joined to the arrow as `!->` and `?->`. A lone marker
occurs only in the elided forms at end of line (Clause 6.9.3). Arrows nest
without parentheses: `(Out) -> (String) !-> String` is pure and returns a
world-touching function; `(Out) !-> (String) -> String` is world-touching and
takes a pure function; `(Out) !-> (String) !-> String` is world-touching in
both positions. Nothing follows `!` or `?`: no label, variable, or annotation
(Clause 6.10).

The arrow form is part of the function type's identity (Clause 7.5.1). `?` is
an anonymous parameter and is instantiated at the call site (Clause 7.9.6). An
extern declaration's own arrow MUST be `!->` or `!`, or `->` with `Pure`; an
open arrow is not admitted (Clause 10.2.1).

### 7.9.4 Bit Inference

A lambda carries no bit annotation. It is checked against an expected function
type, which supplies the parameter and result types; its bit is the OR of the
calls in its body. A pure lambda adapts to a world-touching function type at
an adaptation position, while a world-touching lambda does not adapt to a pure
position (Clause 7.9.5). A lambda passed as the callback argument of an open
arrow instantiates the open bit at its own body bit. The bit of a named
binding's body is the OR of the calls in that body. The declared bit MUST
equal the body bit, and a mismatch in either direction is `E-world-bit` (Clause
7.9.2).

An inverted binding `<-` has no separate bit rule. The remaining block is the
callback body; its bit is inferred as for any lambda, and the dispatcher
instantiates its open bit from it (Clause 6.3.4). A dispatcher that is itself
world-touching has no open bit; its `!` absorbs the callback's bit by OR.

`if` and `case` are bit-transparent: the condition or scrutinee, the guards,
and every branch or arm body contribute their bits by OR (Clause 8.3). `&&`
and `||` are bit-transparent (Clause 7.8.5).

Holding is quiet. A function value MAY be stored, passed, or returned without
setting the bit; only calls contribute. A pure factory may therefore return a
`!` closure:
```eko
mk_logger: (out: Out) -> (Int) !-> Int =
    (n): Out.println(out, show(n)); n + 1
```
A named binding with an elided arrow, `(a) !` or `(a) ?`, follows the same
rules with `_` as the result type.

### 7.9.5 Bit Adaptation

A pure function type adapts to a world-touching function type with the same
parameters and result. `(a) -> b` is accepted where `(a) !-> b` is expected;
the reverse does not hold.

Bit adaptation applies at four positions: an argument, an operand, an expected
type, and a collection element. At an annotated binding, bit adaptation does
not apply; the declared bit and the body bit MUST be equal (Clause 7.9.2).
Adaptation is for values in positions, not for declarations.

Bit adaptation is one-directional and is not a general subtyping relation. It
does not create an ordering among function types; only this single step is
admitted. Adaptation is judged on the whole function type at the position. A
`?->` parameter is instantiated from the argument's bit: a `->` argument gives
a pure instantiation, and a `!->` argument gives a world-touching one (Clause
7.9.6).

Bit adaptation is not subject to the position discipline of shape adaptation;
collection elements are adaptation positions. It does not infect data; a value
of type `[]String` remains an ordinary value.
```eko
apply: (f: (Int) !-> Int, n: Int) !-> Int = f(n)
inc: (Int) -> Int = (n): n + 1

use: (n: Int) !-> Int = apply(inc, n)
```
A world-touching function value does not adapt to a pure position. A pure
callback adapts into a `!->` parameter such as the callback of `Fs.with_file`
(Clause 9.12.4) or the observer of `Out.tee` (Clause 9.14.5).

### 7.9.6 Open Bits

`?` is an anonymous parameter on the bit axis, generalized at the annotated
binding and instantiated at the call site from the bits of the arguments. It
works like a type variable (Clause 7.6) except that it is anonymous and its
instantiation is a bit. All `?` in one signature are the same open bit; there
are no bit variables, and nothing follows `?`.

An open bit is allowed in exactly two positions: the annotated binding's own
arrow, and arrows inside that signature's parameter types. It MUST NOT appear
in a result type, in a container element, or in a record field; each such
occurrence is `E-open-bit-position` (Annex B).

An own `?` arrow MUST be fed by a parameter: at least one parameter type MUST
contain a `?` arrow, judged after expansion of the parameter types. Otherwise
the declaration is `E-open-bit-unfed`, and bit reconciliation (Clause 7.9.2)
is skipped for it, so one defect yields one diagnostic.

A `?` in a parameter type is instantiated by the corresponding argument: `->`
instantiates pure and `!->` world-touching, and several such parameters are
ORed. Lambda arguments are checked last, so the callback's bit is known when
the open bit is instantiated (Clause 7.6.4). Inside the body of a declaration
whose own arrow is open, a call through a `?` arrow instantiates from its
feeder arguments and, when they are pure, from the enclosing declaration's
open bit, so one open body may feed another. A `?` in a result is rejected
whether or not a parameter feeds another open bit, mirroring the result-only
variable rule (Clause 7.6.5). A type variable under a `?` arrow counts as a
use (Clause 7.6.2), and an open arrow MUST NOT appear on an extern
(Clause 10.2.1). A dispatcher signature has the form
`List.map: ([]t, (t) ?-> u) ?-> []u`.
### 7.9.7 Soundness

**World soundness.** If `f: (a) -> b` in safe Eko, then calling `f` does not
touch the world, modulo audited `Pure` declarations on the native track
(Clause 10.1.4).

The argument is structural.

1. The bit originates only in an extern call or in a call of a world-touching
   function value, and every world-touching function value closes,
   transitively, over an extern declaration or over a bundle unwrapped from a
   capability; a real bundle is host-constructed (Clause 10.8). An extern is a
   visible declaration resolved at the link layer; `Pure` is an audited
   exception.
2. Capabilities cannot be forged. A real capability is constructed only by
   `main`'s parameters or by the host; a pure derivation yields a restricted
   or deterministic substitute (Clauses 7.9.1, 9.14.5).
3. A closure's bit is on its arrow type, and its body is checked against that
   annotation (Clause 7.9.2).
4. Bit adaptation is one-directional: a pure function may flow into a
   world-touching position, never the reverse (Clause 7.9.5).

**Corollary (top-level purity).** Top-level initialization is pure. No
capability value is in scope at top level, so a bit-setting call is not
typeable there (Clause 7.1.2).

The absence of `!` does not promise freedom from panic, termination, blocking,
or cancellation. A pure function MAY panic (Clause 8.4.2), diverge (Clause
8.3.5), block, or be cancelled (Clause 8.9.4). The bit governs world contact
only; partiality is an independent dimension and is not tracked.

`Pure` is a trusted-base assertion; a false `Pure` is outside the language
guarantee (Clause 2.6).

### 7.9.8 Terminology

The terms of this layer are fixed as follows.

| Term | Spelling | Meaning |
|---|---|---|
| unit-returning function type | `(t)` | a parameter group with the arrow elided; the result is `_` |
| world marker | `!` | the arrow definitively touches the world |
| open marker | `?` | the bit is a parameter, instantiated from callbacks |
| world bit | bit | the honesty axis as a whole |
| world-touching arrow | `!->` | an arrow that may touch the world |
| open arrow | `?->` | an arrow whose bit is a parameter |

The vocabulary of effect systems is not used in this document or in
diagnostics. In particular, "effect system", "effect row", and "effect
handler" do not name any Eko mechanism. The ordinary English phrase "takes
effect" is not affected.

Use "world bit", "sets the world bit", "open bit", "bit annotation",
"world-touching", and "touches the world" consistently.

### 7.9.9 Exclusions

The following mechanisms do not exist. Their absence is normative.

- **No label sets.** A label set that mirrors capabilities would duplicate the
  capability value and require permanent synchronization. Capability names are
  the only authority vocabulary.
- **No row polymorphism, row algebra, or row extension.** There is one bit per
  arrow and nothing to extend.
- **No algebraic effects and no handlers.** World contact is not handled away;
  errors are values and partiality is not tracked.
- **No IO monad.** The direct style is preserved, and higher-order functions
  are not made monadic.
- **No exceptions.** Recoverable failure is `Result`; invariant violation is
  `panic` (Clause 9.2).
- **No user-defined labels.** Labels would be a path to a second authority
  vocabulary.
- **No `Rights` type.** Attenuation is the pure combinators of Clause 9.12.5.
- **No ambient I/O.** Authorization flows only from `main` (Clause 7.1.3).
- **No FFI capability.** Native code is authorized at the link layer (Clause
  10.1.2).
- **Bit diagnostics are errors, never warnings.** A missing or surplus `!` is
  rejected (Clause 7.9.2).

## 7.10 Static Errors

The table indexes the categories of required rejection (Clause 2.2) and the
clause that owns each. Diagnostic codes and the rule for choosing one are
Annex B and Clause 2.4.

| Category | Required rejection | Owning clause |
|---|---|---|
| Lexical and syntactic | invalid text, layout, token, or literal; a construct outside the grammar | Clauses 5, 6 |
| Declarations and naming | a non-declaration `type` right-hand side; a bad `ctype` field; a wrong case class; a reserved word as identifier | Clauses 6.9.5, 10.3.3, 5.5.2 |
| Program structure | no entry module; a non-public or `_main` entry; an invalid `main` signature | Clause 7.1.3 |
| Modules | an unresolved import path; a user module under `std/`; an import cycle | Clauses 7.2.2, 7.2.4 |
| Visibility and names | a private name outside its module; a private type in an exported signature; split variants; an unbound name; a failed station; a foreign namespace member; a same-layer conflict | Clauses 7.3.1–7.4.6 |
| Types | a nominal mismatch; a failed shape adaptation; a literal without expected type; an unknown field or wrong arity; a `Never` adaptation inside a function type; `-> ()`; a non-conforming annotation | Clauses 7.5.2–7.5.7 |
| Generics | a phantom binder; an arity mismatch; partial application; a variable heading an application; a result-only or undetermined variable; a non-terminating instantiation | Clauses 7.6.1, 7.6.2, 7.6.5, 8.7.3 |
| Patterns and exhaustiveness | an unknown field; a duplicate or mismatched binder; a forbidden parameter pattern; a non-exhaustive `case`; a guarded case without a final unguarded arm | Clauses 7.7.1–7.7.3 |
| Calls and operators | a non-function callee; an arity or argument mismatch; a `<-` without a callback; mismatched operands; equality, ordering, or `++` outside its domain; a chained comparison; a `Float` range | Clauses 7.8.1–7.8.6 |
| Arithmetic | an implicit numeric conversion; a literal outside its target range | Clauses 7.8.4, 5.7.1 |
| Capabilities and world bit | a non-capability or duplicated `main` parameter; a capability construction; a missing or surplus `!`; an unfed or forbidden `?` | Clauses 7.9.1, 7.9.2, 7.9.6 |
| Extern | a signature outside the whitelist; a type variable or variadic in an extern; an open arrow; a reserved symbol | Clauses 10.2.3, 10.3.5, 10.6 |
| Derived service and test | equality, keying, or assertion over a type whose `eq` halts; a JSON instantiation without a default form; a duplicate or misplaced test | Clauses 9.8.4, 9.9.3, 9.14.1 |

Each row is an error diagnostic, not a warning; the warning vocabulary is
Annex B.4. Rejection at link time or run time is not a substitute for a
required static rejection.
