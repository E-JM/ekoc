# 4. Terms and Definitions

For the purposes of this document, the following terms and definitions apply.
The clause column identifies the normative clause that gives the term its
operative rules.

## 4.1 General terms

| Term | Definition | Clause |
|---|---|---|
| **conforming implementation** | an implementation that satisfies every requirement of this document | 2.1 |
| **conforming program** | a program that satisfies every static rule and whose execution does not invoke undefined behavior | 2.2 |
| **diagnostic** | a compile-time report of a condition in a program, with severity error or warning | 2.4 |
| **error** | a diagnostic that rejects a program | 2.4 |
| **warning** | a diagnostic that does not reject a program and does not change its meaning | 2.4 |
| **implementation-defined** | a behavior that the implementation chooses and MUST document | 2.5 |
| **unspecified** | a behavior for which any conforming choice is permitted and need not be documented | 2.5 |
| **undefined behavior** | behavior for which this document places no requirement on the implementation | 2.6 |
| **source text** | the UTF-8 character sequence contained in one `.eko` file | 5.1 |
| **compilation unit** | one source file together with the modules it imports, transitively | 7.1.1 |

## 4.2 Lexical and syntactic terms

| Term | Definition | Clause |
|---|---|---|
| **token** | the output of the lexical grammar: an identifier, keyword, literal, operator, punctuation mark, or layout boundary | 5.3.1 |
| **identifier** | a name that begins with an XID_Start character or `_` and continues with XID_Continue characters | 5.5.1 |
| **keyword** | one of the reserved words `import`, `case`, `if`, `else` | 5.6.1 |
| **contextual word** | a word recognized only in the annotation slot: `type`, `ctype`, `test` | 5.6.2 |
| **literal** | a numeric, character, or string token that denotes a value | 5.7–5.9 |
| **block** | the sequence of members introduced by a header line and indented one level | 6.2.2 |
| **member** | one line, or one semicolon-terminated unit, within a block | 6.2.2 |
| **header** | the line that introduces a block | 6.2.3 |
| **binding** | a construct that associates a pattern with a value, a type, or a test | 6.3.1 |
| **declaration** | a binding in the annotation slot that introduces a type (`type`), a layout record (`ctype`), or a test (`test`) | 6.3.1 |
| **annotation slot** | the position after `:` in a binding, occupied by a type expression or a declaration word | 6.3.1 |
| **lambda** | an anonymous function written as a parameter group, `:`, and a body | 6.4.1 |
| **pattern** | a construct that matches a value and binds names | 6.8.1 |
| **scrutinee** | the value matched by a `case` expression | 6.7.1 |
| **arm** | a pattern, an optional guard, `->`, and an expression within a `case` | 6.7.1 |
| **guard** | a `Bool` expression attached to a `case` arm | 6.7.3 |
| **type expression** | a syntactic type, as defined by the type grammar | 6.9 |
| **arrow annotation** | the `->`, `!->`, or `?->` in a function type, optionally with an elided result | 6.9.3 |

## 4.3 Names and modules

| Term | Definition | Clause |
|---|---|---|
| **module** | a compile-time namespace, equal to one source file | 7.2.1 |
| **entry module** | the module that contains the `main` binding | 7.1.3 |
| **project root** | the directory against which import paths are resolved | 7.2.2 |
| **import** | a declaration that adds a module to the dependency graph and opens its types and constructors | 7.2.3 |
| **dependency graph** | the directed graph of modules connected by imports | 7.2.4 |
| **interface** | the exported types, signatures, and visibility of a module | 7.3.2 |
| **public** | visible outside the declaring module | 7.3.1 |
| **private** | visible only inside the declaring module; spelled with a leading `_` | 7.3.1 |
| **free function** | a function declared at module top level, not attached to a type | 7.1.2 |
| **receiver** | the first argument of a call written in member form | 7.4.3 |
| **uniform function call syntax (UFCS)** | the identity `a.f(b) ≡ f(a, b)` | 6.5.2, 7.4.3 |
| **type namespace** | the namespace attached to a type, holding its members | 7.4.5 |
| **member** | a function in a type namespace, written `T.f` or `x.f()` | 7.4.3 |
| **resolution station** | one of the ordered steps by which `x.f` is resolved | 7.4.3 |

## 4.4 Types

| Term | Definition | Clause |
|---|---|---|
| **type** | a compile-time identity that classifies values | 7.5.1 |
| **value** | a runtime entity of some type | 8.5 |
| **nominal type** | a type introduced by a `type` or `ctype` declaration; identity is the declaration | 7.5.2 |
| **anonymous type** | a composite type written structurally, without a declaration | 7.5.3 |
| **composite type** | a list, dictionary, tuple, or record type | 7.5.3 |
| **record** | a composite with named fields | 7.5.3 |
| **tuple** | a composite with two or more positional components | 7.5.3 |
| **list** | a homogeneous sequence, written `[]t` | 7.5.3 |
| **dictionary** | a key-value map, written `[k]v` | 7.5.3 |
| **algebraic data type (ADT)** | a nominal type declared as a list of variants | 7.5.2 |
| **variant** | one alternative of an ADT | 7.5.2 |
| **constructor** | the value-level name of a variant | 7.5.2 |
| **payload** | the value carried by a variant | 7.5.2 |
| **single-variant type** | an ADT with one constructor and one payload; the newtype form | 7.5.2 |
| **type variable** | a lowercase identifier that stands for a type in a generic declaration or annotation | 7.6.1 |
| **binder** | a type variable listed after `type` in a generic type declaration | 7.6.2 |
| **generic declaration** | a type or function declaration with type variables | 7.6.1 |
| **generalization** | the binding of type variables at an annotated definition | 7.6.3 |
| **instantiation** | the replacement of type variables by concrete types at a use site | 7.6.4 |
| **monomorphization** | compile-time expansion of a generic declaration for each instantiation | 8.7.1 |
| **same shape** | exact structural equality of an anonymous composite with the representation of a nominal type | 7.5.5 |
| **shape adaptation** | one-directional flow of a nominal value into a same-shape anonymous type | 7.5.5 |
| **bottom type** | `Never`, the type with no values | 7.5.4 |
| **unit type** | `_`, the type with exactly one value | 7.5.4 |
| **exhaustiveness** | the property that a `case` covers every constructor or ends in a wildcard | 7.7.2 |

## 4.5 Capabilities and failure

| Term | Definition | Clause |
|---|---|---|
| **world** | everything outside an Eko program: the console, file system, environment, arguments, clock, entropy, process control, and native code | 7.9 |
| **world contact** | an observation or change of the world by a call | 7.9.2 |
| **capability** | an opaque, unforgeable, copyable value that grants authority for a class of world contact | 7.9.1 |
| **birth-granted capability** | a capability supplied by the host at startup: `Out`, `In`, `Fs`, `Env`, `Args` | 7.9.1 |
| **opt-in capability** | a capability requested explicitly: `Clock`, `Rand`, `Proc`, `Conc` | 7.9.1 |
| **derived capability** | a pure combinator that constructs a restricted capability from an existing one | 7.9.1, 9.12.5 |
| **host** | the environment that starts a program and constructs capabilities | 7.9.1 |
| **capability module** | the compiler-embedded Eko module that declares the capability namespaces and the types of their signatures; part of the language definition, with no import path | 9.1.1, 10.8 |
| **world bit** | the marker on an arrow recording whether calls through it may touch the world | 7.9.2 |
| **world-touching** | an arrow or call that may touch the world; marked `!` | 7.9.2 |
| **pure** | an arrow or call that does not touch the world; marked by the absence of `!` | 7.9.2 |
| **open bit** | a bit parameter, marked `?`, instantiated from callback arguments at the call site | 7.9.6 |
| **dispatcher** | a function with an open bit whose own bit is determined by its callbacks | 7.9.6 |
| **bit adaptation** | one-directional flow of a pure function value into a world-touching function position | 7.9.5 |
| **handle** | an opaque value that designates an open resource and carries a generation | 9.12.3 |
| **generation** | a counter that invalidates older copies of a handle when the resource is closed | 9.12.3 |
| **failure taxonomy** | the three-way classification of failure into `Result`, `Option`, and `panic` | 9.2 |
| **panic** | an invariant-violation failure that aborts the process | 8.4.2 |
| **abort** | termination without unwinding, used for panic, stack overflow, memory exhaustion, and native faults | 8.4.3 |

## 4.6 Evaluation and runtime

| Term | Definition | Clause |
|---|---|---|
| **strict evaluation** | evaluation in which a value is constructed only after all of its subvalues are constructed | 8.1.1 |
| **heap object** | an immutable composite value stored on the heap and reached by reference | 8.5.2 |
| **scalar** | a value stored directly: `Bool`, `Char`, `Int`, `UInt`, `Float`, or a machine word | 8.5.1 |
| **capture** | the binding of an enclosing name's value into a closure | 8.2.3 |
| **closure** | a function value together with the values it captured | 8.2.3 |
| **acyclicity** | the property that the heap graph is a directed acyclic graph | 8.6.1 |
| **reference counting** | the memory strategy that reclaims an object when its reference count reaches zero | 8.6.2 |
| **tail position** | a position whose value is the value of the enclosing function or lambda | 8.8.2 |
| **self tail call** | a call to the enclosing function in tail position | 8.8.3 |
| **task** | a unit of concurrent execution created by `Conc.spawn` | 8.9.1 |
| **scope** | the dynamic extent within which spawned tasks run | 8.9.1 |
| **cancellation** | the cooperative termination of a task by a parent's `cancel` or by scope exit | 8.9.2 |

## 4.7 Library and native interface

| Term | Definition | Clause |
|---|---|---|
| **standard library** | the module `std` and its companion modules, delivered as source with the compiler | 9.1.1 |
| **std** | the standard library module, implicitly imported into every file | 9.1.2 |
| **companion module** | a module under the reserved `std/` path, imported explicitly | 9.1.3 |
| **derived service** | a compiler-provided per-type service: `show`, `inspect`, `eq`, `lt`, `with`, `to_json`, `from_json` | 9.8.1 |
| **display channel** | `show`, the overridable human-readable rendering | 9.8.1 |
| **structural channel** | `inspect`, the non-overridable literal rendering | 9.8.1 |
| **value layer** | the types on which operators are defined: numbers, machine words, `Bool`, `Char`, `String`, `Bytes` | 9.8.4 |
| **key domain** | the types that have structural equality and may be dictionary keys | 9.10.4 |
| **test** | a top-level binding in the `test` slot, executed by a test runner | 9.14.1 |
| **test root** | a module containing at least one `test` binding | 9.14.6 |
| **extern declaration** | a binding whose right-hand side names a native symbol | 10.2.1 |
| **Pure** | the audited guarantee that an extern does not touch the world and is reentrant and thread-safe | 10.2.2 |
| **ctype** | a declaration of a record whose layout is pinned to a C struct | 10.3.3 |
| **C.In** | an audited read-borrow parameter type | 10.3.4 |
| **marshalling whitelist** | the types permitted in an extern signature | 10.3.5 |
| **veneer** | the embedding ABI: normative function signatures, opaque layouts | 10.7.1 |
| **reserved symbol** | a native symbol beginning with `eko_`, owned by the host | 10.6 |

## 4.8 Symbols

| Symbol | Meaning | Clause |
|---|---|---|
| `_` | unit type, unit value, wildcard, discard, or private-name prefix, according to position | 7.5.4, 7.3.1 |
| `!` | world-touching marker on an arrow | 7.9.3 |
| `?` | open-bit marker on an arrow | 7.9.3 |
| `->` | pure function arrow | 7.9.3 |
| `!->` | world-touching function arrow | 7.9.3 |
| `?->` | open function arrow | 7.9.3 |
| `=` | initial binding | 6.3.2 |
| `:=` | rebinding | 6.3.3 |
| `<-` | inverted binding | 6.3.4 |
| `->` (in an arm) | arm arrow, separating pattern and result | 6.7.1 |
| `\|` | variant separator or or-pattern separator, according to position | 6.3.5, 6.8.3 |
| `++` | concatenation or dictionary merge | 6.5.4, 7.8.6 |
| `..`, `..=` | range construction, end-exclusive and end-inclusive | 6.5.6 |
