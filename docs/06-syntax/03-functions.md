# 6. Syntax

## 6.4 Functions and Lambdas

### 6.4.1 Lambda Form

A lambda is an anonymous function written as a parameter group, `:`, and a
body (Annex A.3.4).

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

A lambda in operand position — the operand of a unary or binary operator,
or the argument of a constructor application by juxtaposition — is spelled
parenthesized: in those positions `( parameters ) :` is the parenthesized
expression followed by the enclosing header's colon, and the unparenthesized
lambda form is not read. Call arguments, statement positions, and trailing
callbacks read the unparenthesized form.

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

A parameter group is a parenthesized, comma-separated sequence of parameters
(Annex A.3.4). A parameter is an identifier, `_`, or a nested parameter
group — a group of parameters, not a destructuring pattern
(Clause 7.7.1). The zero-parameter group `()` is legal.

A parameter group in a function type contains types, optionally preceded by a
parameter label: `(a: Int, b: Int)`. A parameter group in a lambda contains
binding names: `(a, b):`. The two are distinguished by position: a function
type occurs in an annotation, a lambda in an expression.

```eko
(a, b): a + b
(_, i): i
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
