# 5. Lexical Structure

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
position are specified in Clause 6.4.1 (a lambda parameter), Clause 7.5.4
(the unit type and the unit value), and Clause 7.7.1 (the wildcard).

### 5.10.2 Punctuation Tokens

The punctuation tokens are:

```text
(   )   [   ]   {   }
,   :   .   =   |
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

Longest match holds for every token class, numeric prefixes included: `0x`
followed by no hexadecimal digit is not a hexadecimal literal and lexes as
the integer `0` followed by the identifier `x`; likewise `0b` with no binary
digit.

## 5.11 Lexical Exclusions

The following lexical forms do not exist. Their occurrence is an error.

| Form | Rule |
|---|---|
| Tab as whitespace | `E-tab`; indentation is spaces (Clause 5.2.3) |
| Block comments `/* ... */` | no such form outside a string literal; use `//` (Clause 5.4.1); inside a slot it is two operator tokens |
| Multiline string literals | strings are single-line (Clause 5.9.1) |
| Numeric type suffixes (`1u8`, `1.0f`) | no suffixes; placement is by expected type (Clause 5.7.4) |
| Character literals with more than one scalar | one scalar per `Char` (Clause 5.8) |
| Byte-string literals (`b"..."`) | construction is by function (Clause 9.5) |
| Bitwise operator tokens `&`, `^`, `~`, `<<`, `>>` | no such tokens; use functions (Clause 9.6.5) |
| Prefix `!` as negation | `!` is the world-bit marker only (Clause 7.9.2) |
| Unassigned symbols such as `@`, `$`, `` ` `` | not tokens |

## 5.12 Malformed Tokens and Recovery

A malformed maximal token — an unterminated string, an invalid escape, a bad
digit grouping — is diagnosed, and the lexer resumes so that parsing sees
token shapes the grammar admits:

- When the malformed span has a prefix that is a valid token of the grammar,
  the lexer emits that prefix token and resumes after it. `0x` with no
  hexadecimal digit emits the integer `0` followed by the identifier `x`
  (Clause 5.10.3).
- Every other malformed span is consumed to the recovery boundary — the next
  line terminator or the next bracket — as one unit, with one diagnostic. A
  block comment spelling is such a span: it is consumed through its closing
  `*/` or to the line terminator, whichever comes first. A character
  literal that holds more than one scalar value is consumed to its closing
  quote and emits no character token; a string that holds an invalid escape
  emits its string token with the invalid escape dropped.

One lexical defect yields one diagnostic; recovery does not cascade. The
recovered stream is not a program: the defect is fatal to the compilation
(Clause 2.4), and the recovered shapes exist so that later stages can name
later defects.
