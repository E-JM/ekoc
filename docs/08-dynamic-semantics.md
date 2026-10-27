# 8. Dynamic Semantics

This clause specifies the evaluation of a conforming Eko program. Evaluation
maps a well-typed expression and an environment to a value, or diverges, or
aborts. Static conditions are specified in Clause 7. The classification of
failures is specified in Clause 9.2. The world bit and its typing rules are
specified in Clause 7.9. Examples in this clause are non-normative.

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

## 8.3 Control Flow

### 8.3.1 case

A `case` evaluates its scrutinees once, in left-to-right order. With multiple
scrutinees, the arm's patterns are matched against the corresponding values;
no tuple is formed (Clause 6.7.2).

Arms are tried from top to bottom. For each arm, the pattern is matched against
the scrutinee value. If the pattern does not match, the next arm is tried. If
the pattern matches and the arm has a guard, the guard is evaluated; if the
guard is `False`, the next arm is tried. The first arm whose pattern matches
and whose guard is `True` or absent is selected. Its result expression is
evaluated, and its value is the value of the whole `case`.

No arm body is evaluated before selection. Unselected arm bodies are not
evaluated. There is no fallthrough: after the selected arm produces a value,
evaluation of the `case` is complete.

A well-typed `case` is exhaustive (Clause 7.7.2). Consequently, evaluation of a
conforming program cannot reach a state in which no arm is selected. A `case`
with no matching arm is a static error, not a run-time match failure.

### 8.3.2 if

An `if` is syntactic sugar for a `case` over `Bool`: `True` selects the then
branch and `False` selects the else branch. The condition is evaluated once.
Only the selected branch is evaluated. The condition MUST have type `Bool`;
there is no truthiness conversion.

An omitted `else` is `else: _`. A then-branch without an `else` therefore has
type `_`, or type `Never`, which adapts to `_` (Clause 7.5.4).

The operators `&&` and `||` are sugar for `if` (Clause 6.5.4). Their right
operands are evaluated only when the result requires it, as specified in
Clause 8.1.2.

### 8.3.3 Guards

A guard is evaluated only after its arm pattern has matched. The pattern binds
before the guard is evaluated, and the guard is evaluated in the environment
extended by those bindings. The guard MUST have type `Bool`.

If the guard is `False`, the arm is not selected and matching continues with the
next arm. If the guard is `True`, the arm is selected and its result expression
is evaluated. A guard is not evaluated when the pattern does not match.

A `case` with guarded arms ends in an unguarded arm (Clause 7.7.3). The
unguarded arm closes the `case`; the guards do not participate in
exhaustiveness.

A guard is an ordinary `Bool` expression. Its evaluation follows the rules of
this clause and its world bit contributes as specified in Clause 7.9.

### 8.3.4 Pattern Matching

Matching a value against a pattern yields either failure or an environment of
bindings. Matching performs no user code, no world contact, and no divergence;
it traverses existing structure. It does not copy a composite value, and it
does not observe object identity.

| Pattern | Match condition | Bindings |
|---|---|---|
| variable `x` | always | `x` is the matched value |
| wildcard `_` | always | none |
| literal | value equality with the literal | none |
| zero-payload constructor | constructor tag equality | none |
| constructor `C P` | tag is `C`, then `P` matches the payload | those of `P` |
| tuple `{P, Q}` | arity matches, then components match | union of component bindings |
| record `{f: P}` | every named field is present and matches | those of the field patterns |
| list `{}` | the value is the empty list | none |
| list `{P, Q}` | length matches, then elements match | union of element bindings |
| list `{h, ..t}` | the first element matches `h`; the tail list matches `t` | those of `h` and `t` |
| list `{.., P}` | the last element matches `P`; the head list is not bound | those of `P` |
| list `{_, ..}` | the list is non-empty | none |
| or `P \| Q` | `P` matches, or `P` fails and `Q` matches | the names shared by both alternatives |

A record pattern matches a subset of fields; fields not written in the pattern
are ignored. A fixed-length list pattern may denote a list or a tuple according
to the expected type (Clause 6.8.4). A rest pattern binds the remaining list,
not its elements.

An or-pattern tries its alternatives left to right. The alternatives bind the
same names at the same types (Clause 6.8.3), so the environment produced by a
successful alternative has the same shape whichever alternative matches.

Literal patterns compare values structurally. A literal pattern for a
zero-payload constructor compares the constructor tag. A variable or `_`
pattern always matches. The remaining forms match only values of the
corresponding shape and may fail otherwise. The pattern grammar is specified in
Clause 6.8; this clause specifies only the matching relation.

### 8.3.5 Divergence

An expression diverges when its evaluation does not produce a value after any
finite number of steps. Divergence is not a failure value and does not produce
a diagnostic at run time.

An expression of type `Never` has no value. Evaluating it either diverges or
aborts; it cannot return a value. The type `Never` adapts to any value position
(Clause 7.7.4), but a diverging arm does not discharge exhaustiveness: patterns
MUST still cover the constructors (Clause 7.7.2).

Divergence is independent of the world bit. A function with a pure arrow may
diverge, and a function with a world-touching arrow may terminate. The
annotation `-> t` does not promise termination; `Never` marks a function that
must not return, not one that may not return.

A non-terminating pure computation cannot be interrupted by the language. A
spawned task that never reaches a world-call boundary prevents its scope from
completing (Clause 8.9.4).

## 8.4 Failure

The failure taxonomy is specified in Clause 9.2. This section specifies the
evaluation behavior of failure values and of abnormal termination; it does not
introduce a further failure category.

### 8.4.1 Result and Option

Values of `Result` and `Option` are ordinary algebraic values. A value that
carries a payload is constructed only after the payload has been evaluated.
Matching such a value follows Clause 8.3.4, and a `case` over it is exhaustive
as specified in Clause 7.7.

There is no exception mechanism. A call that fails recoverably returns an
`Err` value or a `None` value; it does not transfer control to a handler. No
failure value propagates implicitly. An inverted binding propagates an `Err`
only because the called combinator returns the `Err` without invoking its
callback (Clause 8.2.4); the remaining block then remains unevaluated.

`None` is a normal value of `Option`, not an abort. Reaching end of input is
reported as `Ok None` by the relevant library operation, not as `Err` (Clause
9.2.4). Operations such as `expect` and `unwrap` may convert an absent value
into a panic; that conversion is specified in Clause 9.2 and evaluated as a
panic under Clause 8.4.2.

The constructors, combinators, and conversion rules of `Result` and `Option`
are specified in Clause 9.2. This clause specifies only how their values are
constructed, matched, and propagated.

### 8.4.2 Panic

`panic` has the signature `panic: (String) -> Never`. Evaluating `panic(msg)`
evaluates `msg` and aborts the process. It never returns a value.

A panic is pure: aborting is not world contact, so a panic call sets no world
bit and may occur in a function with a pure arrow (Clause 7.9). A panic is not
catchable by `case`, by a callback, or by any other language construct. It does
not unwind the stack and does not run cleanup code.

Integer division by zero and integer overflow panic under the arithmetic rules
of Clause 7.8.4. Time arithmetic outside the domain of `Time`, `Mono`, or
`Duration` panics under the same rules (Clause 9.7.5). Operations specified as
total, including the bitwise and wrapping families, do not panic (Clauses 9.6.5
and 9.6.6).

The panic output and process exit status of the command-line host are specified
in Annex B.6; panic codes are listed in Annex B.5.

### 8.4.3 Abort

An abort is immediate termination of the process without unwinding. No Eko
expression is evaluated after an abort, no `case` arm or callback observes it,
and no cleanup or finalization code runs.

Abort is the mechanism used for:

- a panic (Clause 8.4.2);
- stack overflow (Clause 8.8.4);
- memory exhaustion (Clause 8.6.4);
- a native fault at the boundary (Clause 10.4).

Abort is not a value and is not a failure category. It is the common
termination mechanism for conditions from which the language does not resume.
Because the process terminates, the operating system reclaims its storage and
resources; no language-level finalizer is promised.

### 8.4.4 Unreachable Code

Unreachable code is legal Eko. It MUST NOT be rejected as an error.

An expression that follows a diverging expression in the same block cannot be
evaluated. A conforming compiler MUST report a warning for such an expression.
It MAY report further expressions that it determines cannot be reached, also as
warnings. A warning for unreachable code does not change typing, does not
change evaluation, and does not reject the program.

```eko
main: (out: Out) ! =
    out.println("start")
    panic("stop")
    out.println("never")     // warning: unreachable code
```

Unreachable code is not evaluated. An implementation MAY omit it from generated
code. Warning format and severity are specified in Clause 2.4 and Annex B.4.

## 8.5 Values and Representation

### 8.5.1 Scalars

A scalar is stored directly in a register, a stack slot, or an inline slot
inside a heap object. A scalar is never a heap object and is never reached
through a reference.

| Value class | Representation |
|---|---|
| `Bool`, `Char`, `Int`, `UInt`, `Float` | stored directly; no heap allocation |
| machine words `U8`, `I32`, and the other native-width words | stored directly (Clause 10.3.1) |
| `_` (unit) | zero-sized |
| `Never` | unrepresented |

`Bool` is a standard-library algebraic type and `Char` is a standard-library
scalar type; representation follows the value domain, not the declaration form.
The unit type has exactly one value, and that value occupies no storage.
`Never` has no values, so no value of that type needs representation (Clause
7.5.4).

Copying a scalar copies its value in O(1). No sharing question arises for a
scalar.

### 8.5.2 Composite Values

Except for scalars, values of the composite families are immutable heap
objects. The families are records, tuples, lists, dictionaries, algebraic
values, strings, byte sequences, and closures. A name, parameter, field,
element, payload, or capture slot holds a reference to such an object.

Binding, argument passing, capture, and storing a value into a field, element,
or payload never copy the value. Each adds an O(1) reference, independent of
the value's size. Field access such as `p.name` reads through the reference in
O(1).

`with` copies the top level of a record and shares every field that the update
literal does not name (Clause 9.8.6). The language has no deep-copy or
shallow-copy vocabulary. An independent copy is constructed by rebuilding the
value field by field (Clause 7.5.3).

A `String` is an immutable UTF-8 sequence on the heap. String concatenation
produces a fresh string by copying its operands (Clause 9.4.2). A `Bytes` value
is an immutable byte sequence with the same sharing and retention rules (Clause
9.5.2). A closure is a heap object holding a function and its captured
environment.

### 8.5.3 Opaque Values

Capabilities, file handles, ranges, tasks, and scopes have opaque values. Their
representation is implementation-defined and MUST be documented (Clause 2.5).
A program can use only the operations exposed by the corresponding interface.

A file handle carries a generation in addition to its resource identity. The
generation is checked on use, so use after close is detected and reported as
`Closed` rather than becoming undefined behavior (Clause 9.12.3). Opacity
prevents representation freedom from entering the language: no Eko operation
can inspect the bytes of an opaque value.

Copying a capability value delegates the same authority; it does not create a
new authority and does not by itself touch the world (Clause 8.2.3). An opaque
value contributes no edges to the heap graph (Clause 8.6.1).

### 8.5.4 Sharing and Identity

A name holds a reference to a heap object. Binding one name to another,
passing a value, capturing it, and storing it share that object; no operation
observes the sharing.

The language defines the following structural operations:

- `==` compares structure, not address (Clause 9.8.4);
- `show` and `inspect` render structure, not address (Clause 9.8.1);
- pattern matching decomposes structure, not address (Clause 8.3.4).

Consequently, two names may designate the same heap object, and no Eko program
can distinguish that case from two structurally equal objects. Sharing is not
an observable property.

An implementation MAY share, inline, stack-allocate, or deduplicate values
provided that no operation in Clause 8.11 becomes observable. The language
guarantees structure and sharing behavior, not byte identity.

### 8.5.5 Recursive Types

A recursive type is a type whose declaration refers to itself, directly or
through another declaration in the same module (Clause 7.5.6). Every recursive
position is represented by an indirection, never inline. The payload of `Succ
Nat` is a reference, and expansion of the type stops at that reference.

Two consequences follow. First, the size of every value is finite: a recursive
declaration needs no separate well-formedness check for size, and a recursive
type with no base case is legal and has zero inhabitants. Inhabitance is not
checked, in the same manner as for `Never`. Second, derived services that
recurse over structure terminate, because every value is finite (Clause 8.6.1).

```eko
Nat:  type = Zero | Succ Nat
Tree: type t = Leaf t | Node [Tree t, Tree t, t]
Person: type = [name: String, friends: []Person]
```

Mutually recursive declarations in one module have the same standing as
self-recursive ones. Recursion through an applied type constructor or through a
container is ordinary: the recursive position is still an indirection.

A conforming implementation MAY inline or unbox a non-recursive composite. A
recursive position MUST remain indirect; this invariant preserves finite size
and the acyclicity argument of Clause 8.6.1.

## 8.6 Memory Management

### 8.6.1 Acyclicity

The heap graph of an evaluation has heap objects as nodes and references stored
in fields, elements, payloads, and capture slots as edges. Opaque values
contribute no edges.

**Theorem (Acyclicity).** For every evaluation of a conforming program, the
heap graph is a directed acyclic graph.

**Proof.** Evaluation is strict: a heap object is constructed only after all of
its subvalues have been constructed. Number each heap object by the step at
which its construction completes. Every edge stored in an object points to an
object with a lower number, because a new object can refer only to values that
already exist. A cycle would require an edge from a lower-numbered object to a
higher-numbered one, and no evaluation step creates such an edge.

A cycle could arise only through one of three channels, and each is closed:

- **Mutable write-back.** Constructing an object and then assigning a field to
  point back requires mutation. Values are immutable, and field assignment does
  not exist.
- **Lazy suspension.** Allocating a placeholder and filling it later requires
  non-strict evaluation. Evaluation is strict, and a binding's right-hand side
  is evaluated before the name is bound.
- **Capture into an unfinished binding.** A closure environment cannot point
  back at a binding whose value is not complete, because capture takes a
  snapshot of an existing value. Self-reference and mutual recursion of
  module-top-level functions are compile-time name facts resolved through
  static slots at link time; they are not edges written into a heap object
  during construction. Block-level self-reference is a static error.

No other channel creates an edge. Therefore the heap graph is acyclic.

Acyclicity has the following consequences:

- Reference counting is complete: no cycle can keep an unreachable object
  alive.
- Weak and unowned references have no application and do not exist.
- Derived services that traverse structure terminate, because every value is
  finite.
- Sharing an object across tasks is safe; it is a matter of counting, and
  cycles remain impossible.

Panic and abort do not unwind (Clauses 8.4.2 and 8.4.3). An abort discards the
whole process, so no release pass runs and no finalization obligation remains.

### 8.6.2 Reference Counting

Memory management is reference counting. A conforming implementation MUST
satisfy the following guarantees.

- **Automatic.** Storage is reclaimed without program action. There are no
  dangling references, no double frees, and no `free` operation in the language.
- **Bounded.** When the last reference to a heap object is released, the object
  is unreachable and its storage MUST be reclaimed no later than that moment.
  Its child references are released at reclamation; a child whose last
  reference is thereby released is reclaimed in turn.
- **Pause-free.** Reclamation MUST NOT require a tracing pause or a root scan.
- **Race-free.** A count shared between tasks is atomic. The count itself is
  never a data race.
- **Earlier permitted.** An implementation MAY reclaim an object earlier than
  the last-reference moment when static analysis proves that no reference can
  be used again. It MUST NOT reclaim later.

Reference counting is complete because the heap graph is acyclic (Clause
8.6.1). No tracing collector, ownership layer, borrowing layer, lifetime
annotation, or region annotation is part of the language. Regions and arenas
remain implementation and host techniques.

Reclamation timing is not observable by an Eko program (Clause 8.11). The
host-visible promise is specified in Clause 10.7: a host that holds a value
holds a reference, and the language guarantee is automatic, bounded,
pause-free reclamation.

### 8.6.3 Implementation Requirements

The following baseline is descriptive. An implementation MAY depart from it
provided that the guarantees of Clause 8.6.2 and the exclusions of Clause 8.11
are preserved.

- One count per heap object.
- Retain on binding, argument passing, capture, and insertion into a container.
- Release on scope exit and on rebinding.
- On reaching zero, free the object and iteratively release its child
  references.
- Objects shared across tasks use atomic counts; task-private objects may use
  non-atomic counts.
- A value that crosses a boundary, including a concurrency capture or an
  embedding boundary, is shared and uses an atomic count. Whether this is
  decided at allocation or by promotion is unspecified (Clause 10.7.3).
- Where the compiler proves exclusive access, counting may be elided. Elision
  may reclaim earlier, never later.

Capture is not special in this accounting: a closure environment holds a
reference to the captured snapshot and retains and releases it like any other
reference. Byte layout, allocation strategy, and the timing of individual
frees are implementation domain.

An implementation MAY release elements as a combinator scans a long stream, so
that the traversal is O(1) resident. This is a permitted strategy, not a
per-element guarantee.

### 8.6.4 Exhaustion

Memory exhaustion aborts. It is a process-level abort, not a catchable panic,
and it follows Clause 8.4.3. No finalizer runs and no release pass completes.

A leaked resource is reclaimed by the operating system at process exit. No
finalizer is promised. Resources that require deterministic closing use handles
with generations and the `with_*` family specified in Clauses 9.12.3 and
9.12.4, not finalization.

## 8.7 Generic Instantiation

### 8.7.1 Monomorphization

Generic declarations are compiled by monomorphization. An instantiation is
expanded at compile time for its concrete type arguments, and layout follows
the instance. Elements of `[]Float` are stored directly in 8-byte slots, while
elements of `[]Person` are references; a numeric container pays no boxing tax.

- A generic function's body is part of its interface (Clause 7.3.2). A
  downstream instantiation expands downstream.
- Instances with identical layout MAY share machine code, deduplicated at link
  time (Clause 11.6).
- Type arguments are resolved at compile time. There is no run-time type
  information, no run-time dictionary, and no dispatch table for a generic
  call.
- A variant tag is part of a value's structure, not a run-time type identity
  (Clause 7.5.1).

Monomorphization has a code-size cost: code size grows with the number of
instances. Type checking remains free of this cost; only machine-code expansion
is paid. The generated machine code itself is unspecified (Clause 2.5).

### 8.7.2 Derived Services at Instantiation

The operations available on a type variable, including `==`, `show`,
`to_json`, and `from_json`, resolve to concrete derived services at the
instantiation site (Clause 9.8). The instance selects the service for the
concrete type substituted for the variable.

Service selection is static. No run-time dictionary is passed, no dispatch
table is consulted, and no run-time type test is performed. A derived service
generated for an instance traverses the structure of that instance's values.

### 8.7.3 Infinite Instantiation

Instantiation follows the call graph of the program. If expansion does not
terminate because polymorphic recursion generates infinitely many distinct
instances, the program is rejected with a compile-time error (Clause 7.10). A
conforming program has a finite set of generic instances.

## 8.8 Iteration and Tail Calls

### 8.8.1 Iteration Combinators

Eko has no loop syntax. The forms `while`, `for`, `do`, `break`, `continue`, and
`loop` are excluded (Clause 6.10). Iteration is expressed with library
combinators and recursion. The syntax of calls, ranges, and `case` is specified
in Clauses 6.5 and 6.7; this section specifies the evaluation and stack
behavior of iteration.

A combinator that traverses a list or range applies its callback to the
elements in order. Traversal uses native iteration, not recursive call frames:
the stack usage of the traversal MUST be independent of the number of elements.
A conforming implementation MUST provide this guarantee for `each`, `map`,
`filter`, `fold`, and the other standard-library traversal combinators (Clause
9.10).

A short-circuiting combinator such as `find_by`, `any`, or `all` stops
traversing when its result is determined. Elements after the stopping point are
not evaluated, and their callbacks are not invoked. This is the language's
early-termination facility; there is no `break` or `continue`.

If a callback diverges or aborts, the traversal stops with that outcome. A
traversal does not resume after an abort.

```eko
sum_to: (n: Int) -> Int = (1..=n).fold(0, (acc, i): acc + i)
```

The result value of a combinator and the precise library semantics of each
combinator are specified in Clause 9.10.

### 8.8.2 Tail Positions

A tail position is a position whose value is the value of the enclosing
function or lambda. The tail positions are:

- the final expression of a block;
- every arm of a `case`;
- both branches of an `if`;
- after operator expansion, the right operand of `&&` or `||`;
- the final expression of a callback body, which is a tail position of that
  callback, not of the enclosing function.

A lambda has its own arrow and its own tail positions.

World bits and type arguments are orthogonal to tail position. The world bit is
erased during typing, and type arguments are resolved at compile time; a tail
call is a control-flow fact independent of both.

### 8.8.3 Self Tail Call Guarantee

A self tail call is a call to the enclosing function in tail position. Every
conforming implementation MUST compile a self tail call to a control-flow
transfer within the same stack frame. Stack space used by such a call MUST be
independent of the number of recursive calls.

The arguments of the call are evaluated before the transfer, as for any call.
The result is the result of the call. Reusing the frame does not change the
value denoted by any name and does not change the sequence of world contacts.

```eko
_sum: (xs: []Int, acc: Int) -> Int =
    case xs:
        {}       -> acc
        {h, ..t} -> _sum(t, acc + h)      // self tail call: constant stack
```

A self tail call that never reaches a base case still uses constant stack. For
example, `spin: () -> Never = spin()` is a self tail call.

One consequence is observable: frames eliminated by a self tail call do not
appear in panic backtraces. The remaining backtrace format is specified in
Annex B.6.

### 8.8.4 Cross-Function Calls

Mutual recursion within a module is legal, but the stack is not guaranteed;
stack depth follows the input structure. A tail call from one function to
another is optimized on a best-effort basis. The language does not promise
cross-function tail-call elimination, and a program MUST NOT rely on it. A
non-tail call grows the stack with the depth of the recursion.

Stack overflow aborts. It is a process-level abort, neither catchable nor a
panic (Clause 8.4.3). Diagnostics for stack overflow are best-effort, and their
granularity is unspecified.

## 8.9 Concurrency

The concurrency surface has the following signatures:

```text
Conc.scope:  (Conc, (Scope) ?-> a) ?-> a
Scope.spawn: (Scope, () ?-> b) ?-> Task b
Task.join:   (Task b) -> Option b
Task.cancel: (Task b) -> _
```

`Conc` is an opt-in capability (Clause 7.9.1). `Scope` and `Task b` are opaque
types (Clause 8.5.3). The concurrency surface spans three namespaces:
`Conc.scope` takes the capability; `Scope.spawn` is reached at a scope, and
`Task.join` and `Task.cancel` at a task, in the manner of `File` -- every
member takes its capability or handle first. The four
members are compiler-provided declarations, not capability-module source
(Clause 10.8). The world bit of a spawn or scope call is determined by its
callback through the open bit, as specified in Clause 7.9.

### 8.9.1 Scope and Spawn

`Conc.scope(conc, callback)` creates a fresh scope and evaluates `callback`
with that scope. Tasks spawned through the scope are its children. The value of
the scope expression is the value returned by `callback`.

A scope completes only after its callback has returned and every child task
has terminated. When the callback returns, the scope cancels every child that
has not terminated; scope exit waits for all children. A task cannot outlive
its scope.

`scope.spawn(callback)` creates a task and returns a `Task b` value. The
callback is a closure over the values visible at the spawn site, captured by
snapshot under Clause 8.2.3. The task is eligible to execute after `spawn`
returns; the relative order in which the parent and child execute is not
specified. The task's result is observed only through `join`.

A spawn callback's world bit joins the open bit of `spawn`. A pure callback
makes the spawn call pure; a world-touching callback makes it world-touching.
The same rule applies to the callback of `Conc.scope`.

### 8.9.2 Join and Cancel

`task.join()` waits until the task has terminated. It returns `Some v` when
the task completed normally with value `v`, and `None` when the task was
cancelled. `join` and `cancel` observe program-internal state and do not touch
the world; both are pure.

`task.cancel()` requests cancellation of a task and returns `_`. Cancellation
is cooperative. It is applied at world-call boundaries: a task that is executing
a pure computation is not interrupted at an arbitrary point. A task that never
reaches a world-call boundary therefore cannot be cancelled.

Cancellation is not an error and does not produce a panic. A cancelled task has
no result value; `join` reports `None`.

### 8.9.3 Determinism

For a pure task, the result is determined by the values captured at spawn.
Scheduling order cannot change that result, because values are immutable and
there is no communication between tasks. Cancellation has explicit sources
only, a parent's `cancel` or scope exit, so program order determines which
tasks are cancelled.

A pure concurrent program is deterministic. There are no data races: shared
values are immutable, and reference counts shared across tasks are atomic
(Clause 8.6.2). For world-touching tasks, the relative order in which calls in
different tasks reach the world is not specified; the language does not promise
an interleaving.

### 8.9.4 Boundaries

Cancellation is applied at world-call boundaries. A pure task that never
terminates hangs its scope exit rather than being interrupted; divergence is
not tracked (Clause 8.3.5). This is the concurrency consequence of the rule
that partiality is independent of the world bit (Clause 7.9).

A task cannot escape its scope. Scope exit waits for termination of every child
and cancels those still running. Cross-task sharing of immutable values is safe
and is accounted for by atomic counts; acyclicity continues to hold (Clause
8.6.1).

There are no channels, no timeouts, and no shared mutable state; the language
has no channel deadlock. A cancelled task stops at a world-call boundary and
does not resume. A task's world contacts may interleave with those of its
parent and siblings; no order among them is specified.

## 8.10 Cost Model

All magnitudes in this section are upper bounds. A conforming implementation
MUST NOT be asymptotically worse. `n` is the size of the operand, and `|xs|` is
the length of the list.

| Operation | Magnitude |
|---|---|
| Binding, argument passing, capture, insertion into a container (composite) | O(1) reference |
| Scalar arithmetic; field access `p.name` | O(1) |
| `p.with(update)` | O(number of top-level fields) shallow copy; untouched fields shared; nested partial literals paid per path |
| `xs ++ ys` (lists) | O(\|xs\|) new nodes; `ys` fully shared |
| String `++` | O(n) full copy |
| `==`, `show`, `inspect` | O(structure size) |
| Deep structural recursion | proportional to data depth; stack guarantees in Clause 8.8 |
| extern marshalling (whitelisted, Clause 10.3.5) | at most one copy per crossing of the value's size; scalars O(1); `C.In` one copy-in |
| `Bytes.to_u8s` and `Bytes.from_u8s` (Clause 9.5.2) | O(1), zero copy |

Dictionary merge magnitude is implementation-defined, because dictionaries
are persistent structures (Clause 2.5). Index-based access keeps the same accounting: `List.get`
and `Dict.get` are O(1), while `String.at` and `String.slice` are O(n) because
UTF-8 is variable-width (Clause 9.4.2).

The cost model constrains asymptotic magnitude; it does not constrain constant
factors, register allocation, or the machine code generated for a construct.

## 8.11 Excluded Observations

The following exclusions are normative. They make the representation choices
of Clause 8.5 and the instantiations of Clause 8.7 unobservable.

An Eko program cannot:

- **Compare identity.** `===`, physical equality, and address comparison do not
  exist. `==` is structural equality and is the whole of equality (Clause
  9.8.4).
- **Take the address of a value or manage memory manually.** There are no
  pointers, no address-of operator, and no manual `free`.
- **Observe reclamation.** There are no finalizers, destructors, or `drop`.
  Reclamation is not a programmable event. Resources are handled by
  generation-checked handles and the `with_*` family (Clause 9.12).
- **Hold weak or unowned references.** The heap graph is acyclic (Clause 8.6.1),
  so the vocabulary has no application.
- **Speak of deep or shallow copies.** Sharing is the whole story. An
  independent copy is constructed by rebuilding the value field by field
  (Clause 8.5.2).
- **Manage memory programmatically.** There is no `gc()`, no pause control, and
  no memory callback.
- **Write a loop.** There are no `while`, `for`, `do`, or `loop` keywords
  (Clause 6.10). Clause 8.8.1 states the replacements.
- **Use `break` or `continue`.** Early termination is combinator semantics,
  through `find_by`, `any`, and `all`, not a control-flow primitive.
- **Rely on tail-call elimination across functions.** The guarantee covers self
  tail calls only (Clauses 8.8.3 and 8.8.4).
- **Produce a lazy sequence with `yield`.** Generators are excluded; recursion
  and combinators are the iteration vocabulary (Clause 8.8.1).
