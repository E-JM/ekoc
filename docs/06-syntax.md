# 6. Syntax

This clause defines the syntactic forms of Eko. The complete grammar is in
Annex A. This clause states the surface rules that the grammar does not
capture: layout, continuation, and the composition of forms.

## 6.1 Grammar Conventions

A **production** defines the shape of a construct. It does not define
indentation, line breaks, or the placement of a construct within a block;
those are defined by the layout rules of Clause 6.2 and apply uniformly.

Two descriptions are used:

- the **concrete syntax** is the surface written in source: tokens, layout,
  and continuation;
- the **abstract form** is the structure a production denotes, independent of
  layout.

A construct that satisfies a production but violates a layout rule is an
error. A construct that satisfies both but violates a static rule is rejected
under Clause 7.

The examples in this clause use the following context. It is illustrative and
is not part of the specification; a block may declare additional local names,
or restate a context declaration when that declaration is the point of the
example.

```eko
Sex:    type = Man | Woman
Person: type = [name: String, age: UInt, sex: Sex, address: String]
Key:    type = Up | Down | Left | Right
Value:  type t u = First [u, t] | Second t

p:        Person        = {name: "ada", age: 36, sex: Woman, address: "x"}
data:     []String      = {"ada", "36", "woman", "x"}
persons:  []Person      = {p}
content:  String        = "ada,36,woman,x"
row:      [String, Int] = {"ada", 36}
xs:       []Int         = {1, 2, 3}
n:        Int           = 3
x:        Int           = -1
count:    Int           = 0
err_count: Int          = 0
want:     Int           = 3
r:        Result Int String = Ok 3
ev:       Key           = Up
f:        (Int) -> Int  = (i): i + 1
```

Examples that use `out`, `fs`, `file`, or `scope` are read inside a function
whose parameters include `out: Out` and `fs: Fs`; `file` is the callback
parameter of `Fs.with_file`, and `scope` is the callback parameter of
`Conc.scope`.

## 6.2 Compilation Units and Layout

### 6.2.1 Compilation Unit

A compilation unit is a sequence of top-level items:

```ebnf
CompilationUnit := ImportLine* TopLevelItem*
```

An import line MUST precede every top-level binding. An import after a
binding is an error. A file with no bindings is legal; it contributes only
its imports. A trailing `.{f, g}` on an import opens the listed free names
of the module into the file's top layer (Clause 7.2.3); the braces do not
participate in layout — an import line is one depth-zero line.

```eko
// context: modules `person`, `stats`, and `csv/lex` exist.
import person
import stats
import csv/lex

main: (out: Out) ! =
    out.println("hi")
```

### 6.2.2 Blocks and Members

A **block** is a header line followed by one or more members at one
indentation level deeper than the header. A **member** is one line or one
semicolon-terminated unit.

The semicolon-terminated unit is an **inline suite**: one or more expressions
separated by `;` (Clause 5.3.2).

```ebnf
InlineSuite := Expression (';' Expression)*
```

The first member line establishes the block's column. Every subsequent member
MUST begin in that column. A line that begins at a different column either
closes the block or is an error.

```eko
classify: (n: Int) -> String =
    if n < 0:
        "negative"
    else:
        "non-negative"
```

The body of `classify` is a block. The two branches of the `if` are nested
blocks one level deeper.

### 6.2.3 Headers

A header introduces a block and ends in one of two ways:

| Header | Terminator | Introduces |
|---|---|---|
| function, value, `test`, or `extern` definition | `=` | the body block |
| lambda, `if`, `else`, callback | `:` | the body block |
| `type` declaration with vertical variants | `=` | the variant block |
| `case` expression | `:` | the arm block |

A header with `=` MAY be followed by its body on the same line when the body
is a single expression. A header with `:` MAY be followed by its body on the
same line when the body is a single expression.

```eko
double: (n: Int) -> Int = n * 2

classify: (n: Int) -> String =
    if n < 0: "negative" else: "non-negative"
```

### 6.2.4 Line Continuation

A line terminator ends a member except in the following positions, where
continuation is licensed.

1. **Inside brackets.** Inside `(`, `[`, or `{`, line terminators are free
   formatting. The matching closing bracket ends the group.
2. **Before a chain dot.** A receiver chain MAY continue on the next line when
   the line begins with `.`.
3. **A trailing callback.** The last argument of a call MAY occupy its own
   line (Clause 6.2.5).
4. **A continued scrutinee.** The scrutinee of a `case` MAY continue on the
   following lines when it is indented deeper than `case` (Clause 6.7.1).

No other position licenses continuation. In particular, the markers `!` and
`?` never license continuation. A line ending in a lone marker is a complete
member; the joined tokens `!->` and `?->` never wrap.

### 6.2.5 Trailing Callbacks

A **trailing callback** is the last argument of a call, written on a line of
its own. Its header is a parameter group followed by `:`. Its body is a block
one level deeper.

```eko
persons.each_with_index(
    (p, i):
        out.println("{i}: {p}"))
```

A body consisting of one expression MAY follow the colon on the same line:

```eko
persons.each((p): out.println(p))
```

The closing `)` of the call ends both the callback block and the argument
list. A trailing callback is the surface form of a lambda in the last argument
position (Clause 6.4.4).

### 6.2.6 Chained Calls

A receiver chain MAY continue on the next line. Each continuation line begins
with `.` and is aligned with the first `.` of the chain.

```eko
content.split("\n")
       .map(String.trim)
       .filter((l): l != "")
       .length()
```

Each link receives the value of the previous link as its receiver
(Clause 6.5.3). The alignment is a layout requirement; a continuation line
that begins with `.` in a different column is an error.

## 6.3 Bindings and Declarations

### 6.3.1 Binding Forms

A binding has one of the following forms:

```ebnf
Binding      := ValueBinding | TypeDecl | CTypeDecl | TestDecl | ExternDecl

ValueBinding := Pattern [':' TypeExpression] '=' RHS
              | Pattern ':=' RHS
              | Pattern '<-' Call
              | '<-' Call
              | NamespaceBinding

NamespaceBinding := PascalName '.' snake_case Identifier ':' TypeExpression '=' RHS

RHS          := Expression | NEWLINE INDENT {Member} DEDENT
```

The declaration forms are given in Clause 6.3.5 through Clause 6.3.8. The annotation MAY
appear only with `=`. `:=` carries no annotation. In `<-`, the type of the
bound pattern is supplied by the callback parameter.

A binding line begins with its pattern. An expression line begins with its
expression. The binding operators are infix; they are never written at the
start of a line, except for the pattern-less `<-` form.

A namespace binding declares a member of a type namespace: its head is
`TypeName.name`, the annotation is mandatory, and the form is legal only in
the module that declares `TypeName` (Clause 7.4.5). The tail carries the
privacy prefix: a member named `_name` is private to its module. `:=` and
`<-` are not admitted for a namespace binding; the annotation is the
member's public signature.

```eko
Person: type = [name: String, age: UInt]
age: UInt = 36
count := count + 1
(p, i) <- persons.each_with_index()
```

The annotation slot holds a type expression for a value binding, or one of
the contextual words `type`, `ctype`, and `test`. The word `extern` appears on
the right-hand side of a declaration, not in the annotation slot.

### 6.3.2 Value Bindings

`=` introduces a binding. The right-hand side is evaluated and bound to the
pattern. A value binding MAY carry a type annotation between the pattern and
`=`.

```eko
age: UInt = 36
person: Person = {name: "ada", age: 36, sex: Woman, address: "x"}
```

Without an annotation, the type is determined by the right-hand side. A
composite literal with no expected type requires an annotation
(Clause 6.6.2).

A parenthesized tuple on the left of `=` destructures a tuple value:

```eko
(s, want) = row
```

A binding whose pattern is `_` evaluates the right-hand side and discards the
value: `_ := e`. In value position `_` is the unit value; in pattern position
it is the wildcard (Clause 7.5.4).

### 6.3.3 Rebinding

`:=` introduces a binding for a name without an annotation. If the name is
already bound, the new binding shadows the old one in the remainder of the
block.

```eko
count := count + 1
p2 := p
```

`:=` does not generalize; a polymorphic binding requires an annotated `=`
(Clause 7.6.3).

### 6.3.4 Inverted Binding

An inverted binding places the remaining block in the last parameter position
of a call:

```eko
p <- f(a)
rest
```

is defined as:

```eko
f(a, (p): rest)
```

`rest` is the entire remaining block after the binding line. The callback
occupies the last parameter position.

```eko
age <- data.get(1).to_result("short row").try()
(p, i) <- persons.each_with_index()
_ <- save(entry).try()
```

A zero-argument callback omits the pattern:

```eko
<- scope.spawn()
```

`<- f()` and `_ <- f()` are distinct: the first passes a zero-parameter
callback, the second a one-parameter callback that binds nothing. The
callback's shape is fixed by the expected function type (Clause 7.8.1).

Multiple inverted bindings nest in sequence; each later line lies in the
callback body of the earlier one:

```eko
a <- f()
b <- g(a)
h(a, b)
```

is defined as:

```eko
f((a):
    g(a, (b):
        h(a, b)))
```

The admitted pattern forms are exactly those of a lambda parameter position:
an identifier, `_`, or a tuple parameter group. Record, constructor, and list
patterns do not appear after `<-`.

Because the desugaring appends a callback in the last position, `<-` requires
a call with a callback parameter. It is not applicable to a call that returns
`Never` and takes no callback.

### 6.3.5 Type Declarations

A type declaration has the form:

```ebnf
TypeDecl := Name ':' 'type' TypeVar* '=' TypeRhs
TypeVar  := snake_case identifier
```

The right-hand side is a declaration form:

```ebnf
TypeRhs  := VariantList
          | NEWLINE INDENT VariantLine { VariantLine } DEDENT
          | RecordType
          | TupleType
          | ε

VariantList  := Variant ('|' Variant)*
              | '|' Variant ('|' Variant)*
VariantLine  := '|' Variant NEWLINE
```

An empty right-hand side denotes a zero-variant type.

A variant is a PascalCase constructor, optionally followed by one payload
type:

```ebnf
Variant  := Constructor [TypeAtom]
```

If the payload has several components, it is written as a tuple type.

```eko
Sex:    type = Man | Woman
Person: type = [name: String, age: UInt, sex: Sex, address: String]
Result: type t e = Ok t | Err e
Tree:   type t = Leaf t | Node [Tree t, Tree t, t]
Never:  type =
```

A type declaration is top-level. A declaration introduces a new nominal type;
it does not alias an existing type (Clause 7.5.2).

### 6.3.6 ctype Declarations

A `ctype` declaration has the form:

```ebnf
CTypeDecl := Name ':' 'ctype' '=' '[' Field (',' Field)* ']'
Field     := snake_case Name ':' CTypeFieldType
```

Every field type MUST be a machine word or another `ctype` record
(Clause 10.3.3). A `ctype` declaration is top-level.

```eko
DivC: ctype = [quot: I32, rem: I32]
```

### 6.3.7 Test Declarations

A test declaration has the form:

```ebnf
TestDecl := Name ':' 'test' '=' (Expression | NEWLINE INDENT {Member} DEDENT)
```

A test declaration is top-level. Its body is an ordinary block or a single
expression on the same line; its value is discarded. The name is snake_case.
Test declarations are specified in Clause 9.14.

### 6.3.8 extern Declarations

An extern declaration has the form:

```ebnf
ExternDecl := Name ':' FunctionType '=' 'extern' StringLiteral ['Pure']
```

The signature is a function type. The string names a native symbol. The
optional word `Pure` asserts the audited guarantee of Clause 10.2.2.

```eko
_sqrt: (F64) -> F64 = extern "sqrt" Pure
_qsort: (base: C.Ptr.Void, n: C.Size, size: C.Size,
         compar: (C.Ptr.Void, C.Ptr.Void) -> Int) ! = extern "qsort"
```

An extern declaration is top-level. Its signature MUST satisfy the
marshalling whitelist (Clause 10.3.5).

## 6.4 Functions and Lambdas

### 6.4.1 Lambda Form

A lambda is an anonymous function written as a parameter group, `:`, and a
body:

```ebnf
Lambda := ParameterGroup ':' LambdaBody
LambdaBody := InlineSuite | IndentedBlock
```

```eko
(n): n + 1
(p, i): out.println("{i}: {p}")
(xs): xs.map((n): n * 2).length()
```

A multi-line body is a block one level deeper:

```eko
persons.each_with_index((p, i):
    out.println("{i}: {p}"))
```

A lambda carries no annotations: no parameter types, no result type, and no
bit annotation. Its type and bit are supplied by the expected type and by its
body (Clause 7.9.4).

An inline lambda body is a single-line suite: `;` separates its expressions
exactly as it separates the branches of an inline `if` or the arms of an
inline `case` (Clauses 5.3.2, 6.7.5), so `(n): out.println(n); n + 1` is
one body of two expressions.

### 6.4.2 Named Functions

A named function is a binding whose annotation is a function type:

```eko
inc: (Int) -> Int = (n): n + 1

add: (a: Int, b: Int) -> Int =
    a + b
```

The body MAY be a lambda, in which case the lambda is checked against the
annotation. The body MAY instead be a block that uses the parameter names from
the annotation directly.

### 6.4.3 Parameter Groups

A parameter group is a parenthesized, comma-separated sequence of parameters:

```ebnf
ParameterGroup := '(' [Parameter (',' Parameter)*] ')'
Parameter      := Identifier | '_' | NestedParameterGroup
NestedParameterGroup := '(' Parameter (',' Parameter)* ')'
```

A parameter is an identifier, `_`, or a nested parameter group. A nested
parameter group is a group of parameters, not a destructuring pattern
(Clause 7.7.1). The zero-parameter group `()` is legal.

A parameter group in a function type contains types, optionally preceded by a
parameter label: `(a: Int, b: Int)`. A parameter group in a lambda contains
binding names: `(a, b):`. The two are distinguished by position: a function
type occurs in an annotation, a lambda in an expression.

```eko
(): _
(a): a
(a, b): a + b
(_, i): i
(_): _
```

A lambda parameter group is also the shape of a callback. A callback that
receives a tuple value takes it apart on a binding line or in a `case`
pattern, not by writing a tuple pattern in the parameter group.

### 6.4.4 Trailing Callback Position

The last parameter of a function type MAY be a function type. A call to such
a function MAY place the corresponding lambda on a line of its own after the
other arguments, using the trailing-callback form of Clause 6.2.5.

```eko
fs.with_file("data.csv", Read,
    (file):
        file.read(4096))
```

The trailing callback is the last argument. A function type with a callback in
a non-final parameter position has no trailing form; the callback is written
as an ordinary argument.

## 6.5 Expressions

### 6.5.1 Expression Forms

An expression is one of the following forms:

| Form | Example |
|---|---|
| literal | `42`, `'a'`, `"text"` |
| name | `xs`, `Person`, `Math.pi` |
| parenthesized expression | `(a + b)` |
| lambda | `(n): n + 1` |
| call | `f(a, b)`, `a.f(b)`, `Mod.f(a)` |
| operator expression | `a + b`, `-a`, `a && b` |
| composite literal | `{1, 2}`, `{name: "ada"}` |
| `case` expression | `case x: ...` |
| `if` expression | `if c: a else: b` |
| range | `1..=100`, `'a'..'z'` |

A `case` or `if` used as an operand MUST be parenthesized unless it is the
whole right-hand side or the whole argument in a position where a block is
expected.

### 6.5.2 Calls and UFCS

A call is written in prefix form or in receiver form:

```ebnf
Call        := Callee '(' [Argument (',' Argument)*] ')'
Callee      := Name | QualifiedName
ReceiverCall := Expression '.' Name '(' [Argument (',' Argument)*] ')'
QualifiedReceiverCall := Expression '.' ModuleName '.' Name '(' [Argument {',' Argument}] ')'
```

The two forms are identical when `f` resolves to a function — a namespace
member, a module function, or a free function (stations 2 through 4,
Clause 7.4.3):

```text
a.f(b, c)  ≡  f(a, b, c)
a.Mod.f(b, c)  ≡  Mod.f(a, b, c)
```

When `f` is a function-typed field of `a`'s record type, `a.f(b, c)` is the
call of that field value, `(a.f)(b, c)`; station 1 precedes the others, and
the identity does not apply (Clause 7.4.3).

In every resolution, the first parameter is the receiver. There is no other
method-call form.

A constructor is applied by juxtaposition. The argument, when present, is a
postfix expression, and juxtaposition binds tighter than every operator:
`Ok inner`, `Duration 500`, `Succ (Succ Zero)`, and `Ok x || y` reads
`(Ok x) || y`. A nullary constructor is the bare `PascalName`. A qualified
constructor `Mod.Name` selects among same-named constructors (Clause 7.4.6).

Operator notation is sugar for the same calls: `a + b` is `Int.add(a, b)`,
and therefore `a.add(b)` (Clause 7.8.3).

### 6.5.3 Member Chains

A member chain is a sequence of receiver calls:

```eko
content.split("\n")
       .map(String.trim)
       .filter((l): l != "")
       .length()
```

Each link's receiver is the value of the previous link. A chain MAY attach a
module at a link with `.Mod.f()` (Clause 7.4.4). A fully qualified name MAY be
passed as an argument, as in `.map(String.trim)`; that is a name reference,
not a call.

### 6.5.4 Operator Expressions

The operator forms and their desugarings are:

```text
a + b     ≡  Int.add(a, b)
a - b     ≡  Int.sub(a, b)
a * b     ≡  Int.mul(a, b)
a / b     ≡  Int.quot(a, b)
a % b     ≡  Int.rem(a, b)
-a        ≡  Int.neg(a)
a == b    ≡  Int.eq(a, b)
a != b    ≡  Int.ne(a, b)
a < b     ≡  Int.lt(a, b)
a <= b    ≡  Int.le(a, b)
a > b     ≡  Int.gt(a, b)
a >= b    ≡  Int.ge(a, b)
a && b    ≡  if a: b else: False
a || b    ≡  if a: True else: b
xs ++ ys  ≡  List.append(xs, ys)
```

The numeric equations are shown for `Int`; `UInt` and `Float` provide the
corresponding namespace members. `/` is `Int.quot` and `%` is `Int.rem`.
`&&` and `||` are sugar for `if`, not functions. `++` concatenates lists,
strings, and `Bytes`, and merges dictionaries; on a duplicate key the right
operand wins. Tuples and records do not support `++`.

`-` is both the binary subtraction operator and the unary negation operator.
There is no unary `+`.

`not` is a function, written `b.not()` or `Bool.not(b)`. Prefix `!` is the
world-bit marker and is not a logical operator (Clause 7.9.2).

### 6.5.5 Precedence and Associativity

The precedence ladder, from tightest to loosest, is:

| Level | Operators | Associativity |
|---|---|---|
| 1 | postfix `.f()` | left |
| 2 | unary `-` | none |
| 3 | `* / %` | left |
| 4 | `+ - ++` | left |
| 5 | `.. ..=` | none |
| 6 | `== != < <= > >=` | none |
| 7 | `&&` | left |
| 8 | `\|\|` | left |

Comparisons do not associate. A chain such as `a < b < c` is an error; write
`a < b && b < c`. The same rule applies to `==` and the other comparison
operators.

`-x.f()` parses as `-(x.f())`: postfix binds before unary minus. A range
operator binds less tightly than postfix; a chain over a range requires
parentheses (Clause 6.5.6).

### 6.5.6 Ranges

Two range forms exist:

| Form | Meaning | Type |
|---|---|---|
| `a..b` | end-exclusive: `a` through `b - 1` | `Range t` |
| `a..=b` | end-inclusive: `a` through `b` | `Range t` |

Both endpoints MUST have the same type, which MUST be `Int`, `UInt`, or
`Char`. `Range` is a standard-library type. A `Float` endpoint and an
unbounded range do not exist.

Range operators are infix and bind less tightly than postfix. A chain over a
range requires parentheses:

```eko
(1..=100).map(f)
(0..n).filter((i): i % 2 == 0)
```

An integer literal immediately followed by `..` does not begin a floating
literal. The expression `(1..=100)` is a range from 1 through 100.

## 6.6 Composite Literals

### 6.6.1 Brace Forms

Composite literals are written with braces. The form inside the braces
determines the family:

| Inside | Form | Example |
|---|---|---|
| comma-separated items, no colon | list or tuple | `{1, 2, 3}`, `{"first", 2}` |
| colon-separated pairs | dictionary or record | `{"1": 1}`, `{name: "ada", age: 36}` |
| empty | list, dictionary, record, or tuple with an expected type | `{}` |

A tuple has at least two components. A one-component `{x}` is a one-element
list, not a tuple, when the expected type is a list; with a tuple expected
type it is an error.

### 6.6.2 Expected Type

A composite literal never stands alone. The expected type at the position
decides whether `{1, 2}` is a list or a tuple and whether `{k: v}` is a
dictionary or a record.

Where no expected type exists, the literal requires an annotation:

```eko
xs:     []Int       = {1, 2, 3}
counts: [String]Int = {"1": 1, "2": 2}
pair:   [String, Int] = {"first", 2}
p:      Person      = {name: "ada", age: 36, sex: Woman, address: "x"}
```

An empty literal follows the same rule: `xs: []Int = {}` is an empty list;
`d: [String]Int = {}` is an empty dictionary.

### 6.6.3 Partial Record Literals

A **partial record literal** names a subset of a record's fields. It is legal
only in the argument position of `with` (Clause 9.8.6):

```eko
p2 := p.with({age: 37})
p3 := p.with({address: {city: "cd"}})
```

A partial record literal anywhere else is an error. A field name that is not
a field of the record is an error.

Construction descends through record field values: a field whose type is
a record admits a partial literal for that record, completed from the
receiver's field, as `address` above. It does not descend into a list,
dictionary, or tuple element — an element literal there is a complete
value, and a partial literal in that position is an error.

## 6.7 case and if

### 6.7.1 case

A `case` expression has a scrutinee and one or more arms:

```ebnf
CaseExpr := 'case' Expression ':' CaseBlock
CaseBlock := NEWLINE INDENT Arm {Arm} DEDENT
Arm      := Pattern ['if' Expression] '->' Expression
```

```eko
parse_sex: (data: String) -> Result Sex String =
    case data:
        "man"   -> Ok Man
        "woman" -> Ok Woman
        _       -> Err "unknown sex: {data}"
```

The scrutinee is evaluated once. Arms are tried from top to bottom; the first
matching arm produces the value of the whole `case`. The `case` is an
expression.

The arms form a block one level deeper than `case`, and every arm begins in
the same column. The scrutinee MAY continue on following lines when it is
indented deeper than `case`. When the scrutinee contains a multi-line
callback, the line that closes the call ends with `):`, and the arms begin on
the next line:

```eko
main: (out: Out, fs: Fs) ! =
    case fs.with_file("data.csv", Read,
            (file):
                content <- file.read(4096).map_err((e): "io: {e}").try()
                Ok content.split("\n").each((l): out.println(l))):
        Ok inner -> case inner:
            Ok _  -> _
            Err e -> out.println(e)
        Err e -> out.println(e)
```

An arm is always `PATTERN -> EXPRESSION`. A bare expression is not an arm. A
guard is written `PATTERN if CONDITION -> EXPRESSION`; the guard is a `Bool`
expression. If the pattern matches but the guard is false, matching continues
with the next arm.

A `case` that contains a guarded arm MUST end with an unguarded arm. The
unguarded arm closes the match. Exhaustiveness is specified in Clause 7.7.2.

### 6.7.2 Multiple Scrutinees

`case` accepts multiple scrutinees:

```ebnf
CaseExpr := 'case' Expression (',' Expression)+ ':' CaseBlock
Arm      := Pattern (',' Pattern)+ ['if' Expression] '->' Expression
```

Each scrutinee is evaluated once, left to right. An arm supplies one pattern
per scrutinee; the patterns are matched independently against the
corresponding values and bind names together. No tuple value is constructed,
and no tuple type is required.

```eko
case n % 3 == 0, n % 5 == 0:
    True,  True  -> "FizzBuzz"
    True,  False -> "Fizz"
    False, True  -> "Buzz"
    False, False -> show(n)
```

### 6.7.3 Guards

A guard follows the pattern and begins with `if`:

```eko
case n:
    n if n < 0  -> "negative"
    n if n == 0 -> "zero"
    _           -> "positive"
```

The pattern binds before the guard is evaluated. The guard MAY use the names
bound by its pattern. A guard occurs only at arm level; no other pattern
position admits one.

### 6.7.4 if

`if` is sugar for a `case` over a `Bool`:

```text
if c: x else: y   ≡   case c:
                          True  -> x
                          False -> y
```

```eko
n = if x < 0: 0 else: x
```

The condition MUST be a `Bool`. The condition is evaluated once, and only the
selected branch is evaluated.

An omitted `else` is `else: _`. A then-branch without an `else` MUST have type
`_` (or `Never`, which adapts):

```eko
if err_count > 0:
    out.println("errors!")
```

The `else` block starts in the same column as `if`:

```eko
if n < 0:
    "negative"
else:
    "non-negative"
```

`if` is an expression and produces the value of the selected branch. Both
branches MUST have the same type; a `Never` branch adapts
(Clause 7.7.4).

### 6.7.5 Single-Line Form

Within a single line, `;` separates multiple expressions and multiple `case`
arms:

```eko
str = if n < 0: "negative" else: "other"
case r: Ok v -> v; Err e -> 0
if c: x(); y() else: z()
```

The single-line form is a layout variant. It does not change matching or
evaluation. The rule is that `;` is a line-terminator equivalent
(Clause 5.3.2).

## 6.8 Patterns

### 6.8.1 Pattern Forms

A pattern is one of:

| Form | Shape | Examples |
|---|---|---|
| variable | a binding name | `x`, `msg` |
| wildcard | `_` | `_` |
| literal | a literal | `"man"`, `0`, `1.5`, `-1` |
| constructor | a constructor and an optional payload pattern | `Man`, `Ok v`, `Count n`, `Key Up` |
| tuple | `{P, Q, ...}` | `{x, y}` |
| record | `{field}` or `{field: P}` | `{name, age}`, `{name: n}` |
| list | `{}`, `{h, ..t}`, `{.., 42}`, `{_, ..}` | see Clause 6.8.4 |
| or | `P \| Q` | `Key Up \| Key Down` |

A constructor pattern is a constructor name, optionally followed by a payload
pattern. Constructors are written without parentheses. A variable pattern
binds the matched value to the name. `_` matches anything and binds nothing.

Tuple, record, and list patterns share braces; the expected type decides
between them, as it does for composite literals. The same pattern grammar is
used in `case` arms and on the left of binding operators, subject to the
parameter-position restriction of Clause 7.7.1.

### 6.8.2 Record Patterns

A record pattern matches a subset of fields. Only the fields written in the
pattern are bound; all other fields are ignored.

```eko
case r:
    Ok {name, age}        -> greet(name, age)
    Ok {name: n, age: a}  -> "{n}:{a}"
    Err msg               -> log(msg)
```

The shorthand `{name}` binds the field `name` to a binding of the same name.
The renaming form `{name: n}` binds the field `name` to `n`. The two forms MAY
be mixed. Record patterns nest: `Ok {addr: {city}}`.

A field name that is not a field of the record type is an error.

### 6.8.3 Or-Patterns

`P | Q` matches when either alternative matches. Every alternative MUST bind
the same set of names at the same types. Those bindings are available in the
arm body and in the guard.

```eko
case ev:
    Key Up | Key Down  -> nav(ev)
    Key Enter          -> submit()
```

`|` in pattern position is the or-pattern separator. It does not collide with
the `|` that separates variants in a type declaration (Clause 6.3.5); value
position has no `|`.

### 6.8.4 List Patterns

List patterns have five forms:

| Form | Meaning |
|---|---|
| `{}` | the empty list |
| `{x, y, ...}` | fixed length; a list or a tuple, decided by the expected type |
| `{h, ..t}` | suffix rest: binds the first element to `h` and the tail list to `t` |
| `{.., 42}` | prefix rest: matches a list whose last element matches `42` |
| `{_, ..}` | anonymous rest: matches a non-empty list |

```eko
case xs:
    {}                 -> empty()
    {x, y}             -> pair(x, y)
    {h, ..t}           -> walk(h, t)
    {.., 42}           -> ends_42()
    {_, ..}            -> non_empty()
    {Foo, Count n, ..} if n > 0 -> positive()
```

A rest binds the remaining list: the tail list for `..t`, or the head list for
a prefix rest. Prefix and suffix rests have equal status. `{}` reads only as
the empty list. Tuples have at least two elements.

### 6.8.5 Literal Patterns

A literal pattern is one of:

| Literal pattern | Example |
|---|---|
| `String` | `"man"` |
| integer | `0` |
| float | `1.5` |
| negative numeric | `-1` |
| zero-payload constructor | `None`, `True`, `Man` |

A zero-payload constructor is written as a bare name and behaves like a
literal pattern. Literal patterns MAY nest inside constructor patterns:

```eko
case r:
    Err "timeout" -> retry()
    Ok v          -> v
```

An interpolated string is an expression, not a literal pattern.

### 6.8.6 Duplicate Bindings

Within one pattern, a binding name MUST occur at most once. `(x, x)` is an
error. `_` is exempt: it binds nothing, so it MAY occur any number of times.

The rule applies separately to each alternative of an or-pattern.
Clause 6.8.3 requires all alternatives to bind the same names at the same
types. Different arms MAY reuse the same names; each arm binds
independently. The patterns of one arm of a multi-scrutinee `case`
(Clause 6.7.2) bind together as one pattern: a name that occurs in two of
the arm's patterns is a duplicate and is `E-name-conflict` (Annex B,
Clause 7.4.6).

### 6.8.7 Excluded Patterns

Three pattern forms do not exist:

- **Dictionary patterns.** Dictionary keys are values, not structure.
  Retrieval goes through `Dict.get`, which returns `Option`, and the `Option`
  is handled by a `case` (Clause 9.10.2).
- **As-patterns.** A pattern cannot bind both a whole value and its parts. To
  obtain the whole, use a nested `case` or field access.
- **Conjunctive patterns (`P and Q`).** Two structural conditions on one value
  are written as one pattern: a record pattern conjoins its fields, and a
  constructor pattern nests its payload. A `Bool` condition is a guard. An
  `and` form would duplicate these mechanisms and complicate exhaustiveness
  for no expressive gain.

```eko
case p:
    {name, age} if age >= 18 -> name
    _                        -> "minor"
```

## 6.9 Type Expressions

### 6.9.1 Type Atoms

A type expression is a type atom or a composite type. Type atoms are:

| Atom | Example | Meaning |
|---|---|---|
| named type | `Person`, `Int`, `Never` | a declared type |
| type variable | `t`, `elem` | a generic variable |
| module-qualified type | `person.Person` | a type in an imported module |
| unit type | `_` | the unit type |
| parenthesized type | `([]Int)` | grouping |

A bare lowercase name in type position is a type variable. A bare PascalCase
name is a declared type; a leading privacy prefix does not change the class
(Clause 5.5.2). A dotted name is a module path. Classification is by spelling
alone (Clause 7.4.1).

`_` in type position always means the unit type; it never means "infer".

### 6.9.2 Composite Types

Composite types are written with brackets:

| Form | Syntax | Example |
|---|---|---|
| list | `[]t` | `[]Int`, `[]Person` |
| dictionary | `[k]v` | `[String]Int` |
| tuple | `[a, b, ...]` | `[String, Int]` |
| record | `[f: a, g: b, ...]` | `[name: String, age: UInt]` |

List and dictionary are distinguished inside the brackets: an empty bracket
pair means list, one type means dictionary. These are the only forms that
place a type after `]`.

A tuple has at least two elements. A one-element bracket `[Int]` is not a
tuple and is not a valid type. Parentheses group a type or form a parameter
group: in `Option (Value Int Float)` the parentheses group the type argument;
in `(Int)` followed by an arrow, or standing as a function type, they form the
parameter group of the unit-returning function type `(Int) -> _`. A
zero-element `()` is the parameter group of a zero-parameter function type.

Parentheses disambiguate nesting: `[]([String]Int)` is a list of dictionaries;
`[[]String]Int` is a dictionary whose keys are lists.

### 6.9.3 Function Types and Arrows

A function type is a parameter group followed by an arrow and a result type:

```ebnf
FunctionType := ParameterGroup ( '->' Type
                               | '!->' Type
                               | '?->' Type
                               | '!'
                               | '?'
                               | ε )
```

The arrow forms are:

| Form | Meaning |
|---|---|
| `(a) -> b` | pure function from `a` to `b` |
| `(a) !-> b` | world-touching function from `a` to `b` |
| `(a) ?-> b` | open function from `a` to `b` |
| `(a)` | elided arrow: `(a) -> _` |
| `(a) !` | elided arrow: `(a) !-> _` |
| `(a) ?` | elided arrow: `(a) ?-> _` |

`()` is the zero-parameter function type and is an abbreviation of `() -> _`.
A unit result is written `-> _`, not `-> ()`.

Parentheses are context-sensitive. A parenthesized group in a position where a
complete type is expected is a parameter group with the arrow elided:
`(Int)` is `(Int) -> _`. A parenthesized group in a position where a type atom
is expected, such as the argument of a generic application, is a grouped type:
`Option (Value Int Float)`.

Arrows nest without parentheses:

```eko
(Out) -> (String) !-> String      // pure factory returning a world-touching function
(Out) !-> (String) -> String      // world-touching function taking a pure function
(Out) !-> (String) !-> String     // both world-touching
```

The bit markers `!` and `?` are written joined to the arrow as `!->` and
`?->`. A lone marker is legal only in the elided forms. The marker takes no
label, variable, or annotation.

### 6.9.4 Generic Application

A generic type is applied by juxtaposition, with no commas:

```ebnf
AppliedType := TypeAtom TypeAtom*
```

An argument is an arrow-free atom. An application chain stops at `,` and at
`->`.

```eko
Value Int Float                     // t = Int, u = Float
f:  (Value Int Float) -> Int        // one parameter
xs: []Value Int Float               // a list
ts: [Value Int Float, Bool]         // the chain stops at the comma
o:  Option (Value Int Float)        // a composite argument takes parentheses
```

A composite argument MUST be parenthesized; a bare composite atom in
argument position is an error, and the diagnostic identifies this clause. A
type variable MUST NOT head an application: `t Int` is an error.

### 6.9.5 Type Declarations

The right-hand side of a `type` declaration is a declaration form, not an
arbitrary type expression:

- a variant list: `Man | Woman`, or a vertical list with a leading `|` on each
  line;
- a record type: `[name: String, age: UInt]`;
- a tuple type: `[String, Int]`.

A composite type expression such as `[]Int` is not a declaration form. A
declaration introduces a new nominal type; it does not name an existing one.

A generic declaration lists its binders after `type` and before `=`:

```eko
Pair:  type t u = [t, u]
Value: type t u = First [u, t] | Second t
Tree:  type t = Leaf t | Node [Tree t, Tree t, t]
```

Every binder MUST occur in the right-hand side. A phantom binder is an error
(Clause 7.6.2).

## 6.10 Excluded Syntax

The following constructs do not exist. Their occurrence is an error.

| Construct | Rule |
|---|---|
| User-defined operators | The operator set is closed (Clause 7.8.3) |
| Signature overloading | One name has one signature; ad-hoc dispatch is `case` (Clause 7.4.6) |
| Bitwise operators `&`, `^`, `<<`, `>>`, `~` | Not tokens; functions are the vocabulary (Clause 9.6.5) |
| Exponentiation `**` | Not a token; use `Int.pow` or `Float.pow` (Clause 9.6.5) |
| Increment and decrement `++`, `--` | Not statements; `++` is concatenation only (Clause 6.5.4) |
| Ternary `?:` | Not a form; `if` is an expression (Clause 6.7.4) |
| Chained comparisons | Comparisons do not associate (Clause 6.5.5) |
| Block comments | Not a form; use `//` (Clause 5.4.1) |
| Numeric literal type suffixes | Not a form; placement is by expected type (Clause 5.7.4) |
| Tab as whitespace | `E-tab` (Clause 5.2.3) |
| Truthiness coercion | Conditions and guards are `Bool` (Clause 7.8.5) |
| Word operators `and`, `or`, `not` | `not` is a member; `and` and `or` have no meaning (Clause 6.5.4) |
| Prefix `!x` | `!` is the world-bit marker only (Clause 7.9.2) |
| Labels or variables after `!` or `?` | The markers take no operand (Clause 6.9.3) |
| Index syntax `x[i]` | Not a form; use `get`, or `at` for strings (Clause 9.10) |
| Value-position `[...]` | Braces are the only value brackets (Clause 6.6.1) |
| Multiline string literals | Strings are single-line (Clause 5.9.1) |
| Loop syntax `while`, `for`, `do`, `loop` | Not forms; iteration is combinators and recursion (Clause 8.8) |
| `break`, `continue` | Not forms; early exit is combinator semantics (Clause 8.8) |
| Dictionary patterns | Not a form (Clause 6.8.7) |
| As-patterns | Not a form (Clause 6.8.7) |
| Conjunctive patterns `P and Q` | Not a form; one pattern conjoins fields, a guard conjoins a `Bool` (Clause 6.8.7) |
| Type aliases | The right-hand side of `type` is a declaration form only (Clause 6.9.5) |
