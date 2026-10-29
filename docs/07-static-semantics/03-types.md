# 7. Static Semantics

## 7.5 Types

A type is a compile-time identity that classifies values (Clause 4.4). Types
exist only at compile time; no expression denotes a type and no test inspects
one (Clause 8.11).

### 7.5.1 Type Identity

A declared type is nominal: its identity is its declaration, identified by the
declaring module and the declared name. Two declarations are different types
even when their names and right-hand sides coincide.

An anonymous composite type is identified structurally: a list by its element
type, a dictionary by its key and value types, a tuple by its components in
order, and a record by its field names and field types.

A function type is identified by its parameter types, its result type, and its
arrow form. `(a) -> b`, `(a) !-> b`, and `(a) ?-> b` are three different
types. The world bit is part of identity and is erased at run time (Clause
7.9.3).

A type variable is identified by its binder (Clause 7.6.1). An instantiated
type is identified by its declaration and its type arguments, compared
componentwise. `_` is the unit type and `Never` is the bottom type; each is
equal only to itself (Clause 7.5.4).

Instantiation does not weaken identity: `Pair Int String` and `Pair Int Float`
are different nominal types (Clause 7.6.4). Identity is independent of
representation (Clause 8.5) and of the path by which a type is imported.

The maximum nesting depth of a type is implementation-defined (Clause 2.5); a
type that exceeds it is rejected with a diagnostic (Annex B).

### 7.5.2 Nominal Types

Every `type` declaration introduces a new nominal type. The right-hand side
MUST be a declaration form: a variant list, or a bracketed record or tuple
type (Clause 6.9.5). A composite type expression such as `[]Int` is not a
declaration form, and there are no type aliases.

A variant list declares an algebraic data type. A variant name is a PascalCase
constructor; a payload follows by juxtaposition, and a multi-component payload
is a tuple type. A variant list has at least one variant: a type with no
values is not a declaration form, and the bottom type is vocabulary of the
language (Clause 7.5.4). A constructor applied by juxtaposition is check-only:
it requires an expected type; its instantiation is taken from that type
(Clause 7.6.4). A bare constructor is never written with parentheses; a
module-qualified constructor is applied as a call on its dotted head (Clauses
6.5.2, 6.8.1). Its visibility follows the type (Clause 7.3.3), and its name
MAY equal the type name.

A bracketed right-hand side declares a record or a tuple-shaped nominal type.
Field access does not apply to a tuple-shaped nominal type; its values are
taken apart by patterns (Clause 7.7.1).

A single-variant declaration with one payload is the newtype form. It admits
no implicit conversion in either direction; the wrapped type is reachable by
pattern matching.

Two nominal types with the same shape are mutually incompatible. Conversion is
explicit: a literal or a field-by-field rebuild. A nominal value MAY flow into
a same-shape anonymous type by shape adaptation (Clause 7.5.5); the reverse
flow does not exist.

A `ctype` declaration introduces a nominal record type whose layout is pinned
to C. Its field predicate is checked at the declaration line (Clause 10.3.3).

### 7.5.3 Composite Types

| Form | Type syntax | Constraint |
|---|---|---|
| List | `[]t` | one element type |
| Dictionary | `[k]v` | key and value types |
| Tuple | `[a, b, ...]` | at least two components |
| Record | `[f: a, g: b, ...]` | declared field names |

List and dictionary are distinguished inside the brackets: an empty bracket
pair is a list, and a single type is a dictionary. These are the only forms
that place a type after `]`.

A tuple has at least two components. A one-element bracket `[Int]` is not a
tuple and is not a valid type. Parentheses are context-sensitive: in a
complete-type position `(Int)` is the unit-returning function type `(Int) ->
_`, while in a type-atom position it groups a type; a zero-element `()` is the
parameter group of a zero-parameter function type (Clause 6.9.3). The unit
value is `_`, not a zero-element tuple. Parentheses disambiguate nesting:
`[]([String]Int)` is a list of dictionaries, and `[[]String]Int` is a
dictionary whose keys are lists.

A composite literal is resolved by its expected type:

| Inside braces | Literal form | Resolved by |
|---|---|---|
| comma items, no colon | list or tuple | expected type |
| colon pairs | dictionary or record | expected type |

A composite literal never stands alone. Where no expected type exists, the
literal requires an annotation (Clause 6.6.2); an empty literal follows the
same rule. A partial record literal is legal only in the argument position of
`with` (Clause 9.8.6); anywhere else it is rejected (Clause 6.6.3). A full
record literal MUST name every field exactly once and no field outside the
record type; list elements, dictionary keys and values, and tuple components
MUST have the expected types.

A composite type is invariant in its component types. Shape adaptation does
not enter a component (Clause 7.5.5). The exceptions at a collection-element
position are `Never` adaptation (Clause 7.5.4) and bit adaptation (Clause
7.9.5). `Set` has no composite notation and no literal (Clause 9.10.3).

### 7.5.4 Unit Type and Never

`_` at type position is the unit type: exactly one value, the unit value `_`.
It never means inference. An annotation has no inference holes.

`()` at type position is the zero-parameter function type and abbreviates `()
-> _`. A unit result is written `-> _`; `-> ()` is not a unit result. The
elided arrow `(a)` abbreviates `(a) -> _` (Clause 6.9.3).

`Never` is a type of the language, the dual of `_`: as `_` is the unit type
with exactly one value, `Never` is the bottom type with no values. It is
vocabulary rather than a library type — no module declares it, `std` does not
carry it, and a declaration of the spelling is rejected with `E-name-conflict` —
and it is distinct from the unit type.

`Never` is the bottom type. A `Never` expression adapts to any value type at
the following positions: an argument, an operand, an expected type, a
collection element, a `case` arm, and a declared return position. The
adaptation applies at value positions only. It does not apply inside a
function type: `(a) -> Never` does not adapt to `(a) -> Int`; a lambda wrapper
is the explicit form. A `[]Never` value has only the empty list. `Never` has
no members and no services: the receiver of an access never evaluates, so a
member of a `Never` expression types as `Never`.

A function MAY declare any arrow with result `Never`. `panic` is pure and has
type `(String) -> Never`; `Proc.exit` is world-touching and has type `(Proc,
Int) !-> Never` (Clause 9.2.3). A declared result type `t` does not promise
termination, and divergence neither sets nor clears the world bit (Clause
7.9.2).

`Never` marks a call that does not return. `<-` is not applicable to a
`Never`-returning call that has no callback parameter (Clause 7.8.1). A `case`
arm whose body is `Never` adapts to the case type, but its pattern still
counts for exhaustiveness (Clause 7.7.4).

Unreachable code after a call of type `Never` is legal and produces
W-unreachable (Clause 8.4.4).

The language has exactly two subtyping relations: bit adaptation (Clause
7.9.5) and `Never` adaptation. Shape adaptation (Clause 7.5.5) is a separate
one-way adaptation, not subtyping.

### 7.5.5 Shape Adaptation

A nominal value MAY flow into a same-shape anonymous type. Same shape is exact
structural equality between the anonymous type and the nominal type's
representation.

For a record, the field names and field types MUST match exactly. For a tuple,
the arity and component types MUST match exactly. At a recursive position the
comparison is by nominal name; cycles do not enter the comparison (Clause
7.5.6).

Adaptation is one-directional: nominal to anonymous. A nominal value never
flows into a different nominal type, and an anonymous value never flows into a
nominal type. Conversion to a nominal type is a literal or a field-by-field
rebuild.

Adaptation applies at three positions: an argument, an operand, and an
expected type.

Same shape is an exact match, not a width relation: an anonymous type with
fewer fields does not accept a nominal value with more, and row polymorphism
does not exist. There is no automatic conjunction; operand typing is fixed by
the left operand, the right operand MUST adapt, and two nominal operands do
not search for a common anonymous type.

Shape adaptation does not enter container elements or the interior of a
function type. An explicit map re-establishes the element shape. Bit
adaptation is not subject to this restriction (Clause 7.9.5).

Anonymous ADTs do not exist, so shape adaptation is available for records and
tuples only. A tuple shape extends to a generic instantiation by the shape of
its right-hand side: for `p: Pair Int String`, `q: [Int, String] = p` is
legal. An algebraic shape does not extend.

A failed nominal-to-nominal flow is `E-nominal-mismatch`; a failed same-shape
flow is `E-shape-mismatch` (Annex B). Shape adaptation creates no supertype and
does not compose: an adapted anonymous value cannot flow into a nominal type.
```eko
Human: type = [name: String, age: Int]
Wide: type = [name: String, age: Int, city: String]

view: (p: [name: String, age: Int]) -> String = p.name
h: Human = {name: "ada", age: 36}
w: Wide = {name: "ada", age: 36, city: "x"}

view(h)
q: [name: String, age: Int] = h
view(w)   // error: `Wide` does not have the same shape as the parameter
```

### 7.5.6 Recursive Types

A type declaration's right-hand side MAY refer to the name being declared.
Self-recursion and mutual recursion within one module are equally open.
Cross-module type recursion is closed by the prohibition on cyclic imports
(Clause 7.2.4).

A recursive position is indirect: a self-reference is represented by a
reference, so a recursive type has finite size. Opening recursion requires no
additional check. A recursive type with no base case is legal and has zero
inhabitants. Inhabitance is not checked; `[]Never` needs no inhabitance
analysis.

`show` and `eq` recurse structurally, and the derived `lt` of an ADT does so;
termination follows from acyclicity of the heap graph (Clause 8.6.1).
Same-shape comparison at a recursive position compares by nominal name, and
cycles do not enter the judgment (Clause 7.5.5).

An anonymous recursive type does not exist: a recursive reference requires a
declared name. A recursive generic reference MUST be fully applied; `Tree t`
in its own right-hand side is well formed, while a bare `Tree` is an arity
error (Clause 7.6.2).
```eko
Nat: type = Zero | Succ Nat
Tree: type t = Leaf t | Node [Tree t, Tree t, t]
Person: type = [name: String, friends: []Person]

Expr: type = Num Int | Add [Expr, Expr] | Block []Stmt
Stmt: type = Let [String, Expr] | Ret Expr

Loop: type = Wrap Loop
```

### 7.5.7 Type Equality

The judgment `t = u` holds when the two types have the same identity.

| Type form | Equality condition |
|---|---|
| nominal `T a1 ... an` | same declaration and equal arguments in order |
| list `[]t` | equal element types |
| dictionary `[k]v` | equal key and value types |
| tuple `[t1, ..., tn]` | same arity and equal components in order |
| record `[f1: t1, ...]` | same field names with equal field types |
| function type | equal parameter types, equal result type, same arrow form |
| type variable | same binder |
| `_` | both `_` |
| `Never` | both `Never` |

Type equality is symmetric and transitive. It is decidable by nominal lookup
and structural comparison. A record type's field order is not part of
equality; declaration order remains the representation order (Clause 8.5.2).
Two anonymous record types with the same field names and types are therefore
equal regardless of the order in which the fields are written.

An expression of type `t` conforms to an expected type `u` when `t = u`, or
when one of the one-way adaptations applies at the position: `Never`
adaptation (Clause 7.5.4), shape adaptation (Clause 7.5.5), or bit adaptation
(Clause 7.9.5). No other conversion is applied, and an annotation is checked
exactly.

A function type's arrow form is part of equality. An open arrow `?->` is equal
only to an open arrow with the same parameter and result types. The maximum
nesting depth of a checked type is implementation-defined (Clause 2.5).

## 7.6 Generics

Generics are parametric. Type variables are generalized at annotated bindings
and in type declarations, and instantiated at use sites. There are no bounds,
traits, or constraint solving.

### 7.6.1 Type Variables

A type variable is a lowercase identifier in type position. In a function
annotation, an unbound lowercase name is a type variable of that function; its
scope is the whole annotation. In a type declaration's right-hand side, the
declared binders are the type variables (Clause 7.6.2). Variables are fresh
per declaration.

A type variable MUST NOT have the same name as an imported module; the
same-layer conflict is `E-name-conflict` (Clause 7.4.6).

A variable MAY appear in a parameter type, a result type, a bracketed shape,
an application argument, and a nested function type. A variable MUST NOT head
an application: `t Int` is an error; a variable denotes a type, not a type
constructor (Clause 6.9.4). A variable MUST NOT appear at value position, in
an extern signature (Clause 10.2.3), or in a `ctype` field (Clause 10.3.3). A
variable MAY stand for any type; it has no bounds.

### 7.6.2 Type Binders

A `type` declaration lists its type variables after `type` and before `=`
(Clause 6.3.5). The list is ordered and positional: in `Pair: type t u = [t,
u]`, `t` receives the first argument and `u` the second. A declaration with no
binders is monomorphic.

A binder MUST occur in the right-hand side. A binder that occurs nowhere is
phantom and is `E-phantom` (Annex B).

Binder order is independent of variant order. In `Value: type t u = First [u,
t] | Second t`, `t` is still the first argument and `u` the second; the
payload order does not reorder the binders. Every binder is in scope
throughout the right-hand side and not outside the declaration.

A reference to the declaration inside its own right-hand side MUST be fully
applied. A generic type constructor is not a first-class type; partial
application does not exist.

An application of a generic type MUST supply exactly one argument per binder.
Too few or too many arguments is an error. An argument is an arrow-free type
atom; a composite argument is parenthesized (Clause 6.9.4). Application is by
juxtaposition with no commas, and the chain stops at a comma or an arrow
(Clause 6.9.4).

The phantom check sees through `?` positions: a type variable that occurs
under an open arrow counts as a use (Clause 7.9.6).
```eko
Pair:  type t u = [t, u]
Value: type t u =
    |First [u, t]
    |Second t
Tree:  type t = Leaf t | Node [Tree t, Tree t, t]

p: Pair Int String = {1, "a"}
v: Value Int Float = First {2.5, 1}
```
A function type is not a declaration form (Clause 6.9.5). A named function
type appears in an annotation or a parameter type, not as the right-hand side
of `type`. The `?` phantom rule applies to those positions:

```eko
apply: (f: (t) ?-> _, x: t) ?-> _ = f(x)
```

Here `t` occurs under an open arrow, so it is not phantom; the `?` is fed by
the parameter `f` (Clause 7.9.6).

A declared type applied to arguments is a nominal type whose identity includes
the arguments (Clause 7.5.1). The open bit runs on the same machinery (Clause
7.9.6).

### 7.6.3 Function Generalization

The generalization point is the annotated binding, at top level and at block
level alike. In a function annotation, every unbound lowercase name is a type
variable of that function, and generalization binds those variables over the
annotation.

An unannotated binding is monomorphic; its type is the type of its right-hand
side. `:=` does not generalize, and a lambda is always check-only, so a
polymorphic binding MUST use an annotated `=`.

A polymorphic recursive or mutually recursive function is admitted. The
annotation supplies the type, and each recursive call MAY use any
instantiation. A body may call itself or a mutually recursive partner at a
different instantiation. A type variable is rigid within its declaration. It
is not a unification variable: nothing refines or narrows it after
generalization, and one variable never unifies with another. Instantiation
substitutes a variable exactly, at the call site (Clause 7.6.4); no variance,
constraint, or partial order attaches to a variable. Generalization has no
explicit quantifier syntax; the annotation's variables are implicitly
generalized. The open bit is generalized at the same point (Clause 7.9.6).
There are no inference holes: `_` in type position is the unit type, never a
placeholder (Clause 7.5.4). A binding whose right-hand side is a lambda MUST
carry an annotation; a lambda is always check-only and `:=` carries none.

### 7.6.4 Instantiation

Instantiation replaces each type variable by a concrete type at a use site.
Type arguments are never written; the replacement is determined by unification
with the argument types and by the expected type. A variable is bound
literally to an argument type; no bounds or coercions apply.

Lambda arguments are checked last. In `List.map({1, 2, 3}, (x): x * 2)`, the
list argument fixes `t = Int` before the lambda is checked; otherwise the
lambda parameter would be undetermined. A user-written generic function
behaves identically to a standard-library one at the call site.

The parameter-binding rule (Clause 7.6.5) guarantees that every variable is
determined by the parameters or by the expected type. An undetermined variable
is an error.

Constructor application by juxtaposition is check-only and takes its
instantiation from the expected type. A bare generic constructor has no
self-anchoring type: `Leaf 1.eq(Leaf 2)` has no unique instantiation and is an
error. When one side has a known type, the instance is determined. A
constructor of a non-generic ADT has a unique type and is self-anchoring —
legal only when it is the sole visible constructor of its name; with several,
an annotation anchors the reference (Clause 7.4.6).
```eko
Tree: type t = Leaf t | Node [Tree t, Tree t, t]

x: Tree Int = Leaf 1
y := x.eq(Leaf 1)          // legal: the receiver fixes `Tree Int`
z := Leaf 1.eq(Leaf 2)     // error: no unique union anchors the instantiation
```
Different instantiations are different nominal types. `Pair Int String` and
`Pair Int Float` do not interchange. A literal lands at the expected
instantiation: `p: Pair Int String = {1, "a"}`.

Instantiation is expanded by monomorphization (Clause 8.7.1). Derived services
resolve at the instantiation site (Clause 8.7.2). An expansion that does not
terminate is rejected with `E-instantiation-depth` (Clause 8.7.3; Annex B). An
instantiation MUST be complete; partial application is an error (Clause
7.6.2).

### 7.6.5 Parameter-Binding Rule

Every type variable of a function annotation MUST occur in at least one
parameter type. A variable that occurs only in the result is a
declaration-time error with code `E-phantom` (Annex B). An occurrence anywhere
inside a parameter type satisfies the rule, including inside a nested function
type.
```eko
f: (g: () -> t) -> t = g()            // `t` occurs in parameter `g`
empty: () -> []t = {}                 // error: `t` occurs only in the result
read: (n: Int) -> Option t = None     // error: `t` occurs only in the result
```
The rule is symmetric with the phantom binder ban (Clause 7.6.2): both reject
a variable that no value can determine at the declaration line. The
result-only prohibition for open bits is the same rule (Clause 7.9.6). The
remedy is the annotation discipline itself: bind the variable through a
parameter, or give the composite its concrete type at the binding — `ys: []Int
= {}`.

### 7.6.6 Operations on Type Variables

A value whose type is a type variable supports exactly the following
operations.

| Operation | Form | Basis |
|---|---|---|
| equality | `==`, `!=`, `T.eq` | structural derivation (Clause 9.8.4) |
| display | `show`, interpolation | derived `show` (Clause 9.8.1) |
| JSON | `to_json`, `from_json` | derived mapping (Clause 9.9.3) |
| binding, passing, storing, destructuring | language forms | representation (Clause 8.5) |

Ordering is not available. `<` and `T.lt` are not defined on a type variable;
sorting requires an explicit comparator (Clause 9.10.1). There are no bounds,
traits, or constraint solving; the table is closed.

At an instantiation, each operation resolves to the concrete derived service
or declaration for the substituted type (Clause 8.7.2). An instantiation with
a capability or a function where `eq`, `show`, or the JSON pair is required is
rejected by that service's rule (Clauses 9.8.4, 9.9.5).

A type variable does not support arithmetic, concatenation, or indexing. A
value of type `t` MAY be bound, passed, stored, captured, and destructured;
these are representation operations (Clause 8.5).
