# Negative Conformance Manifest

Each entry is a minimal program that must fail to compile. The expected
outcome is a published code from `Annex B.3` where one exists;
otherwise it names the rejection and the normative clause.

| ID | Rejection | Expected outcome |
|---|---|---|
| N01 | nominal value into another nominal type | `E-nominal-mismatch` |
| N02 | wrong shape into an anonymous parameter | `E-shape-mismatch` |
| N03 | missing constructor arm | `E-exhaust` |
| N04 | bare panic as a case arm | compile error: bare divergence is not an arm |
| N05 | world bit under-declared | `E-world-bit` |
| N06 | world bit under-declared through a dispatcher | `E-world-bit` |
| N07 | world bit over-declared | `E-world-bit` |
| N08 | open bit not fed by a parameter | `E-open-bit-unfed` |
| N09 | result-only generic variable | `E-phantom` |
| N10 | phantom type binder | `E-phantom` |
| N11 | lowercase type name | `E-name-case` |
| N12 | import and top-level binding share a name | `E-name-conflict` |
| N13 | assertion on capabilities | `E-no-eq` |
| N14 | integer literal above `Int.max` | `E-literal-overflow` |
| N15 | raw tab as indentation | `E-tab` |
| N16 | chained comparison | parse error |
| N17 | index syntax | parse error |
| N18 | mixed numeric operands | compile error: no implicit conversion |
| N19 | `:=` on an unannotated lambda | compile error: `:=` does not generalize |
| N20 | `?` in a container element position | compile error: `?` position rule |
| N21 | extern naming a reserved `eko_` symbol | `E-reserved-symbol` |
| N22 | `ctype` field outside machine words | `E-ctype-field` |
| N23 | extern parameter outside the marshalling whitelist | `E-extern-type` |
| N24 | type variable in an extern signature | `E-extern-type` |
| N25 | one name bound by two patterns of one case arm | `E-name-conflict` |
| N26 | floating literal that overflows the finite range | `E-literal-overflow` |
| N27 | two visible constructors and no anchor | `E-undetermined` |
| N28 | opened free name and a top-level binding share a name | `E-name-conflict` |
| N29 | two imports open the same free name | `E-name-conflict` |

## N01 — nominal mismatch

`Celsius` and `Fahrenheit` have the same shape but different identities.

```eko
Celsius: type = [deg: Float]
Fahrenheit: type = [deg: Float]

c: Celsius = {deg: 0.0}
f: Fahrenheit = c
```

Expected: `E-nominal-mismatch`. A nominal value never flows into another
nominal type (`Clause 7.5.2`).

## N02 — shape mismatch

Same-shape adaptation is an exact match; a wider record does not adapt.

```eko
Wide: type = [name: String, age: Int, city: String]

take: (p: [name: String, age: Int]) -> String = p.name
w: Wide = {name: "ada", age: 36, city: "x"}

use: () -> String = take(w)
```

Expected: `E-shape-mismatch`. The anonymous shape has fewer fields
(`Clause 7.5.5`).

## N03 — missing arm

```eko
Sex: type = Man | Woman

label: (s: Sex) -> String =
    case s:
        Man -> "m"
```

Expected: `E-exhaust`. `Woman` is not covered (`Clause 7.7.2`).

## N04 — bare panic arm

```eko
Sex: type = Man | Woman

label: (s: Sex) -> String =
    case s:
        Man -> "m"
        panic("unreachable")
```

Expected: compile error: a bare divergence is not an arm; every arm is
`PATTERN -> expression` (`Clause 6.7.1`). No diagnostic code is
published for this parse-level rejection.

## N05 — world bit under-declared

```eko
greet: (out: Out) -> _ =
    out.println("hi")
```

Expected: `E-world-bit`. The body touches the world but the arrow is pure
(`Clause 7.9.2`).

## N06 — under-declared through a dispatcher

```eko
print_all: (out: Out, xs: []String) -> _ =
    xs.each((s): out.println(s))
```

Expected: `E-world-bit`. `each` instantiates its open bit at `!` from the
printing callback, so the body is world-touching while the arrow is pure
(`Clause 7.9.6`).

## N07 — world bit over-declared

```eko
double: (n: Int) !-> Int = n * 2
```

Expected: `E-world-bit`. A pure body cannot carry a surplus `!`
(`Clause 7.9.2`).

## N08 — open bit unfed

```eko
count: (xs: []Int) ?-> Int = List.length(xs)
```

Expected: `E-open-bit-unfed`. No parameter contains a `?` arrow, so nothing
feeds the open bit (`Clause 7.9.6`).

## N09 — result-only generic

```eko
empty: () -> []t = {}
```

Expected: `E-phantom`. A type variable must occur in a parameter type
(`Clause 7.6.5`).

## N10 — phantom binder

```eko
Value: type t u = Second t
```

Expected: `E-phantom`. The binder `u` does not occur in the right-hand side
(`Clause 7.6.2`).

## N11 — type name case

```eko
person: type = [name: String]
```

Expected: `E-name-case`. Declared types are PascalCase (`Clause 5.5.2`, `Clause 7.4.1`).

## N12 — name conflict

```eko
import geolib

geolib: Int = 1
```

Expected: `E-name-conflict`. An import and a file top-level binding share one
layer (`Clause 7.4.6`). The snippet assumes `geolib.eko` from
`tests/conformance/` is on the module path.

## N13 — no equality on capabilities

```eko
check: () -> _ = assert_eq(Out.null(), Out.null())
```

Expected: `E-no-eq`. Capabilities have no `==`, so `assert_eq` rejects them
(`Clause 9.8.4`, `Clause 9.14.3`).

## N14 — literal overflow

```eko
big: Int = 9223372036854775808
```

Expected: `E-literal-overflow`. The literal is `Int.max + 1`
(`Clause 5.7.4`).

## N15 — tab indentation

```eko
count: Int =
→   0        // the arrow marks one raw tab
```

The arrow marks one raw tab character; the spaces after it complete the
indentation. Expected: `E-tab`. A raw tab is not whitespace
(`Clause 5.2.3`).

## N16 — chained comparison

```eko
between: (a: Int, b: Int, c: Int) -> Bool = a < b < c
```

Expected: parse error. Comparisons have no associativity; write
`a < b && b < c` (`Clause 6.5.5`).

## N17 — index syntax

```eko
head: (xs: []Int) -> Int = xs[0]
```

Expected: parse error. Expression-position `[` does not exist; use `get`
(`Clause 6.10`, `Clause 9.10`).

## N18 — mixed numeric operands

```eko
ratio: Float = 1 / 2.0
```

Expected: compile error: type mismatch between `Int` and `Float`; there is no
implicit numeric conversion (`Clause 7.8.4`). No diagnostic code is
published for ordinary type mismatch.

## N19 — no generalization at `:=`

```eko
id := (x): x
```

Expected: compile error: `:=` does not generalize, and a lambda is check-only,
so no expected type supplies the parameter type (`Clause 7.6.3`). No
diagnostic code is published.

## N20 — open bit in a container element

```eko
is_zero: (n: Int) -> Bool = n == 0
is_negative: (n: Int) -> Bool = n < 0

handlers: [](Int) ?-> Bool = {is_zero, is_negative}
```

Expected: `E-open-bit-position`. `?` is not allowed in a container element
position; it is allowed only on the binding's own arrow and in arrows inside
its parameter types (`Clause 7.9.6`).

## N21 — reserved symbol

```eko
_wrap: (_OutB) -> Out = extern "eko_out_wrap" Pure
```

Expected: `E-reserved-symbol`. Symbols beginning `eko_` are the host's; the
lexical rule "the word is the audit" cannot stop a link-time symbol, so the
symbol space itself draws the line (`Clause 10.6`).

## N22 — ctype field predicate

```eko
Stat: ctype = [size: I64, label: String]
```

Expected: `E-ctype-field`, at the declaration line. A `ctype` mirrors a C
struct; every field is a machine word or a nested `ctype` record
(`Clause 10.3.3`).

## N23 — extern parameter outside the whitelist

```eko
_sum: (xs: []I64) !-> I64 = extern "sum64"
```

Expected: `E-extern-type`. An extern signature carries only whitelisted
types (`Clause 10.3.5`); a list is ferried by the library layer.

## N24 — type variable in an extern signature

```eko
_id: (x: t) !-> t = extern "id_any"
```

Expected: `E-extern-type`. C symbols have no template semantics; an extern
signature is monomorphic and generic wrapping is ordinary library code
(`Clause 10.2.3`).

## N25 — duplicate binding across one arm's patterns

```eko
pair: (p: [Int, Int], q: [Int, Int]) -> Int =
    case p, q:
        {x, _}, {x, _} -> x
```

Expected: `E-name-conflict`, at the second occurrence. The patterns of one
arm of a multi-scrutinee `case` bind together: a name that occurs in two of
the arm's patterns is a duplicate (`Clause 6.8.6`, `Clause 7.4.6`). Distinct
names across the arm's patterns, the same name in different arms, and an
or-pattern's alternatives are not affected.

## N26 — floating literal overflow

```eko
big: Float = 1e999
```

Expected: `E-literal-overflow`. The literal's value is not finite after
conversion — a naive conversion delivers infinity (`Clause 5.7.2`). A
literal that stays finite, including one that underflows to a subnormal or
zero, is not affected.

## N27 — two visible constructors without an anchor

`MyBool.True` and std's `Bool` constructor `True` are both visible, and
nothing in an unannotated binding says which one `True` denotes.

```eko
MyBool: type = True | False

x: Bool = True
y: MyBool = True
z = True
```

Expected: `E-undetermined`. Constructors never conflict by visibility:
same-named constructors of different types coexist, and an expected type
anchors a reference (`Clause 7.4.6`). An unanchored reference is legal
only when exactly one constructor of that name is visible and it is
non-generic (`Clause 7.6`); here two are visible, so `x: Bool` and
`y: MyBool` are legal and `z` is not. Writing `z: Bool = True` or
`z: MyBool = True` anchors it.

## N28 — opened free name and a top-level binding

`import geolib.{area}` opens `area` into the file's top layer, which a
top-level binding of the same spelling already occupies.

```eko
import geolib.{area}

area: (s: Shape) -> Float = 0.0
```

Expected: `E-name-conflict`, at the binding's declaration. An opened free
name and a file top-level binding share one layer (`Clause 7.2.3`,
`Clause 7.4.6`). A local binding of the name stays legal. The snippet
assumes `geolib.eko` from `tests/conformance/` is on the module path.

## N29 — two imports opening one free name

The second import opens a name the first import already opened.

```eko
import geolib.{area}
import geolib.{move, area}

r: Shape = Rect {2.0, 3.0}
a: Float = area(r)
```

Expected: `E-name-conflict`, at the second import. Two imports that open
the same free name conflict at the declaration point (`Clause 7.2.3`).
The duplicate may sit in one list or across two lists; both are the same
rejection.
