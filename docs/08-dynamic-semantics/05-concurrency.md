# 8. Dynamic Semantics

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
