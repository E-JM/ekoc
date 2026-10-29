# 8. Dynamic Semantics

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

Dictionary merge magnitude is implementation-defined, because dictionaries are
persistent structures (Clause 2.5). Index-based access keeps the same
accounting: `List.get` and `Dict.get` are O(1), while `String.at` and
`String.slice` are O(n) because UTF-8 is variable-width (Clause 9.4.2).

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
