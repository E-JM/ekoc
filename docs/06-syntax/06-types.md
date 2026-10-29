# 6. Syntax

## 6.9 Type Expressions

### 6.9.1 Type Atoms

A type expression is a type atom or a composite type. Type atoms are:

| Atom | Example | Meaning |
|---|---|---|
| named type | `Person`, `Int` | a declared type |
| type variable | `t`, `elem` | a generic variable |
| module-qualified type | `person.Person` | a type in an imported module |
| unit type | `_` | the unit type |
| bottom type | `Never` | the bottom type |
| parenthesized type | `([]Int)` | grouping |

A bare lowercase name in type position is a type variable. A bare PascalCase
name is a declared type; a leading privacy prefix does not change the class
(Clause 5.5.2). A dotted name is a module path. Classification is by spelling
alone (Clause 7.4.1).

`_` in type position always means the unit type; it never means "infer". `Never`
in type position always means the bottom type; it is vocabulary of the language,
not a declared name, and no module declares it (Clause 7.5.4).

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
tuple and is not a valid type. Parentheses group a type, or form a parameter
group when an arrow follows or when the group stands as a function type
(Clause 6.9.3).

Parentheses disambiguate nesting: `[]([String]Int)` is a list of dictionaries;
`[[]String]Int` is a dictionary whose keys are lists.

### 6.9.3 Function Types and Arrows

A function type is a parameter group followed by an arrow and a result type
(Annex A.3.7). The arrow forms are:

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

A generic type is applied by juxtaposition, with no commas (Annex A.3.7). An
argument is an arrow-free atom. An application chain stops at `,` and at
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

A `type` declaration is specified in Clause 6.3.5. Its right-hand side is a
declaration form — a variant list, a record type, or a tuple type — not an
arbitrary type expression; a declaration introduces a new nominal type, not a
name for an existing one.

## 6.10 Excluded Syntax

The following constructs do not exist. Their occurrence is an error.

| Construct | Rule |
|---|---|
| User-defined operators | The operator set is closed (Clause 7.8.3) |
| Signature overloading | One name has one signature; ad-hoc dispatch is `case` (Clause 7.4.6) |
| Exponentiation `**` | Not a token; use `Int.pow` or `Float.pow` (Clause 9.6.5) |
| Increment and decrement `++`, `--` | Not statements; `++` is concatenation only (Clause 6.5.4) |
| Ternary `?:` | Not a form; `if` is an expression (Clause 6.7.4) |
| Chained comparisons | Comparisons do not associate (Clause 6.5.5) |
| Truthiness coercion | Conditions and guards are `Bool` (Clause 7.8.5) |
| Word operators `and`, `or`, `not` | `not` is a member; `and` and `or` have no meaning (Clause 6.5.4) |
| Labels or variables after `!` or `?` | The markers take no operand (Clause 6.9.3) |
| Index syntax `x[i]` | Not a form; use `get`, or `at` for strings (Clause 9.10) |
| Value-position `[...]` | Braces are the only value brackets (Clause 6.6.1) |
| Loop syntax `while`, `for`, `do`, `loop` | Not forms; iteration is combinators and recursion (Clause 8.8) |
| `break`, `continue` | Not forms; early exit is combinator semantics (Clause 8.8) |
| Dictionary patterns | Not a form (Clause 6.8.7) |
| As-patterns | Not a form (Clause 6.8.7) |
| Conjunctive patterns `P and Q` | Not a form; one pattern conjoins fields, a guard conjoins a `Bool` (Clause 6.8.7) |
| Type aliases | The right-hand side of `type` is a declaration form only (Clause 6.9.5) |

Lexical forms that do not exist — tabs, block comments, multiline string
literals, numeric type suffixes, bitwise tokens, and prefix `!` as negation —
are listed in Clause 5.11.
