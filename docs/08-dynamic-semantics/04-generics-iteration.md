# 8. Dynamic Semantics

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
