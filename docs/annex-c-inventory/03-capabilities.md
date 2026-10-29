# Annex C. Standard Library Inventory

## C.9 Capability Surface

Capabilities are opaque, unforgeable values (Clause 7.9.1). Every member of a
capability namespace takes its capability as the first parameter; `Scope.spawn`,
`Task.join`, and `Task.cancel` take their handle first, as the members of
`File` do. A member marked `!` sets the world bit;
a derivation is pure. `File`, `Scope`, and `Task t` are handles rather than
capabilities; their namespaces are included here because each is reached only
through a capability member. The capability module carries one ordinary
import, `std/time`, for the time types in `FileInfo.mtime`, the `_ClockB`
bundle, and `Clock.sleep`; it imports nothing else (Clause 9.1.3).
Every member in this section is capability-module source (Clause 9.1.1), except
the fifteen compiler-provided declarations (Clause 10.8): `Proc.exit`, the four
concurrency members, the eight `File` members, and the two advancing derivations
`In.from_lines` and `Rand.with_seed`.

| Capability | Member | Signature | Clause |
|---|---|---|---|
| `Out` | `Out.print`, `Out.println` | `(Out, String) !-> _` | Clause 7.9.1 |
| `Out` | `Out.tee` | `(Out, (String) !-> _) -> Out` | Clause 9.14.5 |
| `Out` | `Out.null` | `() -> Out` | Clause 9.14.5 |
| `In` | `In.read_line` | `(In) !-> Result (Option String) IoError` | Clause 7.9.1 |
| `In` | `In.empty` | `() -> In` | Clause 9.14.5 |
| `In` | `In.from_lines` | `([]String) -> In` | Clause 9.14.5 |
| `Fs` | `Fs.open` | `(Fs, String, OpenMode) !-> Result File IoError` | Clause 9.12.2 |
| `Fs` | `Fs.with_file` | `(Fs, String, OpenMode, (File) !-> a) !-> Result a IoError` | Clause 9.12.4 |
| `Fs` | `Fs.exists` | `(Fs, String) !-> Bool` | Clause 9.12.2 |
| `Fs` | `Fs.read` | `(Fs, String) !-> Result String IoError` | Clause 9.12.2 |
| `Fs` | `Fs.write`, `Fs.append` | `(Fs, String, String) !-> Result _ IoError` | Clause 9.12.2 |
| `Fs` | `Fs.read_bytes` | `(Fs, String) !-> Result Bytes IoError` | Clause 9.12.2 |
| `Fs` | `Fs.append_bytes` | `(Fs, String, Bytes) !-> Result _ IoError` | Clause 9.12.2 |
| `Fs` | `Fs.list_dir` | `(Fs, String) !-> Result []String IoError` | Clause 9.12.6 |
| `Fs` | `Fs.mkdir`, `Fs.mkdir_all`, `Fs.rm`, `Fs.rm_dir` | `(Fs, String) !-> Result _ IoError` | Clause 9.12.6 |
| `Fs` | `Fs.rename` | `(Fs, String, String) !-> Result _ IoError` | Clause 9.12.6 |
| `Fs` | `Fs.stat` | `(Fs, String) !-> Result FileInfo IoError` | Clause 9.12.6 |
| `Fs` | `Fs.realpath` | `(Fs, String) !-> Result String IoError` | Clause 9.12.6 |
| `Fs` | `Fs.null` | `() -> Fs` | Clause 9.14.5 |
| `Fs` | `Fs.root` | `(Fs, String) -> Fs` | Clause 9.12.5 |
| `Fs` | `Fs.read_only` | `(Fs) -> Fs` | Clause 9.12.5 |
| `File` | `File.read` | `(File, Int) !-> Result String IoError` | Clause 9.12.2 |
| `File` | `File.read_bytes` | `(File, Int) !-> Result Bytes IoError` | Clause 9.12.2 |
| `File` | `File.read_line` | `(File) !-> Result (Option String) IoError` | Clause 9.12.2 |
| `File` | `File.seek` | `(File, UInt) !-> Result _ IoError` | Clause 9.12.2 |
| `File` | `File.write` | `(File, String) !-> Result _ IoError` | Clause 9.12.2 |
| `File` | `File.write_bytes` | `(File, Bytes) !-> Result _ IoError` | Clause 9.12.2 |
| `File` | `File.flush` | `(File) !-> Result _ IoError` | Clause 9.12.2 |
| `File` | `File.close` | `(File) !-> Result _ IoError` | Clause 9.12.2 |
| `Env` | `Env.get` | `(Env, String) !-> Option String` | Clause 7.9.1 |
| `Env` | `Env.frozen` | `([String]String) -> Env` | Clause 9.14.5 |
| `Args` | `Args.all` | `(Args) !-> []String` | Clause 7.9.1 |
| `Args` | `Args.frozen` | `([]String) -> Args` | Clause 9.14.5 |
| `Clock` | `Clock.now` | `(Clock) !-> Time` | Clause 9.7.2 |
| `Clock` | `Clock.mono` | `(Clock) !-> Mono` | Clause 9.7.2 |
| `Clock` | `Clock.sleep` | `(Clock, Duration) !-> _` | Clause 9.7.2 |
| `Clock` | `Clock.frozen` | `(Time) -> Clock` | Clause 9.7.3 |
| `Rand` | `Rand.int` | `(Rand, UInt) !-> UInt` | Clause 7.9.1 |
| `Rand` | `Rand.with_seed` | `(Int) -> Rand` | Clause 9.14.5 |
| `Proc` | `Proc.exit` | `(Proc, Int) !-> Never` | Clause 8.4.3 |
| `Conc` | `Conc.scope` | `(Conc, (Scope) ?-> a) ?-> a` | Clause 8.9.1 |
| `Scope` | `Scope.spawn` | `(Scope, () ?-> b) ?-> Task b` | Clause 8.9.1 |
| `Task` | `Task.join` | `(Task b) -> Option b` | Clause 8.9.2 |
| `Task` | `Task.cancel` | `(Task b) -> _` | Clause 8.9.2 |

`read_line` yields `Ok None` at end of input; EOF is not an error
(Clause 9.2.4). `Out.tee` writes once and reports once, invoking its observer
on each write. `Out.null` swallows every write, `In.empty` reports EOF
immediately, and `In.from_lines` yields the given lines in order and then EOF
(Clause 9.14.5). `with_file` closes the handle on both the value path and the
error path (Clause 9.12.4). `read` keeps the text gate and reports invalid
UTF-8 as `Err InvalidData`; `read_bytes` never reports `InvalidData`
(Clause 9.12.2). `list_dir` returns names, not paths, in lexicographic order
and excludes `.` and `..` (Clause 9.12.6). `Fs.null` is a pure derivation that
reports `Err NotFound` for every path (Clause 9.14.5). `Fs.root` and
`Fs.read_only` are pure derivations; `root` restricts a subtree without an
existence check, and `read_only` withdraws write rights (Clause 9.12.5).
`close` advances the handle's generation; an operation on a stale handle,
including a second `close`, yields `Err Closed` (Clause 9.12.3). A short read
is end of input, not an error: the call returns `Ok` with a short value
(Clauses 9.2.4, 9.5.4).

`Env.get` yields `None` for an unset variable; presence is a domain check
(Clause 7.9.1). `Env.frozen` reads a fixed table and is pure (Clause 9.14.5).
`Args.all` yields the argument vector without the program name. `Args.frozen`
reads a fixed vector and is pure (Clause 7.9.1). `Clock.now` reads the wall
clock, `Clock.mono` reads the monotonic clock, and `Clock.sleep` accepts a
negative duration by returning immediately (Clause 9.7.2). `Clock.frozen` is a
pure derivation whose `now` and `mono` are constant and whose `sleep` returns
immediately (Clause 9.7.3). `Rand.int` is uniform over the range from zero to
the bound, exclusive of the bound; a zero bound is an invariant violation and
panics (Clause 9.2.3, Annex B.5). `Rand.with_seed` fixes the order; the
sequence itself is an implementation detail (Clause 9.14.5). `Proc.exit`
terminates the process abruptly with the given exit code (Clause 8.4.3).
`Conc.scope` and `Scope.spawn` are open-bit dispatchers; a child's world bit
joins the open bit (Clause 8.9.1). `Task.join` and `Task.cancel` are pure;
`join` yields `None` when the task was cancelled (Clause 8.9.2).
