# 6. Syntax

## 6.7 case and if

### 6.7.1 case

A `case` expression has a scrutinee and one or more arms (Annex A.3.3,
A.3.5).

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
        Ok _   -> _
        Err e  -> out.println(e)
```

An arm is always `PATTERN -> EXPRESSION`. A bare expression is not an arm. A
guard is written `PATTERN if CONDITION -> EXPRESSION` (Clause 6.7.3).

A `case` that contains a guarded arm MUST end with an unguarded arm. The
unguarded arm closes the match. Exhaustiveness is specified in Clause 7.7.2.

### 6.7.2 Multiple Scrutinees

`case` accepts multiple scrutinees (Annex A.3.5).

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

The pattern binds before the guard is evaluated; the guard MAY use the names
bound by its pattern. If the pattern matches and the guard is false, matching
continues with the next arm. A guard occurs only at arm level; no other
pattern position admits one.

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

The `else` block starts in the same column as `if` (Clause 6.2.2).

`if` is an expression and produces the value of the selected branch. Both
branches MUST have the same type; a `Never` branch adapts
(Clause 7.7.4).

### 6.7.5 Single-Line Form

Within a single line, `;` separates multiple expressions and multiple `case`
arms:

```eko
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
pattern. The constructor MAY be module-qualified (Clause 7.4.4). Constructors
are written without parentheses. A variable pattern binds the matched value
to the name. `_` matches anything and binds nothing.

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
the arm's patterns is a duplicate and is `E-name-conflict` (Annex B).

### 6.8.7 Excluded Patterns

Dictionary patterns, as-patterns, and conjunctive patterns (`P and Q`) do not
exist (Clause 6.10); two structural conditions on one value are written as one
pattern, and a `Bool` condition is a guard. The rationale is in Annex E.

```eko
case p:
    {name, age} if age >= 18 -> name
    _                        -> "minor"
```
