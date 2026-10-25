# 5. Lexical Structure

This clause defines the translation from source text to tokens. The lexical
grammar is given in Annex A.2. A source text that cannot be translated into
tokens is rejected with an error diagnostic.

## 5.1 Source Text

### 5.1.1 Encoding

An Eko source file has the extension `.eko`. Its content is a sequence of
Unicode scalar values encoded in UTF-8 (RFC 3629). The encoding MUST be valid
UTF-8; an invalid byte sequence is a lexical error.

A byte-order mark (U+FEFF) at the start of the file is ignored. A U+FEFF at
any other position is an error.

### 5.1.2 Line Terminators

A **line terminator** is U+000A (LINE FEED). A U+000D (CARRIAGE RETURN)
immediately preceding a line terminator is ignored. A U+000D at any other
position is an error.

A source file MAY end with or without a final line terminator. The end of the
file is treated as a line terminator for layout purposes.

### 5.1.3 End of File

The end of the file terminates the last line and closes every block that is
still open. A block that cannot be closed consistently with the layout rules
of Clause 5.2 is an error.

## 5.2 Layout and Indentation

### 5.2.1 Indentation Levels

One **indentation level** is exactly four U+0020 SPACE characters. Indentation
is significant: it determines block membership.

A line's **indentation** is the sequence of spaces at its start. Two lines are
at the same indentation level when they have the same number of leading
spaces.

### 5.2.2 Block Structure

A block is introduced by a header line and consists of the lines that follow
it at one indentation level deeper than the header. The first member line
establishes the block's column. Every member of the block begins in that
column. A nested block begins one or more levels to the right of its header.

The indentation of a member, relative to the enclosing block, MUST be a
multiple of four spaces. Any other relative indentation is an error.

Blank lines and lines that contain only a comment do not establish a block
column and do not terminate a block.

### 5.2.3 Tabs

U+0009 (CHARACTER TABULATION) is not valid whitespace in indentation or
between tokens. Its occurrence there is an error with code `E-tab` (Annex B).

A tab inside a comment is ignored. A tab inside a string or character literal
is written with the escape `\t` (Clause 5.9.2).

### 5.2.4 Blank Lines

Blank lines and comment-only lines are ignored for layout. They MAY appear
anywhere a line terminator is permitted.

## 5.3 Tokens and Separators

### 5.3.1 Token Classes

The following token classes exist:

| Class | Examples |
|---|---|
| identifier | `person`, `Person`, `_tmp` |
| keyword | `import`, `case`, `if`, `else` |
| literal | `42`, `1.5`, `'a'`, `"text"` |
| operator | `+`, `==`, `&&`, `++`, `..=` |
| punctuation | `(`, `)`, `[`, `]`, `{`, `}`, `,`, `:`, `.` |
| line terminator | U+000A |
| semicolon | `;` |
| end of file | the end of the source text |

The contextual words `type`, `ctype`, `test`, `extern`, and `Pure` are
lexically identifiers. They acquire their special meaning only in the
positions specified in Clause 5.6.2.

### 5.3.2 Semicolons

A semicolon `;` is equivalent to a line terminator as a member separator. It
terminates the current member and every block opened within that member.
Whether the enclosing construct continues after the semicolon is decided by
that construct, exactly as for a line terminator.

### 5.3.3 Newlines

A line terminator ends a block member. It does not end a member where a
continuation is licensed by Clause 6.2.4. A line terminator inside a bracket
group is free formatting.

## 5.4 Comments

### 5.4.1 Line Comments

The characters `//` begin a line comment. The comment extends to the end of
the line and is ignored. Block comments do not exist.

### 5.4.2 Documentation Comments

The characters `///` begin a documentation comment. A documentation comment
attaches to the binding on the immediately following line. Consecutive `///`
lines form one document.

A documentation comment MAY appear before a top-level or block-level binding.
It has no effect on compilation and does not suppress a diagnostic. Its
content is free text. Extraction and rendering are toolchain concerns
(Clause 11.3).

```eko
/// Returns a greeting for the given name.
/// The name is interpolated into the result.
greet: (name: String) -> String =
    "hi {name}"
```

## 5.5 Identifiers

### 5.5.1 Syntax

An **identifier** begins with a character that has the Unicode property
XID_Start or is U+005F LOW LINE (`_`). Every following character MUST have the
Unicode property XID_Continue. Non-ASCII identifiers are permitted.

Identifiers are case-sensitive. Two identifiers that differ only in case are
different identifiers.

### 5.5.2 Case Classes

Every identifier belongs to one of two case classes.

A leading `_` is the privacy prefix (Clause 7.3.1). It is not part of the
name's case class; the class is determined by the characters after the prefix.
A name consisting only of `_` is the unit value and wildcard token; it is not
a binding name and has no case class.

| Class | Condition | Used for |
|---|---|---|
| **PascalCase** | the first character after the privacy prefix is in Unicode category `Lu` or `Lt`, and the remainder contains no `_` | type names, constructors, namespace names |
| **snake_case** | every other identifier | value bindings, functions, modules, type variables |

A name that violates the case class required at its position is an error with
code `E-name-case` (Annex B). The required class for each position is
specified in Clause 7.4 and Clause 7.5. A private type is therefore spelled
`_Person`, and a private function `_parse`; the prefix does not change the
class.

```eko
Person: type = [name: String]
_Person: type = [name: String]     // private type; PascalCase after the prefix
show_name: (p: Person) -> String = p.name
_show_name: (p: Person) -> String = p.name  // private function; snake_case
```

## 5.6 Keywords and Contextual Words

### 5.6.1 Reserved Words

The reserved words are exactly:

```text
import   case   if   else
```

A reserved word MUST NOT be used as an identifier.

### 5.6.2 Contextual Words

The following words have a special meaning only in the indicated position and
are otherwise ordinary identifiers.

| Word | Position | Meaning |
|---|---|---|
| `type` | annotation slot after `:` | type declaration (Clause 6.3.5) |
| `ctype` | annotation slot after `:` | C layout record declaration (Clause 6.3.6) |
| `test` | annotation slot after `:` | test declaration (Clause 6.3.7) |
| `extern` | right-hand side beginning a native declaration | extern declaration (Clause 6.3.8) |
| `Pure` | tail of an extern declaration | audited purity guarantee (Clause 10.2.2) |

A contextual word remains legal as a binding name, a record field name, and a
module name. For example, a binding named `type` is legal; `x: type = ...` is a
type declaration only when `type` occupies the annotation slot.

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

### 5.7.3 Digit Grouping

An underscore MAY separate digits within an integer or floating literal, as
in `1_000_000`. An underscore MUST NOT be the first or last character of the
digit sequence, and two underscores MUST NOT be adjacent. Grouping is
available in decimal, hexadecimal, and binary literals.

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
- An empty slot `{}` is an error.

The slot expression is an arbitrary expression. Its type MUST have a `show`
service; every type does (Clause 9.8.1). A capability renders as `<cap>`.

```eko
name:  String = "ada"
n:     Int    = 3
greet: String = "hi {name}, n={n}"
open:  String = "set \{ is literal"
nested: String = "outer {"inner {n}"}"
```

## 5.10 Operators and Punctuation

### 5.10.1 Operator Tokens

The operator tokens are:

```text
+   -   *   /   %   ++
==  !=  <   <=  >   >=
&&  ||  !   ?
->  <-  !->  ?->
..  ..=
```

The tokens `!` and `?` are markers (Clause 7.9.3). They are not prefix or
infix operators. `!->` and `?->` are single tokens, written joined.

The token `|` separates variants in a type declaration and alternatives in an
or-pattern. It is not an operator.

The token `_` is an identifier-like token, not punctuation. Its meanings by
position are specified in Clause 7.5.4.

### 5.10.2 Punctuation Tokens

The punctuation tokens are:

```text
(   )   [   ]   {   }
,   :   ;   .   =   |
:=  
```

`=` is both the initial-binding operator and the separator in a type
declaration. `:` introduces an annotation, a block, a `case` arm result, or a
record field, according to position.

### 5.10.3 Longest Match

The lexer consumes the longest token that matches at the current position. The
following pairs and triples are therefore single tokens and never split:

| Sequence | Token |
|---|---|
| `!->` | world-touching arrow |
| `?->` | open arrow |
| `->` | pure arrow or arm arrow |
| `<-` | inverted binding |
| `:=` | rebinding |
| `..=` | inclusive range |
| `..` | exclusive range |
| `==`, `!=`, `<=`, `>=` | comparison |
| `&&`, `\|\|` | logical |
| `++` | concatenation |

A `.` begins a fractional part only when it is immediately preceded by a
decimal digit and immediately followed by a decimal digit (Clause 5.7.2). The
expression `1..2` is `1`, `..`, `2`; it is not `1.` followed by `.2`. The
expression `255.to_hex()` is `255`, `.`, `to_hex`, `(` , `)`.

## 5.11 Lexical Exclusions

The following lexical forms do not exist. Their occurrence is an error.

| Form | Rule |
|---|---|
| Tab as whitespace | `E-tab`; indentation is spaces (Clause 5.2.3) |
| Block comments `/* ... */` | no such form; use `//` (Clause 5.4.1) |
| Multiline string literals | strings are single-line (Clause 5.9.1) |
| Numeric type suffixes (`1u8`, `1.0f`) | no suffixes; placement is by expected type (Clause 5.7.4) |
| Character literals with more than one scalar | one scalar per `Char` (Clause 5.8) |
| Byte-string literals (`b"..."`) | construction is by function (Clause 9.5) |
| Bitwise operator tokens `&`, `^`, `~`, `<<`, `>>` | no such tokens; use functions (Clause 9.6.5) |
| Prefix `!` as negation | `!` is the world-bit marker only (Clause 7.9.2) |
| Unassigned symbols such as `@`, `$`, `` ` `` | not tokens |
