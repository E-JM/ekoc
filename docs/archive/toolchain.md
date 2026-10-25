# Design Record — Implementation Strategy

Non-normative. Rulings touched: Clauses 11.1–11.6, Annex D. This record
fixes the implementation-language and backend decisions, the pipeline
architecture, and the self-hosting roadmap.

## 1. The ruling

**C everywhere at the base.** The compiler (`ekoc`), the runtime library
(`ekort`), and the emitted target are C:

- the compiler and its runtime are written in C99, built by `make` with
  `cc` as the only toolchain dependency;
- Eko lowers to C source text; the AOT leg is the system C compiler at
  `-O2`, the in-process leg is libtcc (tests and development iteration);
- self tail calls are satisfied by the lowering itself — a self-recursive
  tail call becomes a labeled retry loop, never relying on the C
  compiler's own tail optimization (Clause 8.8.3);
- the runtime — reference counting, scheduler, veneer — is C and stays C.

## 2. Why C: the living base

Two reasons, one decisive.

**Breadth.** With a C backend, the toolchain's whole external dependency
is "a C compiler exists". A Rust base would need rustc to build the
toolchain and cc to run its output — two dependencies, and rustc's own
bootstrap on exotic hosts is a known cost.

**Self-hosting (decisive).** Bootstrapping is a firm goal. A C base is a
*living base*: Eko modules lower to C and link against the C remainder,
so the compiler can be replaced stage by stage. Precedents: Go replaced
its C compiler with Go incrementally across releases (the 1.0→1.5 arc);
Nim's C backend closed the bootstrap loop the same way this project
will. The counter-precedent is Zig: a C++ base admits no incremental
replacement, and self-hosting required a whole-compiler rewrite. Rust
here would have been the same dead base — chosen for its own virtues,
then paid for twice.

## 3. Rejected alternatives

- **Rust.** Best fit for the Clause 11 tooling obligations (library
  compiler, semantic API, incremental caches), but a dead base for the
  bootstrap goal and a second toolchain dependency.
- **OCaml / Go.** Strong front end / strong tooling respectively; both
  lose the living-base property.
- **GNU Lightning (JIT leg).** A code-emission library, not a compiler:
  choosing it means writing a second, native backend — instruction
  selection, ABI, frames — and keeping two backends semantically in
  sync (self-TCE transform, RC protocol, panic paths, world-call order).
  It breaks the single-lowering invariant. Its genuine edges — widest
  target set (ppc, s390x), no C compiler at run time — did not apply.
- **libgccjit.** GCC's optimizer in process; the license (GPLv3 with
  exception) is a grey zone for an MIT toolchain, and it presumes a GCC
  install.
- **Cranelift.** Clean license and good quality, but Rust-coupled; it
  becomes attractive only if the language decision is ever revisited.
- **QBE.** MIT, roughly 70% of `-O2`, in production use by Hare; no
  in-process JIT mode, so it cannot serve the fast leg. A candidate if
  the AOT leg ever needs to shed its `cc` dependency.

## 4. Pipeline architecture

The compiler is a one-way pipeline of replaceable stages. Domain
objects: source bytes, token stream, layout-annotated token stream,
tree (AST), symbols, types, diagnostics, C unit. A stage consumes only
its predecessor's public output; no stage references a later stage; the
include graph is acyclic and layered. Stages: lex → layout → parse →
resolve → check → lower → emit. The shared foundation is an arena, a
diagnostics buffer, and containers; stages never abort and never
allocate past the arena. Replaceability is the self-hosting mechanism:
each stage entry is a pure function over its input, and an Eko
implementation of a stage links in through the veneer boundary.

## 5. Self-hosting roadmap

Replacement order follows the pipeline: lexer first (pure, easily
tested), then parser, then lowering, then the checker. Permanent C: the
runtime, derived-cache and build glue, the CLI, and FFI shims. The
bootstrap seed is the last all-C release (stage0), as in Go, Nim, and
GHC. The compiler dogfoods the whole surface as it is colonized: `Fs`
for sources, `Out` for diagnostics, `Conc` for parallel roots, and the
conformance suite as the gate at every step.
