# 8. Dynamic Semantics

## 8.1 Evaluation Model

### 8.1.1 Strict Evaluation

Evaluation is strict. A form that constructs a value evaluates every
constituent expression to a value before the value is constructed. No operand
is passed unevaluated, and no thunk, suspension, or placeholder is created and
filled later.

Strictness applies to the following positions in particular:

- the right-hand side of a binding, before the pattern binds names;
- every argument of a call, before the function body is entered;
- every component of a tuple, record, list, or dictionary literal, before the
  composite value is constructed;
- the scrutinee of a `case`, before any arm is tried;
- the condition of an `if`, before either branch is selected;
- a guard, after its pattern has matched and before the arm body is evaluated.

A lambda expression is already a value: evaluating it creates a closure and
does not evaluate the lambda body. A branch that is not selected is not
evaluated. A guard whose pattern does not match is not evaluated. A function
body is evaluated only when the function is called.

Strict evaluation does not imply that every expression terminates. An
expression may diverge, and an expression of type `Never` never produces a
value. See Clause 8.3.5.

### 8.1.2 Evaluation Order

Subexpressions are evaluated in source order, left to right, subject to the
following rules.

| Form | Order |
|---|---|
| Block | members in source order; each binding completes before the next member |
| Binding `P = e` | `e`, then matching of `P` and binding of its names |
| Call `f(a, b)` | `f`, then `a`, then `b`, then the body of `f` |
| Operator expression | operands left to right after expansion to a function call |
| `a && b` | `a`; `b` only when `a` is `True` |
| `a \|\| b` | `a`; `b` only when `a` is `False` |
| Composite literal | components in written order |
| `case` | scrutinees left to right, once each; then arms top to bottom |
| `if` | condition, then exactly one selected branch |
| Inverted binding | the call, then the callback body when the callee invokes the callback |

A receiver-form call follows the identity in Clause 6.5.2: the receiver is the
first argument and is evaluated before the remaining arguments. A call to a
function that returns without invoking a callback does not evaluate the
callback body.

The specified order is observable through world-touching calls, through
divergence, and through abort. A conforming implementation MUST NOT reorder
evaluations in a way that changes the sequence of world contacts, changes
whether a diverging expression is reached, or changes which value is bound to
a name.

### 8.1.3 Values and Expressions

An environment is a finite mapping from names to values. Name resolution is
static (Clause 7.4); evaluation only looks up names that the static rules have
resolved.

Evaluating an expression of type `t` either yields a value of type `t`,
diverges, or aborts. It does not yield an exception object, a failure token, or
a special result outside the value domain of `t`. Failure values are ordinary
values of `Result` or `Option` (Clause 8.4.1). Abnormal termination is an
abort (Clause 8.4.3).

Values are immutable. Evaluation may allocate heap objects; allocation may
fail and abort (Clause 8.6.4). An expression can diverge even when its static
type is not `Never`; the type system does not track termination.

## 8.2 Bindings and Closures

### 8.2.1 Initial Binding

An initial binding has the form `P = e` or `P: t = e`, where `P` is a pattern.
The right-hand side `e` is evaluated to a value, `P` is matched against that
value, every name in `P` is bound to the matched subvalue, and those bindings
become visible in the remainder of the enclosing block. `P` is irrefutable
(Clause 7.7.1), so the match always succeeds and binding never fails.

The annotation `: t` is static and has no evaluation effect. It fixes the type
against which `e` is checked; it does not evaluate a type expression at run
time.

An initial binding produces no value of its own; it extends the environment. A
tuple pattern destructures a tuple value. The wildcard `_` binds nothing. A
binding holds a scalar directly or a reference to a heap object; it never
copies a composite value (Clause 8.5.2).

A block-level binding is not in scope in its own right-hand side. A reference
to the name before the binding completes is a static error (Clause 7.4).
Module-top-level functions may be recursive because their names are resolved
statically; recursion is not a run-time edge written into an object (Clause
8.6.1).

### 8.2.2 Rebinding and Shadowing

A rebinding has the form `n := e`, where `n` is a name. It evaluates `e`,
introduces a fresh binding for `n`, and makes that binding visible from that
point to the end of the enclosing block. If `n` was already bound, the new
binding shadows the old one; the old value is unchanged.

Rebinding changes which value a name denotes. It does not write into a value,
and it does not write into an environment already captured by a closure. An
annotation is not permitted on `:=`; the type of the new binding is determined
by the right-hand side (Clause 6.3.3).

The form `_ := e` evaluates `e` and discards the value.

Shadowing is lexical. A name bound in an inner block or in a `case` arm hides
an outer binding of the same name only within that block or arm. Different arms
of a `case` bind independently, and a name bound by one arm is not visible in
another.

### 8.2.3 Capture

Evaluating a lambda creates a closure: a function value together with the
values of the names it refers to from enclosing scopes. Those free names are
resolved statically. Capture takes place when the closure is created, not when
it is called.

A closure captures the value that each name holds at the moment of capture. It
captures that value by reference: capture is O(1) and allocates no copy. The
captured value is the value denoted by the name then, not a location that later
rebindings can update.

A later `:=` of the same name introduces a new binding. A closure created
before the rebinding continues to see the old value; a closure created after it
sees the new value. There is no write-back from a binding into a captured
environment.

```eko
base: Int = 10
read: () -> Int = (): base
base := 20
read()                    // 10
```

Capturing a capability copies the capability value, and copying a capability
is delegation of the same authority (Clause 7.9.1). Holding a captured
capability does not touch the world; calling through it is governed by the
world bit of the closure's arrow (Clause 7.9).

The lambda body is evaluated on each call, in the captured environment. A
closure environment holds references to captured values and is subject to the
same retention rules as any other reference (Clause 8.6.3).

### 8.2.4 Inverted Binding

An inverted binding places the remaining block of the enclosing scope in the
last parameter position of a call. The form

```eko
p <- f(a, b)
rest
```

is defined as

```eko
f(a, b, (p): rest)
```

Here `a` and `b` stand for the arguments already present in the call. The
desugaring is the complete evaluation rule; the form introduces no other
semantics. The value of the binding line is the value of the call. The callback
occupies the last parameter position. `rest` is the entire remaining block of
the enclosing scope, or of the enclosing `case` arm when the binding occurs in
an arm.

The admitted pattern forms are those of a lambda parameter position: an
identifier, the wildcard `_`, or a tuple parameter group (Clause 6.3.4). The
zero-argument form `<- f()` passes a zero-parameter callback; `_ <- f()` passes
a one-parameter callback that binds nothing. The two forms are distinct.

Multiple inverted bindings nest: each later line lies in the callback body of
the earlier one. The outermost call is evaluated first, with the callback as
its last argument. The callback body is evaluated only if the callee invokes
the callback. If the callee returns without invoking it, as a `Result`
combinator does when it returns `Err`, the remaining block is not evaluated.
This is the evaluation basis of the propagation behavior specified in Clause
9.2.

```eko
// context: inside a function with parameter `data: []String`
age <- data.get(1).to_result("short row").try()
Ok age
```

If `data.get(1)` yields `None`, the `to_result` bridge returns `Err`, `try`
returns that `Err` without invoking its callback, and `Ok age` is not
evaluated.

An inverted binding requires a call with a callback parameter. It does not
apply to a `Never`-returning call that takes no callback. The form has no world
bit rule of its own; the callback's bit is inferred as for any lambda, and the
callee's open bit is instantiated as specified in Clause 7.9.
