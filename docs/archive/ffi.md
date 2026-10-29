# Design Record — FFI and Native Interop

Non-normative. Ruling clause: 10. The option analysis behind the clause: the
three-direction co-design, the F-series rulings, and the rejected
alternatives.

## 1. Three directions, one design

The internal channel (capability module → OS), the user direction (binding C
libraries), and the embedding ABI (host ↔ Eko) were ruled in one pass because
the internal channel's hardest demand — `Out.tee` implementable as ordinary
capability-module Eko with zero host magic — drags the other two in regardless
(**the tee sieve**). Any "compile capability operations as intrinsics" scheme
fails the sieve: every derived capability becomes compiler magic, and the
"members are ordinary Eko source" promise leaks one function at a time.

The convergence (**B′**): a capability is internally a bundle of world-touching
closures constructed by the host. The capability module never calls libc
(`Clause 10.8`); the OS surface lives in host-constructed bundles, and the
module composes derived bundles from one wrap/unwrap pair per capability
(`world.md` §9).

## 2. Authorization is the link layer

`Unsafe`, an early capability gating user FFI, was **cancelled** (F10). Honesty
is carried by the world bit; authorization moves to the link layer — every
extern is a visible declaration in a module interface, statically greppable,
resolved at link time. A host that refuses native code refuses externs;
`--sandbox` restricts the grant *and* rejects externs. Both schemes enforce at
the same granularity (whole-surface); the difference is visibility and the
absence of a runtime grant to subvert (`Clause 10.1.2`).

## 3. `Pure`

`= extern "sqrt" Pure` — a tail word, no word means default `!`, and the
extern stays a declaration primitive taking no bracketed arguments (v3 of
three spellings). The guarantee is **two-part**: no world contact (the bit
axis — `C.get_errno` is pure by this half) and reentrancy plus thread safety
(the concurrency axis: static state of the `strtok` shape voids the word,
`Clause 8.9`).

**Why user-declared `Pure` exists** (the honest tax): forcing `!` on provably
pure libm functions is a false assertion that dilutes the bit across every
wrapper, violating rule 3's spirit from the other side. The price is paid
openly — the soundness theorem becomes *audited* (a lying `Pure` is a
trusted-computing-base failure, the class of a lying C shim), and interfaces
disclose `Pure` declarations so the audit surface is visible downstream.

## 4. User-defined capability types

Rejected: capability types stay built-in and closed, since unforgeability is a
premise of the theorem and user-declarable types would break "constructed only
by `main`, the host, or pure derivation". The user idiom is the **closure
record**:

```eko
Db: type = [query: (Sql) !-> Result Rows DbError, exec: (Sql) !-> Result _ DbError]
```

Holding authorizes, attenuation is a pure function building a narrower record,
and mock construction is free. What it lacks honestly: the manifest shows `Net`,
not `Db` — the manifest audits the OS surface, and domain authorization flows
through values. Unforgeable inter-module tokens (S5) were rejected: with no
identity comparison, unforgeability has no lexical basis, and the language does
not model inter-module distrust.

For hosts, the **γ′ route** won (F3): the host calls an exported entry point
directly and passes application surfaces as ordinary record values — no new
lexicon, the manifest stays a pure OS surface, the audit surface moves to the
entry signature. Host-registered capability types (β) is the recorded
fallback.

## 5. `ctype`

`StatResult: ctype = [ok: I32, size: I64, mode: U32, err: I32]` — an
annotation-slot word beside `type` and `test`. Implicit layout inference fails
on three points: interface compilation order (an interface cannot depend on a
later module's *use*), error locality (the error reports at the use site while
the cause sits in the declaration file), and visible cost (pinning layout is a
cost, and costs are visible at the declaration). Fields are machine words or
nested `ctype` records, checked at the declaration line; crossing is by value
only; partial re-declaration is impossible; out-parameters are shim work; no
address-of ever, so `Clause 8.11` survives intact. `ctype` is the single
explicit exception to representation freedom.

`C.In T` (F17) is the audited **read borrow**: copy-in, valid for the call,
value unchanged — choosing the marker asserts C reads, retains nothing, writes
nothing, the same audit class as `Pure`. Read-style `&` APIs go without shims;
write-style ones stay shim-only.

## 6. The veneer

Function signatures are normative, layouts are not; `eko_value` is opaque
throughout (`Clause 10.7`). Fully specifying the ABI (RC header, closure
layout) would freeze code generation and kill the optimization surface; the
fast path calling generated C directly is a compiler detail. Holding values is
bare `eko_retain`/`eko_release` — a handle table would be root registration and
a second value representation in disguise. **Any thread may call in**: there
is no global mutable state, a boundary-crossing value is shared and therefore
atomically counted, and thread safety of a capability's operations is the
bundle author's guarantee. No registration, no global lock, no runtime. Shims
share the extern symbol space — one resolution mechanism.

## 7. Symbols

Beginning `eko_` are the host's (`Clause 10.6`): retain/release and veneer
entries. The lexical rule "the word is the audit" cannot stop a link-time
symbol, so the prefix draws the line in symbol space; a user extern naming a
reserved symbol is `E-reserved-symbol`. The wrap/unwrap pairs, formerly
std-private `Pure` externs to reserved symbols, are now compiler-injected
primitives of the capability module — pure, invisible outside it, no
declaration form, no extern symbol (`Clause 10.8`). The wall is nameability:
a program cannot name the pair, cannot declare an extern mentioning a
capability type (`E-extern-type`, `Clause 10.3.5`), and cannot name std's
private bindings.

## 8. Machine words and the extern surface

Machine words hold, compare, show, and ferry — never compute (F15): one
overflow rule in the language. The bitwise family is
`UInt.band/bor/bxor/bnot/shl/shr`, domain-word functions with ℤ/2⁶⁴ total
semantics — truncation is the semantics, a shift count ≥ 64 yields 0.
`0x`/`0b` literals (F7) are lowercase-prefixed, case-insensitive in digits,
placed by expected type, out of range is `E-literal-overflow`, no suffixes.
No variadic externs: a signature is complete. Async callback registration is
absent *vocabulary*, not a semantic invariant (F12) — the ban is worded so a
future handle would not break it. The recommended binding shape is a private
`_extern` below an idiomatic wrapper (F4/F13), and a wrapper naming a
capability must use it — a decorative capability is a dead annotation. Error
codes, errno and NULL sentinels are data (F8): wrapping into `Result`/`Option`
is library work, `longjmp` taming is shim work, and segfaults are aborts.

## 9. The C-backend premise

The specification mandates no backend, so the FFI surface must not depend on
"C is the only backend". But the design had to be compatible with a C backend,
under which B′ converges naturally: the bundle is a C function-pointer struct
and the extern surface is zero-translated. Layout temptations ("just hand the
generated struct across") were rejected regardless of backend — promising
layout freezes code generation, which is why `ctype` is the scoped exception.

## 10. Rejected surface

| Area | Rejected | Reason |
|---|---|---|
| Authorization | `Unsafe` capability (F10) | honesty is the bit; authorization is the link layer, with no runtime grant to subvert |
| `Pure` | header-attribute cross-checking | the compiler does not read C headers, and attribute semantics do not cover part 2 |
| `Pure` | compiler-shipped whitelist registry | user-bound C libraries could never qualify |
| Capability types | user-definable types | unforgeability is a premise of the soundness theorem |
| Capability types | unforgeable inter-module tokens (S5) | with no identity comparison, unforgeability has no lexical basis |
| Externs | variadic declarations | a signature is complete; `printf`-shaped functions have no declaration |
