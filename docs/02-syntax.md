# Syntax and Lexical Structure

Eko source files use the `.eko` extension. The language is indentation-based: blocks are delimited by layout rather than braces, and a newline normally ends a block member. This chapter defines the surface rules: block layout, licensed line continuations, comments, literals, numeric type names, and the operator surface. Rules that depend on a type or a capability are cross-referenced to the chapter that defines them.

Two levels of description are used throughout the manual. **Concrete syntax**
is the surface spelled in source: layout, tokens, and the continuation rules
of this chapter. **Grammar productions** — the binding grammar
(`03-control.md §1.1`), `TypeDecl` (`04-types.md §1.1`), the marker grammar
(`06-world.md §5.2`) — describe the structure of a construct without
encoding its layout. The two levels evolve independently: a production fixes
the shape of a construct, never its indentation or line breaks.

## 1. Blocks and indentation

Function bodies, `if` branches, callback bodies, and the arms of `case` are blocks. A block is introduced by a header line and consists of the lines that follow it at one indentation level. The header ends with `:` for a callback, `if`, or `case`, and with `=` for a definition whose body is a block. The first member line establishes the block's column; every member of the block begins in that column. A nested block is indented one level further. Indentation therefore determines block membership, and braces are not used to delimit blocks.

```eko
classify: (n: Int) -> String =
    if n < 0:
        "negative"
    else:
        "non-negative"
```

The body of `classify` is a block; the two branches of the `if` are nested blocks one level deeper.

The layout of `case` scrutinees and arms is specified in `03-control.md §3.4`.

### 1.1 The indentation rule

One indentation level is exactly **four spaces**. A block member starts in the column established by the first member; a nested block starts one or more levels — a multiple of four columns — to the right of its header. A member whose column is not a multiple of four relative to the enclosing block is a compile error.

A tab character is not valid whitespace. A raw tab is a compile error (`E-tab`, 09-library.md §7) in indentation and between tokens; inside comments it is ignored. String and character literals write a tab as `\t` (§4.3).

### 1.2 Keywords and identifiers

The reserved words are exactly `import`, `case`, `if`, and `else`; they cannot be used as identifiers. `type` and `test` are **not** reserved: each is recognized only in the annotation slot after `:` (04-types.md §1, 09-library.md §6), and both remain legal as binding names, record field names, and module names.

An identifier is a sequence of Unicode characters: the first character has the Unicode property XID_Start or is `_`, and every following character has XID_Continue. Non-ASCII identifiers are legal. The case rules (04-types.md §3) are enforced on this spelling: a name whose first character is uppercase or titlecase (Unicode categories `Lu` or `Lt`) and which contains no underscore is a PascalCase type or constructor name; any other name in binding position is snake_case.

### 1.3 Semicolons as newlines

`;` is equivalent to a newline as a member terminator: it ends the current block member and every block opened within that member. Whether the enclosing construct then continues is decided by the construct itself — a `case` continues while further arms follow, and the then-block of an `if` continues until its `else` or the end of the enclosing member (03-control.md §3.5).

## 2. Line continuation

A newline normally ends a block member. Continuation is licensed inside brackets and before a chain dot; a trailing callback may also occupy its own line.

### 2.1 Inside brackets

Inside parentheses, newlines are free formatting. The matching `)` closes the bracket group, and every line up to it belongs to the same call. The closing `)` also ends any callback block opened within the group.

```eko
fs.with_file(
    "data.csv",
    Read,
    callback)
```

```eko
fs.with_file("data.csv", Read,
    callback)
```

Both examples denote the same call. Indentation inside the parentheses is free formatting for the argument list; callback bodies follow §2.2.

### 2.2 Trailing callbacks

A trailing callback is the last argument of a call. It may be placed on a line of its own. The callback header `(p, i):` occupies that line and opens a block; the body is indented one level further. A body consisting of a single expression may instead follow the colon on the same line.

```eko
persons.each_with_index(
    (p, i):
        out.println("{i}: {p}"))
```

The closing `)` of the call ends both the callback block and the argument list.

### 2.3 Chained calls

A receiver chain may continue on the next line. Each continuation line begins with `.` and is aligned in the same column as the first `.` of the chain.

```eko
content.split("\n")
       .map(String.trim)
       .filter((l): l != "")
       .length()
```

Each link receives the value of the previous link as its receiver (`05-names.md §3`).

### 2.4 Bit markers at end of line

The world-bit markers `!` and `?` never license a continuation. A lone
marker occurs only in the elided-arrow forms `(a) !` and `(a) ?`
(`06-world.md §5.2`): it is a self-delimiting token, and a line that ends
with one — `main: (out: Out) !` — is a complete block member. The joined
tokens `!->` and `?->` are single tokens and never wrap. Continuation is
licensed only inside brackets (§2.1), by a trailing callback (§2.2), and
before a chain dot (§2.3).

## 3. Comments

`//` begins a line comment that extends to the end of the line. Block comments do not exist.

`///` begins a documentation comment. A documentation comment is written on the line or lines immediately before the binding it documents. It attaches to the following binding; top-level and block-level bindings are both eligible, and `type`, `test`, and `main` declarations are treated alike. Consecutive `///` lines aggregate into one document. The content is free text.

`///` has no semantics: it does not participate in compilation and does not suppress warnings. Extraction and rendering belong to the documentation toolchain; the language surface is the token itself.

```eko
/// Returns a greeting for the given name.
/// The name is interpolated into the result.
greet: (name: String) -> String =
    "hi {name}"
```

## 4. Literals

### 4.1 Numbers

Integer literals are decimal by default; hexadecimal and binary forms
(`0x`, `0b`) complete the numeric surface below. An underscore may group digits. A floating literal contains a decimal point or an exponent, with digits on both sides of the point: `1.5` and `1.5e3` are floats, while `255.to_hex()` is the integer `255` followed by a postfix call, not an incomplete float.

An integer literal is `Int`; a floating literal is `Float`. The two types are distinct, and there is no implicit conversion between them (`1 / 2.0` is a type error; see §6.2). A leading `-` is the unary operator of §6, not part of the literal.

Hexadecimal and binary literals are written `0x` and `0b`. The prefix is
lowercase; the digits are case-insensitive (`0xFF` and `0xff` are equal).
They are placed by expected type exactly as decimal literals are —
default `Int`, a `UInt` or machine-word position by expectation — and a
value outside the target range is a compile error (`E-literal-overflow`).
There are no digit separators beyond `_` and no type suffixes.

```eko
mask: UInt = 0xFF                  // hex literal in a UInt position
b:    U8   = 0b1010                // binary literal in a machine word
magic: Int = 0x1FEED               // default Int
```

```eko
big: Int   = 1_000_000      // underscore digit grouping
pi:  Float = 3.14159        // a dot makes the literal a Float
k:   Float = 1.5e3          // an exponent also makes it a Float
age: UInt  = 36             // integer literal in a UInt position
```

Integer literals are placed by expected type, with `Int` as the default: an
integer literal in a `UInt` position is a `UInt` (`age: UInt = 36`), and an
integer literal with no expectation is an `Int`. `Int` and `Float` literals
are never implicitly converted into each other. An integer literal whose
value exceeds the range of its target type is a compile error
(`E-literal-overflow`, 09-library.md §7); arbitrary-precision integers do not
exist (§5).

### 4.2 Characters

`Char` literals use single quotes. A `Char` is a Unicode scalar value: a code point in `0..0x10FFFF`, excluding surrogate code points. Non-ASCII characters are written directly in UTF-8.

The escapes are those of strings plus `\'`.

`Char` is not a one-character `String`: the types are not interchangeable, and `'a' ++ "bc"` is a type error. Conversions are explicit (`08-builtins.md §2`).

```eko
c:      Char = 'a'
letter: Char = 'λ'      // non-ASCII written directly
nl:     Char = '\n'     // same escapes as String, plus \'
```

Details of `Char`, including code-point ordering and the bridge functions, are in `08-builtins.md §2`.

### 4.3 Strings, escapes, interpolation

String literals use double quotes, are encoded as UTF-8, and are single-line. A newline must be written `\n`; multiline string literals do not exist.

The escapes are `\{`, `\n`, `\t`, `\r`, `\\`, `\"`, and `\u{…}`. The escape `\{` produces a literal `{` and does not open an interpolation slot; `\t` and `\r` produce the tab and carriage-return characters. `\u{…}` produces one character: one to six hexadecimal digits name a Unicode scalar value (`"\u{41}"` is `"A"`, `'\u{1F600}'`), and a value that is not a scalar — a lone surrogate — is a compile error.

An interpolation slot `{e}` is replaced by `show(e)`. `show` is defined for every type, is the display channel, and may be overridden; a capability displays as `<cap>`. The structural channel `inspect` is not overridable. The display of an `IoError` in `"io: {e}"` is produced by `show` (`08-builtins.md §6`). At top level, `show` renders a `String` without quotes, so `"hi {name}"` inserts the characters of `name`. An interpolation slot contains an arbitrary expression — arithmetic, calls, field access, and so on. Braces inside the slot must balance, and nested string literals, with their own interpolations of their own, are allowed.

```eko
s:     String = "line\n"
greet: String = "hi {name}, n={n}"      // {name} and {n} are show(e) slots
```

### 4.4 Ranges

Two range forms exist: `a..b` is end-exclusive (open at `b`), and `a..=b` is end-inclusive. Both produce a `Range` value (a standard-library type); the element type is `Int`, `UInt`, or `Char`.

Range operators are infix and bind less tightly than postfix operators, so a chain over a range requires parentheses: `(1..=100).map(f)`.

Lexically, an integer literal immediately followed by `..` does not begin a floating literal.

Unbounded and `Float` ranges are excluded: an unbounded range offers no end to strict iteration, and a `Float` endpoint admits no exact unit step. A stepped traversal filters an index range: `(0..n).filter((i): i % s == 0)`.

Range values are consumed by the iteration combinators; `Range.each` and `Range.map` are `?` dispatchers (`06-world.md §5`).

```eko
(1..=100).map(f)        // inclusive: 1 through 100
(0..n).filter(g)        // end-exclusive: 0 through n-1
```

Both endpoints of a range must have the same type, and the range's type is
`Range` instantiated at that type: Int endpoints produce `Range Int`, UInt
endpoints produce `Range UInt`, and Char endpoints produce `Range Char` —
`'a'..'z'` iterates code points in code-point order.

## 5. Numeric types

Eko has two families of numeric type names.

| Family | Names | Meaning |
|---|---|---|
| Domain types | `Int`, `UInt`, `Float` | Fixed 64-bit values: `Int64`, `UInt64`, `Float64`. |
| Machine types | `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `F32`, `F64` | Abbreviations that signal a machine boundary; they correspond to machine-level names such as `i32` and `u32`. Machine-interface lexicon (`11-ffi.md §3.1`): they hold, compare, show, and ferry — no arithmetic. |

Full spelling denotes a domain type; an abbreviation denotes a machine boundary.

Domain types are the default surface. `Int` is a 64-bit signed integer, `UInt` a 64-bit unsigned integer, and `Float` a 64-bit floating-point value. The width is fixed at 64 bits, not the platform word size, so the domain types do not vary with the host.

`UInt` is the primary type for natural numbers and the correct domain for non-negative modular arithmetic.

Machine types are not interchangeable with domain types; conversions are explicit (`08-builtins.md §4`).

Arbitrary-precision integers (bignum) are rejected by default. A bignum type would conflict with `UInt` as the primary numeric type, with FFI mapping, and with the zero-cost erasure model; overflow panics (§6.2) instead of silently widening.

Conversions that may fall outside the target domain return `Option`; `Int.to_float` and `UInt.to_float` are total (`08-builtins.md §4`).

## 6. Operators

Operator symbols are notation over the function namespace. The operator set is closed: no user-defined operators exist.

### 6.1 Desugaring to namespace functions

Most operators are sugar for a function in the type namespace of their operands. The following equations are shown for `Int`, `List`, and `Bool`; `UInt` and `Float` provide analogous namespace members for the arithmetic rows.

```eko
a + b    ≡  Int.add(a, b)        // numeric domain; UInt and Float analogous
a % b    ≡  Int.rem(a, b)
a < b    ≡  Int.lt(a, b)         // yields Bool
a && b   ≡  if a: b else: False
a || b   ≡  if a: True else: b
-a       ≡  Int.neg(a)
xs ++ ys ≡  List.append(xs, ys)   // concatenation; dictionary merge keeps the right side
```

`&&` and `||` are sugar for `if`, not functions. All other operators in the table are sugar for namespace functions.

`/` is `Int.quot`; `%` is `Int.rem`. The remaining arithmetic symbols resolve analogously to members of the operand type's namespace.

UFCS makes the member form and the operator form equally available: `a.add(b)` and `a + b` denote the same call (`05-names.md §1`). Overloading is namespace resolution: a symbol resolves into the namespace of its operand type; there is no second overloading mechanism.

Concatenation, bitwise operations, and exponentiation are functions underneath (`List.append`; bitwise operations are the `UInt` function family, `08-builtins.md §4.6`; exponentiation is `Int.pow`, or `Float.pow` for floats).

The governing axis is: notation for machine primitives and short-circuiting; functions for everything else.

### 6.2 Arithmetic semantics

Arithmetic comprises the binary operators `+`, `-`, `*`, `/`, `%` and unary `-`.

`/` and `%` truncate toward zero and are paired on the same basis: `-7 / 2 = -3` and `-7 % 2 = -1` (LLVM `sdiv`/`srem`).

The floor family is provided by functions: `Int.div(-7, 2) = -4` and `Int.mod(-7, 2) = 1`; the remainder takes the sign of the divisor. Mathematics, calendars, and ring domains use this family.

`%` is defined over the numeric domain. For `Float`, it uses the same truncation basis (`fmod`).

`UInt` values are non-negative, so the truncation/floor distinction vanishes; the correct domain for non-negative modular arithmetic is `UInt`.

Overflow and division by zero panic. This includes `Int.min / -1`, which overflows. A panic does not set the world bit (`06-world.md §5`).

Wrapping forms are explicit functions: `Int.wrapping_*` and `UInt.wrapping_*`
(`08-builtins.md §4.6`).

An out-of-range collection access is not a panic: it is a domain check that returns `Option` (`05-names.md §4`).

No implicit numeric conversion is applied: `1 / 2.0` is a type error. Conversion is explicit through namespace functions such as `Int.to_float()` and `UInt.to_int()`. `UInt.to_int()` returns `None` above `Int.max`; the failure taxonomy is in `08-builtins.md §4`.

### 6.3 Logical operators

`&&` and `||` are lazy: the right operand is evaluated only when the result requires it. Short-circuiting is branching, so both are sugar for `if` (§6.1). Both operands are `Bool`.

The world bit is transparent through both: if either operand is world-touching, the enclosing expression sets the world bit as usual (`06-world.md §5`).

```eko
if size > 0 && fs.exists(path):      // right operand is world-touching; the bit is set
    …
```

`not` requires no laziness and is a function: `b.not()` or `Bool.not(b)`.

`!` belongs exclusively to the world-bit marker (`06-world.md §1`). Prefix `!x` does not exist.

Symbols are used instead of the words `and`, `or`, and `not`. Using words would retire three identifiers, rule out `Bool.and` and `Bool.not` as member names, and break the rule that keywords open blocks. Symbols consume no identifiers; the single-character forms `&` and `|` are unassigned — `|` in pattern position is the or-pattern separator (03-control.md §5.3).

The `Bool` type is a standard-library algebraic data type (ADT):

```eko
Bool: type = True | False
```

Conditions and guards are `Bool`; there is no truthiness coercion, and `True` and `False` are the only constructors (`03-control.md §3`, `§4`).

### 6.4 Concatenation and merging `++`

`++` concatenates lists, strings, and `Bytes`. For dictionaries it merges; on a duplicate key the right operand wins.

`++` does not wrap elements. `prefix ++ {x} ++ suffix` appends the list `{x}`; the type checker does not guess that `x` should be wrapped.

Tuples do not support `++`; they are fixed-length heterogeneous products. Records do not support `++`; a partial record literal is legal only in the argument position of `with` (`08-builtins.md §6`).

Characters do not concatenate with strings: `'a' ++ "bc"` is a type error.

`++` has the same precedence as `+` and `-` and associates to the left (§6.5).

```eko
xs ++ ys ++ {7}          // append one element; the element is wrapped explicitly
defaults ++ overrides    // dictionary merge; the right side wins
```

### 6.5 Precedence and associativity

The precedence ladder, from tightest to loosest, is:

| Level | Operators | Associativity |
|---|---|---|
| 1 | postfix `.f()` | left |
| 2 | unary `-` | — |
| 3 | `* / %` | left |
| 4 | `+ - ++` | left |
| 5 | `== != < <= > >=` | none (no chaining) |
| 6 | `&&` | left |
| 7 | `\|\|` | left |

Comparisons have no associativity. A chain such as `a < b < c` is a parse error, not a left-associative sequence; write `a < b && b < c`. The same applies to `==` and the other comparison operators.

`-x.f()` parses as `-(x.f())`: postfix binds before unary minus. `b.not()` is a postfix call and therefore binds before every operator.

Range operators are infix and bind less tightly than postfix; see §4.4.

Comparison operators produce `Bool` and dispatch by operand type (`Int.eq`, `String.lt`). Both operands must have the same type; `1 == 1.0` is a type error. The comparison operators are defined only on the value layer — the numeric types, `Bool`, `Char`, `String`, `Bytes` — where `Float` equality is IEEE (`NaN != NaN`). Every other type compares through its `eq`/`lt` members, called explicitly (`08-builtins.md §6.4`, `§6.5`); capabilities and functions have no equality at all.

## 7. Excluded syntax

The following constructs are not part of Eko; every exclusion is permanent.

| Construct | Rule |
|---|---|
| User-defined operators | No mechanism exists; the operator set is closed. |
| Bitwise operators `&`, `\|`, `^`, `<<`, `>>`, `~` | Do not exist — the functions are the vocabulary (`08-builtins.md §4.6`). The single-character forms `&` and `\|` remain reserved for or-patterns. |
| Exponentiation `**` | Does not exist. Use `Int.pow` or `Float.pow`. |
| Increment and decrement `++`, `--` | Do not exist as statements; values are immutable. `++` is concatenation only. |
| Ternary `?:` | Does not exist; `if` is an expression (`03-control.md §4`). |
| Chained comparisons | Parse error; write `a < b && b < c` (§6.5). |
| Block comments `/* ... */` | Do not exist; use `//`. |
| Numeric literal type suffixes | Do not exist; literals are placed by expected type (§4.1). Formatting and parsing are functions (`09-library.md §2`). |
| Tab characters as whitespace | Compile error (`E-tab`); indentation is spaces, four per level (§1.1). A raw tab is ignored only inside comments; literals write `\t`. |
| Truthiness coercion | Does not exist; conditions and guards are `Bool`. |
| Word operators `and`, `or`, `not` | Not operators and carry no special meaning; use `&&`, `\|\|`, and `b.not()`. |
| Prefix `!x` | Does not exist; `!` is the world-bit marker only (`06-world.md §1`). |
| Labels or bit variables after `!` or `?` | Not permitted; the markers take no labels or variables (`06-world.md §1`). |
| Index syntax `x[i]` | Does not exist for any structure; use `get`, or `at` for strings (`05-names.md §4`). |
| Multiline string literals | Do not exist; use `\n`. |
