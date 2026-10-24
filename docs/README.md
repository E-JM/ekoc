# Eko Language Reference

This manual is the semantic authority for the Eko programming language.
It is organized as eleven chapters; each chapter is normative unless marked
otherwise.

| Chapter | Contents |
|---|---|
| [01-overview.md](01-overview.md) | Design goals, core mechanisms, failure taxonomy, a complete program, terminology |
| [02-syntax.md](02-syntax.md) | Blocks and indentation, line continuation, comments, literals, operators and precedence |
| [03-control.md](03-control.md) | Binding forms (`=`, `:=`, `<-`), lambdas, `case`/`if`, patterns, working with `Result` |
| [04-types.md](04-types.md) | Type declarations, composite types, nominal typing and shape adaptation, generics, `Never` |
| [05-names.md](05-names.md) | Calls and UFCS, member resolution, structural access, modules, `import`, visibility, prelude |
| [06-effects.md](06-effects.md) | Capabilities, the world bit, failure layer, file handles, concurrency |
| [07-runtime.md](07-runtime.md) | Value representation, memory management, monomorphization, iteration and tail calls |
| [08-builtins.md](08-builtins.md) | `Bool`, `Char`/`String`, `Bytes`, conversions, time, derived services, JSON |
| [09-library.md](09-library.md) | Standard library surface, testing, diagnostics |
| [10-prelude.md](10-prelude.md) | Complete prelude inventory: delivery model, free functions, operator namespaces, every namespace member |
| [11-ffi.md](11-ffi.md) | `extern` and `Pure`, machine words, the `C` namespace, `ctype` and `C.In`, failure and callbacks, linking and reserved symbols, the embedding veneer, the prelude channel |

## Reading order

01 → 02 → 03 → 04 → 05 → 06 → 07 → 08 → 09 → 10 → 11.
Chapters 01–07 define the language; 08–09 define the built-in data types,
their services, and the library surface; 10 is the complete prelude inventory;
11 is the native track.

## Conventions

- Normative statements use *is*, *must*, and *may*.
- Code examples are fenced as ```eko; string literals and comments use
  English.
- Cross-references use the form `06-effects.md §5.2`.
- Signatures are written in Eko's own type syntax, e.g.
  `List.map: ([]t, (t) ?-> u) ?-> []u`.
