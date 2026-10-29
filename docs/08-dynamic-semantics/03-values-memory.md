# 8. Dynamic Semantics

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
