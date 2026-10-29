# 5. Lexical Structure

## 5.7 Numeric Literals

### 5.7.1 Integer Literals

Integer literals are written in decimal, hexadecimal, or binary form.

| Form | Prefix | Digits | Example |
|---|---|---|---|
| decimal | none | `0`–`9` | `255` |
| hexadecimal | `0x` | `0`–`9`, `a`–`f`, `A`–`F` | `0xFF` |
| binary | `0b` | `0`, `1` | `0b1010` |

The prefix is lowercase. Hexadecimal digits are case-insensitive: `0xFF` and
`0xff` denote the same value. There are no type suffixes.

An integer literal denotes a mathematical integer. Its type is determined by
the expected type (Clause 7.6.4):

- under an expected `Int`, the literal is `Int`;
- under an expected `UInt` or machine-word type, the literal is placed at that
  type, and a value outside the target range is an error with code
  `E-literal-overflow` (Annex B);
- with no expected type, the literal is `Int`.

A literal whose value is outside the range of its target type is an error with
code `E-literal-overflow` (Annex B).

`Int` and `Float` are distinct, and an integer literal is never implicitly
converted to `Float` (Clause 7.8.4).

### 5.7.2 Floating Literals

A floating literal contains a decimal point or an exponent.

| Form | Example | Notes |
|---|---|---|
| decimal point | `1.5` | digits on both sides of `.` |
| exponent | `1.5e3`, `1e3` | the exponent marker is `e` |
| both | `1.5e-3` | optional sign after `e` |

A floating literal denotes a `Float`. A leading `-` is the unary operator of
Clause 6.5.4, not part of the literal.

The forms `.5` and `1.` are not floating literals. A `.` is part of a floating
literal only when it is preceded by a decimal digit and followed by one.

A floating literal denotes a finite `Float`. A literal whose value is not
finite after conversion — an overflow such as `1e999`, which a naive
conversion would deliver as infinity — is an error with code
`E-literal-overflow` (Annex B).

### 5.7.3 Digit Grouping

An underscore MAY separate digits of the integer or fraction part of an
integer or floating literal, as in `1_000_000`; the exponent part of a
floating literal carries no grouping. An underscore MUST NOT be the first or
last character of the digit sequence, and two underscores MUST NOT be
adjacent. Grouping is available in decimal, hexadecimal, and binary literals.

### 5.7.4 Placement by Expected Type

A numeric literal is placed by the expected type at its position. The
following rules apply.

- An integer literal in a `UInt` or machine-word position is placed at that
  type. If the value is outside the type's range, the program is rejected.
- A floating literal in a `Float` position is a `Float`. A floating literal in
  an `Int` or machine-word position is an error.
- An integer literal in a `Float` position is an error; write `Int.to_float`.
- A literal in a position with no expected type defaults to `Int` for integers
  and `Float` for floats.

```eko
mask:  UInt = 0xFF
bits:  U8   = 0b1010
magic: Int  = 0x1FEED
big:   Int   = 1_000_000
pi:    Float = 3.14159
k:     Float = 1.5e3
age:   UInt  = 36
```

The expression `255.to_hex()` is the integer literal `255` followed by a
postfix member call, not an incomplete floating literal; the `.` is not
followed by a digit.

## 5.8 Character Literals

A character literal is written between single quotes and denotes exactly one
Unicode scalar value. The scalar value MUST be in the range `0..0x10FFFF` and
MUST NOT be a surrogate code point.

The escapes are those of string literals (Clause 5.9.2), plus `\'` for a single
quote. A character literal MUST contain exactly one scalar value; `''` and
`'ab'` are errors.

Non-ASCII characters MAY be written directly in UTF-8.

```eko
c:      Char = 'a'
letter: Char = 'λ'
nl:     Char = '\n'
quote:  Char = '\''
```

`Char` is not a one-character `String`. The types do not interchange, and
`'a' ++ "bc"` is an error (Clause 7.8.6). Conversions are specified in
Clause 9.4.3.

## 5.9 String Literals

### 5.9.1 Encoding and Single-Line Rule

A string literal is written between double quotes and denotes a `String`. The
content is encoded as UTF-8. A string literal is single-line: a literal line
terminator inside the quotes is an error. A newline is written `\n`.

### 5.9.2 Escapes

The escapes are:

| Escape | Value |
|---|---|
| `\{` | a literal `{`, with no interpolation |
| `\n` | U+000A |
| `\t` | U+0009 |
| `\r` | U+000D |
| `\\` | U+005C |
| `\"` | U+0022 |
| `\'` | U+0027 (character literals only) |
| `\u{...}` | one Unicode scalar value, written with one to six hexadecimal digits |

A `\u{...}` escape MUST denote a Unicode scalar value. A surrogate code point
or a value above U+10FFFF is an error. An escape not listed in this table is
an error.

### 5.9.3 Interpolation

Within a string literal, an unescaped `{` begins an **interpolation slot**.
The slot ends at the matching `}`. The expression inside the slot is
evaluated, and its value is rendered with `show` (Clause 9.8.1).

- Braces inside a slot MUST balance.
- A `}` that does not close an open slot is an ordinary character.
- A slot MAY contain a nested string literal with its own slots.
- An empty slot `{}` is an error with code `E-empty-slot` (Annex B).

The slot expression is an arbitrary expression. Its type MUST have a `show`
service; every type does (Clause 9.8.1). A capability renders as `<cap>`.
The slot is an expression region, not source text: comment forms do not
occur inside it (Clause 5.4.1), and `//` and `/*` lex as operator tokens.

```eko
name:  String = "ada"
n:     Int    = 3
greet: String = "hi {name}, n={n}"
open:  String = "set \{ is literal"
nested: String = "outer {"inner {n}"}"
```
