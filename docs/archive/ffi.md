# Design Record — FFI and Native Interop

Non-normative. Rulings: `11-ffi.md`. This record preserves the option
analysis behind the chapter: the three-direction co-design, the F-series
rulings, and the rejected alternatives.

## 1. Why the three directions were designed together

The internal channel (prelude → OS), the user direction (binding C
libraries), and the embedding ABI (host ↔ Eko) were ruled in one pass,
because the internal channel's hardest demand — `Out.tee` must be
implementable as ordinary prelude Eko with zero host magic — drags the
other two directions' mechanisms in regardless (**the tee sieve**). Any
"compile the capability operations as intrinsics" scheme fails the sieve:
every derived capability becomes compiler magic, and the
"prelude is ordinary Eko source" promise leaks one function at a time.

The convergence (**B′**): the lexicon is `extern` declarations; a
capability is internally a bundle of world-touching closures constructed
by the host. The prelude never calls libc (`11-ffi.md §8`) — the operating
system surface lives in host-constructed bundles, and the prelude composes
derived bundles from one wrap/unwrap pair per capability.

## 2. The authorization axis: `Unsafe` cancelled

An early design carried an `Unsafe` capability gating user FFI. It was
**cancelled** (F10): honesty is carried by the world bit (`!` on the extern
arrow); authorization moves to the **link layer** — every extern is a
visible declaration in a module interface, statically greppable, and every
symbol resolves at link time. A host or sandbox that refuses native code
refuses extern declarations; `--sandbox` restricts the capability grant
*and* rejects externs. The enforcement granularity of both schemes is the
same (whole-surface); what differs is visibility and the absence of a
runtime grant to subvert. `11-ffi.md §1.2` and the world-soundness
restatement ("modulo audited link inputs") follow from this ruling.

## 3. `Pure`: spelling and the two-part guarantee

The guarantee word went through three spellings; v3 won:
`= extern "sqrt" Pure` — a tail word on the declaration, no word = default
`!` (rule 7 unchanged), the extern stays a declaration primitive that
takes no bracketed arguments.

The guarantee is **two-part**, and the second half is easily missed:

1. **No world contact** (the bit axis) — no I/O, signals, clock, entropy.
   Program-internal state is not world (`C.get_errno` is pure on this
   basis).
2. **Reentrancy and thread safety** (the concurrency axis) — the target may
   be called concurrently from two tasks; internal static state of the
   `strtok` shape voids the word. This half comes from the determinism
   argument of `06-world.md §8`, not from the bit system.

**The honest-tax argument** (why user-declared `Pure` exists at all):
forcing `!` on provably pure libm functions is a false assertion — the bit
would be diluted across every wrapper, and rule 3's spirit ("annotations
must be true") would be violated from the other side. The price is paid
openly: the soundness theorem becomes *audited* (a lying `Pure` is a
trusted-computing-base failure, the same class as a lying C shim), and
interfaces disclose `Pure` declarations so the audit surface is visible
downstream. Rejected alternatives for the guarantee: header-attribute
cross-checking (the compiler does not read C headers; attribute semantics
do not exactly cover part 2) and a compiler-shipped whitelist registry
(user-bound C libraries could never qualify).

## 4. User-defined capability types: rejected

Capability types stay built-in and closed — unforgeability is a premise of
the soundness theorem, and user-declarable capability types would break
the "constructed only by `main`, the host, or pure derivation" step. The
user-domain idiom is the **closure record**:

```eko
Db: type = [query: (Sql) !-> Result Rows DbError, exec: (Sql) !-> Result _ DbError]
```

Holding authorizes (as with capabilities), attenuation is a pure function
building a narrower record, and mock construction is free — the derivation
surface is the construction surface. What it honestly lacks: the manifest
shows the underlying capabilities (`Net`), not "`Db`" — the manifest audits
the operating-system surface, and domain-level authorization flows through
values. Unforgeable inter-module tokens (S5) were explicitly rejected:
with no identity comparison, verifiable unforgeability has no lexical
basis; the language does not model inter-module distrust.

For embedding hosts, the **γ′ route** won (F3): the host calls an exported
entry point directly and passes application surfaces as ordinary record
values — zero new lexicon, the manifest stays a pure OS surface, the audit
surface moves to the entry signature. Host-registered capability types
(β) is the recorded fallback, to be reopened only if ecosystem evidence
demands manifest-level application-surface auditing.

## 5. `ctype`: why the marker is explicit

`StatResult: ctype = [ok: I32, size: I64, mode: U32, err: I32]` — an
annotation-slot word beside `type` and `test`. An *implicit* layout
inference was rejected on three collapse points: (1) interface compilation
order — an interface cannot depend on the *usage* of a type in a
later-compiled module; (2) error locality — in an implicit world the error
reports at the use site while the cause sits in the declaration file;
(3) visible cost — pinning layout is a cost, and costs must be visible at
the declaration. Companion rulings: fields are machine words or nested
`ctype` records, checked at the declaration line; crossing is **by value
only**; partial re-declaration is impossible (complete mirror or shim);
out-parameters are shim work; no address-of ever — `07-runtime.md §9`
survives intact. `ctype` is the single explicit exception to
representation freedom.

`C.In T` (F17) is the audited **read borrow**: copy-in, valid for the
call, value unchanged — choosing the marker asserts C reads, retains
nothing, writes nothing; the same audit class as `Pure`. Read-style `&`
APIs (bind, setsockopt) go without shims; write-style `&` APIs stay
shim-only.

## 6. Embedding ABI: the opaque veneer

The veneer (`11-ffi.md §7`) rules: **function signatures are normative,
layouts are not**; `eko_value` is opaque throughout. The rejected
alternative — fully specifying the ABI (RC header, closure layout) — would
freeze code generation and kill the optimization surface. The fast path
(calling the compiler's generated C directly) exists and is a compiler
detail, not contract. Holding values is bare `eko_retain`/`eko_release` —
no handle table, which would be root registration plus a second value
representation in disguise.

**Any thread may call in** — the protocol is three sentences: the language
has no global mutable state; a value that crosses a boundary is shared and
shared means atomically counted; thread safety of a capability's
operations is the bundle author's guarantee (the `Pure` audit class). No
thread registration, no global lock, no runtime. Conc scheduling belongs
to the host bundle. Shims share the extern symbol space — one resolution
mechanism, no registration step.

## 7. The reserved prefix and the wrap/unwrap pairs

Symbols beginning `eko_` are the host's (`11-ffi.md §6`). The lexical rule
"the word is the audit" cannot stop a *link-time* symbol: a user extern
naming `eko_out_wrap` would reach the forging surface. The reserved prefix
draws the line in symbol space. The wrap/unwrap pairs themselves are
prelude-private `Pure` externs to reserved host symbols — the
implementation of the derivation surface, not public inventory
(`11-ffi.md §8`).

## 8. Machine words, bitwise family, literals

- **Machine words hold, compare, show, and ferry — never compute** (F15):
  one overflow rule in the language, zero exceptions; domain words panic
  on overflow, machine words simply do not have arithmetic. The bitwise
  family is `UInt.band/bor/bxor/bnot/shl/shr` — domain-word functions, no
  operators ever — with ℤ/2⁶⁴ total semantics: **truncation is the
  semantics, not an overflow**; a shift count ≥ 64 yields 0.
- **`0x`/`0b` literals** (F7): prefix lowercase, digits case-insensitive,
  placed by expected type, out of range is `E-literal-overflow`, no
  separators beyond `_`, no suffixes.
- **No variadic externs**: a signature is complete; `printf`-shaped
  functions have no declaration.
- **Async callback registration is absent vocabulary, not a semantic
  invariant** (F12): the ban's wording matters — the trampoline mechanism
  is the same shape a future callback handle would use, so the promise
  made today must not be worded in a way a handle would break.
- **Binding conventions** (F4/F13): the recommended shape is a private
  `_extern` below an idiomatic wrapper; a wrapper that names a capability
  must actually use it (precheck, scope, or attenuate) — a decorative
  capability is a dead annotation.
- **Failure mapping** (F8): error codes / errno / NULL sentinels are data;
  wrapping into `Result`/`Option` is library-layer work; segfault and
  `longjmp` crossing are aborts; taming `longjmp` is shim work.

## 9. The C-backend premise

The specification does not mandate a backend ("compiles to machine code"),
and the FFI surface must not depend on "C is the only backend". But the
design had to be *compatible* with a C backend (Eko compiles to C, then a
C compiler), under which B′ converges naturally: the bundle is a C
function-pointer struct, and the extern surface is zero-translated. Layout
temptations ("just hand the generated C struct across") were rejected
regardless of backend: promising layout freezes code generation, which is
why `ctype` is the explicit, scoped exception.
