# Design Record — Lowering Invariants for Self-Hosting

Non-normative. Ruling clauses: 11.1–11.6. The implicit protocols the
lowerer and the runtime keep between them. An Eko implementation of the
lower stage must honor these or produce different reference behavior;
they are recorded here so a self-hosting effort can find them. They are
implementation invariants, not language promises.

## 1. The borrowed-argument ABI

The calling convention of the emitted functions and of every runtime
entry they call: the caller keeps every argument value alive across the
whole call, a parameter is borrowed — the callee retains only what it
stores — and the caller's own frame releases its arguments' owning
references when its scopes close. A call argument the caller's frame
already owns travels without a keep, and a parameter the body only
reads stays borrowed, aliasing its ABI slot with no keep born. Any
indirect-call mechanism (interfaces, dynamic dispatch) must honor the
same invariant, or its arguments need explicit keeps again.

## 2. The parameter-borrow analysis

Whether a parameter may stay borrowed for the whole body is decided by
two scans over one child walk: the rebind scan asks whether the body
rebinds the parameter — a rebind releases the local's own reference
before the new value moves in — and the mentions scan asks whether the
body names the parameter anywhere, entering lambdas, since a capture
reads the slot. The shallow scan stops at a lambda, whose body owns its
own scopes and cannot rebind a capture.

## 3. The pending-closure queue

An inverted binding and a lambda body do not emit where they stand:
their text is deferred to a pending queue that drains after the
enclosing function's own body, so a body's reads of names the closure
uses resolve against the emission state of the function, not of the
closure. The queue is a linked list drained to completion; a closure
lowered while another drains appends, it does not recurse.

## 4. The self tail-call transform

A direct call to the function being emitted, standing in one of its
tail positions (the body's final expression, if-branch tails, case-arm
body tails, nested block tails), lowers to a frame-local jump: every
argument evaluates into a temp first, then the abandoned references
release, then each owned parameter takes its temp, and the body
reenters at its retry label. The stack the call uses is constant; the
C compiler's own tail optimization is never relied upon.

## 5. The clean-stack walk

Every scope close releases its owned locals in reverse declaration
order, and the releases are ordinary statements in the emitted body —
no cleanup runs on the C stack. A diverging position (a `Never`-typed
tail) skips the releases of the scopes it leaves: nothing observes the
frame again.
