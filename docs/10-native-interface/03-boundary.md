# 10. Native Interface

## 10.4 Failure at the Boundary

The following table specifies the behavior at the boundary.

| C behavior | Eko behavior |
|---|---|
| error code, `errno`, or NULL sentinel | data; the extern returns it; `Result` or `Option` wrapping is library-layer work (Clause 10.2.4); `C.get_errno` reads `errno` and is pure (Clause 10.3.2) |
| segmentation fault or `SIGABRT` | process abort (Clause 8.4.3); not catchable |
| `longjmp` or a C++ exception crossing the boundary | abort; taming the crossing, by capturing with `setjmp` before it, is shim work (Clause 10.3.6) |
| panic inside a callback | abort propagates into C; C does not unwind |
| `EINTR` | the host MAY retry internally; a user extern promises nothing |

The choice between `Result`, `Option`, and panic follows the taxonomy of
Clause 9.2. External failure with a reason is `Result` at the wrapper. An
invariant violation that the wrapper itself asserts MAY panic. The extern
mechanism chooses none of them; it returns data or aborts.

## 10.5 Callbacks

Callbacks are synchronous only. Where C takes a function pointer and a context,
an Eko closure is passed. The compiler generates a trampoline whose lifetime is
the call. The callback's parameter and result types MUST follow the marshalling
whitelist (Clause 10.3.5). The closure's arrow carries its world bit as usual;
the bit lives on arrows and does not infect data (Clause 7.9.2).

A callback that outlives its call is undefined behavior (Clause 2.6). No
vocabulary makes a closure outlive its call. There is no asynchronous
registration form. The absence is an absence of vocabulary, not a semantic
invariant: a future callback registration form would be new vocabulary and would
not contradict any promise in this clause.

Reentrancy from C into Eko and back into C is permitted. Self tail calls remain
jumps on the Eko side (Clause 8.8.3).

## 10.6 Linking and Symbols

The declaration names the symbol. The build layer names the libraries, through
command-line `-l` flags or host link configuration. Library names are
platform-dependent. Conditional compilation does not exist, so a library name
has no place in a declaration.

Platform variance, including symbol decoration and library sets, is the business
of the host and the library.

Every extern symbol MUST resolve at link time. An unresolved symbol is a link
error.

Symbols beginning with `eko_` belong to the host: `eko_retain`, `eko_release`,
and the veneer entries (Clause 10.7.1). An extern declaration that names a
reserved symbol is a compile error with code `E-reserved-symbol`. The
restriction is on the symbol name, not on the Eko binding name.

Static linking and dynamic linking are implementation domain.
