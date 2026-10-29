# 7. Static Semantics

## 7.8 Calls and Operators

A call is checked against the callee's function type. An operator is a call in
notation form (Clause 6.5.4).

### 7.8.1 Call Typing

A call `f(a1, ..., an)` requires `f` to have a function type with exactly `n`
parameters. There are no optional parameters, default arguments, or variadic
parameters; arity MUST match exactly.

Each argument is checked against the corresponding parameter type at an
argument position. The argument position admits shape adaptation (Clause
7.5.5), `Never` adaptation (Clause 7.5.4), and bit adaptation (Clause 7.9.5).

The type of the call is the callee's result type. The expected type at the
call site MAY determine type variables of the callee (Clause 7.6.4). Type
arguments are never written; the callee's variables are instantiated by
unification with the argument types and the expected type.

Lambda arguments are checked last (Clause 7.6.4). A callee that is not a
function type is an error. A call through a value of function type is a call;
holding the value sets no bit (Clause 7.9.2). The world bit of a call is the
bit of the callee's arrow: `->` contributes pure, `!->` contributes
world-touching, and `?->` contributes its instantiation (Clause 7.9.4). A call
in receiver form is resolved by Clause 7.4.3 and typed by Clause 7.8.2.

An inverted binding `p <- f(a...)` is the call `f(a..., (p): rest)` (Clause
6.3.4). The bound pattern's type is the callback's parameter type, and the
admitted pattern forms are those of a parameter position (Clause 7.7.1). The
zero-argument form `<- f()` passes a zero-parameter callback. The call MUST
have a last callback parameter; `<-` is not applicable to a `Never`-returning
call without one.

An `extern` declaration is checked as a binding of its declared function type.
Its signature MUST satisfy Clause 10.2.3: every parameter and result type is
in the marshalling whitelist, the signature is monomorphic, and variadic
parameters are absent. The declaration's own arrow MUST be `!->` or `!`, or
`->` when it carries `Pure` (Clause 10.2.1). A callback parameter carries its
own bit. A `C.In T` parameter requires `T` to be a `ctype` record (Clause
10.3.4).

A violation of the whitelist, monomorphism, or variadic rule is `E-extern-type`.
A violation of the arrow rule is `E-extern-arrow` (Clause 10.2.3). A call to an
extern contributes the bit of its declared arrow (Clause 7.9.2). Signature
overloading does not exist; a callee name denotes one binding (Clause 7.4.6).

### 7.8.2 UFCS Typing

The identity `a.f(b, c) ≡ f(a, b, c)` is specified in Clause 6.5.2. This
clause specifies its typing.

Resolution selects `f` by the stations of Clause 7.4.3. The receiver supplies
the first argument, and the remaining arguments map left to right. The call is
then typed exactly as the direct call.

The receiver and the direct first argument occupy the same argument position;
shape, `Never`, and bit adaptation apply to the receiver as to any argument. A
qualified receiver call `a.Mod.f(b)` is identical to `Mod.f(a, b)`; module
qualification affects resolution only. An operator is the same call: `a + b`
is `Int.add(a, b)`, hence `a.add(b)` (Clause 7.8.3).

A receiver chain types each link independently. The receiver of a link is the
result of the previous link, and the static type fixes the namespace station
(Clause 7.4.3). A fully qualified name in argument position, as in
`.map(String.trim)`, is a name reference, not a call (Clause 6.5.3). A bare
`x.f` without a call is a field access (Clause 7.4.3). There is no dynamic
dispatch; the static type of the receiver selects the member (Clause 7.5.1). A
member call's result is the member's result type, and the first parameter is
not repeated in the argument list.

### 7.8.3 Operator Typing

The operator set is closed (Clause 6.10), with the precedence and
associativity of Clause 6.5.5.

An operator is notation for a type-namespace member (Clause 6.5.4) resolved in
the namespace of the operand type; overloading is namespace resolution, not a
second dispatch. Both operands MUST have the same type, and no implicit
conversion is applied.

| Operators | Operand type | Result |
|---|---|---|
| `+ - * / %`, unary `-` | numeric domain | same numeric type |
| `== !=` | value layer | `Bool` |
| `< <= > >=` | value layer with order | `Bool` |
| `&& \|\|` | `Bool` | `Bool` |
| `++` | list, `String`, `Bytes`, dictionary | same type |
| `.. ..=` | `Int`, `UInt`, `Char` | `Range t` |

`==` and `!=` exist on exactly the value layer (`Int`, `UInt`, `Float`, the
machine words, `Bool`, `Char`, `String`, `Bytes`), where `a == b` is `a.eq(b)`
and `eq` and `ne` are compiler-owned (Clause 9.8.4). Elsewhere equality is
`T.eq`, called as `x.eq(y)` or `T.eq(x, y)`, with `T.ne` as its negation.
`1 == 1.0` and `UInt` against `Int` are errors. A value whose static type is a
type variable MAY use `==` and `!=`; the operation is the derived `eq` selected
at instantiation (Clause 7.6.6).

`T.eq` is derived on demand: records and tuples compare field by field in
declaration order with short-circuit, ADTs compare constructor then payload
recursively, containers compare element by element, and every component
compares through its own `eq`. A `T.eq` declaration in the declaring module
replaces the derivation (Clause 7.4.5). Derivation halts at capabilities and
functions, recursively: a type with such a field has no `eq`, and equality,
keying, or assertion over it is `E-no-eq` (Clause 9.8.4). `Float.eq` follows
IEEE 754, so `NaN == NaN` is `False`.

A bare generic constructor has no self-anchoring equality; the instance is
fixed when one operand has a known type (Clause 7.6.4). A constructor of a
non-generic ADT is self-anchoring.

`<`, `<=`, `>`, and `>=` follow the equality shape on the value layer with an
order: the numeric types, the machine words, `Char` by code point, `String`
lexicographically, and `Bytes` by byte order. Operands MUST have the same type
and the result is `Bool`; `Bool` has no ordering. An ADT derives `T.lt` by
declaration order with recursive payloads; it backs no operator and is called
`a.lt(b)`. Records have no derived ordering: use `sort_by` with an explicit
comparator (Clause 9.10.1), and a hand-written `T.lt` is an ordinary member.

Chained comparisons are an error (Clause 6.5.5), `not` is a function, and `!`
is not a logical operator (Clause 6.5.4). Range endpoints MUST have the same
type, `Int`, `UInt`, or `Char`, and the result is `Range t` (Clause 6.5.6).

### 7.8.4 Arithmetic

Arithmetic comprises the binary operators `+`, `-`, `*`, `/`, `%` and unary
`-` (Clause 6.5.4). Operands are the domain numeric types `Int`, `UInt`, and
`Float`; the machine words have no arithmetic (Clause 10.3.1). Both operands
MUST have the same type, and the result has that type. Unary `-` follows the
operand type's namespace. No implicit numeric conversion is applied; `1 / 2.0`
is an error. A numeric literal is placed by the expected type (Clause 5.7.4),
and a value outside the target range is `E-literal-overflow` (Clause 5.7.1).

`/` and `%` truncate toward zero and are paired: `-7 / 2` is `-3`, and `-7 %
2` is `-1`. The floor family is `Int.div` and `Int.mod` (Annex C.8). `UInt`
values are non-negative, so the distinction vanishes. `Float` `%` uses the
same truncation basis.

Overflow and division by zero panic at run time (Clause 8.4.2); `Int.min / -1`
overflows. This is not a static error, and a panic sets no world bit (Clause
7.9.2). Wrapping arithmetic is the explicit `Int.wrapping_*` and
`UInt.wrapping_*` family (Clause 9.6.6). Bitwise operations are functions of
the `UInt` family (Clause 9.6.5). `Int.pow` and `Float.pow` are functions; no
exponentiation operator exists (Clause 6.10). Conversions between numeric
types are explicit and return `Option` where the target range is narrower
(Clause 9.6.2).

### 7.8.5 Logical Operators

`&&` and `||` are sugar for `if` (Clause 6.5.4). Both operands MUST have type
`Bool`, and the result is `Bool`. They are lazy: the right operand is
evaluated only when the result requires it (Clause 8.1.2). There is no
truthiness coercion; `True` and `False` are the only `Bool` constructors.

The world bit is transparent: the operands contribute their bits by OR to the
enclosing context (Clause 7.9.4). `not` is an ordinary function, written
`b.not()` or `Bool.not(b)`; prefix `!` is the world-bit marker, not negation.
`&&` and `||` associate to the left (Clause 6.5.5). `&` and `|` are not
logical operators; `|` in pattern position is the or-pattern separator (Clause
6.8.3). A condition of `if` and a guard MUST be `Bool` (Clause 6.7.4).

### 7.8.6 Concatenation

`++` is defined on `[]t`, `String`, `Bytes`, and `[k]v`; both operands MUST
have the same type, and the result has that type. For a list, both operands
MUST be lists with the same element type; `++` appends and does not wrap
elements, as in `prefix ++ {x} ++ suffix`. For a dictionary, both operands
MUST have the same key and value types; `++` merges, and the right operand
wins on a duplicate key (Clause 9.10.2). For `String` and `Bytes`, `++`
concatenates. Characters do not concatenate with strings: `'a' ++ "bc"` is an
error. Tuples and records do not support `++`; a partial record literal is
legal only with `with` (Clause 9.8.6).

`++` has the same precedence as `+` and `-` and associates to the left (Clause
6.5.5). No implicit conversion of elements or keys is applied.
