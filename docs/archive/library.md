# Design Record — Standard Library, Testing, and Diagnostics

Non-normative. Rulings: `09-library.md`. Rejected alternatives, prior art,
and rationale.

## 1. The library principle: surface without syntax

Every library addition is a function in an existing type namespace. No new
syntax, binding form, literal, or format-string language enters with the
library (`09-library.md` intro).

- **No format DSL.** `show` handles display; formatting is explicitly
  invoked functions (`to_hex`, `to_fixed`, `to_exp`, `pad_left`). Names are
  greppable; format strings are not. Permanently excluded.
- **Float mathematics: out-of-domain is a value, not an error.**
  `sqrt(-1) = NaN`, `log(0) = -Inf` follow the hardware; predicates
  (`is_nan`, `is_inf`, `is_finite`) accompany them. The checked gate sits
  at the conversion boundary (`to_int`/`to_uint` → `None`); inside the
  value domain operations are free. libm bit differences are documented per
  implementation — determinism is claimed per platform.
- **Stable sort is the only commitment** (`sort_by`); unstable options
  never exist, and quicksort-style internals are unobservable
  implementation freedom.
- **`Set` has no notation and no literal**; construction goes through
  `Set.of`. The Dict key domain is the data types that have `==`
  (capabilities and functions excluded); structural equality is key
  equality. Float keys: a NaN key always misses (`NaN != NaN`), `0.0` and
  `-0.0` are the same key — the IEEE doctrine extended, honestly recorded.
- `slice` is a domain check → `Option`, never clamping — the contrast with
  truncating `take`/`drop` is deliberate and stated.

## 2. Testing: the rejected framework surface

Testing adds one annotation-slot word (`test`), prelude assertion
functions, and pure capability constructors — and refuses everything else.
Each refusal with its reason (`09-library.md §6`):

| Rejected | Reason |
|---|---|
| Test-file suffix (`_test.eko`) | A suffix convention creates a second-class module whose private surface is unreachable, forcing the target to be published for tests. The same file is the same truth |
| Attributes (`#[test]`, decorators) | The annotation slot already exists, with `type` as the precedent; an attribute layer is a second concept for one idea |
| `describe`/`it`, nested suites | Nesting is narrative sugar; a table plus a pure `_helper` is equivalent and adds no surface |
| `setup`/`teardown` hooks | Resources go through `with_file` and construction through pure functions; hooks are implicit control flow |
| Mock frameworks | A mock is a derived capability (`Out.null`, `Fs.null`, `Clock.frozen`); a framework would be a second vocabulary for the capability system |
| Parameterized-test syntax | The table is the parameter and `each` is the driver |
| Bit exemption / relaxed checking in tests | Same compiler, same rules; determinism comes from values, not from a lie |

Tests live **in the module under test**: the private surface is directly
testable (`_parse_sex` callable as it stands), test helpers stay private,
and the interface never lies on account of tests. Tests cannot obtain real
capabilities — construction right belongs to the host and a test has no
parameter table — so the capability layer is structurally deterministic.
A test block has no arrow and therefore no bit to reconcile; world-touching
calls inside are evaluated as usual.

The `assert_eq` failure message goes through `inspect` — the structural
channel is never overridable, which is exactly why it discharges the
diagnostics promise. `assert_ne` mirrors it. Custom failure messages
compose existing vocabulary (`if cond: panic(...)`).

## 3. Diagnostics: codes, warnings, and the contract

- **English messages, always.** Toolchain convention and ecosystem
  affinity: error text stays searchable and citable, independent of the
  language a document is written in.
- **Symbolic codes, never numeric.** `E-` plus a kebab-case semantic word
  is greppable; a numeric code requires a lookup table and cannot be
  grepped. A published code is never reused and never redefined — the code
  is the stable anchor while message wording may evolve; documentation,
  issues, and tests cite codes, not messages.
- **The warning list is closed and is a contract.** Exactly `W-unreachable`
  and `W-unused`; users perform zero-warning engineering against this list,
  so a new warning requires amending the specification, never
  implementation discretion.
- **Warnings are not suppressible.** No `#[allow]`, no pragma, no comment
  switch; suppression would be a lie against the contract. Silencing means
  fixing: `_x` prefix or deletion for `W-unused`; restructure for
  `W-unreachable`. Parameters never participate in `W-unused` — callbacks
  receive parameters positionally and surplus positions are normal.
- **Bit diagnostics are always errors, never warnings** — the one domain
  exempt from the general three-severity model (an earlier draft banned
  the warning channel entirely; that was withdrawn, and the bit-domain
  exemption is what remains).
- **Runtime panic output is not a diagnostic** but its CLI format is
  normative: `panic [P-code]: message`, backtrace with `at` frames
  (self-tail-eliminated frames absent), exit code 1; the `P-` family
  follows the same freeze discipline as `E-`/`W-`. Aborts (stack overflow,
  memory exhaustion) are a separate class with a best-effort single line
  and a platform abort exit.

The diagnostic style rules (start lowercase, no trailing period, state the
fact, repair suggestions on a hint line, names in backticks) are normative
so that implementations cannot degrade the reading experience — the
vocabulary's value is exactly its uniformity.
