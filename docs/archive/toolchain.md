# Design Record — Implementation Strategy

Non-normative. Ruling clauses: 11.1–11.6, Annex D. The implementation
language, the backend legs, the pipeline, and the self-hosting roadmap.

## 1. The ruling

**C everywhere at the base.** The compiler (`ekoc`), the runtime (`ekort`),
and the emitted target are C99, built by `make` with `cc` as the only
toolchain dependency. Eko lowers to C source text: the AOT leg is the system C
compiler at `-O2`, the in-process leg is libtcc (tests and development
iteration). Self tail calls are satisfied by the lowering — a labeled retry
loop, never the C compiler's own tail optimization (`Clause 8.8.3`). The
runtime (reference counting, scheduler, veneer) is C and stays C.

## 2. Why C

**Breadth.** The toolchain's whole external dependency is "a C compiler
exists"; a Rust base would need rustc to build and cc to run its output.

**Self-hosting (decisive).** A C base is a *living base*: Eko modules lower to
C and link against the C remainder, so the compiler is replaced stage by
stage. Go replaced its C compiler incrementally across releases; Nim's C
backend closed the bootstrap loop the same way. Zig is the counter-precedent:
a C++ base admits no incremental replacement.

## 3. Pipeline

A one-way pipeline of replaceable stages over domain objects: source bytes,
token stream, layout-annotated stream, tree, symbols, types, diagnostics, C
unit. Stages: lex → layout → parse → resolve → check → lower → emit. A stage
consumes only its predecessor's public output; the include graph is acyclic
and layered. The shared foundation is an arena, a diagnostics buffer, and
containers; stages never abort and never allocate past the arena.
Replaceability is the self-hosting mechanism: each stage entry is a pure
function over its input, and an Eko implementation links in through the veneer
boundary.

## 4. Rejected backends

| Rejected | Reason |
|---|---|
| Rust base | dead base for bootstrap; second toolchain dependency |
| OCaml / Go base | strong front end / tooling, but neither is a living base |
| GNU Lightning (JIT) | an emitter, not a compiler: a second native backend kept semantically in sync, breaking the single-lowering invariant |
| libgccjit | GPLv3 grey zone for an MIT toolchain; presumes a GCC install |
| Cranelift | Rust-coupled; attractive only if the language decision is revisited |
| QBE | ~70% of `-O2`, no in-process JIT; a fallback if the AOT leg sheds `cc` |

## 5. Self-hosting roadmap

Replacement order follows the pipeline: lexer, parser, lowering, checker.
Permanent C: the runtime, derived-cache and build glue, the CLI, FFI shims.
The bootstrap seed is the last all-C release (stage0), as in Go, Nim, GHC.
The compiler dogfoods the surface as it is colonized: `Fs` for sources, `Out`
for diagnostics, `Conc` for parallel roots, the conformance suite as the gate
at every step.
