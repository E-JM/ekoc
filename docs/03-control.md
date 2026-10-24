# Bindings, Functions, and Control Flow

This chapter defines how names are introduced and rebound, how anonymous
functions are written, how `case` and `if` branch, how patterns decompose
values, and how `Result` chains are written. `case` is the branching
primitive of the language; `if` is syntactic sugar for a `case` over a
`Bool`. Patterns are shared by `case` arms and binding lines.

## 1. Binding forms

### 1.1 Grammar

Eko has a single binding grammar. A binding or declaration line consists
of a pattern, an optional annotation, a binding operator, and a
right-hand side:

```text
PATTERN (':' ANNOTATION)? '=' RHS
PATTERN ':='              RHS
PATTERN '<-'              CALL
```

The annotation may appear only with `=`. For a value binding, the
right-hand side is an expression or an indented block. For a `type`
declaration it is a declaration form (`04-types.md §1`); for a `test`
declaration it is a block (`09-library.md §6`). `:=` carries no
annotation. In `<-`, the type of the bound pattern is given by the
callback parameter (§1.3).

**Line-start invariant.** A line with a pattern begins with that
pattern; an expression line begins with that expression. The operators
are infix, not prefix. The zero-argument inverted binding is the
pattern-less form and begins with `<-` (§1.3).

Structural markers occupy slots rather than introducing keywords. The
annotation slot holds a type expression for a value binding, `type` for
a type declaration (`04-types.md §1`), and `test` for a test declaration
(`09-library.md §6`). Bit annotations occupy the arrow (`!->`, `?->`,
`06-effects.md §5`). The binding operators occupy the binding slot:

```eko
Person: type = [name: String, age: UInt, sex: Sex, address: String]
age: UInt = 36
count := count + 1
(p, i) <- persons.each_with_index()
```

### 1.2 Initial binding `=` and rebinding `:=`

`=` introduces a binding. The right-hand side is evaluated and bound to
the pattern. A value binding may carry a type annotation between the
pattern and `=`; the annotation fixes the binding's type:

```eko
age: UInt = 36
person: Person = {name: "ada", age: 36, sex: Woman, address: "x"}
```

Without an annotation, the type is determined by the right-hand side. A
composite literal with no expected type requires an annotation
(`04-types.md §2`).

A parenthesized tuple on the left of `=` destructures a tuple value:

```eko
(s, want) = row
```

In lambda and `<-` parameter positions, the same parenthesized form is a
group of parameters, not destructuring (§5.6).

`:=` is the rebinding form. It introduces a binding for a name without
an annotation; if the name is already bound, the new binding shadows the
old one. Its type is determined by the right-hand side. Values are
immutable: `:=` changes which value the name denotes, never the previous
value. A closure that captured the name before the rebinding keeps the
value captured at capture time (`07-runtime.md §1.3`). `:=` does not
generalize; a polymorphic binding must be introduced by an annotated
`=` (`04-types.md §8`).

```eko
count := count + 1
p2 := p
```

`_ := e` evaluates `e` and discards the value. In value position `_` is
Unit; in pattern position it is the wildcard (`06-effects.md §5`).

### 1.3 Inverted binding `<-`

An inverted binding places the remaining block of the enclosing scope in
the last parameter position of a call. The `<-` token is reserved for
inverted binding and has no other use. The form is:

```eko
p <- f(a...)
rest
```

is defined as

```eko
f(a..., (p): rest)
```

`rest` is the entire remaining block after the binding line. The callback
occupies the last parameter position; this is the prelude's contract for
higher-order functions, where the callback is last (`06-effects.md §4`,
`06-effects.md §9`).

The value of the binding line is the value of the call, for example
`Task b`. To obtain both the call's value and the inverted binding,
write the nested-lambda form directly.

An inverted binding is the layout form of a lambda call. The equivalent
explicit form of

```eko
age <- data.get(1).to_result("short row").try()
```

is

```eko
data.get(1).to_result("short row").try((age):
    rest)
```

The admitted pattern forms are exactly those of a lambda parameter position
(§5.6): an identifier, the wildcard `_`, or a tuple parameter group. Record,
constructor, and list patterns do not appear after `<-`; destructuring
happens on a binding line or in a `case`.

```eko
age <- data.get(1).to_result("short row").try()
(p, i) <- persons.each_with_index()
_ <- save(entry).try()   // save returns Result _ e: the payload is discarded
```

A zero-argument callback omits the pattern:

```eko
<- scope.spawn()            // the rest is the task body (06-effects.md §8)
```

`<- f()` and `_ <- f()` are distinct forms: the pattern-less form passes a
zero-parameter callback, the wildcard form a one-parameter callback that
binds nothing. The callback's shape is fixed by the expected type, so the
two readings never compete (§5.6).

Multiple `<-` bindings nest in sequence: each later line lies in the
callback body of the earlier one.

```eko
a <- f()
b <- g(a)
h(a, b)
```

is defined as

```eko
f((a):
    g(a, (b):
        h(a, b)))
```

Inside a `case` arm, `rest` is the remaining block of that arm.

The `<-` form introduces no world-bit rule of its own. `rest` is the
callback body, and its bit is inferred as for any lambda (§2.2). The
dispatcher instantiates its open bit from `rest` (`06-effects.md §5`). A
combinator that is itself world-touching, such as `with_file`, has no
open bit; its `!` absorbs the callback's bit by OR (`06-effects.md §4`).

Because the desugaring appends a callback in the last position, `<-`
requires a call that has a callback parameter. It is not applicable to a
`Never`-returning call that takes none (`04-types.md §10`).

### 1.4 Data-flow reading

The direction of the arrow is the direction of data flow. In a `case`
arm the arrow points to the result:

```eko
case r:
    Ok v -> v
```

In a binding line the arrow points to the name:

```eko
age <- data.get(1).to_result("short row").try()
```

The arm arrow `->` points at the value produced from the pattern
bindings; the binding arrow `<-` points at the name that receives the
call's value.

## 2. Lambdas

### 2.1 Form

A lambda is an anonymous function written as a parameter group, a colon,
and a body:

```eko
(n): n + 1
(p, i): out.println("{i}: {p}")
(xs): xs.map((n): n * 2).length()
```

The body may be a multi-line block; the block is indented one level:

```eko
persons.each_with_index((p, i):
    out.println("{i}: {p}"))
```

A last-position callback may occupy its own line; its body is indented
one level.

Parameters are identifiers, the wildcard `_`, or tuple parameter groups. A
tuple parameter group is a group of parameters, not a destructuring pattern
(§5.6). Parameter types come from the expected function type; actual
arguments are checked last, when open bits are collected (`06-effects.md
§5`). `_` is the wildcard parameter: it occupies a parameter position,
binds nothing, and may occur in any parameter position of a group —
`(_, i): i` keeps the index and discards the element (`06-effects.md §5.3`).

A lambda carries no annotations: no parameter types, no return type, and
no bit annotation. Its type and bit are supplied by context and by its
body (§2.2).

Named functions use the binding form of §1: a name with a type annotation
and a body. The body may itself be a lambda, or a block that uses the
parameters from the annotation:

```eko
inc: (Int) -> Int = (n): n + 1

add: (a: Int, b: Int) -> Int =
    a + b
```

### 2.2 Type and world-bit inference

A lambda is checked against an expected function type. The expected type
supplies the parameter types and the result type; the body is checked
against the result type. A lambda is never generalized: a polymorphic
binding needs an annotated `=` (`04-types.md §8`).

A lambda has no bit annotation. Its world bit is the OR of the bits of
the calls in its body: a call that touches the world makes the lambda
`!`; a body with no such call is pure (`06-effects.md §5`). The bit
annotation lives on the arrow of a named binding:

```eko
each: ([]String, (String) ?-> _) ?-> _      // dispatcher signature
```

A lambda passed to a dispatcher instantiates the dispatcher's open bit:

```eko
main: (out: Out) ! =
    persons.each((p): out.println(p))
    // lambda bit = !, so each is instantiated at !; the call contributes !, and main is !
```

A signature may contain several `?` annotations; all of them are the
same open bit (`06-effects.md §5`).

### 2.3 Captures

A lambda that refers to names from its enclosing scope captures their
values; the resulting function value is a closure. Capturing a capability
copies it, and copying a capability is delegation (`06-effects.md §2`).
Holding is quiet; calling sets the bit (`06-effects.md §5`). A pure
factory may therefore construct and return a `!` closure:

```eko
mk_logger: (out: Out) -> (Int) !-> Int =
    (n): out.println(show(n)); n + 1
```

The lambda captures `out`; its bit is decided by the call to
`out.println` in its body. Capturing `out` itself is quiet, and
`mk_logger` is pure.

A closure captures the value that the name holds at capture time. A
later `:=` of the same name does not write back into the closure's
environment; the new value belongs to the new binding
(`07-runtime.md §1.3`).

## 3. case

### 3.1 Arms and matching

`case` is the branching primitive. An arm is a pattern, an arrow, and an
expression:

```eko
parse_sex: (data: String) -> Result Sex String =
    case data:
        "man"   -> Ok Man
        "woman" -> Ok Woman
        _       -> Err "unknown sex: {data}"
```

The scrutinee is evaluated once. Arms are tried from top to bottom, and
the first matching arm produces the value of the whole `case`. A `case`
is an expression; the arms are its block (`02-syntax.md §1`).

A pattern binds names that are in scope in the arm's result expression
and in its guard, if any. In pattern position `_` is the wildcard, which
matches every value and binds nothing; in value position `_` is Unit
(`06-effects.md §5`).

All arms must produce the same type. A `Never` arm adapts to the type of
the other arms (`04-types.md §10`). A well-typed `case` has no
fallthrough and no match failure.

### 3.2 Exhaustiveness

A `case` must be exhaustive. An algebraic-data-type scrutinee is
exhaustive when the arms cover every constructor of the type
(`04-types.md §1`, `04-types.md §2`), or when the case ends with a `_`
wildcard arm. A scrutinee of any other type requires a final `_` arm.
The wildcard matches everything.

Exhaustiveness is counted by constructors, not by inhabitation. A
`Result t Never` still requires an `Err` arm even though no `Err` value
can be built (`04-types.md §10`).

A diverging arm does not discharge exhaustiveness. A `Never` expression
can make an arm type-check, but the pattern must still cover its
constructor; an arm is always `PATTERN -> EXPRESSION`, never a bare
`panic(...)`:

```eko
case r:
    Ok _ -> proceed()
    _    -> panic("unreachable")     // the pattern covers the case; panic fixes the type
```

### 3.3 Guards

An arm may carry a guard: a pattern, `if`, a `Bool` expression, an arrow,
and the result:

```eko
case n:
    n if n < 0  -> "negative"
    n if n == 0 -> "zero"
    _           -> "positive"
```

The pattern binds before the guard is evaluated. The guard is a `Bool`
expression and may use the names bound by the pattern. If the pattern
matches but the guard is false, matching continues with the next arm.

A `case` that contains guarded arms must end with an unguarded arm.
Exhaustiveness is closed at the unguarded arm, and the unguarded arm
closes the whole `case`. Guards occur only at arm level; no other pattern
position admits one.

The word `if` occupies two slots: the head of an `if` expression (§4) and
the guard slot. The lexicon is the same.

A guard is an ordinary `Bool` expression; its world bit is ORed into the
enclosing context like any other (`06-effects.md §5`).

### 3.4 Layout

`:` opens the case block. Arms are one level deeper than `case` and
start in the same column. The scrutinee may continue on following lines;
a continued scrutinee is indented deeper than `case`. When the scrutinee
contains a multi-line callback, the line that closes the call ends with
`):`, and the arms begin on the next line. Line breaks inside
parentheses are free, and the closing `)` also ends the callback block
inside the call (`02-syntax.md §2`):

```eko
main: (out: Out, fs: Fs) ! =
    case fs.with_file("data.csv", Read,
            (file):
                content <- file.read(4096).map_err((e): "io: {e}").try()
                persons <- content.split("\n").traverse((l): l.split(",").make_person()).try()
                Ok persons.each_with_index((p, i): out.println("{i}: {p}"))):
        Ok _  -> _
        Err e -> out.println(e)
```

The structure is:

```text
case fs.with_file(path, mode, (file): callback body):
    arms...
```

The nesting follows from the type of `with_file`: it returns
`Result a IoError`, where `a` is the callback's return type. The error
exists only after the callback finishes, so the `case` that consumes it
encloses the whole call. A chain whose values flow forward is flattened
with `<-` (§1.3); a call whose value flows outward is enclosed by
`case`. Two data-flow directions, two shapes, one level each.

### 3.5 Single-line form

Within a single line, `;` separates multiple expressions. It also
separates multiple `case` arms:

```eko
str = if n < 0: "negative" else: "other"
case r: Ok v -> v; Err e -> 0
if c: x(); y() else: z()         // the then-block has two expressions
```

The single-line form is a layout variant; it does not change matching or
evaluation. The rule behind it is that `;` is a newline equivalent for layout
purposes: it terminates the current member and the blocks opened within it
(02-syntax.md §1.3).

### 3.6 Multiple scrutinees

`case` accepts multiple scrutinees. The form

```text
case a, b: P, Q -> R
```

is sugar for a `case` over the tuple of the scrutinees: the scrutinee is the
tuple literal `{a, b}`, and each arm pattern is the tuple pattern `{P, Q}`.
Expression-position `[` does not exist in Eko, so the tuple is spelled with
braces at both value and pattern position (`04-types.md §2`).

Each scrutinee is evaluated once, and the comma mirrors the tuple
(`04-types.md §2`). Arms may carry guards as usual:

```eko
case n % 3 == 0, n % 5 == 0:
    True,  True  -> "FizzBuzz"
    True,  False -> "Fizz"
    False, True  -> "Buzz"
    False, False -> show(n)
```

## 4. if

`if` is syntactic sugar for a `case` over a `Bool`:

```text
if c: x else: y   ≡   case c:
                          True  -> x
                          False -> y
```

```eko
n = if x < 0: 0 else: x
```

The condition must be a `Bool`; there is no truthy coercion, and `True`
and `False` are the only `Bool` constructors (`08-builtins.md §1`). The
condition is evaluated once, and only the selected branch is evaluated.

Both arms must have the same type. An omitted `else` is `else: _`, so a
then-branch without an `else` must be `_` (or `Never`, which adapts):

```eko
if err_count > 0:
    out.println("errors!")       // the then-branch is _, and so is the omitted else
```

A `Never` arm adapts to the type of the other arm (`04-types.md §10`):

```eko
value: Int = if c: compute() else: die("no")
```

`if` is an expression and produces the value of the selected branch.
`else` is the second block of the `if` and starts in the same column as
`if`:

```eko
if n < 0:
    "negative"
else:
    "non-negative"
```

Branching is transparent to the world bit. The condition and every
branch body contribute their bits by OR to the enclosing context; there
is no separate bit rule for `if` or `case` (`06-effects.md §5`). For
`case`, the scrutinee, the guards, and every arm body contribute their
bits in the same way.

```eko
if fs.exists(path):              // the condition sets the bit, so the enclosing context is world-touching
    ...
```

The operators `&&` and `||` are themselves sugar for `if`
(`02-syntax.md §6`), and are bit-transparent in the same way.

## 5. Patterns

### 5.1 Grammar overview

Patterns form a compositional grammar: a pattern is a variable, `_`, a
literal, a constructor applied to a payload pattern, a tuple, a record, a
list, or an or-pattern. Nesting has no depth limit: `Ok {addr: {city}}`
and `{Some x, ..rest}` are ordinary patterns.

| Form | Shape | Examples |
|---|---|---|
| variable | a name | `x`, `msg` |
| wildcard | `_` | `_` |
| literal | a literal | `"man"`, `0`, `1.5`, `-1` |
| constructor | a constructor and an optional payload pattern | `Man`, `Ok v`, `Count n`, `Key Up` |
| tuple | `{P, Q, ...}` | `{x, y}` |
| record | `{field}` or `{field: P}` | `{name, age}`, `{name: n}` |
| list | see §5.4 | `{}`, `{h, ..t}`, `{.., 42}`, `{_, ..}` |
| or | `P \| Q` | `Key Up \| Key Down` |

A constructor pattern is a constructor name, optionally followed by a
payload pattern. Constructors are written without parentheses
(`04-types.md §3`). A variable pattern binds the matched value to the
name; `_` matches anything and binds nothing. Tuple, record, and list
patterns match a composite value and recursively match their components.
Tuple and list patterns share braces; the expected type decides between
them, as it does for composite literals (`04-types.md §2`).

The same pattern grammar is used in `case` arms and on the left of
binding operators, subject to the parameter-position restriction of
§5.6.

### 5.2 Record patterns

A record pattern matches a subset of fields. Only the fields written in
the pattern are bound; all other fields are ignored. Field names are
checked against the record type, so a misspelling is a compile error
rather than a field that silently never matches.

```eko
case r:
    Ok {name, age}        -> greet(name, age)   // shorthand: the field name is the binding name
    Ok {name: n, age: a}  -> "{n}:{a}"           // renaming
    Err msg               -> log(msg)
```

The shorthand `{name}` binds the field `name` to a binding of the same
name. The renaming form `{name: n}` binds the field `name` to `n`. The
two forms may be mixed in one pattern. Record patterns nest:

```eko
Ok {addr: {city}}
```

### 5.3 Or-patterns

`P | Q` matches when either alternative matches. Every alternative must
bind the same set of names at the same types; those bindings are
available in the arm body and in the guard:

```eko
case ev:
    Key Up | Key Down  -> nav(ev)                // each alternative binds the same names at the same types
    Key Enter          -> submit()
```

`|` in pattern position is the or-pattern separator. It does not collide
with the `|` that separates variants in a type declaration
(`04-types.md §2`); value position has no `|`.

### 5.4 List patterns

List patterns have five forms:

| Form | Meaning |
|---|---|
| `{}` | the empty list |
| `{x, y}` | fixed length; a list or a tuple, decided by the expected type |
| `{h, ..t}` | suffix rest: binds the first element to `h` and the tail list to `t` |
| `{.., 42}` | prefix rest: matches a list whose last element matches `42` |
| `{_, ..}` | anonymous rest: matches a non-empty list |

```eko
case xs:
    {}                 -> empty()
    {x, y}             -> pair(x, y)             // fixed length: list or tuple, decided by the expected type
    {h, ..t}           -> walk(h, t)             // suffix rest
    {.., 42}           -> ends_42()              // prefix rest
    {_, ..}            -> non_empty()            // anonymous rest
    {Foo, Count n, ..} if n > 0 -> ...           // constructor + payload + guard
```

Tuples have at least two elements (`04-types.md §2`). `{}` reads only as
the empty list. A rest binds the remaining list: the tail list for
`..t`, or the head list for a prefix rest. Prefix and suffix rests have
equal status. A list pattern may combine constructors, payload patterns,
and a guard, as the last line shows.

Eko has no loop syntax; recursive list processing is written with these
patterns and self tail-recursive calls (`07-runtime.md §8`).

### 5.5 Literal patterns

A literal pattern is one of the following forms:

| Literal pattern | Example |
|---|---|
| String | `"man"` |
| Integer | `0` |
| Float | `1.5` |
| Negative numeric | `-1` |
| Zero-payload constructor | `None`, `True`, `Man` |

A zero-payload constructor is written as a bare name and behaves like a
literal pattern; `True`, `False`, `None`, and user-declared variants
without a payload are examples. Literal patterns may nest inside
constructor patterns:

```eko
case r:
    Err "timeout" -> retry()
    Ok v          -> v
```

Interpolated strings are expressions, not literal patterns.

### 5.6 Duplicate bindings

Within one pattern, a binding name may occur at most once. `(x, x)` is a
compile error. `_` is exempt: it binds nothing, so it may occur any
number of times. The rule applies separately to each alternative of an
or-pattern, while §5.3 requires all alternatives to bind the same names
at the same types. Different arms may reuse the same names; each arm
binds independently.

Lambda and `<-` parameter positions admit identifiers, the wildcard `_`,
and tuple parameter groups; no other pattern form. Record, constructor,
and list patterns do not appear in parameter positions. A tuple parameter
group `(p, i)` is a two-parameter group, not automatic destructuring
(`04-types.md §2`); to destructure a tuple value, use a `case` pattern or
an `=` binding line. `_` is the one pattern besides a variable that a
parameter position admits: it binds nothing, so `(_, i)` and `(_, _)`
are legal, and `<-` inherits it (`06-effects.md §5.3`).

### 5.7 Excluded patterns

Two pattern forms are permanently excluded:

- **Dict patterns.** The keys of a dictionary are values, not structure.
  Retrieval goes through `Dict.get`, which returns `Option`, and the
  `Option` is handled by a `case` (`05-names.md §4`).
- **As-patterns.** Binding both a whole value and its parts in one
  pattern is excluded. To obtain the whole, use a nested `case` or field
  access.

These exclusions are permanent.

## 6. Working with Result

### 6.1 Combinators

The prelude provides four `Result` combinators (`05-names.md §11`):

```text
Result.try:      (Result t e, (t) ?-> Result u e) ?-> Result u e
Result.map:      (Result t e, (t) ?-> u)          ?-> Result u e
Result.map_err:  (Result t e, (e) ?-> f)          ?-> Result t f
Result.traverse: ([]t, (t) ?-> Result u e)        ?-> Result []u e
```

`Result.try` applies a `Result`-returning callback to the payload of
`Ok` and propagates `Err`. `Result.map` applies a callback to the payload
of `Ok` and wraps the result in `Ok`; `Err` passes through.
`Result.map_err` applies a callback to the payload of `Err` and wraps the
result in `Err`; `Ok` passes through. `Result.traverse` applies a
`Result`-returning callback to every element of a list and collects the
successful results into a list; an `Err` yields an `Err`.

The callbacks are open-bit: a pure callback makes the combinator pure,
and a world-touching callback makes it world-touching
(`06-effects.md §5`). The receiver forms are the usual UFCS forms
(`05-names.md §1`): `expr.try()`, `lines.traverse(f)`, and
`expr.map_err(g)`. A bare `expr.try()` is completed by the `<-`
desugaring; otherwise the callback is written explicitly.

`<-` appends a last-position callback to the call, so every unwrapping
in a chain appears explicitly as `.try()`. The error type must agree at
every link, and `map_err` is the conversion point. The explicit `.try()`
is the visible propagation point — the role `?` plays in Rust — earned by
the general callback-slot rule rather than a dedicated operator; no line
propagates an error silently.

### 6.2 Inverted-binding chains

A chain of `<-` bindings over `Result` unwraps one `Result` per line and
ends with the callback return form of the last combinator. A `try` chain
ends with `Ok ...` on its success path:

```eko
age_s <- data.get(1).to_result("short row").try()
age   <- UInt.parse(age_s).map_err((e): "age: {e}").try()
Ok age
```

The chain is the nested-lambda form written linearly:

```eko
data.get(1).to_result("short row").try((age_s):
    UInt.parse(age_s).map_err((e): "age: {e}").try((age):
        Ok age))
```

An `Err` at any link propagates out; the remaining lines belong to
callbacks that are not invoked. Each link's error type is `String`;
`map_err` converts the parse error to `String` before the next `try`.

The example builds a `Person` from a row:

```eko
make_person: (data: []String) -> Result Person String =
    name    <- data.get(0).to_result("short row: missing name").try()
    age_s   <- data.get(1).to_result("short row: missing age").try()
    age     <- UInt.parse(age_s).map_err((e): "age: {e}").try()
    sex_s   <- data.get(2).to_result("short row: missing sex").try()
    sex     <- parse_sex(sex_s).try()
    address <- data.get(3).to_result("short row: missing address").try()
    Ok {name: name, age: age, sex: sex, address: address}
```

`parse_sex` is defined in §3.1. The final `Ok {...}` is the innermost
callback's return value; it constructs the `Result Person String` that
the whole chain produces.

The `main` example in §3.4 combines the two shapes: a `case` encloses
`with_file`, and `<-` chains flatten the success path inside the
callback. The resulting `Result` is consumed by the `case` arms.
