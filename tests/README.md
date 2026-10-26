# Eko Conformance Tests

The normative clauses are the specification; this directory is their
executable reading. Positive snippets conform to the clauses cited in their
comments; negative snippets are specified rejections.

## Positive tests

`conformance/` is a flat directory of Eko modules. The file stem is the module
name; `import` paths locate files (`Clause 7.2.1`, `Clause 7.2.2`). A
module proves a topic by declaring top-level bindings whose annotation slot
holds `test`:

```eko
smoke: test =
    assert_eq(1 + 1, 2)
```

The test runner collects roots — modules that contain at least one `test`
binding — compiles each root separately, executes every test in it, and
captures a panic per test (`Clause 9.14.6`). A panic marks that test as
failed; the rest of the module still runs. Tests do not enter the module
interface and are absent from ordinary compilation. A module with no `test`
binding is an ordinary dependency under a test root, which is exactly how
`geolib.eko` serves `geolib_use.eko`.

Tests use the std assertions. `assert(c)` panics when `c` is `False`;
`assert_eq(a, b)` locks both arguments to one type, compares with derived
`==`, and reports failure through `inspect` (`Clause 9.14.3`). A
comparison of capabilities or functions is therefore a compile error, not a
test failure.

## Determinism

Tests never touch the real world. Code under test consumes capabilities;
the test feeds derived capabilities: `Out.null`, `Out.tee`, `In.from_lines`,
`Fs.null`, `Env.frozen`, `Args.frozen`, `Clock.frozen`, `Rand.with_seed`
(`Clause 9.12`, `Clause 9.14.5`). These constructors are pure,
need no host, and make every assertion reproducible. A test binding has no
arrow, so it has no world bit to reconcile; world-touching calls inside it
are evaluated normally.

## Negative tests

`negative/manifest.md` lists programs that must fail to compile. Each entry
carries an id, a minimal snippet, and the expected outcome: a published
diagnostic code from `Annex B.3` where one exists, otherwise
`compile error: <reason>` with the normative clause. Negative cases are
specifications of rejection, not tests that run.

## Conventions

- Four spaces per indentation level; tabs are an `E-tab` error
  (Clause 5.2.3).
- Test and helper names are snake_case and unique per module.
- Helpers are module-private with a leading `_`; tests may call them.
- Every module compiles standalone; `geolib_use.eko` imports only `geolib`.
- Strings and comments are English, and every module remains small enough to
  read as documentation.
