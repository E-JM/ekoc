# 8. Dynamic Semantics

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
