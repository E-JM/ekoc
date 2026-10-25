# Annex E. Design Rationale

## E.1 Purpose

Annex E is informative. It records the reasons for the choices stated in the
normative clauses: the goals, a worked example, the design record index, the
non-goals, and the prior art. It states no requirements. Where this annex and
a normative clause differ, the clause prevails.

The normative clauses state what Eko is. The design record states why,
including the rejected alternatives. This annex is the short form: it
identifies the goals, reproduces the complete program, and maps each record to
the clauses it supports. The records remain the evidence; this annex is the
route into them.

Design rationale is not a source of conformance. A behavior is required only
when a normative clause states it. The reasons below explain the shape of the
rules; they do not extend them.

## E.2 Design Goals

The design record states three goals: visible cost, one mechanism per
problem, and uniform rules. The table decomposes them into properties of the
specification and identifies the clauses that express each property.

| Goal | Property | Clauses |
|---|---|---|
| Visible authority | Authority for world contact appears in `main`'s parameter list; world contact appears on arrows; there is no ambient I/O. | 7.1.3, 7.9.1, 7.9.2 |
| Visible cost | The cost model is stated rather than inferred; indexing syntax is absent; operations with non-constant cost are named where the surface permits. | 6.10, 8.10 |
| One mechanism per problem | `case` is the only branching primitive; `if` is sugar; operators are namespace functions; a module is a file; a test uses the annotation slot. | 6.5.2, 6.7.1, 7.2.1, 9.14.1 |
| Uniform rules across positions | `_` has one family of meanings by position; markers occupy slots; composite literals are resolved by expected type; failure has three words. | 4.8, 6.3.1, 6.6.2, 7.5.5, 9.2 |
| Compile-time resolution | Type identity, generics, and world bits are resolved before execution; there is no runtime type dictionary and no virtual machine. | 7.5, 7.6, 8.7 |
| Deterministic execution | Immutable values, strict evaluation, an acyclic heap, and reference counting; concurrency adds no observable scheduling. | 8.1, 8.6, 8.9 |
| Small trusted base | Safe Eko has no undefined behavior; native code is the only source, behind `extern`, `Pure`, `C.In`, and `ctype`. | 2.6, 10.1 |
| Library surface, not syntax | The library adds functions and types, not grammar; the free-name set is closed. | 9.1, 9.15 |
| Fixed layout | One indentation width is accepted; a raw tab is rejected; layout is not configurable. | 5.2 |

The goals are not independent. Visible authority costs parameter-list
verbosity: every capability a program uses appears in `main`'s signature.
Compile-time resolution costs code size: each instantiation is compiled
separately (Clause 8.7.1). Deterministic concurrency excludes channels and
timeouts (Clause 8.9.4). The design record preserves these costs; the
normative clauses state the resulting rules. The table is not a priority
order.

## E.3 Worked Example

The program below is the complete running example. It reads a CSV file, parses
each row into `Person`, and prints the rows with line numbers. The example is
informative; the clauses, not the example, define its behavior. `main` carries
the elided world-touching arrow `!`, defined in Clause 6.9.3.

```eko
Sex:    type = Man | Woman
Person: type = [name: String, age: UInt, sex: Sex, address: String]

parse_sex: (data: String) -> Result Sex String =
    case data:
        "man"   -> Ok Man
        "woman" -> Ok Woman
        _       -> Err "unknown sex: {data}"

make_person: (data: []String) -> Result Person String =
    name    <- data.get(0).to_result("short row: missing name").try()
    age_s   <- data.get(1).to_result("short row: missing age").try()
    age     <- UInt.parse(age_s).map_err((e): "age: {e}").try()
    sex_s   <- data.get(2).to_result("short row: missing sex").try()
    sex     <- parse_sex(sex_s).try()
    address <- data.get(3).to_result("short row: missing address").try()
    Ok {name: name, age: age, sex: sex, address: address}

main: (out: Out, fs: Fs) ! =
    case fs.with_file("data.csv", Read, (file):
        content <- file.read(4096).map_err((e): "io: {e}").try()
        persons <- content.lines()
            .traverse((l): l.split(",").make_person())
            .try()
        Ok persons.each_with_index((p, i): out.println("{i}: {p}"))):
        Ok inner -> case inner:
            Ok _  -> _
            Err e -> out.println(e)
        Err e -> out.println(e)
```

The table identifies the clause that governs each element. It is a reading
guide, not a complete derivation.

| Element | Clauses | Reading |
|---|---|---|
| `Sex`, `Person` | 6.3.5, 7.5.2 | Two nominal declarations: a variant list and a record. |
| `parse_sex` | 6.7.1, 6.8.5, 9.2.1 | A `case` over string literals; the wildcard arm is the only catch-all. |
| `make_person` | 6.3.4, 8.2.4, 9.2.1 | Each `<-` inserts the rest of the block as the trailing callback of `.try()`; `map_err` changes the error type once. |
| `main` parameters | 7.1.3, 7.9.1, 7.9.2 | The parameter list is the authority manifest; `!` records world contact. |
| `Fs.with_file` | 9.12.4 | The call closes the file on both paths and returns the callback's result as the success payload. |
| `case` over `with_file` | 6.7.1, 9.12.4 | The outer `case` handles open and close errors; the callback returns a `Result` of its own, so the success payload is a second `Result` and is matched by the inner `case`. |
| `.traverse`, `.try`, `.map_err` | 9.2.1 | Result combinators preserve the first `Err` and convert its type at the boundary. |
| `get` | 6.10, 9.10.1, 9.2.2 | Access has no index syntax; it returns `Option`; the consumption site chooses the failure. |
| `lines`, `split`, `UInt.parse` | 9.11.1, 9.6.3 | Line splitting drops the final empty element; field splitting and checked parsing turn a bad row into `Err`. |
| `"{i}: {p}"` | 5.9.3, 9.8.1 | Interpolation uses `show`; `Person`'s record form is derived. |
| `out.println` | 7.9.2 | The call touches the world; its arrow carries `!`. |

The example has two flow shapes. `case` encloses `fs.with_file` because the
call's error value exists only after the callback returns; the `case` consumes
a value flowing outward. `<-` flattens the parse steps because each step's
value flows forward into the rest of the block. The distinction is semantic:
`Fs.with_file` returns `Result a IoError` with the callback's result as `a`
(Clause 9.12.4), and `<-` passes the rest of the block as the trailing
callback (Clause 6.3.4). Neither shape is a stylistic alternative to the
other; the position of the value selects it.


## E.4 Archive Index

The design record is the set of non-normative files in `archive/`. Each design
record states the clauses it supports. The table restates that mapping and
identifies the clauses in this document that carry the record's rulings. The
clause column is the normative owner; the record is evidence, not a rule.

| Record | Covers | Subject |
|---|---|---|
| `archive/README.md` | 1.3 | directory index; status of the records; reading guide |
| `archive/syntax.md` | 5, 6 | layout, operators, literals, comments |
| `archive/control.md` | 6.3, 6.7, 6.8, 7.7, 8.2, 8.3, 8.8 | bindings, lambdas, branching, patterns, `Never`, iteration |
| `archive/types.md` | 7.5, 7.6 | declarations, nominality, composites, generics |
| `archive/names.md` | 7.2, 7.3, 7.4 | UFCS, access, modules, visibility, `std` |
| `archive/world.md` | 7.9, 8.4, 8.9, 9.12 | capabilities, world bit, failure, handles, concurrency |
| `archive/runtime.md` | 8.5, 8.6, 8.7, 8.8, 8.10 | representation, acyclicity, memory, monomorphization |
| `archive/builtins.md` | 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9 | text, conversions, services, JSON, bytes, time |
| `archive/library.md` | 9.10, 9.11, 9.12, 9.13, 9.14, 9.15, Annex B | `std` surface, testing, diagnostics |
| `archive/ffi.md` | 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9 | `extern`, `Pure`, `C.*`, `ctype`, veneer, the capability channel |
| `archive/stdlib.md` | 7.2.2, 7.4.5, 9.1, 9.8.4, 9.8.7, 9.15, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6 | library name and delivery, implicit import, operators and equality, derived services, toolchain |
| `archive/toolchain.md` | Clauses 11, Annex D | implementation language (C base), backend legs (cc + libtcc), pipeline architecture, self-hosting roadmap |

Each record has three kinds of content: rejected alternatives, prior art, and
rationale. The rejected alternatives are the load-bearing content: the
normative clauses state what is; the record preserves why the alternatives are
not. A record may support several clauses; a clause may be supported by
several records.

The directory index is `archive/README.md`; it states the non-normative status
of the records and the reading order. It is not itself a design record.

## E.5 Non-Goals

A non-goal is a property the language deliberately does not have. Each entry
names the boundary and the mechanism that replaces the absent feature. The
table is a summary; the clause states the rule.

| Non-goal | Consequence | Clauses |
|---|---|---|
| No virtual machine or runtime type information | Compilation resolves type identity, generics, and bits; generics are expanded per instantiation. | 8.7 |
| No mutable state | Values are immutable; bindings are initial, rebinding, or inverted; there is no field assignment. | 6.3, 8.2 |
| No loop syntax | Iteration is a combinator or a recursion; self tail calls are guaranteed. | 8.8 |
| No exception system | Failure is `Result`, `Option`, or `panic`; `panic` aborts. | 8.4, 9.2 |
| No label rows or handlers | One world bit per arrow; capabilities carry authority. | 7.9 |
| No user-defined capability types | The capability set is closed; domain authority uses closure records. | 7.9.1, 10.1 |
| No implicit numeric conversion or saturation | Conversions are named and checked; failure is `Option`. | 9.6.2 |
| No bignum in the language | `Int` and `UInt` are 64-bit; overflow is a panic. | 7.8.4, 9.6.1 |
| No index syntax | Access is `get` or `at` and returns `Option`; out-of-bounds is a domain check. | 6.10, 9.10.1 |
| No type aliases | A `type` declaration introduces a nominal identity. | 6.3.5, 7.5.2 |
| No user-defined operators or ad-hoc overloading | The operator set is closed; operators are namespace functions on the value layer. | 6.5.4, 6.5.5 |
| No module functors, module values, re-export, or cyclic imports | A file is a module; imports form a DAG; forwarding is a function. | 7.2 |
| No format-string DSL | Formatting is named functions; interpolation uses `show`. | 9.8.1, 9.13.1 |
| No JSON override | The derived `to_json` and `from_json` pair is fixed. | 9.9.3 |
| No tracing or cycle-collecting garbage collector | Reference counting is complete under acyclicity. | 8.6 |
| No observable representation | Identity comparison, address-of, finalizers, and memory queries are absent; `ctype` is the scoped exception. | 8.11, 10.3.3 |
| No async/await, channels, or timeouts | Concurrency is structured; tasks return values. | 8.9 |
| No macro system, attributes, or decorators | Declarations use the annotation slot; `test` is a slot word. | 6.3, 9.14.1 |
| No warning suppression | The warning list is closed; a warning is fixed by changing the program. | Annex B.4 |
| No unsafe block | Native code is the only source of undefined behavior; `extern` is the boundary. | 2.6, 10.1 |
| No cross-function tail-call guarantee | Self tail calls are guaranteed; other tail calls are best-effort. | 8.8.4 |
| No general-purpose test framework | The `test` slot, assertions, and derived capabilities cover testing. | 9.14 |

Several non-goals are consequences of one decision. The absence of mutation
and a cycle collector follows from immutable values and strict evaluation
(Clauses 8.1, 8.6). The absence of loop syntax follows from immutable values
and the combinator vocabulary (Clause 8.8). The absence of label rows and
user-defined capabilities follows from the capability model (Clause 7.9). The absence of a
format DSL, JSON overrides, and a test framework follows from the library
principle (Clauses 9.1, 9.15). The table lists the boundaries; the records
state the arguments.

## E.6 Prior Art

Eko selects mechanisms from existing languages and rejects others. The table
summarizes the comparisons recorded in the design record. It is not a
bibliography; Annex G lists the works.

| Tradition or source | Mechanism compared | Eko position | Clauses |
|---|---|---|---|
| ML-family variants and pattern matching | Nominal variants, exhaustive `case`, guards | Adopted; `case` is the only branching primitive | 6.7, 7.7 |
| Constraint-directed overloading | Type-class-style constraints on generic functions | Rejected; UFCS and namespace members; no constraint syntax | 6.5.2, 7.4.3 |
| `IO` monads and handler-based I/O | I/O tracking in the type | Rejected; capabilities plus one world bit | 7.9 |
| Ownership and borrowing | Lifetime-annotated signatures and move semantics | Rejected; immutable values, O(1) sharing, reference counting | 8.5, 8.6 |
| Saturating numeric casts | Silent clamp of out-of-range conversions | Rejected; checked conversions return `Option` | 9.6.2 |
| Variable-width characters | Grapheme-cluster character type | Rejected; `Char` is one Unicode scalar | 9.4.1 |
| Dual index access | `[]` panics, `.get` returns `Option` | Rejected; `get` and `at` only | 6.10 |
| Function-only access | Access as a function, branch at consumption | Followed | 6.10 |
| Module functors and signatures | Parameterized modules | Rejected; generics and file modules | 7.2, 7.6 |
| Single overridable display channel | One `toString`-style rendering | Rejected; `show` is overridable and `inspect` is structural | 9.8.1, 9.8.2, 9.8.3 |
| Bytes as a list of words | `[Word8]`-style byte list | Rejected; `Bytes` is contiguous, `[]t` keeps list semantics | 9.5.1 |
| C ABI and pinned layout | Native calling convention and struct layout | Adopted at `extern`; `ctype` is the explicit layout exception | 10.2, 10.3.3 |
| Channel concurrency | Message passing and observable scheduling | Rejected; tasks return values; no channels | 8.9 |
| JSON text syntax | RFC 8259 syntax and escaping | Adopted; external tagging and fixed rendering | 9.9 |
| IEEE 754 floating point | `NaN`, rounding, special values | Adopted; conversions are checked at the boundary | 9.6, 9.13 |
| Unicode scalar values | Character properties and identifiers | Adopted; `Char` is a scalar, `String` is UTF-8 | 5.5, 9.4 |
| ISO 8601 time display | Calendar and time representations | Adopted for `Time`; UTC only | 9.7.4 |
| Source distribution with a derived cache | Source plus a per-machine cache | Followed for `std` | 9.1, 11.2 |

The pattern is selective adoption at boundaries. A mechanism is adopted when
it removes a second vocabulary or a runtime negotiation. It is rejected when
it makes a rule conditional, hides a cost, or requires a runtime component the
language does not have. The table records decisions, not novelty.
