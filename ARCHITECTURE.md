# ekoc architecture

One-way pipeline; stages are replaceable; the base is C.

    bytes ─▶ LEX ─▶ LAYOUT ─▶ PARSE ─▶ RESOLVE ─▶ CHECK ─▶ LOWER ─▶ EMIT
             tokens  layout     AST      symbols     checked   C text  objects /
                     tokens               + worlds    AST                in-memory

## Rules

1. **Single direction.** A stage includes only `support/` and the public
   headers of stages before it. No forward references, no callbacks into
   earlier stages.
2. **High cohesion.** Each stage owns one domain object and its
   transformation. Its public header declares only the output type and
   the entry point; internals stay in `.c` files.
3. **Replaceable stages.** An entry point is a pure function:
   `(input, arena, diag) -> output`. Any stage can be reimplemented in
   Eko and linked through the veneer. Self-hosting replacement order:
   lex, parse, lower, check.
4. **No stage aborts.** Errors are diagnostics; the pipeline drains and
   reports.
5. **Arenas.** Compilation allocates from arenas freed wholesale; stages
   perform no individual object management.

## Permanent C

`rt/` (runtime: RC, scheduler, veneer), derived-cache and build glue,
CLI, FFI shims. Everything else is colonizable by Eko over time
(docs/archive/toolchain.md §5).

## Layout

    src/support/  arena, diagnostics buffer, containers, UTF-8, XID tables
    src/lex/      source bytes      -> token stream          (Clause 5)
    src/layout/   token stream      -> layout token stream   (Annex A.4)
    src/parse/    layout tokens     -> AST                   (Clause 6, Annex A.3)
    src/resolve/  AST               -> symbols and bindings  (Clause 7.2–7.4)
    src/check/    AST + symbols     -> types and world bits  (Clause 7.5–7.9)
    src/lower/    checked AST       -> C source text
    src/emit/     C text            -> objects (cc) / in-memory (libtcc)
    src/cli/      commands: build, test, doc, where
    rt/           ekort: the runtime library
    cap/          the capability module (Eko source, embedded at build)
    lib/          std.eko and companions (Eko source, release files)
    docs/         the specification
    tests/        the conformance suite (Annex D)
