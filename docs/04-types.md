# Types and Generics

A type is a compile-time identity; only values live at runtime. Declarations are bindings whose annotation slot holds `type`, and each declaration introduces a new nominal type whose right-hand side is a declaration form: an algebraic variant list or a record type. The four composite forms — list, dictionary, tuple, record — share two bracket pairs, `[...]` at type position and `{...}` at value position, with the expected type resolving which form a literal denotes. Generic declarations bind positional type variables and are applied by juxtaposition; generic functions generalize at annotated bindings and instantiate by unification at call sites. A nominal value may flow, one-directionally and only when the shapes match exactly, into an anonymous form. `Never` is the bottom type: a diverging expression adapts to any value position, while exhaustiveness remains the job of patterns.

## 1. Type declarations

### 1.1 The `type` slot

`type` is a word in the annotation slot. A type declaration has the same grammatical shape as every other binding: the line starts with a name.

```eko
Sex:     type = Man | Woman
Person:  type = [name: String, age: UInt, sex: Sex, address: String]
Result:  type t e = Ok t | Err e
```

The annotation slot also admits `ctype`, the C-layout record declaration
of `11-ffi.md §3.3`: same binding shape, field types restricted to machine
words and nested `ctype` records, checked at the declaration line.

```
TypeDecl := Name ':' 'type' TypeVar* '=' TypeRhs
```

The marker sits in the slot, the same design act as the bit markers on arrows (`!->`, `?->`; 06-effects.md §5.2) and the binding operators (`=`, `:=`, `<-`; 03-control.md §1).

`TypeRhs` is a declaration form, not an arbitrary type expression: an algebraic variant list (§1.2, §1.4) or a bracketed record or tuple type (§1.3, §7.1). A composite type expression such as `[]Int` or `[String]Int` is not a declaration form — a declaration introduces a new nominal type, it does not name an existing one (§5.1).

### 1.2 Algebraic types

`|` separates variants; a variant may carry a payload. In vertical form every line is introduced by `|`. In horizontal form variants are separated by `|` with no leading `|`.

```eko
Sex: type =
    |Man
    |Woman

IoError: type = NotFound | Permission | Exists | NotFile
              | Closed | InvalidData String | Other String
```

- A variant name is a constructor and is PascalCase (§3). A payload follows its constructor by juxtaposition: `InvalidData String`, `Some t`, `Node [Tree t, Tree t, t]`.
- A payload with several components is written as a tuple type `[a, b, …]` in the declaration. At value and pattern positions the same payload is written `{...}` (§2.3, §9): `Node [Tree t, Tree t, t]` ↔ `Node {Leaf 1, 2, Leaf 3}` ↔ `Node {l, r, _}`.
- The variant list may be empty: `Never: type =` declares a zero-variant ADT (§10.1). A user-declared zero-variant type has the same standing as the prelude one.
- A constructor applied by juxtaposition is check-only and needs an expected type (§9).
- Exhaustiveness is counted by constructors (03-control.md §3; §10.3).

### 1.3 Record types

`[...]` lists fields; a field name and its type are separated by `:`, fields by `,`. The value form is `{...}` (§2.2).

```eko
Person: type = [name: String, age: UInt, sex: Sex, address: String]

p: Person = {name: "ada", age: 36, sex: Woman, address: "x"}
```

Fields are named and accessed by name, `p.name` (05-names.md §4); field update is the derived `with` service (08-builtins.md §6). A bracketed right-hand side may also be positional — `Pair: type t u = [t, u]` (§7.1) — which declares a tuple-shaped nominal type: field access does not apply to it, and values are taken apart by patterns (§2.3).

### 1.4 Single-variant types

One variant with one payload is the newtype wrapper:

```eko
Age: type = Age UInt
```

`Age 36` constructs a value; the wrapped `UInt` is reachable only by pattern matching. A single-variant declaration is a full nominal type: no implicit conversion holds in either direction (§5.1).

## 2. Composite types

Lists, dictionaries, tuples, and records are the four composite forms. They share two bracket pairs and are distinguished by the syntax inside.

### 2.1 Type position `[...]`

| Form | Syntax | Example |
|---|---|---|
| List | `[]t` — empty slot, then an element type | `[]Int`, `[]Person` |
| Dictionary | `[k]v` — key type, then value type | `[String]Int` |
| Tuple | `[a, b, …]` — two or more comma-separated types | `[String, Int]` |
| Record | `[f: a, g: b, …]` — a list of `name: type` fields | `[name: String, age: UInt]` |

- List and dictionary are distinguished inside the brackets: empty means list, one type means dictionary. These two are the only forms that place a type after the `]`.
- `[k]v` reads as a map from `k` to `v`; lookup is `d.get(k) -> Option v` (05-names.md §4).
- A tuple has at least two elements. A one-element `(Int)` is a parenthesized type, not a tuple; a zero-element `()` is the parameter group of a zero-parameter function type (06-effects.md §5.3). The unit value is `_`, not a zero-element tuple.
- Parenthesize nested composites where the reading would otherwise be ambiguous: `[]([String]Int)` is a list of dictionaries, `[[]String]Int` is a dictionary whose keys are lists.
- `_` at type position is the unit type, never "infer" (06-effects.md §5.3): `Result _ IoError`, `Option _`.

### 2.2 Value position `{...}`

| Inside | Form | Example |
|---|---|---|
| Comma items (no colon) | list or tuple | `{1, 2, 3}`, `{"first", 2}` |
| Colon pairs | dictionary or record | `{"1": 1, "2": 2}`, `{name: "first", age: 2}` |

- A composite literal never stands alone. Whether `{1, 2}` is a list or a tuple, and whether `{x: 1}` is a dictionary or a record, is decided by the **expected type** — the same machinery that types lambda parameters (03-control.md §2). Where no expected type exists, an annotation is required.
- A **partial literal** is legal only in the argument position of `with` (08-builtins.md §6): `p.with({age: 37})`. This is the single localized exception to the never-stand-alone rule.
- The type side has four forms and the value side two: list and tuple share the shape `{a, b}`, dictionary and record share the shape `{k: v}`, and the expected type closes the choice. Empty literals follow the same rule: `xs: []Int = {}`.
- Value-position tuples do not use `(...)`: parentheses are already taken by parameter groups, call syntax, and parenthesized expressions. `{...}` is the only composite bracket at value position.
- `Set` is in the prelude as `Set t` and has **no notation and no literal**: `{a, b}` is already given to lists and tuples. Construction goes through `Set.of` (09-library.md §5).

```eko
xs:     []Int          = {1, 2, 3, 4, 5}
counts: [String]Int    = {"1": 1, "2": 2}
pair:   [String, Int]  = {"first", 2}
p:      [name: String, age: Int] = {name: "first", age: 2}
Nums:   type = First | Second | Third

a: Nums = First
b: Nums = Second
```

### 2.3 Tuples and parameter groups

Comma means tuple in four places — parameter groups, patterns, literals, and types — and an arrow has no n-ary versus tupled distinction:

- The parameter group of `(A, B) -> C` is the tuple type `[A, B]`.
- A lambda `(p, i):` is a two-parameter group, which is the shape of the `each_with_index` callback. The sameness with a tuple is unification of spelling, **not automatic destructuring**: a one-parameter callback that receives a tuple value takes it apart with a binding line, `(s, want) = row`, or with a case pattern (03-control.md §1).
- A tuple outside parameter position — a field, an element, a return — is spelled `[A, B]`: `mk: () -> [String, Int]`.
- The `(p, i)` pattern destructures a tuple value (03-control.md §5).

## 3. Naming rules

The following is enforced at compile time. Types and constructors are PascalCase (`Person`, `Ok`, `Man`). Functions and type variables are snake_case (`make_person`, `t`, `elem`). Module names are snake_case and equal the file name (05-names.md §5). A violation is a compile error.

Case makes the name space layered: PascalCase names carry identity (declared types, constructors, namespaces), while snake_case names are bindings in the value and type domains and module paths, so disambiguation has a structural basis (05-names.md §7.2).

Constructors are bare names, never parenthesized: `Ok Man` at value position, `Ok x -> ...` at pattern position.

The lexical rule for type position is: a bare lowercase name is a type variable, a bare PascalCase name is a declared type, and a dotted name is a module path. Classification is by spelling alone and is locally decidable (§7.2, §8.1).

## 4. Compile-time identity

Types live at compile time; only values live at runtime. No expression denotes a type, and no test inspects one (07-runtime.md §1, §9). Declared types are compared by identity, anonymous composite forms by structure, and a function type includes its world bit in its identity while the bit itself is erased at runtime (06-effects.md §5.2).

Services dispatched by type are carried by compile-time generics:

```eko
show: (t) -> String          // prelude: a pure function expanded per instantiation of t
```

`show(x)` and `x.show()` resolve to the namespace of the static type of `x` (08-builtins.md §6), and instantiation is monomorphized (07-runtime.md §6). `Result`, `Option`, `IoError`, and the other prelude types are provided by the prelude (05-names.md §11); their values are constructible anywhere.

```eko
r:  Result Int String
o:  Option Int
u:  Result _ IoError         // _ at type position is the unit type
xs: []Person                 // []t is the prefix form of the list type
```

## 5. Nominal types and shape adaptation

Identity is bought explicitly: a declaration is an identity, an anonymous form is a view.

### 5.1 Declarations are nominal

Every `type` declaration creates a new type. Same-shaped types with different names are mutually incompatible, and conversion is explicit — a literal or a field-by-field rebuild. There are no aliases: the right-hand side is a declaration form only, so naming never renames an existing identity (§1.1).

```eko
Celsius:    type = [deg: Float]
Fahrenheit: type = [deg: Float]
Human:      type = [name: String, age: Int]

c:    Celsius = {deg: 0.0}
anon: [name: String, age: Int] = {name: "ada", age: 36}

f2: Fahrenheit = c                                   // ✗ c: Celsius — nominal to nominal, never passes
r:  Human      = anon                                // ✗ an anonymous shape does not impersonate a nominal
r2: Human      = {name: anon.name, age: anon.age}    // ✓ rebuild into the identity
```

Temperature accidents are structurally impossible, and a newtype wrapper is airtight against every declared type. Instantiation does not weaken identity: `Pair Int String` and `Pair Int Float` are different nominal types (§9).

### 5.2 Shape adaptation

A nominal value may flow into a **same-shape** anonymous form. Adaptation applies at three positions: an argument, an operand, and an expected type. It is the only mechanism for decomposing data by shape in a language without traits.

```eko
Human:  type = [name: String, age: Int]
Person: type = [name: String, age: UInt, sex: Sex, address: String]

foo: (p: [name: String, age: Int]) -> _ = …
bar: (h: Human) -> _ = …

foo(h)      // ✓ h: Human flows into the same-shape anonymous view
bar(p)      // ✗ Person ≠ Human — both are declared types
foo(p)      // ✗ Person has more fields and its age is UInt — not the same shape
q: [name: String, age: Int] = h     // ✓ adaptation at the expected position
```

Same shape is an exact match, not a width relation: an anonymous form with fewer fields does not accept a nominal value with more, and row polymorphism is excluded. Field-evolution brittleness is the contract: adding a field to `Person` breaks every same-shape anonymous signature, because the shape changed and the signatures should change with it.

### 5.3 No auto-conjunction and position discipline

- **No automatic conjunction.** Operand typing is fixed by the left operand; the right operand must adapt. Mixed nominal operands do not search for a common anonymous supertype:

  ```eko
  c: Celsius    = {deg: 20.0}
  f: Fahrenheit = {deg: 68.0}
  c == f             // ✗ typed as Celsius; Fahrenheit does not adapt
  ```

  The temperature accident is thus closed off from the equality side as well.

- **Position discipline.** Shape adaptation does not enter container elements or the interior of function types — the same spirit as the position bans on `?` and `Never`. Bit adaptation (06-effects.md §5.1) is not subject to this restriction: bits are erased, so they move freely, while shapes carry identity, so they do not. The escape hatch is an explicit map that re-establishes the anonymous element shape: `ys: [][name: String, age: Int] = xs.map((p): p)`.

Anonymous ADTs do not exist — there is no union form outside a right-hand side — so shape adaptation is exclusive to records and tuples; tuple shapes extend to generic instantiations (§9). The diagnostic is: `Human does not accept Person (both are declared types); convert through a same-shape anonymous form.`

## 6. Recursive types

A right-hand side may refer to the name being declared: recursion is open. Representation and memory management are fixed in 07-runtime.md §2.

```eko
Nat:    type = Zero | Succ Nat
Tree:   type t = Leaf t | Node [Tree t, Tree t, t]
Person: type = [name: String, friends: []Person]    // recursion through a container
Expr:   type = Num Int | Add [Expr, Expr] | Block []Stmt
Stmt:   type = Let [String, Expr] | Ret Expr         // mutual recursion in one module is equally open
```

- Self-recursion and mutual recursion within a module are equally open, the same ruling as for function recursion. Cross-module type recursion is closed automatically by the prohibition on cyclic dependencies (05-names.md §10).
- A recursive position is always indirect: a self-reference is always a pointer, so a recursive type always has finite size, and opening recursion costs no new checks. A recursive type with no base case (`Loop: type = Wrap Loop`) is legal and has zero inhabitants; inhabitance is not checked — `[]Never` needs no inhabitance analysis either.
- `show`, `==`, and `<` recurse structurally; termination is guaranteed by acyclicity of the heap graph (07-runtime.md §3).
- Same-shape judgment at a recursive position compares by name: shapes are expanded for comparison, but cycles do not enter the judgment.

## 7. Generic declarations

A type declaration needs binders because application is positional. A function needs none: its instances are reached by unification and never written positionally, so there is no slot to define (§8).

### 7.1 Binders

```
TypeDecl := Name ':' 'type' TypeVar* '=' TypeRhs
TypeVar  := lowercase identifier
```

A generic declaration lists its type variables after `type` and before `=`:

```eko
Pair:  type t u = [t, u]
Value: type t u =
    |First [u, t]
    |Second t
Tree:  type t = Leaf t | Node [Tree t, Tree t, t]
```

- The declaration head keeps the invariant `Name ':'`, the common prefix of all declaration forms. The grammar is a pure extension: no token was legal between `type` and `=` before.
- Binders are filled positionally: `Value Int Float` sets `t = Int` and `u = Float`, so the `First` payload is `[Float, Int]`. Binder order is independent of variant writing order; the use site answers positional questions by binder order alone.
- Binders are local to the declaration's right-hand side. In `depth: (tree: Tree t) -> Int`, `t` is the annotation's own type variable; it has the same spelling as `Tree`'s binder but a different scope, and no capture occurs.
- Every binder must occur in the right-hand side. A phantom parameter is a declaration-time error: it would entangle `==`, `show`, and conversion semantics with a position that no value can distinguish, so no exception is opened.

Binders are explicit and ordered because a union carries no positional skeleton of its own. If variables were collected by first occurrence, reordering variants or fields would silently flip the meaning of positional arguments at distant use sites.

### 7.2 Application by juxtaposition

A generic declaration is applied by juxtaposing it with its arguments, with no commas. An argument is an arrow-free atom, and `->` terminates the chain.

```
Value Int Float                     // t = Int, u = Float
f:  (Value Int Float) -> Int        // one parameter
xs: []Value Int Float               // a list
ts: [Value Int Float, Bool]         // the chain terminates at the comma
o:  Option (Value Int Float)        // a composite argument takes parentheses
```

- `Value Int Float` applies `Value` to `Int` and `Float`; commas are never written between type arguments.
- A composite argument is parenthesized: `Option (Value Int Float)`.
- An application chain stops at `,` and at `->`: in `[Value Int Float, Bool]` the first element is `Value Int Float`, not a three-element application.

Commas are not used because a function parameter list already separates its parameters with commas: `(Value Int, Float) -> Int` can only be read as two parameters, and writing a single argument would require double parentheses in the position where function types appear most often. The language's existing convention is application by adjacency and enumeration by commas: `[k]v` has no comma, `[a, b]` does.

**Lowercase variables.** Type position reads by spelling: bare lowercase is a variable, bare PascalCase is a declared type, dotted is a module path. Snake_case keeps the identity layering of §3 intact (a variable is a binding, not an identity), makes `Tree t` immediately distinguishable from `Tree T` in unseparated juxtaposition, and keeps misspellings from becoming variables: `Preson` stays uppercase and is reported as undeclared. The cost: in a declaration's right-hand side and in patterns, case no longer signals the domain — the payload `Ok t` and the pattern `Ok t` look alike; position settles the parse and human scanning pays.

## 8. Generic functions

### 8.1 Implicit type variables

In a function annotation, an unbound lowercase name is a type variable of that function. Its scope is the whole annotation. Each declaration's variables are fresh and unrelated to the variables of any other declaration.

A type variable may not have the same name as an imported module: a bare `json` then reads two ways, as a variable and as a module, and the same-layer conflict is a compile error (05-names.md §6).

### 8.2 Generalization points

The generalization point is the annotated binding, at top level and at block level alike.

```eko
first: (xs: []t) -> Option t =
    case xs:
        {x, ..rest} -> Some x
        {}          -> None

id: (t) -> t = (x): x
swap: (p: [t, u]) -> [u, t] =
    case p:
        {a, b} -> {b, a}
```

- Polymorphic recursion and polymorphic mutual recursion come for free: the type comes from the annotation rather than from inference, so a recursive call may use any instantiation.

```eko
even_len: (xs: []t) -> Bool =
    case xs:
        {}          -> True
        {_, ..rest} -> odd_len(rest)

odd_len: (xs: []t) -> Bool =
    case xs:
        {}          -> False
        {_, ..rest} -> even_len(rest)
```

- `:=` does not generalize: `id := (x): x` is an error, because a lambda is always check-only (03-control.md §2). A polymorphic binding must be annotated: `my_first: ([]t) -> Option t = first`. Eko has no mutable state, so declaring generalization through an annotation carries no soundness risk.

### 8.3 Parameter-binding rule

Every type variable of a function annotation must occur in at least one parameter type; a result-only variable is a declaration-time error.

```
first: (xs: []t) -> Option t      // ✓
f: (g: () -> t) -> t              // ✓ t occurs in the type of parameter g
empty: () -> []t = {}             // ✗ t is not determined by any parameter
read: (n: Int) -> Option t        // ✗ likewise
```

A type that is not determined by the parameters should be determined. The rule is symmetric with the phantom ban on the declaration side: both eliminate dangling variables at declaration time. `empty` loses nothing — an empty list with an expected type suffices (`ys: []Int = {}`, §2.2). The open-bit result-only ban (06-effects.md §5.2) is the same rule; bits and types run on the same machinery.

### 8.4 Instantiation

- A variable is bound literally to an argument type. Instantiation is unification at the call site, and type arguments are never written at use sites. The parameter-binding rule guarantees that either unification or the expected type determines every variable.
- **Lambda arguments are checked last.** In `List.map({1, 2, 3}, (x): x * 2)`, the list argument fixes `t = Int` first and the lambda is checked afterwards; otherwise its parameter would be an undetermined variable and the literal `2` would have nowhere to adapt. A user-written generic function behaves identically to a prelude one at the call site.

### 8.5 What type variables can do

A variable may appear in parameter types, return types, inside bracketed shapes, as an application argument (`Tree t`), and in nested function types: `(f: (t, u) -> v, xs: []t, ys: []u) -> []v`. It may not head an application: `t Int` is an error, because a variable denotes a type, not a type constructor. It may not appear at value position: `t` is not a value.

A body may do the following with a value of type `t`:

| Available | Basis |
|---|---|
| `==` / `!=` (structural equality) | derived (08-builtins.md §6) |
| derived `show` (interpolation, `println`) | 08-builtins.md §6 |
| derived `to_json` / `from_json` | 08-builtins.md §7; at an instantiation the concrete derivation or an override applies |
| binding, passing, storing, pattern destructuring | representation mechanism (07-runtime.md §1) |

Ordering is not available: `sort_by` takes an explicit comparator for this reason (09-library.md §5), and with no bounds in the language there is no constraint solving.

Polymorphism divides in two:

> Open polymorphism is parametric generics: little can be done with a value, but everything holds everywhere. Ad-hoc polymorphism is closed unions with `case`: the exhaustiveness check is the gatekeeper of closed-world dispatch.

## 9. Generics with unions, constructors, patterns

```eko
root: Tree Int = Node {Leaf 1, 2, Leaf 3}

depth: (tree: Tree elem) -> Int =
    case tree:
        Leaf _          -> 0
        Node {l, r, _}  -> 1 + deeper(depth(l), depth(r))

deeper: (a: Int, b: Int) -> Int = if a >= b: a else: b
```

- Constructor application by juxtaposition is **check-only**: it requires an expected type, because the instantiation of the constructor comes from that type.
- A bare generic constructor has no self-anchoring equality. `Leaf 1 == Leaf 2` is an error — there is no unique union to anchor the instantiation and no qualified form to escape to. If one side is a variable, the instance is determined and the equality is legal:

  ```eko
  x: Tree Int = Leaf 1
  x == Leaf 1        // ✓ one side is a variable: the instance is determined
  Leaf 1 == Leaf 2   // ✗ no unique union to anchor the instantiation
  ```

  Bare constructors of single-variant unions self-anchor, so `Red == Green` remains legal.
- Pattern typing comes from instantiation: a pattern binder's type is taken from the instantiated scrutinee type, and exhaustiveness is checked against the instantiated union. No new pattern machinery is involved.
- Bracket direction mirrors: type position `[...]`, value and pattern positions `{...}` — `Node [Tree t, Tree t, t]` ↔ `Node {Leaf 1, 2, Leaf 3}` ↔ `Node {l, r, _}`.
- Recursive unions (`Tree` is one) and mutual recursion work as in §6.
- Shape adaptation extends to instantiations by the shape of the right-hand side: for `p: Pair Int String`, `q: [Int, String] = p` is legal, and it is the tuple shape that adapts. Algebraic shapes do not extend: anonymous ADTs do not exist (§5.3).
- Different instantiations are different nominal types; `Pair Int String` and `Pair Int Float` do not interchange.
- Literals land through the expected type: `p: Pair Int String = {1, "a"}`.
- The open bit runs on the same machinery (06-effects.md §6): `?` is an anonymous parameter on bits, fed by parameters, instantiated at the call site, with lambda arguments checked last. The phantom check sees through `?` positions: `Callback: type t = (t) ?-> _` is legal, because the open bit appears in the right-hand side through `?->`.

## 10. Never

### 10.1 Identity

```eko
Never: type =          // zero variants: the degenerate | list
```

`Never` is a prelude ADT of the same kind as `Bool`: zero constructors, zero values. It is distinct from the unit type — `_` has exactly one value, `Never` has none. Functions are values, results are not: `never_fn` can be passed and referenced, but `never_fn()` never finishes on a return.

### 10.2 Bottom-type adaptation

`Never <: t` lets a diverging expression such as `panic` land in any expression position:

```eko
x := if c: compute() else: die("no")     // the Never arm adapts to Int
case r:
    Ok v  -> v
    Err e -> die("{e}")                   // likewise
```

The adaptation positions are the same list as for bit subtyping (06-effects.md §5.1) — argument, operand, expected type, collection element — plus two: **arms** and **declared return positions**. Eko's subtyping is exactly two relations, bit adaptation and the `Never` bottom type, so it stays bounded.

Adaptation applies only at **value positions**. It does not apply inside function types: a named `g: (a) -> Never` cannot be passed where `(a) -> Int` is expected, in the same spirit as the position discipline on `?`. The escape is a lambda wrapper — and a lambda literal already reaches `u := Never` by inference, so no subtyping is needed:

```eko
List.each(xs, (s): die("no head"))       // callback type (String) -> Never;
                                         // u := Never, reached by inference
```

### 10.3 Exhaustiveness via patterns

An arm with a diverging body does two jobs, credited to different mechanisms:

| Job | Done by |
|---|---|
| The case is closed (compile-time guarantee) | **patterns** (`Err e`, `_`) |
| The arm's type is compatible with the case's type | the **Never body** (§10.2 adaptation) |

"The branch does not return normally" explains only the second. If divergence could stand in for exhaustiveness:

```eko
case light:
    Red -> stop()
    panic("unreachable")     // ✗ this form does not exist: a bare divergence is not an arm
```

`Green` is handled by no pattern, yet the case would pass compilation: exhaustiveness would degrade from a compile-time guarantee to a runtime crash, and the guarantee would be hollow. An arm is therefore always `PATTERN -> expression` (03-control.md §3). The idiom is:

```eko
case r:
    Ok _ -> proceed()
    _    -> panic("unreachable")     // pattern _ covers; panic handles the type
```

Exhaustiveness counts constructors (03-control.md §5), never inhabitance: `Result t Never` still requires an `Err` arm, because the compiler does not prove "cannot be inhabited".

### 10.4 Control flow and the world bit

```eko
main: (out: Out) ! =
    out.println("start")
    die("boom")
    out.println("never")     // unreachable: legal, warns
```

Unreachable code is legal and produces a **warning**, not an error; diagnostics have three levels — error, warning, none (09-library.md §7).

The world bit is independent: `Never` is a legal return for any arrow. `panic` is pure, `-> Never`; `Proc.exit` is `!-> Never` — abrupt exit touches the world (`06-effects.md §2`). A `-> t` annotation does not promise termination — divergence neither sets the world bit nor is tracked by it (06-effects.md §5, §7). `Never` marks "must not return", not "may not return".

### 10.5 Writing diverging functions

```eko
spin: () -> Never = spin()                 // recursion is divergence; recursive bindings are allowed
die: (msg: String) -> Never = panic("[die] {msg}")

xs: []Never = {}                           // zero values: only the empty list exists
f:  (t) -> Never                           // a legal signature
g:  (f: (Int) -> Never, n: Int) -> Int =   // passing a diverging function is legal (it is a value)
    f(n)                                   // the call is Never and adapts to Int
```

- `<-` does not apply to a `Never` call: the inverted binding is a callback contract and needs a last-position callback (03-control.md §1).
- `show` and `==` hold trivially for `Never`: no value can arrive.
