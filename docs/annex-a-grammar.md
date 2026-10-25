# Annex A. Grammar

This annex is normative. It gives the complete grammar of Eko. Where this
annex and a prose clause disagree, the prose clause states the rule and this
annex is corrected.

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
`PascalName` and `snake_case Identifier` in Annex A.3 denote an identifier of that
case class; a leading privacy prefix does not change the class.

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

An underscore MUST be between two digits.

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

## A.3 Syntactic Grammar

### A.3.1 Compilation Units

```ebnf
CompilationUnit := { ImportLine } { TopLevelItem }

ImportLine      := 'import' ModulePath NEWLINE
ModulePath      := Identifier { '/' Identifier }

TopLevelItem    := ValueBinding
                 | TypeDecl
                 | CTypeDecl
                 | TestDecl
                 | ExternDecl
```

An import line MUST precede every top-level item.

### A.3.2 Bindings and Declarations

```ebnf
ValueBinding    := Pattern [ ':' Type ] '=' BindingRHS
                 | Pattern ':=' Expression
                 | Pattern '<-' CallExpression
                 | '<-' CallExpression

BindingRHS      := Expression
                 | NEWLINE INDENT { Member } DEDENT

TypeDecl        := PascalName ':' 'type' { TypeVar } '=' TypeRhs
TypeVar         := snake_case Identifier

TypeRhs         := VariantList
                 | NEWLINE INDENT VariantLine { VariantLine } DEDENT
                 | RecordType
                 | TupleType
                 | ε
VariantList     := [ '|' ] Variant { '|' Variant }
VariantLine     := '|' Variant NEWLINE
Variant         := PascalName [ TypeAtom ]

CTypeDecl       := PascalName ':' 'ctype' '=' RecordType

TestDecl        := snake_case Identifier ':' 'test' '='
                   ( Expression | NEWLINE INDENT { Member } DEDENT )

ExternDecl      := snake_case Identifier ':' FunctionType '='
                   'extern' StringLiteral [ 'Pure' ]
```

A `type` declaration with an empty right-hand side denotes a zero-variant
type. A `ctype` right-hand side MUST be a record type whose field types are
machine words or other `ctype` records (Clause 10.3.3).

### A.3.3 Blocks and Members

```ebnf
Member          := Statement

Statement       := ValueBinding
                 | Expression

Suite           := ':' InlineSuite
                 | ':' NEWLINE INDENT { Member } DEDENT

InlineSuite     := Expression { ';' Expression }
```

A `case` block and a `case` arm block use `Arm` rather than `Statement`:

```ebnf
CaseBlock       := ':' NEWLINE INDENT Arm { Arm } DEDENT
Arm             := Pattern [ 'if' Expression ] '->' Expression NEWLINE

InlineCase      := ':' InlineArm { ';' InlineArm }
InlineArm       := Pattern [ 'if' Expression ] '->' Expression
```

The layout algorithm of Annex A.4 supplies `NEWLINE`, `INDENT`, and `DEDENT`.

### A.3.4 Lambdas and Parameter Groups

```ebnf
Lambda          := LambdaParameterGroup ':' LambdaBody
LambdaBody      := Expression
                 | NEWLINE INDENT { Member } DEDENT

LambdaParameterGroup := '(' [ LambdaParameter { ',' LambdaParameter } ] ')'
LambdaParameter := Identifier
                 | '_'
                 | '(' LambdaParameter { ',' LambdaParameter } ')'

ParameterGroup  := '(' [ Parameter { ',' Parameter } ] ')'
Parameter       := [ Identifier ':' ] Type
```

A parameter group in a function type contains types; a lambda parameter group
contains binding names. The two are distinguished by position.

### A.3.5 Expressions

```ebnf
Expression      := OrExpression

OrExpression    := AndExpression { '||' AndExpression }
AndExpression   := ComparisonExpression { '&&' ComparisonExpression }
ComparisonExpression := RangeExpression
                 [ ( '==' | '!=' | '<' | '<=' | '>' | '>=' ) RangeExpression ]
RangeExpression := AddExpression [ ( '..' | '..=' ) AddExpression ]
AddExpression   := MulExpression { ( '+' | '-' | '++' ) MulExpression }
MulExpression   := UnaryExpression { ( '*' | '/' | '%' ) UnaryExpression }
UnaryExpression := [ '-' ] PostfixExpression

PostfixExpression := PrimaryExpression { Postfix }
Postfix         := '.' Identifier [ '(' [ Argument { ',' Argument } ] ')' ]

PrimaryExpression := Literal
                 | Identifier
                 | '(' Expression ')'
                 | Lambda
                 | CaseExpression
                 | IfExpression
                 | CompositeLiteral

CaseExpression  := 'case' Expression { ',' Expression } CaseBlock
                 | 'case' Expression { ',' Expression } InlineCase

IfExpression    := 'if' Expression Suite [ 'else' Suite ]

CompositeLiteral := '{' '}'
                 | '{' CompositeItem { ',' CompositeItem } '}'
CompositeItem   := Expression
                 | Identifier ':' Expression

Argument        := Expression
```

`CallExpression` is `PostfixExpression` where the postfix ends in a call.

### A.3.6 Patterns

```ebnf
Pattern         := OrPattern
OrPattern       := PrimaryPattern { '|' PrimaryPattern }

PrimaryPattern  := '_'
                 | Literal
                 | '-' ( IntegerLiteral | FloatLiteral )
                 | PascalName [ PrimaryPattern ]
                 | Identifier
                 | '{' '}'
                 | '{' Pattern { ',' Pattern } '}'
                 | '{' Pattern { ',' Pattern } ',' '..' [ Identifier ] '}'
                 | '{' '..' [ Identifier ] ',' Pattern '}'
                 | '{' FieldPattern { ',' FieldPattern } '}'

FieldPattern    := Identifier [ ':' Pattern ]
```

A `PascalName` pattern is a constructor; an `Identifier` pattern is a
variable. A variable pattern MUST be snake_case; a constructor pattern MUST
be PascalCase. A constructor pattern with a payload is check-only
(Clause 7.7.1).

### A.3.7 Type Expressions

```ebnf
Type            := FunctionType | AppliedType

FunctionType    := ParameterGroup ( '->' Type
                                  | '!->' Type
                                  | '?->' Type
                                  | '!'
                                  | '?'
                                  | ε )

AppliedType     := TypeAtom { TypeAtom }

TypeAtom        := PascalName
                 | TypeVar
                 | '_'
                 | '(' Type ')'
                 | ListType
                 | DictType
                 | TupleType
                 | RecordType

ListType        := '[]' Type
DictType        := '[' Type ']' Type
TupleType       := '[' Type ',' Type { ',' Type } ']'
RecordType      := '[' Field { ',' Field } ']'
Field           := Identifier ':' Type
```

`()` is the zero-parameter function type and is an abbreviation of `() -> _`.
`_` is the unit type. A composite argument of a generic application MUST be
parenthesized.

Parentheses are context-sensitive. When a complete type is expected, a
parenthesized group is a parameter group: `(Int)` is the unit-returning
function type `(Int) -> _`. When a type atom is expected, as in the argument of
a generic application, the same form is a parenthesized type:
`Option (Value Int Float)`. The position decides, and no type is ambiguous
between the two readings.

### A.3.8 Imports and Qualified Names

```ebnf
QualifiedName   := Identifier { '.' Identifier }
```

A dotted name in type position is a module path. A dotted name in expression
position is resolved by Clause 7.4.4.

## A.4 Layout Algorithm

The layout algorithm runs after lexical analysis and before syntactic
analysis. It inserts `NEWLINE`, `INDENT`, and `DEDENT` tokens.

1. **Physical lines.** Split the source at line terminators. Compute each
   line's indentation as its leading spaces. A tab in the indentation is an
   error (`E-tab`).
2. **Significant lines.** Discard blank lines and lines whose first non-space
   characters are `//` or `///`. They do not affect the indentation stack.
3. **Indentation stack.** Maintain a stack of indentation columns, initially
   `[0]`. For each significant line with column `c`:
   - if `c` equals the top, emit `NEWLINE`;
   - if `c` is greater, `c - top` MUST be a multiple of four; push `c`
     and emit `NEWLINE INDENT`. The first member establishes the block
     column. A column that is not a multiple of four relative to the
     enclosing block is an error;
   - if `c` is smaller, pop until the top equals `c`, emitting one `DEDENT`
     per pop; if no stack entry equals `c`, the line is an error.
4. **Headers.** A line that opens a block MUST end in `:` or `=` (or be an
   `extern` declaration, whose right-hand side is complete). A greater
   indentation after a line that does not open a block is an error.
5. **Brackets.** While the bracket depth is greater than zero, line breaks
   emit no `NEWLINE` or `INDENT`. Bracket depth is increased by `(`, `[`, and
   `{` and decreased by the matching closing bracket.
6. **Trailing callbacks.** A callback header of the form `( ... ) :` inside a
   bracket group opens a block whose indentation is relative to the callback
   header's line. The block ends at the closing bracket of the call or at a
   line whose indentation is at or left of the callback header's column.
7. **Chain continuation.** A line whose first token is `.` and whose column
   equals the column of the first `.` of the chain continues the expression.
   No `NEWLINE` is emitted at the end of the previous line.
8. **else.** An `else` line at the same column as the `if` it matches
   continues that `if`. It does not begin a new member. The layout algorithm
   emits the `DEDENT`s required by the stack; the parser reattaches `else` to
   the `if`.
9. **Semicolons.** `;` is a member terminator equivalent to `NEWLINE`. It
   does not change the indentation stack and does not open or close a block.
10. **End of file.** Emit one `DEDENT` for every open level, then `EOF`.

## A.5 Precedence and Associativity

The precedence table of Clause 6.5.5 is repeated here for reference.

| Level | Operators | Associativity |
|---|---|---|
| 1 | postfix `.f()` | left |
| 2 | unary `-` | none |
| 3 | `* / %` | left |
| 4 | `+ - ++` | left |
| 5 | range `.. ..=` | none |
| 6 | `== != < <= > >=` | none |
| 7 | `&&` | left |
| 8 | `\|\|` | left |

A range expression at level 5 is not a value constructor in the same sense as
the other levels; it produces a `Range` value. A chain over a range requires
parentheses (Clause 6.5.6).

## A.6 Excluded Productions

The grammar contains no production for:

- a loop (`while`, `for`, `do`, `loop`);
- `break` or `continue`;
- a user-defined operator declaration;
- a type alias;
- a bitwise or exponentiation operator;
- an index expression `x[i]`;
- a dictionary pattern or an as-pattern;
- a multiline string literal;
- a block comment;
- a ternary expression.

Their absence from the grammar is their specification. Clause 6.10 gives the
corresponding normative prohibition.
