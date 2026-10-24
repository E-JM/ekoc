# Representation and Execution

Eko compiles to machine code. There is no virtual machine, no bytecode, and no interpreter loop at run time, so representation and memory cannot be delegated to an implicit runtime layer. The specification must answer three questions directly: how a value is laid out, how a name shares it, and how its storage is reclaimed. §1 and §2 answer layout and sharing; §3 proves the graph property on which the memory strategy of §4 rests; §5–§7 record the optimization freedom, the monomorphization strategy, and the cost model that follow; §8 covers iteration, where representation choices surface as stack guarantees; §9 lists the observations the language excludes.

## 1. Value representation

An Eko value is a scalar held directly, an immutable heap object reached through references, or an opaque value whose representation belongs to the implementation. The governing principle is one sentence: **names hold references; operations see structure.** Binding, argument passing, capture, and storing into a field or element copy a reference, never a value, and no operation in the language can observe that two names designate the same object.

### 1.1 Scalars

| Value class | Representation |
|---|---|
| `Bool`, `Char`, `Int`, `UInt`, `Float` | stored directly in registers and slots; no heap |
| `_` (unit) | zero-sized |
| `Never` | unrepresented |

Scalar values are held directly in registers and slots — including inline slots inside a heap object — and are never heap objects themselves. `Bool` is a standard-library algebraic type and `Char` a standard-library scalar type; representation follows the value domain, not the declaration form.

The unit type has exactly one value, and that value occupies no storage. `Never` has no values at all, so it needs no representation: no expression of type `Never` produces a value (04-types.md §10).

### 1.2 Composite values

Except for the scalars of §1.1, values of the composite families — records, tuples, lists, dictionaries, algebraic values, strings, byte sequences, and closures — are immutable heap objects. A name, parameter, field, element, payload, or capture slot holds a reference to such an object. Binding, argument passing, capture, and storing a value into a field, element, or payload never copy the value; they add an O(1) reference, independent of the value's size.

```eko
p2 = p                     // O(1): shared, zero copy
big2 = big                 // the same String, not a copy
q = p.with({age: 37})      // top-level shallow copy; untouched fields shared
n2 = p2.name               // O(1) field access; p2 and p are the same object
```

`with` is the record update service (08-builtins.md §6): it copies the top level and shares every field the literal does not name. The language has no deep-copy or shallow-copy vocabulary (§9); an independent copy is built by reconstructing the value field by field (04-types.md §5). A `String` is an immutable UTF-8 sequence on the heap (08-builtins.md §2); bindings share it, and `++` produces a fresh string by copying its operands (§7).

Values of opaque type — capabilities, file handles, `Range` — have implementation-defined representation. A program can use only the operations those interfaces expose. A handle additionally carries a generation, so that use after close is detected rather than undefined (06-world.md §4). Opacity is what keeps representation freedom from leaking into the language.

Eko has value semantics: `==`, `show`/`inspect`, and pattern matching are defined over structure (08-builtins.md §6), never over addresses, so sharing cannot be detected.

### 1.3 Capture semantics

A closure captures the value a name holds at the moment of capture, and it captures that value by reference: capturing is O(1) and allocates no copy.

Rebinding with `:=` (03-control.md §1) introduces a new binding for the name. It does not write into an environment that has already been captured, so a closure created before a rebinding keeps seeing the old value, and a closure created after it sees the new one. There is no write-back from a binding into a captured environment.

```eko
p: [name: String, age: Int] = {name: "ada", age: 36}
get_name: () -> String = (): p.name   // captures the record by reference, no copy
p := {name: "grace", age: 41}
get_name()                  // "ada"
```

This snapshot rule is the third premise of §3.

## 2. Recursive types

A recursive type is a type whose declaration refers to itself, directly or through another declaration in the same module (04-types.md §6). One layout rule makes recursive types tractable: **every recursive position is a pointer.**

Two consequences follow directly.

1. **Size is always finite.** The payload of `Succ Nat` is a pointer, and expansion of the type stops at the pointer. Recursive declarations (04-types.md §6) therefore need no new well-formedness check: a recursive type with no base case is legal and has zero inhabitants, and inhabitance is not checked, in the same spirit as `Never` (04-types.md §10).
2. **Derived services terminate.** `show`, `==`, and `<` are generated as functions that recurse over structure (08-builtins.md §6). Termination follows from §3: every value is finite, so every traversal stops.

```eko
Nat:     type = Zero | Succ Nat
IntList: type = Nil | Cons [Int, IntList]
Tree:    type t = Leaf t | Node [Tree t, Tree t, t]
Person:  type = [name: String, friends: []Person]   // recursion through a container

sum: (xs: IntList) -> Int =
    case xs:
        Nil         -> 0
        Cons {h, t} -> h + sum(t)      // structural recursion; stack per §8.2
```

Mutually recursive type declarations within a module have the same standing as self-recursive ones, and recursion through an applied type constructor (`Tree t` appearing in the payload of `Node`) is ordinary: the recursive position is still a pointer.

## 3. The acyclicity theorem

Three properties together rule cycles out: evaluation is strict, values are immutable, and captures are snapshots.

**Theorem.** The heap graph of an Eko program — nodes are heap objects, edges are references stored in fields, elements, payloads, and capture slots — is always a directed acyclic graph. Opaque values contribute no edges to this graph.

**Proof.** Evaluation is strict: a value is constructed only after all of its subvalues have been constructed. Number each heap object by the moment its construction completes. Every edge points from a later object to an earlier one, because a new value can only reference values that already exist. An edge closing a cycle would have to point from an earlier object to a later one, and no such edge is ever created.

A cycle could arise only through one of three channels, and each is closed:

- **Mutable write-back** — construct a value, then assign a field so that it points back. Values are immutable (03-control.md §1), and field assignment does not exist.
- **Lazy suspension** — allocate a placeholder and fill it in later. Evaluation is strict, and a binding's right-hand side is evaluated before the name is bound (03-control.md §1); nothing is filled in after the fact.
- **Capture into an unfinished binding** — let a closure environment point back at a binding whose value is not complete. Captures are snapshots (§1.3). Self-reference and mutual recursion of module-top-level functions are compile-time name facts, resolved through static slots at link time; such a reference is not an edge written into an object during construction. Block-level self-reference does not exist: using a name before it has a value is a compile error. Lifting such a binding to module level is a mechanical transformation.

Four consequences follow.

- **Reference counting is complete.** When a count reaches zero, the object is freed and its child references are released; a child that thereby reaches zero is freed in turn. No cycle can keep an object alive, so there is no leak channel that counting would miss, and no cycle collector exists.
- **Weak and unowned references do not exist.** Strong and weak references are self-rescue vocabulary for mutable object graphs; Eko's graph has no cycles, so the vocabulary has no application.
- **Derived services terminate.** See §2.
- **Cross-task sharing is safe.** Tasks capture a snapshot, and sharing an object across tasks is then only a matter of counting (06-world.md §8); cycles remain impossible.

Panic does not unwind (06-world.md §3). An abort discards the whole process, so no release pass runs and no finalization obligation remains to be discharged. The memory strategy and the failure model are consistent.

## 4. Memory management

Representation is fixed by §1–§2; lifetime is fixed here. Memory management is reference counting, specified at the normative level: storage is reclaimed when its count reaches zero — or earlier, never later — without pauses or root scanning. Its entire infrastructure is three operations — allocate, retain, release — emitted by code generation; it is not a runtime service. The embedding contract (06-world.md §2) is correspondingly plain: a host that holds a value holds a reference, with no root registration and no pause negotiation.

### 4.1 Reference counting

Every conforming implementation guarantees the following.

- **Automatic.** There are no dangling references, no double frees, and no `free` vocabulary. Memory safety is the floor of the language, not a capability: capabilities are explicit, and the floor is free.
- **Deterministic.** Storage is reclaimed no later than the moment its count reaches zero, with no pauses and no root scanning. Determinism is unobservable to Eko programs, which have no way to probe reclamation (§9), but observable to the host through the embedding contract; the promise is written for the host.
- **No worse in magnitude.** The magnitudes of §7 are upper bounds. Bulk release and count elision are implementation domain, and count elision may cause storage to be reclaimed earlier than the baseline, never later.

Cross-task sharing uses atomic counts (§4.2; task semantics are 06-world.md §8), so the count itself is never a data race.

No tracing collector exists. Given §3, tracing would be redundant infrastructure, and its pauses and root scanning would contradict the determinism promise. No ownership or borrowing layer exists either: signatures carry no lifetime or region annotations, and values may flow and be captured freely. Regions and arenas are not language features; they remain implementation- and host-side techniques.

Memory exhaustion aborts: a process-level abort, not a catchable panic, exactly as for stack overflow (§8.4). A leaked resource is reclaimed by the operating system at process exit, and no finalizer is promised (06-world.md §4).

### 4.2 Implementation requirements

The following is the reference baseline. It is descriptive: an implementation may depart from it wherever the guarantees of §4.1 and the exclusions of §9 are preserved.

- One count per heap object.
- Retain on binding, argument passing, capture, and insertion into a container; release on scope exit and on rebinding.
- On reaching zero, free the object and iteratively release its child references.
- Objects shared across tasks use atomic counts; task-private objects skip atomics.
- A value that crosses a boundary — a Conc capture or an embedding boundary —
  is shared: its count is atomic; whether this is decided at allocation or by
  promotion is implementation domain (`11-ffi.md §7.3`).
- Where the compiler proves exclusive access, counting is elided.

Capture is not special in this accounting: a closure environment holds a reference to the captured snapshot (§1.3), retained and released like any other reference. Capture semantics themselves are fixed in §1.3; this section only accounts for the references they create.

Streaming is an engineering dividend of the strategy: `xs.each` releases elements as it scans, so a long stream is O(1) resident. This is recorded as a consequence of the chosen strategy, not as a per-element contractual guarantee.

## 5. Optimization freedom

The observation surface is empty: Eko has no identity comparison, no mutation, no finalizers, and no memory-footprint vocabulary (§9). The optimization surface is therefore fully open. Any transformation is permitted provided it does not make an excluded observation possible.

| Optimization | Technique | Invariant |
|---|---|---|
| Unboxing | small non-recursive composites inlined into slots | recursive positions remain indirect (§2) |
| Stack allocation / escape analysis | non-escaping objects constructed directly on the stack | proven non-escape |
| Count elision | statically proven exclusive access skips retain and release | earlier only, never later (§4.1) |
| Shared instantiations | monomorphized instances with identical layout reuse the same machine code | link-time deduplication (§6) |
| `ctype` freeze | layout records are pinned by their declaration | the one explicit exception; ordinary records keep the full surface (`11-ffi.md §3.3`) |

The specification fixes shape and magnitude; the implementation fixes bytes and timing. In particular, the specification promises the model and the magnitudes of §7, not the byte layout of any particular value.

## 6. Monomorphization

Generics are compiled by monomorphization: an instantiation is expanded at compile time, and layout follows the instance. Elements of `[]Float` are stored directly in 8-byte slots, while elements of `[]Person` are references; numeric containers pay no boxing tax.

```eko
xs: []Float  = {1.0, 2.0}    // elements stored directly
ys: []Person = {ada, bob}    // elements are references
```

- **Generic bodies travel with the interface.** A generic function's body is part of its interface (05-names.md §10); downstream instantiations expand downstream, and machine code for identical instances is deduplicated at link time.
- **Derived services resolve at instantiation.** The operations available on a type variable — `==`, `show`, `to_json`, `from_json` — resolve to concrete derived services where the instance is created (04-types.md §8, 08-builtins.md §6). There is no runtime type information and no dispatch table. Variant tags are part of a value's structure, not a type identity (04-types.md §4).
- **Costs are recorded.** Code size grows with the number of instances, and deduplication and shared bodies are permanent work. Instantiation follows the call graph; an expansion that does not terminate — polymorphic recursion generating infinitely many instances — is a compile error. Type checking remains free, and only machine-code expansion is paid.

There are no runtime dictionaries. Uniform boxing with a runtime dictionary would reintroduce exactly the runtime dispatch surface that compile-time type identity removes.

## 7. Cost model

All magnitudes are upper bounds; no conforming implementation is asymptotically worse. `n` is the size of the operand, and `|xs|` the length of the list.

| Operation | Magnitude |
|---|---|
| Binding, argument passing, capture, insertion into a container (composite) | O(1) reference |
| Scalar arithmetic; field access `p.name` | O(1) |
| `p.with({...})` | O(number of top-level fields) shallow copy; untouched fields shared; nested partial literals paid per path |
| `xs ++ ys` (lists) | O(\|xs\|) new nodes; `ys` fully shared |
| String `++` | O(n) full copy |
| `==`, `show`, `inspect` | O(structure size) |
| Deep structural recursion | proportional to data depth; stack guarantees layered per §8.2 |
| extern marshalling (whitelisted, `11-ffi.md §3.5`) | at most one copy per crossing of the value's size; scalars O(1); `C.In` one copy-in | 
| `Bytes.to_u8s` / `from_u8s` (`08-builtins.md §3.2`) | O(1) — zero copy |

Dictionary merge magnitude is implementation domain, because dictionaries are persistent structures. Index-based access keeps the same accounting: `List.get` and `Dict.get` are O(1), while `String.at` and `String.slice` are O(n) because UTF-8 is variable-width (05-names.md §4, 08-builtins.md §2).

## 8. Iteration

### 8.1 No loop syntax

`while`, `for`, `do`, `break`, `continue`, and `loop` are never part of Eko. Iteration is expressed with library functions and recursion.

The rationale is short. Values are immutable (03-control.md §1), so a loop variable is the ceremony of mutable state: an increment has nowhere to take root, while an accumulator carried as a parameter has the same shape with zero assignment. A loop body does two things — "for each element" and "how to combine" — and `each` and `fold` give each one a name; named operations are greppable, bit-transparent (`?` dispatchers, 06-world.md §9), and replaceable one at a time. `case` and calls are the entire control flow; `if` is sugar over `case` (03-control.md §4); and `while` is an implicit `goto`. Combinators compose (`xs.filter(f).map(g)`, 05-names.md §3); a syntactic loop does not.

### 8.2 Three idioms

| Idiom | Form | Stack guarantee |
|---|---|---|
| Combinator | `xs.each/map/filter/fold/…`, `(1..=n).fold` | native iteration; stack-safe at any length (normative) |
| Self tail recursion | a function calls itself in tail position | always compiled to a jump (normative) |
| Other recursion | structural recursion, mutual recursion, non-tail calls | none; stack depth follows data depth |

Combinators are the default (09-library.md §5). Structural recursion through the pattern repertoire (03-control.md §5) is for shapes the library does not cover, and self tail recursion is for state machines and loops with complex state.

```eko
// for form → Range combinator
sum_to: (n: Int) -> Int = (1..=n).fold(0, (acc, i): acc + i)

// while form → self tail recursion with a guard
count_down: (out: Out, n: Int) !-> _ =
    if n > 0:
        out.println(show(n))
        count_down(out, n - 1)            // self tail: a jump, constant stack

// structural recursion → pattern repertoire; elegant, but deep lists risk the stack
sum: (xs: []Int) -> Int =
    case xs:
        {}       -> 0
        {h, ..t} -> h + sum(t)            // not tail; use List.fold in ordinary code

// the same function in self tail form: an accumulator parameter, a private helper
_sum: (xs: []Int, acc: Int) -> Int =
    case xs:
        {}       -> acc
        {h, ..t} -> _sum(t, acc + h)      // self tail: constant stack

// infinite loop → Never recursion (04-types.md §10)
spin: () -> Never = spin()
```

The loop habits map as follows.

| Loop habit | Eko vocabulary |
|---|---|
| Counting loop | `(a..=b).each/map/fold` (Range combinators) |
| Traversing loop | `xs.each/map/…` (list combinators) |
| `while` loop | self tail recursion + guarded `if`, accumulator carried as a parameter |
| `break` | `find_by`/`any`/`all` — first-hit termination |
| `continue` | `filter` pre-selection, or a guarded branch |
| Infinite loop | `-> Never` recursion: a typed form whose return type states that it never returns |

### 8.3 Tail calls

**Promise.** A self tail-recursive call — a call to the function itself in tail position — is compiled to a control-flow transfer within the same stack frame; stack space is independent of the number of recursive calls. Every conforming implementation guarantees this.

Tail positions are:

- the final expression of a block;
- every arm of a `case`;
- both arms of an `if`;
- after operator sugar is expanded: in `a || b`, `b` is in tail position along with the `if` arm into which the operator expands (02-syntax.md §6);
- the final expression of a callback body, which is the tail position of that callback, not of the enclosing function: each lambda has its own arrow (03-control.md §2).

Bits and types are orthogonal to this. The world bit is erased (06-world.md §5), and type arguments are resolved at compile time (§6); a tail call is a pure control-flow fact, unaffected by either.

One observable cost: eliminated frames do not appear in panic backtraces.

### 8.4 No cross-function promises

- **Mutual recursion** within a module is legal (05-names.md §10), but the stack is not guaranteed. Depth follows the input structure; shallow domains are idiomatic, and deep domains should move to self tail recursion or combinators.

```eko
even: (n: Int) -> Bool = if n == 0: True else: odd(n - 1)
odd:  (n: Int) -> Bool = if n == 0: False else: even(n - 1)
// legal; a deep chain risks the stack
```

- **Cross-function tail calls** are optimized on a best-effort basis and are not promised. Backends differ in their tail-call guarantees, and the specification promises only what every conforming implementation can deliver.
- **Non-tail recursion** grows the stack with data depth. On deep data, first ask whether a combinator can express the traversal.
- **Stack overflow aborts.** It is a process-level abort, neither catchable nor a panic. Diagnostics are best-effort, and their granularity is implementation domain.

## 9. Excluded observations

The following exclusions are normative; they are what makes §5 sound. An Eko program cannot:

- **compare identity.** `===`, physical equality, and address comparison do not exist. `==` is structural equality and is the whole of equality (08-builtins.md §6).
- **take the address of a value or manage memory manually.** There are no pointers, no address-of operator, and no manual `free`.
- **observe reclamation.** There are no finalizers, destructors, or `drop`. Reclamation is not a programmable event; resources are handled by generation-checked handles and the `with_*` family (06-world.md §4).
- **hold weak or unowned references.** Cycles are structurally impossible (§3), so the vocabulary has no application.
- **speak of deep or shallow copies.** Sharing is the whole story; an independent copy is built by reconstructing the value field by field (04-types.md §5).
- **manage memory programmatically.** There is no `gc()`, no pause control, and no memory callback.
- **write a loop.** There are no `while`, `for`, `do`, or `loop` keywords; §8.2 gives the replacements.
- **use `break` or `continue`.** Early termination is combinator semantics — `find_by`, `any`, `all` — not a control-flow primitive.
- **rely on tail-call elimination across functions.** The promise covers self tail calls only (§8.3, §8.4).
- **produce a lazy sequence with `yield`.** Generators are excluded; recursion and combinators are the iteration vocabulary (§8.2).

Each exclusion is also a guarantee: with no way to observe representation, the optimization freedom of §5 and the monomorphization strategy of §6 remain invisible, which is what makes them sound.
