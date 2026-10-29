# 6. Syntax

## 6.3 Bindings and Declarations

### 6.3.1 Binding Forms

The productions are in Annex A.3.2. The declaration forms are given in
Clause 6.3.5 through Clause 6.3.8. The annotation MAY appear only with `=`;
`:=` carries no annotation. In `<-`, the type of the bound pattern is supplied
by the callback parameter.

A binding line begins with its pattern. An expression line begins with its
expression. The binding operators are infix; they are never written at the
start of a line, except for the pattern-less `<-` form.

A namespace binding declares a member of a type namespace: its head is
`TypeName.name`, or a module-qualified owner path whose last segment is the
type name (Annex A.3.8), the annotation is mandatory, and the form is legal
only in the module that declares `TypeName` (Clause 7.4.5). The tail carries the
privacy prefix: a member named `_name` is private to its module. `:=` and
`<-` are not admitted for a namespace binding; the annotation is the
member's public signature.

```eko
age: UInt = 36
(p, i) <- persons.each_with_index()
```

The annotation slot holds a type expression for a value binding, or one of
the contextual words `type`, `ctype`, and `test`. The word `extern` appears on
the right-hand side of a declaration, not in the annotation slot.

### 6.3.2 Value Bindings

`=` introduces a binding. The right-hand side is evaluated and bound to the
pattern. A value binding MAY carry a type annotation between the pattern and
`=`.

```eko
person: Person = {name: "ada", age: 36, sex: Woman, address: "x"}
```

Without an annotation, the type is determined by the right-hand side. A
composite literal with no expected type requires an annotation
(Clause 6.6.2).

A parenthesized tuple on the left of `=` destructures a tuple value:

```eko
(s, want) = row
```

A binding whose pattern is `_` evaluates the right-hand side and discards the
value: `_ := e`. In value position `_` is the unit value; in pattern position
it is the wildcard (Clause 7.5.4).

### 6.3.3 Rebinding

`:=` introduces a binding for a name without an annotation. If the name is
already bound, the new binding shadows the old one in the remainder of the
block.

```eko
count := count + 1
p2 := p
```

`:=` does not generalize; a polymorphic binding requires an annotated `=`
(Clause 7.6.3).

### 6.3.4 Inverted Binding

An inverted binding places the remaining block in the last parameter position
of a call:

```eko
p <- f(a)
rest
```

is defined as:

```eko
f(a, (p): rest)
```

`rest` is the entire remaining block after the binding line. The callback
occupies the last parameter position.

```eko
age <- r.map((v): v + 1).try()
(p, i) <- persons.each_with_index()
_ <- save(entry).try()
```

A zero-argument callback omits the pattern:

```eko
<- scope.spawn()
```

`<- f()` and `_ <- f()` are distinct: the first passes a zero-parameter
callback, the second a one-parameter callback that binds nothing. The
callback's shape is fixed by the expected function type (Clause 7.8.1).

Multiple inverted bindings nest in sequence; each later line lies in the
callback body of the earlier one:

```eko
a <- f()
b <- g(a)
h(a, b)
```

is defined as:

```eko
f((a):
    g(a, (b):
        h(a, b)))
```

The admitted pattern forms are exactly those of a lambda parameter position:
an identifier, `_`, or a tuple parameter group. Record, constructor, and list
patterns do not appear after `<-`.

Because the desugaring appends a callback in the last position, `<-` requires
a call with a callback parameter. It is not applicable to a call that returns
`Never` and takes no callback.

### 6.3.5 Type Declarations

A type declaration names a type, lists its type-variable binders after `type`,
and gives its right-hand side (Annex A.3.2). Every binder MUST occur on the
right-hand side, and a phantom binder is an error (Clause 7.6.2).

The right-hand side is a declaration form — a variant list, a record type, or
a tuple type — not an arbitrary type expression:

```eko
Sex:    type = Man | Woman
Person: type = [name: String, age: UInt, sex: Sex, address: String]
Result: type t e = Ok t | Err e
Tree:   type t = Leaf t | Node [Tree t, Tree t, t]
```

A variant is a PascalCase constructor, optionally followed by one payload
type; several payload components are written as a tuple type. A vertical
variant list writes `|` at the head of each line.

A type declaration is top-level. A declaration introduces a new nominal type;
it does not alias an existing type (Clause 7.5.2). A composite type expression
such as `[]Int` is not a declaration form.

### 6.3.6 ctype Declarations

A `ctype` declaration has the form `Name: ctype = [field: type, ...]`
(Annex A.3.2). Every field type MUST be a machine word or another `ctype`
record (Clause 10.3.3). A `ctype` declaration is top-level.

```eko
DivC: ctype = [quot: I32, rem: I32]
```

### 6.3.7 Test Declarations

A test declaration has the form `Name: test = body` (Annex A.3.2). It is
top-level. Its body is an ordinary block or a single expression on the same
line; its value is discarded. The name is snake_case. Test declarations
are specified in Clause 9.14.

### 6.3.8 extern Declarations

An extern declaration is a name, a function type, `=`, `extern`, a string
literal, and an optional `Pure` (Annex A.3.2). The signature is a function
type. The string names a native symbol. The optional word `Pure` asserts the
audited guarantee of Clause 10.2.2.

```eko
_sqrt: (F64) -> F64 = extern "sqrt" Pure
_qsort: (base: C.Ptr.Void, n: C.Size, size: C.Size,
         compar: (C.Ptr.Void, C.Ptr.Void) -> Int) ! = extern "qsort"
```

An extern declaration is top-level. Its signature MUST satisfy the
marshalling whitelist (Clause 10.3.5). The word `extern` begins the
declaration only when a string literal follows it; in every other position
`extern` is an ordinary name (Clause 5.6.2).
