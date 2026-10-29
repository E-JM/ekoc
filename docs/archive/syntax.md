# Design Record — Syntax and Lexical Structure

Non-normative. Ruling clauses: 5, 6. Rejected alternatives and rationale for
the surface rules.

## 1. Layout

`:`, `=` and a `case` arm's `->` open layout blocks; indentation is fixed at
four spaces and nothing else, so tab (`E-tab` outside comments) and 2/8-space
widths are not a configuration axis (`Clauses 6.2.2, 6.2.3`). `case` wraps a
value flowing outward; `<-` flattens one flowing forward (`Clause 6.3.4`).

## 2. Operators

Symbols, not words: `and`/`or`/`not` would retire three identifiers, preclude
`Bool.and`/`Bool.not`, and break "every keyword opens a block"; `&` and `|`
stay reserved for the or-pattern separator (`Clauses 7.8.5, 6.8`).

## 3. Literals

Decimal by default; `0x`/`0b` prefixes are lowercase, digits case-insensitive.
`_` groups digits; no suffixes. A leading `-` is the unary operator, never
literal text. `1e999` is `E-literal-overflow`, not `inf`. `255.to_hex()` is a
postfix call, and `255` followed by `..` is not a float — both longest match
(`Clauses 5.7, 5.7.2, 5.10.3`).

## 4. Domain vs machine words

Full spellings (`Int`, `UInt`, `Float`) are the domain; abbreviations
(`I8`…`F64`) are the machine boundary, where values hold, compare, show and
ferry but never compute (`Clauses 9.6.1, 10.3.1`). `Int` is 64-bit on every
host, not the platform word; overflow panics rather than widening, and bignum
is library domain (`Clause 9.6.1`). `UInt` is the domain for natural numbers,
so the truncation/floor split does not arise.

## 5. Comments

`//` comments; `///` documents the following binding, carrying no semantics
and suppressing no warnings — markdown is toolchain business (`Clauses 6.3.5,
11.3`). Comments are trivia outside string literals only: inside an
interpolation slot `//` and `/*` are operator tokens (`Clauses 5.4.1, 5.9.3`).

## 6. Rejected surface

| Area | Rejected | Reason (clause) |
|---|---|---|
| Operator | user-defined operators | closed set; overloading is namespace resolution (`6.5.4`) |
| | bitwise `& \| ^ << >> ~` | functions are the vocabulary (`9.6.5`) |
| | `**` | `Int.pow`/`Float.pow` name the cost; no `-a ** b` trap |
| | `++`/`--` | values are immutable; `++` is concatenation only |
| | ternary `?:` | `if` is already an expression (`6.7.4`) |
| | `a < b < c` | parse error; write `&&` (`6.5.5`) |
| | prefix `!x` | `!` is the world-bit marker (`7.9`) |
| | word `and`/`or`/`not` | §2 |
| | labels after `!`/`?` | closed (`7.9.3`) |
| Literal | type suffixes (`255u`) | placement by expected type (`5.7`) |
| | octal `0o` | `Int.to_oct`/`parse_oct` (`9.13.1`) |
| | byte literals | construction goes through functions (`9.5.5`) |
| | multiline strings | YAGNI; `\n` is explicit |
| | block comments | nesting is a pseudo-need (`5.4`) |
| Number | bignum | conflicts with `UInt`, FFI mapping, zero-cost erasure (`9.6.1`) |
| Toolchain | doc syntax inside comments | markdown is not the language's business (`11.3`) |
