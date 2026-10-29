# 9. Standard Library

## 9.8 Derived Services

### 9.8.1 show and inspect

| Channel | Name | Semantics | Overridable |
|---|---|---|---|
| display | `show` | human-readable rendering | yes, by a namespace declaration |
| structural | `inspect` | faithful literal form | no; always derived |

`show` is the display channel used by string interpolation and logging.
`inspect` is the structural channel used by diagnostics, tests, and debugging.
Both are available as free functions and as type-namespace members: `show(t)`,
`t.show()`, `Money.show(m)`, `inspect(t)`, `t.inspect()`. The free-call form
is the identity of Clause 6.5.2 — `show(t)` is `t.show()` at station 2 (Clause
7.4.3) — and neither name is opened (Clause 9.1.2).

### 9.8.2 Default show and Overrides

The default `show` is `inspect` with the top-level `String` quotes removed. A
`String` nested inside a composite keeps its quotes. Interpolation uses
`show`: `"{name}"` produces the bare name. A type's declaring module MAY
override `show` with an ordinary namespace declaration. The override adds no
syntax and follows namespace ownership (Clause 7.4.5). An override outside the
declaring module is an error (`E-namespace-owner`, Annex B); orphan overrides
do not exist. A type without an override takes the default. The compiler
supplies `Range.show`, which is the operator form (Clause 9.10.5).

```eko
Money: type = [cents: Int, cur: String]

Money.show: (m: Money) -> String = "{m.cur} {m.cents}"

m: Money = {cents: 12500, cur: "USD"}
"{m}"          // USD 12500
inspect(m)     // {cents: 12500, cur: "USD"}
```

### 9.8.3 inspect Forms

| Type | `inspect` |
|---|---|
| scalars | `3`, `0.5`, `NaN`, `True`, `'a'` |
| `String`, `Bytes` | `"wd"`, `Bytes.from_hex("4142")` |
| list, tuple, dictionary, record | `{1, 2, 3}`, `{"1": 1}`, `{name: "wd"}` |
| ADT | constructor application: `Man`, `Some 3`, `Err "x"` |
| `Range` | the operator forms: `0..9`, `0..=9` |
| `_`, capability, function | `_`, `<cap>`, `<fn>` |

Empty containers are `{}`. Record fields appear in declaration order.
Dictionaries appear in insertion order, and iteration follows that order. The
time types are specified in Clause 9.7.4.

### 9.8.4 Equality

`==` and `!=` are defined on exactly the value layer: `Int`, `UInt`, `Float`,
the machine words, `Bool`, `Char`, `String`, and `Bytes`. On this domain `a ==
b` is identical to `a.eq(b)`, and the `eq` and `ne` names are compiler-owned.
For every other type, equality is the member `T.eq`: records, tuples, ADTs,
`Option`, `Result`, the time types, `Range`, `Set`, `Dict`, `List`, and
`Json`. `T.ne` is the negation of `T.eq`. Both operands MUST have the same
type; a comparison between different types is a compile error. `T.eq` is
derived structurally and materialized on demand, except where the compiler
supplies the row. The compiler supplies `Range.eq`, which compares the
endpoints and the closed flag (Clause 9.10.5). Records and tuples compare
field by field in declaration order with short-circuit. ADTs compare when
constructors match and payloads compare recursively. Containers compare
element by element. Each field or payload compares through that type's `eq`
member, so derivation composes at every level. A declaration `T.eq:` in the
module that declares `T` replaces the derivation entirely (Clause 7.4.5).
Derivation halts at capabilities, handles, and functions, recursively: a type
with a capability, handle, or function field has no `eq`, and equality,
keying, or assertion over it is a compile error (`E-no-eq`, Annex B).
`Float.eq` follows IEEE 754: `NaN == NaN` is `False`. This rule also governs
`Float` dictionary keys and `Set` elements (Clause 9.10.4).

### 9.8.5 Ordering

`<`, `<=`, `>`, and `>=` follow the same shape as equality. They are defined
on the value layer with an arithmetic or textual order: the numeric types and
machine words, `Char` by code point, `String` lexicographically, `Bytes` by
byte order. They are compiler-owned members. `Range` carries ordering too:
`Range.lt` and its family (`le`, `gt`, `ge`) are compiler-supplied and compare
lexicographically over the endpoints, the closed flag last (Clause 9.10.5).
`Bool` has no ordering; `True` and `False` are not ordered. Partition with
`filter`. An ADT derives `T.lt` by declaration order, comparing payloads
recursively in order. `Sex: type = Man | Woman` implies `Man.lt(Woman)` is
`True`. The derived member backs no operator; the call is `a.lt(b)`. Records
have no derived ordering. A field set has no magnitude semantics; sorting a
record collection requires `sort_by` and an explicit comparator (Clause
9.10.1). A hand-written `T.lt` is an ordinary member and is called explicitly.

### 9.8.6 with

`with` is record construction from a base: `r.with({...})` constructs a new
record of the receiver's type. The literal supplies the named fields; every
field it does not name takes the receiver's value — the counterpart of a
struct literal with base completion, worn in member-call form. It is a
construction primitive of the language, not a function; the underlying
construction is beneath the surface. The original value is unchanged; there is
no mutation. The partial field literal is legal only in its argument position
(Clause 6.6.3), is checked layer by layer against the field types, and its
permission follows the record field chain only (Clause 6.6.3); a misspelling
is an error.

The second parameter type of `T.with`, `(t, partial fields) -> t`, is not
expressible in the type syntax; the typing of the form is a localized
exception. `with` is the record update vocabulary; `insert` and `remove`
belong to dictionaries, and `append` and `merge` belong to the body of `++`.

### 9.8.7 Dispatch

`show`, `inspect`, `eq`, `lt`, `with`, `to_json`, and `from_json` are
compile-time members of every type namespace: the declaration where the type
declares one, the derived default otherwise. World bits do not participate;
every one of these services is pure. The free function `show(t)` and the
member `t.show()` denote the same call, and `inspect(t)` and `t.inspect()`
likewise. On the value layer `a == b` and `a.eq(b)` coincide, and
interpolation applies `show`. `to_json` and `from_json` follow the same
dispatch model and admit no override (Clause 9.9.5). Dispatch is compile-time
and uses no runtime reflection (Clause 8.7.2).

## 9.9 JSON

### 9.9.1 The Json Type

```eko
Json: type = Null | Bool Bool | Int Int | Float Float | Str String | Arr []Json | Obj ([String]Json)
```

`Json` is a standard-library ADT with seven variants. `Obj` is a string-keyed
dictionary that preserves insertion order; `Json.show` follows that order, so
JSON text is reproducible. Construction is check-only: constructors are
applied by juxtaposition, and literals land by expected type (`Obj {"a": Int
1}`). `Int` and `Float` are separate variants. An integer literal remains
`Int` on a round trip; the variant, not the surface spelling, decides the
interpretation.

### 9.9.2 Parsing and Showing

```eko
Json.parse: (String) -> Result Json ParseError
Json.show:  (Json) -> String
```

`Json.parse` is input validation and reuses `ParseError` (Clauses 9.2 and
9.6.3). Its payload identifies the offset and context of the first error.
`Json.show` produces the minimal form with no whitespace. It is a
standard-library override of `show`; interpolation `"{j}"` produces JSON text
directly. `inspect(j)` remains the constructor form (Clause 9.8.3). A `Float`
uses the shortest round trip: `3.5` prints `3.5` and `3.0` prints `3.0`. An
`Int` prints without a fractional part. String escapes follow RFC 8259,
including `\uXXXX`. Surrogate pairs are parsed as pairs; a lone surrogate is
rejected. Every `String` is valid UTF-8. `parse(show(j)).eq(Ok j)` holds.
Non-finite floats render as `NaN`, `Inf`, and `-Inf`, following the `Float`
display vocabulary, and `Json.parse` accepts those tokens back. These
spellings are an Eko-side extension of strict JSON text; conversion for strict
external consumers is a library concern.

### 9.9.3 Derived Mapping

Every data type derives `to_json: (t) -> Json` and `from_json: (Json) ->
Result t ParseError` by default.

| Eko type | JSON form |
|---|---|
| record, string-keyed dictionary | `Obj` with field names as keys |
| list, tuple, `Set` | `Arr` in iteration order |
| `String`, `Char`, `Bytes` | `Str`; `Bytes` in lowercase hexadecimal |
| `Int`, `Float`, `Bool`, `_` | number, `true` or `false`, `null` |
| ADT | external tagging (Clause 9.9.4) |
| non-string-keyed dictionary, capability, function | no default; compile error (`E-json-shape`) |

Every value whose type participates satisfies `from_json(to_json(v)).eq(Ok
v)`. A missing field, an extra key, an unknown variant, or a payload type
mismatch yields `Err`. Capabilities and functions have no equality (Clause
9.8.4) and do not enter the mapping. `Bool` is special-cased to JSON booleans.
A user-declared enumeration still uses external tagging as strings; a declared
type does not inherit the `Bool` mapping. `Bytes` maps to a lowercase hex
string, the same basis as `to_hex` and `from_hex` (Clause 9.5.3); the field
type determines decoding.

### 9.9.4 External Tagging

An ADT maps by external tagging. A zero-payload variant maps to a JSON string.
A payload-bearing variant maps to a single-key object: a single payload is
embedded directly, and multiple payloads are an array under the key (for
example `Ok 3` maps to `{"Ok": 3}`).

`from_json` reports an unknown variant or key, a missing field, or a payload
type mismatch as `Err`, with an `Invalid` payload naming the unknown variant
where applicable. Constructor fields are checked at compile time, so a
misspelling is a compile error (Clause 6.6).

### 9.9.5 Derivation and Generics

The derived pair is fixed. `to_json` and `from_json` are derived for every
suitable type and admit no override. The pair is an involution:
`from_json(to_json(x))` yields `x`. A representation choice such as field
renaming, omission, or versioning is a protocol decision. It is written as a
named codec pair in the protocol's module; it is not an override of the
derived pair. `to_json` and `from_json` join the type-variable capability set
(Clause 7.6.6). A generic function MAY call them on a value of type `t`; at
instantiation the concrete derivation or declaration applies. A capability, a
function, or a non-string-keyed dictionary at an instantiation site is an
error, by the same rule as `eq`. Dispatch is compile-time generic dispatch
(Clause 9.8.7). There is no runtime type information and no reflection
surface. Pretty printing, path queries, and format-string DSLs are outside the
standard library. YAML and TOML are outside the standard library; JSON is the
only built-in serialization surface.
