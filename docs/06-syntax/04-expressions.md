# 6. Syntax

## 6.5 Expressions

### 6.5.1 Expression Forms

An expression is one of the following forms:

| Form | Example |
|---|---|
| literal | `42`, `'a'`, `"text"` |
| name | `xs`, `Person`, `Int.add` |
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
expected. A juxtaposed constructor argument is an operand, and no block is
expected there.

### 6.5.2 Calls and UFCS

A call is written in prefix form, `f(a, b)`, or in receiver form, `a.f(b)`
(Annex A.3.5). The two forms are identical when `f` resolves to a function —
a namespace member, a module function, or a free function (stations 2 through
4, Clause 7.4.3):

```text
a.f(b, c)  ≡  f(a, b, c)
a.Mod.f(b, c)  ≡  Mod.f(a, b, c)
```

When `f` is a function-typed field of `a`'s record type, `a.f(b, c)` is the
call of that field value, `(a.f)(b, c)`; station 1 precedes the others, and
the identity does not apply (Clause 7.4.3).

In every resolution, the first parameter is the receiver. There is no other
method-call form.

A bare constructor is applied by juxtaposition. The argument, when present,
is a postfix expression, and juxtaposition binds tighter than every
operator: `Ok inner`, `Duration 500`, `Succ (Succ Zero)`, and `Ok x || y`
reads `(Ok x) || y`. A nullary constructor is the bare `PascalName`. A
module-qualified constructor `Mod.Name` selects among same-named
constructors (Clause 7.4.6) and is applied as a call on its dotted head,
`geolib.Circle(r)`; the juxtaposition form belongs to the bare spelling.

### 6.5.3 Member Chains

A member chain is a sequence of receiver calls:

```eko
content.split("\n")
       .map(String.trim)
       .filter((l): l != "")
       .length()
```

Each link's receiver is the value of the previous link. A chain MAY attach a
module at a link with `.Mod.f()` or a module-qualified type namespace with
`.Mod.Type.f()` (Clause 7.4.4). A fully qualified name MAY be passed as an
argument, as in `.map(String.trim)`; that is a name reference, not a call.

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
`Char`. `Range` is a compiler-supplied nominal type (Clauses 9.1.1 and
9.10.5); it has no constructor and no pattern form, and the range operators
are its only construction. A `Float` endpoint and an unbounded range do not
exist.

Range operators are infix and bind less tightly than postfix. A chain over a
range requires parentheses:

```eko
(1..=100).map(f)
(0..n).to_list().filter((i): i % 2 == 0)
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
