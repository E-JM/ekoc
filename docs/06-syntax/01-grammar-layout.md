# 6. Syntax

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
```

Examples that use `out`, `fs`, `file`, or `scope` are read inside a function
whose parameters include `out: Out` and `fs: Fs`; `file` and `scope` are
callback parameters.

## 6.2 Compilation Units and Layout

### 6.2.1 Compilation Unit

A compilation unit is a sequence of top-level items (Annex A.3.1). An import
line MUST precede every top-level binding; an import after a binding is an
error. A file with no bindings is legal; it contributes only
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
separated by `;` (Clause 5.3.2, Annex A.3.3).

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

A header introduces a block and ends in one of three ways:

| Header | Terminator | Introduces |
|---|---|---|
| function, value, `test`, or `extern` definition; `type` with vertical variants | `=` | the body block, or the variant block |
| lambda, `if`, `else`, callback; `case` | `:` | the body block, or the arm block |
| `case` arm | `->` | the arm body block |

A header MAY be followed by its body on the same line when the body is a
single expression. A deeper following line opens the body block; for a `case`
arm, the block's last member is the arm's value.

```eko
double: (n: Int) -> Int = n * 2
```

### 6.2.4 Line Continuation

A line terminator ends a member except in the following positions, where
continuation is licensed.

1. **Inside brackets.** Inside `(`, `[`, or `{`, line terminators are free
   formatting. The matching closing bracket ends the group.
2. **Before a chain dot.** A receiver chain MAY continue on the next line when
   the line begins with `.`. Each continuation line begins with `.` and is
   aligned with the first `.` of the chain; a `.` in a different column is an
   error (Clause 6.5.3).
3. **A trailing callback.** The last argument of a call MAY occupy its own
   line (Clause 6.2.5).
4. **A continued scrutinee.** The scrutinee of a `case` MAY continue on the
   following lines when it is indented deeper than `case` (Clause 6.7.1).
5. **A trailing operator or separator.** A line whose final significant
   token is a binary operator or one of `,`, `:=`, `.`, and `|` licenses the
   following deeper line as its continuation; the tokens continue the
   member begun on the earlier line.

No other position licenses continuation. In particular, `!` and `?` never
license continuation: a line ending in a lone marker is a complete member, and
`!->` and `?->` never wrap.

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
