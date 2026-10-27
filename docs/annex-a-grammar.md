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

## A.3 Syntactic Grammar

### A.3.1 Compilation Units

```ebnf
CompilationUnit := { ImportLine } { TopLevelItem }

ImportLine      := 'import' ModulePath [ '.{' Identifier { ',' Identifier } '}' ] NEWLINE
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
                 | NamespaceBinding

NamespaceBinding := QualifiedPascalName '.' snake_case Identifier
                    ':' Type '=' BindingRHS

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

A `ctype` right-hand side MUST be a record type whose field types are
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

`InlineSuite` is the inline suite of Clause 6.2.2: the `;`-separated
expressions of a single line (Clause 5.3.2).

A `case` block and a `case` arm block use `Arm` rather than `Statement`:

```ebnf
CaseBlock       := ':' NEWLINE INDENT Arm { Arm } DEDENT
Arm             := Pattern [ 'if' Expression ] '->' ArmBody
ArmBody         := InlineSuite
                 | NEWLINE INDENT { Member } DEDENT

InlineCase      := ':' InlineArm { ';' InlineArm }
InlineArm       := Pattern [ 'if' Expression ] '->' Expression
```

An arm body is an inline suite or an indented block (Clause 6.2.2); the
block's last member is the arm's value. An inline arm's body is one
expression: the `;` of an inline case separates arms (Clause 6.7.5).

The layout algorithm of Annex A.4 supplies `NEWLINE`, `INDENT`, and `DEDENT`.

### A.3.4 Lambdas and Parameter Groups

```ebnf
Lambda          := LambdaParameterGroup ':' LambdaBody
LambdaBody      := InlineSuite
                 | NEWLINE INDENT { Member } DEDENT

LambdaParameterGroup := '(' [ LambdaParameter { ',' LambdaParameter } ] ')'
LambdaParameter := Identifier
                 | '_'
                 | '(' LambdaParameter { ',' LambdaParameter } ')'

ParameterGroup  := '(' [ Parameter { ',' Parameter } ] ')'
Parameter       := [ Identifier ':' ] Type
```

A parameter group in a function type contains types; a lambda parameter group
contains binding names. The two are distinguished by position. An inline
lambda body is an inline suite (Clause 6.2.2): `(n): out.println(n); n + 1`
is one member of two expressions.

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
                 | QualifiedName [ '(' [ Argument { ',' Argument } ] ')' ]
                 | ConstructorExpression
                 | '(' Expression ')'
                 | Lambda
                 | CaseExpression
                 | IfExpression
                 | CompositeLiteral

ConstructorExpression := PascalName PostfixExpression

CaseExpression  := 'case' Expression { ',' Expression } CaseBlock
                 | 'case' Expression { ',' Expression } InlineCase

IfExpression    := 'if' Expression Suite [ 'else' Suite ]

CompositeLiteral := '{' '}'
                 | '{' CompositeItem { ',' CompositeItem } '}'
CompositeItem   := Expression [ ':' Expression ]

Argument        := Expression
```

A bare or dotted name of any case class is a `QualifiedName`; a `PascalName`
followed by a juxtaposed postfix expression is a constructor application
(Clause 6.5.2). An item of a composite literal with a `:` is a keyed entry;
without one it is an element (Clause 6.6.1).

A keyed composite in the argument of the derived `with` is a partial
record literal; the grammar does not distinguish it, the position does
(Clause 6.6.3).

A call in prefix form is a `QualifiedName` with arguments (Clause 6.5.2); a
call in receiver form is a `PostfixExpression` whose postfix ends in a call.
`CallExpression` covers both.

### A.3.6 Patterns

```ebnf
Pattern         := OrPattern
OrPattern       := PrimaryPattern { '|' PrimaryPattern }

PrimaryPattern  := '_'
                 | Literal
                 | '-' ( IntegerLiteral | FloatLiteral )
                 | QualifiedPascalName [ PrimaryPattern ]
                 | Identifier
                 | '(' Pattern { ',' Pattern } ')'
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

TypeAtom        := QualifiedPascalName
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
parenthesized; a bare composite atom in argument position is an error
(Clause 6.9.4).

Parentheses are context-sensitive. When a complete type is expected, a
parenthesized group is a parameter group: `(Int)` is the unit-returning
function type `(Int) -> _`. When a type atom is expected, as in the argument of
a generic application, the same form is a parenthesized type:
`Option (Value Int Float)`. The position decides, and no type is ambiguous
between the two readings.

### A.3.8 Imports and Qualified Names

```ebnf
QualifiedName       := Identifier { '.' Identifier }
QualifiedPascalName := { Identifier '.' } PascalName
```

A dotted name in expression position is resolved by Clause 7.4.4 and may
carry a call's arguments (Clause 6.5.2). A `QualifiedPascalName` is a
dotted name whose last segment is PascalCase and whose head segments form
a module path of any case class (Clause 5.5.2): in type position
`geolib.Shape` is a module-qualified type (Clause 7.4.5); in pattern
position `geolib.Circle` is a module-qualified constructor (Clause 7.4.6).
As the head of a `NamespaceBinding` it names the owner type. A
module-qualified constructor application in value position is spelled as a
call, `geolib.Circle(r)` (Clause 6.5.2).

## A.4 Layout Algorithm

The layout algorithm runs after lexical analysis and before syntactic
analysis. It inserts `NEWLINE`, `INDENT`, and `DEDENT` tokens. The licensed
continuation positions are exactly those of Clause 6.2.4; every other line
break ends a member.

1. **Physical lines.** Split the source at line terminators. Compute each
   line's indentation as its leading spaces. A tab in the indentation is an
   error (`E-tab`).
2. **Significant lines.** Discard blank lines and lines whose first non-space
   characters are `//` or `///`. They do not affect the indentation stack.
3. **Bracket suppression.** Bracket depth is the number of `(`, `[`, and `{`
   open at the end of a line. A line break at depth greater than zero emits
   no token, and the next line's indentation is ignored — except after a
   callback header (rule 4).
4. **Callback blocks.** A line whose final bracket depth is greater than
   zero and whose last tokens are a parameter group followed by `:` — a
   lambda or trailing-callback header (Clause 6.2.5) — opens a block. A
   line whose closing parameter group and colon return the depth to zero
   is not a callback header; it is a depth-zero line, and its colon opens
   a block under rule 6 (the case-arm colon of Clause 6.7.1). The block
   consists of the following lines whose indentation is greater than that
   of the header line. Before its first member the algorithm emits
   `NEWLINE INDENT`; the first member fixes the block's column exactly as
   rule 5 fixes a depth-zero block's (Clause 5.2.2), every later member of
   the block MUST equal it, and every member ends with a `NEWLINE` token.
   At a line indented at or left of the header's column the algorithm
   emits the pending `DEDENT`s. The closing bracket of the call closes any
   callback block still open and emits its `DEDENT`s.
5. **Indentation stack.** For a line at bracket depth zero with column `c`,
   against the stack of block columns, initially `[0]`:
   - `c` equals the top: emit `NEWLINE`.
   - `c` is greater: the step `c - top` MUST be a positive multiple of four;
     push `c` and emit `NEWLINE INDENT`. The first member of a block fixes
     its column, and every later line of that block MUST equal it.
   - `c` is smaller: pop until the top equals `c`, emitting one `DEDENT` per
     pop; if no stack entry equals `c`, the line is an error.

   `NEWLINE` is a terminator, not a separator: the token of this rule ends
   the previous significant line, so every significant depth-zero line ends
   with a `NEWLINE` marker, as the productions of Annex A.3 write it
   (`Arm`, `VariantLine`, `ImportLine`). An `INDENT` follows the header
   line's `NEWLINE`; the `DEDENT`s that close a block follow the last
   member's `NEWLINE`, precede the closing bracket of a callback block
   (rule 4), and precede `EOF` (rule 10).

   The first significant line opens no block, whatever its indentation: the
   stack remains `[0]`, the line's indentation is ignored, and a line the
   grammar rejects there is diagnosed at parse. An indented `else` as the
   first significant line is an error (rule 8).
6. **Headers and continuation.** A line at depth zero that ends in `:`, `=`,
   or `->` opens a block (Clause 6.2.3); a deeper following line is the
   block's first member and is processed by rule 5. The block-opening tokens
   are `:`, `=`, and `->`; a line that ends in `:=` opens no block, and a
   multi-line rebinding right-hand side is a continued expression under
   Clause 6.2.4. The `->` opener is the `case` arm header: the block it
   opens is the arm body, whose last member is the arm's value. A deeper line after a line that
   opens no block is a continuation: it emits no token, and its indentation
   is ignored. Continuation is licensed by Clause 6.2.4; a deeper line in
   any other position is diagnosed at parse, not by this algorithm.
7. **Chain continuation.** A line whose first token is `.` and whose column
   equals the column of the first `.` of the chain continues the expression.
   No `NEWLINE` is emitted at the end of the previous line.
8. **else.** An `else` line at the same column as the `if` it matches
   continues that `if`; it does not begin a new member. The algorithm emits
   the `DEDENT`s required by the stack, and the parser reattaches `else`.
   The rule holds whether the branches are inline or block form.
9. **Semicolons.** `;` terminates the current inline item (Clause 5.3.2). It
   emits no token, does not change the indentation stack, and does not open
   or close a block.
10. **End of file.** Emit one `DEDENT` for every open level, then `EOF`.

## A.5 Precedence and Associativity

The precedence table of Clause 6.5.5 is repeated here for reference.

| Level | Operators | Associativity |
|---|---|---|
| 1 | postfix `.f()`, constructor juxtaposition | left |
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
