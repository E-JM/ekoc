# Design Record — Standard Library, Testing, and Diagnostics

Non-normative. Ruling clauses: 9.10–9.15, Annex B. Rejected alternatives,
prior art, and rationale.

## 1. Surface without syntax

Every library addition is a function in an existing type namespace; no syntax,
binding form, literal, or format language enters with the library
(`Clause 9` intro).

- **No format DSL.** `show` handles display; formatting is named functions
  (`to_hex`, `to_fixed`, `to_exp`, `pad_left`) — greppable, permanently
  excluded.
- **Float mathematics: out-of-domain is a value, not an error.** `sqrt(-1)`
  is `NaN`, `log(0)` is `-Inf`, following the hardware, with `is_nan`,
  `is_inf`, `is_finite` alongside. The checked gate sits at the conversion
  boundary; libm bit differences are documented per implementation, so
  determinism is claimed per platform.
- **Stable sort is the only commitment** (`sort_by`); unstable options never
  exist and quicksort-style internals are unobservable freedom.
- **No implicit notation.** `Set` has no literal (`Set.of`); `slice` is a
  domain check returning `Option`, never clamping, while `take`/`drop`
  truncate deliberately.
- **Dict keys are the types that have `==`.** Structural equality is key
  equality; a NaN key always misses (`NaN != NaN`), `0.0` and `-0.0` are the
  same key — the IEEE doctrine extended and recorded.

## 2. Testing

Testing adds one annotation-slot word (`test`), the three assertion functions
opened by the injected selective import, and pure capability constructors —
and refuses everything else (`Clause 9.14`). The assertions are std source; a
failure is `P-panic` (`Clause 9.14.3`), not a dedicated code.

Tests live **in the module under test**: the private surface is directly
testable, helpers stay private, and the interface never lies for tests. A test
obtains no real capabilities — construction belongs to the host and a test has
no parameter table — so the capability layer is structurally deterministic. A
test block has no arrow and therefore no bit. `assert_eq`'s message goes
through `inspect`: the structural channel is never overridable, which is why it
discharges the diagnostics promise.

| Rejected | Reason |
|---|---|
| `_test.eko` suffix | creates a second-class module; the target must be published for tests |
| attributes (`#[test]`) | the annotation slot already exists, with `type` as precedent |
| `describe`/`it`, suites | nesting is narrative sugar; a table plus a pure `_helper` is equivalent |
| `setup`/`teardown` | `with_file` and pure construction; hooks are implicit control flow |
| mock frameworks | a mock is a derived capability (`Out.null`, `Fs.null`, `Clock.frozen`) |
| parameterized-test syntax | the table is the parameter, `each` the driver |
| relaxed checking in tests | same compiler, same rules; determinism comes from values |

## 3. Diagnostics

- **English messages, always**: error text stays searchable and citable
  independent of document language.
- **Symbolic codes, never numeric.** `E-` plus a kebab-case word is greppable;
  numbers need a lookup table. A published code is never reused or redefined —
  the code is the stable anchor while wording evolves.
- **The warning list is closed and is a contract**: exactly `W-unreachable`
  and `W-unused`, so a new warning means amending the specification.
- **Warnings are not suppressible**: no `#[allow]`, no pragma, no comment
  switch. Silencing means fixing — rename or delete for `W-unused`, restructure
  for `W-unreachable`. Parameters never participate in `W-unused`.
- **Bit diagnostics are always errors**, the one domain exempt from the
  three-severity model.
- **Panic output is not a diagnostic, but its CLI format is normative**:
  `panic [P-code]: message`, backtrace with `at` frames (self-tail-eliminated
  frames absent), exit code 1; the `P-` family follows the `E-`/`W-` freeze
  discipline. Aborts are a separate class with a best-effort single line.

The style rules — start lowercase, no trailing period, state the fact, hints on
their own line, names in backticks — are normative, because the vocabulary's
value is its uniformity.

## 4. Rejected surface

| Area | Rejected | Reason (clause) |
|---|---|---|
| Library | format strings / DSL | ungreppable; names carry the operation |
| Library | saturating or panicking conversions | silent misrepresentation (`9.2`) |
| Library | unstable sort options | an unobservable commitment split into a choice |
| Library | clamping `slice` | a domain check returning `Option` is honest |
| Diagnostics | numeric codes | ungreppable without a lookup table |
| Diagnostics | suppressible warnings | a lie against the closed-list contract |
| Diagnostics | warning-severity bit diagnostics | the bit domain is error-always |
| Diagnostics | localized messages | breaks citation and search |
