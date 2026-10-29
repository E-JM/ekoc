# Annex A. Grammar

## A.1 Notation

The notation is defined in Clause 1.4.2. Terminals are written in single
quotes. A production written with `NEWLINE`, `INDENT`, and `DEDENT` refers to
the virtual tokens inserted by the layout algorithm of Annex A.4.

The lexical grammar (Annex A.2) consumes Unicode scalar values and produces
tokens. The syntactic grammar (Annex A.3) consumes tokens.

## A.2 Lexical Grammar

### A.2.1 Source Text

```ebnf
SourceFile      := { Character }
Character       := Unicode scalar value
LineTerminator  := U+000A
```

A U+000D immediately before a line terminator is ignored. A U+FEFF at the
start of the file is ignored. The source MUST be valid UTF-8.

### A.2.2 Tokens

```ebnf
Token           := Identifier
                 | Keyword
                 | Literal
                 | Operator
                 | Punctuation
                 | NEWLINE
                 | ';'
                 | EOF

Identifier      := XID_Start { XID_Continue }
XID_Start       := Unicode property XID_Start | '_'
XID_Continue    := Unicode property XID_Continue

Keyword         := 'import' | 'case' | 'if' | 'else'

Literal         := IntegerLiteral
                 | FloatLiteral
                 | CharLiteral
                 | StringLiteral

Operator        := '+' | '-' | '*' | '/' | '%' | '++'
                 | '==' | '!=' | '<' | '<=' | '>' | '>='
                 | '&&' | '||' | '!' | '?'
                 | '->' | '<-' | '!->' | '?->'
                 | '..' | '..='

Punctuation     := '(' | ')' | '[' | ']' | '{' | '}'
                 | ',' | ':' | '.' | '=' | ':=' | '|'
```

The tokens `!->`, `?->`, `->`, `<-`, `:=`, `..=`, `..`, `==`, `!=`, `<=`,
`>=`, `&&`, `||`, and `++` are matched before their prefixes (Clause 5.10.3).

### A.2.3 Identifiers

```ebnf
Identifier      := XID_Start { XID_Continue }
```

The case class of an identifier is defined in Clause 5.5.2. The nonterminals
`PascalName` and `snake_case Identifier` in Annex A.3 denote an identifier of
that case class; a leading privacy prefix does not change the class.

### A.2.4 Integer Literals

```ebnf
IntegerLiteral  := DecimalInteger | HexInteger | BinaryInteger
DecimalInteger  := DecDigit { ['_'] DecDigit }
HexInteger      := '0x' HexDigit { ['_'] HexDigit }
BinaryInteger   := '0b' BinDigit { ['_'] BinDigit }
DecDigit        := '0' | ... | '9'
HexDigit        := DecDigit | 'a' | ... | 'f' | 'A' | ... | 'F'
BinDigit        := '0' | '1'
```

An underscore MUST be between two digits. A prefix with no digits is not a
hexadecimal or binary literal; longest match (Clause 5.10.3) yields the
integer `0` followed by an identifier.

### A.2.5 Floating Literals

```ebnf
FloatLiteral    := DecDigit { ['_'] DecDigit }
                   ( '.' DecDigit { ['_'] DecDigit } [ Exponent ]
                   | Exponent )
Exponent        := 'e' [ '+' | '-' ] DecDigit { DecDigit }
```

A floating literal contains a decimal point or an exponent. The forms `.5`
and `1.` are not floating literals.

### A.2.6 Character Literals

```ebnf
CharLiteral     := "'" ( CharEscape | CharChar ) "'"
CharChar        := any Unicode scalar value except "'" and '\' and U+000A
CharEscape      := '\' ( 'n' | 't' | 'r' | '\' | '"' | "'" | 'u{' HexDigits '}' )
HexDigits       := HexDigit [ HexDigit [ HexDigit [ HexDigit [ HexDigit [ HexDigit ] ] ] ] ]
```

A character literal contains exactly one Unicode scalar value. A surrogate
code point is not a scalar value.

### A.2.7 String Literals

```ebnf
StringLiteral   := '"' { StringItem } '"'
StringItem      := StringChar | StringEscape | Interpolation
StringChar      := any Unicode scalar value except '"' and '\' and U+000A and '{'
StringEscape    := '\' ( '{' | 'n' | 't' | 'r' | '\' | '"' | 'u{' HexDigits '}' )
Interpolation   := '{' Expression '}'
```

`Expression` inside an interpolation is the syntactic nonterminal of Annex A.3.
Braces inside the slot MUST balance. An empty slot is an error. A `}` that
does not close a slot is an ordinary character.

### A.2.8 Operators and Punctuation

The complete token set is the union of Annex A.2.2. No other operator or
punctuation token exists. In particular, `&`, `|` as an operator, `^`, `~`,
`<<`, `>>`, `**`, `?:`, and backtick are not tokens. `|` is a punctuation
mark used only in type declarations and patterns.
