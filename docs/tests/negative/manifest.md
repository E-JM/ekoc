# Negative Conformance Manifest

Each entry is a minimal program that must fail to compile. The expected
outcome is a published code from `09-library.md §7.2` where one exists;
otherwise it names the rejection and the normative section.

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

## N01 — nominal mismatch

`Celsius` and `Fahrenheit` have the same shape but different identities.

```eko
Celsius: type = [deg: Float]
Fahrenheit: type = [deg: Float]

c: Celsius = {deg: 0.0}
f: Fahrenheit = c
```

Expected: `E-nominal-mismatch`. A nominal value never flows into another
nominal type (`04-types.md §5.1`).

## N02 — shape mismatch

Same-shape adaptation is an exact match; a wider record does not adapt.

```eko
Wide: type = [name: String, age: Int, city: String]

take: (p: [name: String, age: Int]) -> String = p.name
w: Wide = {name: "ada", age: 36, city: "x"}

use: () -> String = take(w)
```

Expected: `E-shape-mismatch`. The anonymous shape has fewer fields
(`04-types.md §5.2`).

## N03 — missing arm

```eko
Sex: type = Man | Woman

label: (s: Sex) -> String =
    case s:
        Man -> "m"
```

Expected: `E-exhaust`. `Woman` is not covered (`03-control.md §3.2`).

## N04 — bare panic arm

```eko
Sex: type = Man | Woman

label: (s: Sex) -> String =
    case s:
        Man -> "m"
        panic("unreachable")
```

Expected: compile error: a bare divergence is not an arm; every arm is
`PATTERN -> expression` (`03-control.md §3.2`). No diagnostic code is
published for this parse-level rejection.

## N05 — world bit under-declared

```eko
greet: (out: Out) -> _ =
    out.println("hi")
```

Expected: `E-world-bit`. The body touches the world but the arrow is pure
(`06-world.md §5.1`).

## N06 — under-declared through a dispatcher

```eko
print_all: (out: Out, xs: []String) -> _ =
    xs.each((s): out.println(s))
```

Expected: `E-world-bit`. `each` instantiates its open bit at `!` from the
printing callback, so the body is world-touching while the arrow is pure
(`06-world.md §5.5`).

## N07 — world bit over-declared

```eko
double: (n: Int) !-> Int = n * 2
```

Expected: `E-world-bit`. A pure body cannot carry a surplus `!`
(`06-world.md §5.1`).

## N08 — open bit unfed

```eko
count: (xs: []Int) ?-> Int = List.length(xs)
```

Expected: `E-open-bit-unfed`. No parameter contains a `?` arrow, so nothing
feeds the open bit (`06-world.md §5.2`).

## N09 — result-only generic

```eko
empty: () -> []t = {}
```

Expected: `E-phantom`. A type variable must occur in a parameter type
(`04-types.md §8.3`).

## N10 — phantom binder

```eko
Value: type t u = Second t
```

Expected: `E-phantom`. The binder `u` does not occur in the right-hand side
(`04-types.md §7.1`).

## N11 — type name case

```eko
person: type = [name: String]
```

Expected: `E-name-case`. Declared types are PascalCase (`04-types.md §3`).

## N12 — name conflict

```eko
import geolib

geolib: Int = 1
```

Expected: `E-name-conflict`. An import and a file top-level binding share one
layer (`05-names.md §7.2`). The snippet assumes `geolib.eko` from
`tests/conformance/` is on the module path.

## N13 — no equality on capabilities

```eko
check: () -> _ = assert_eq(Out.null(), Out.null())
```

Expected: `E-no-eq`. Capabilities have no `==`, so `assert_eq` rejects them
(`08-builtins.md §6.4`, `09-library.md §6.3`).

## N14 — literal overflow

```eko
big: Int = 9223372036854775808
```

Expected: `E-literal-overflow`. The literal is `Int.max + 1`
(`02-syntax.md §4.1`).

## N15 — tab indentation

```eko
count: Int =
→   0        // the arrow marks one raw tab
```

The arrow marks one raw tab character; the spaces after it complete the
indentation. Expected: `E-tab`. A raw tab is not whitespace
(`02-syntax.md §1.1`).

## N16 — chained comparison

```eko
between: (a: Int, b: Int, c: Int) -> Bool = a < b < c
```

Expected: parse error. Comparisons have no associativity; write
`a < b && b < c` (`02-syntax.md §6.5`).

## N17 — index syntax

```eko
head: (xs: []Int) -> Int = xs[0]
```

Expected: parse error. Expression-position `[` does not exist; use `get`
(`05-names.md §4.1`).

## N18 — mixed numeric operands

```eko
ratio: Float = 1 / 2.0
```

Expected: compile error: type mismatch between `Int` and `Float`; there is no
implicit numeric conversion (`02-syntax.md §6.2`). No diagnostic code is
published for ordinary type mismatch.

## N19 — no generalization at `:=`

```eko
id := (x): x
```

Expected: compile error: `:=` does not generalize, and a lambda is check-only,
so no expected type supplies the parameter type (`04-types.md §8.2`). No
diagnostic code is published.

## N20 — open bit in a container element

```eko
is_zero: (n: Int) -> Bool = n == 0
is_negative: (n: Int) -> Bool = n < 0

handlers: [](Int) ?-> Bool = {is_zero, is_negative}
```

Expected: compile error: `?` is not allowed in a container element position;
it is allowed only on the binding's own arrow and in arrows inside its
parameter types (`06-world.md §5.2`). No diagnostic code is published for
the position rule.

## N21 — reserved symbol

```eko
_wrap: (_OutB) -> Out = extern "eko_out_wrap" Pure
```

Expected: `E-reserved-symbol`. Symbols beginning `eko_` are the host's; the
lexical rule "the word is the audit" cannot stop a link-time symbol, so the
symbol space itself draws the line (`11-ffi.md §6`).

## N22 — ctype field predicate

```eko
Stat: ctype = [size: I64, label: String]
```

Expected: `E-ctype-field`, at the declaration line. A `ctype` mirrors a C
struct; every field is a machine word or a nested `ctype` record
(`11-ffi.md §3.3`).

## N23 — extern parameter outside the whitelist

```eko
_sum: (xs: []I64) !-> I64 = extern "sum64"
```

Expected: `E-extern-type`. An extern signature carries only whitelisted
types (`11-ffi.md §3.5`); a list is ferried by the library layer.

## N24 — type variable in an extern signature

```eko
_id: (x: t) !-> t = extern "id_any"
```

Expected: `E-extern-type`. C symbols have no template semantics; an extern
signature is monomorphic and generic wrapping is ordinary library code
(`11-ffi.md §2.3`).
