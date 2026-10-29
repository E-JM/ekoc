# Annex A. Grammar

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
                 | NEWLINE INDENT Member { Member } DEDENT

TypeDecl        := PascalName ':' 'type' { TypeVar } '=' TypeRhs
TypeVar         := snake_case Identifier

TypeRhs         := VariantList
                 | NEWLINE INDENT VariantLine { VariantLine } DEDENT
                 | RecordType
                 | TupleType
VariantList     := [ '|' ] Variant { '|' Variant }
VariantLine     := '|' Variant NEWLINE
Variant         := PascalName [ TypeAtom ]

CTypeDecl       := PascalName ':' 'ctype' '=' RecordType

TestDecl        := snake_case Identifier ':' 'test' '='
                   ( Expression | NEWLINE INDENT Member { Member } DEDENT )

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
                 | ':' NEWLINE INDENT Member { Member } DEDENT

InlineSuite     := Expression { ';' Expression }
```

`InlineSuite` is the inline suite of Clause 6.2.2: the `;`-separated
expressions of a single line (Clause 5.3.2).

A `case` block and a `case` arm block use `Arm` rather than `Statement`:

```ebnf
CaseBlock       := ':' NEWLINE INDENT Arm { Arm } DEDENT
Arm             := Pattern [ 'if' Expression ] '->' ArmBody
ArmBody         := InlineSuite
                 | NEWLINE INDENT Member { Member } DEDENT

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
                 | NEWLINE INDENT Member { Member } DEDENT

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
is a single body of two expressions.

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

A dotted name in expression position is resolved by Clause 7.4.4 and may carry
a call's arguments (Clause 6.5.2). A `QualifiedPascalName` is a dotted name
whose last segment is PascalCase and whose head is a module name of any case
class (Clause 5.5.2): in type position `geolib.Shape` is a module-qualified
type (Clause 7.4.5); in pattern position `geolib.Circle` is a module-qualified
constructor (Clause 7.4.6). As the head of a `NamespaceBinding` it names the
owner type. The production admits any number of head segments; a
module-qualified `QualifiedPascalName` carries exactly one module segment
(Clause 7.4.4), and a reference with a longer prefix is `E-unbound-name`. A
module-qualified constructor application in value position is spelled as a
call, `geolib.Circle(r)` (Clause 6.5.2).
