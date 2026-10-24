# Design Record — Syntax and Lexical Structure

Non-normative. Rulings: `02-syntax.md`. Rejected alternatives, prior art,
and rationale for the surface rules.

## 1. Layout: indentation and the two flow shapes

Blocks are delimited by layout, never braces; `:` opens a block, the first
member line establishes the column, nesting indents one level
(`02-syntax.md §1`). Four spaces per level is a fixed constant, not a style
choice — a conforming parser accepts exactly one indentation width, which
removes an entire configuration axis (tab vs space, width 2/4/8) from
tooling forever. A raw tab is a compile error (`E-tab`), ignored only inside
comments.

The two flow shapes of the running example are semantically forced, not
stylistic: `with_file`'s error value exists only *after* the callback
finishes, so the `case` that consumes it must enclose the whole call — a
value flowing *outward* is wrapped by `case`; a value flowing *forward* is
flattened by `<-` (`archive/control.md §1`). One layer each, no mixing.

## 2. Operators: symbols, not words

`&&`/`||`/`not` symbols instead of the words `and`/`or`/`not`
(`02-syntax.md §6.3`). Reasons, in order:

1. Words would permanently retire three identifiers from every scope.
2. Words would rule out `Bool.and` and `Bool.not` as namespace members.
3. Words would break the rule that every keyword opens a block
   (`if`, `case`, `else` all do).
4. Symbols consume no identifiers; the single-character forms `&` and `|`
   remain reserved — `|` for the or-pattern separator (`03-control.md §5`).

### Rejected operator surface

| Rejected | Reason (chapter) |
|---|---|
| User-defined operators | The set is closed; overloading is namespace resolution, one mechanism (`§6.1`) |
| Bitwise `& \| ^ << >> ~` | Functions are the vocabulary (`UInt.band/...`, `08-builtins.md §4.6`); the closed set keeps the precedence ladder small and finite |
| `**` exponentiation | `Int.pow` / `Float.pow` say the cost in a name; also removes the `-a ** b` precedence trap |
| `++`/`--` increment statements | Values are immutable; `++` is concatenation only |
| Ternary `?:` | `if` is already an expression (`03-control.md §4`); a second spelling for one construct |
| Chained comparisons `a < b < c` | Parse error, the C classic; write `a < b && b < c` (`§6.5`) |
| Prefix `!x` | `!` belongs exclusively to the world-bit marker (`06-world.md §1`) |
| Word operators `and`/`or`/`not` | See above |
| Labels after `!`/`?` | Permanently closed — `06-world.md §10`, `archive/world.md §2` |

## 3. Literals

- **Decimal by default, `0x`/`0b` completed later.** The binary literals
  arrived with the FFI wave: prefix lowercase, digits case-insensitive,
  placed by expected type exactly as decimal (`02-syntax.md §4.1`). The
  original decimal-only surface was a staging decision, not a philosophical
  one; what was philosophical is the *refusal of type suffixes* — `255u`,
  `10LL` and friends — because Eko places integer literals by expected
  type, a single mechanism that suffixes would bypass and thereby fork.
- **`_` digit grouping only.** No separators beyond `_`, no suffixes.
- **A leading `-` is the unary operator**, never part of the literal — one
  lexing rule, no negative-literal corner cases in patterns.
- **`255.to_hex()` is a postfix call on `255`**, not an incomplete float;
  an integer literal immediately followed by `..` does not begin a float.
  Both are longest-match consequences, stated in the chapter so no lexer
  invents its own reading.

### Rejected literal surface

| Rejected | Reason |
|---|---|
| Numeric type suffixes | Placement is by expected type (`§4.1`); suffixes fork the mechanism |
| Octal `0o` | Never requested by any consumer surface; `Int.to_oct`/`parse_oct` are functions (`09-library.md §2`) |
| Byte literals `b"…"`, `bytes"…"` | Construction goes through functions; `0x` literals are integers, not byte construction (`08-builtins.md §3.5`) |
| Multiline string literals | YAGNI; `\n` is sufficient and explicit; multiline strings drag trailing-newline and indentation semantics behind them |
| Block comments `/* ... */` | Nesting freedom is a pseudo-need; deletion is rewriting (`§3`) |

`///` documentation comments carry zero semantics by design: they do not
participate in compilation and do not suppress warnings, so extraction and
rendering are purely toolchain domain (`archive/library.md §3`).

## 4. Numeric vocabulary: domain words and machine words

The two-family split — full spellings `Int`/`UInt`/`Float` for the domain,
abbreviations `I8…F64` for the machine boundary (`02-syntax.md §5`) — is a
lexical signal, not a typing distinction alone: *full spelling = domain,
abbreviation = machine interface*. Machine words hold, compare, show, and
ferry; they never compute (`11-ffi.md §3.1`).

- **Fixed 64-bit domain types**, not platform word size: determinism across
  hosts; `Int` means Int64 everywhere.
- **bignum rejected by default** (`§5`): a bignum type would conflict with
  `UInt` as the primary numeric type, with FFI mapping, and with the
  zero-cost erasure model. Overflow panics instead of silently widening —
  the honest cheap option; bignum is library domain.
- **`UInt` is the primary type for natural numbers** and the correct domain
  for non-negative modular arithmetic; the truncation/floor distinction
  vanishes there.

## 5. Comments and documentation

One comment syntax (`//`) plus one documentation marker (`///`). The
documentation marker attaches to the *following* binding and aggregates
consecutive lines; treating `type`, `test`, and `main` declarations alike
costs nothing because the annotation slot already made declarations
bindings (`04-types.md §1`). Rejected: any doc-syntax inside the comment
(markdown is toolchain's business, not the language's).
