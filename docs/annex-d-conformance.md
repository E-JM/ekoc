# Annex D. Conformance Suite

## D.1 Purpose

This annex is normative and specifies the conformance suite that accompanies
this document. The suite has two test classes and a runner: positive modules,
which compile, and negative entries, which are rejected at compile time. This
annex defines the required outcome of each class, the determinism conditions
on positive tests, the mapping from each positive module to the clauses it
exercises, and the procedure for a conformance run. Clause 11 is an
implementation requirement and is not exercised by the suite; it is verified by
inspection of the implementation.

The suite is a reading of this document, not a substitute for it (Clause 2.7).
A test that conflicts with a normative clause is an error in the suite; the
clause governs. An implementation that passes the suite is not thereby
conforming; conformance is defined by Clause 2.1. The absence of a test for a
clause does not weaken the clause. A conforming implementation SHOULD pass
every test (Clause 2.7).

| Component | Location | Form | Required outcome |
|---|---|---|---|
| Positive modules | `tests/conformance/` | 18 Eko modules: 17 test roots and one fixture | compile; every test binding executes without panic |
| Negative entries | `tests/negative/manifest.md` | 30 minimal programs | compile-time rejection with the stated outcome |
| Runner | toolchain | test runner over both classes | per-test report (Clause D.6) |

## D.2 Positive Tests

Positive tests are Eko modules. The file stem is the module name (Clause
7.2.1). The directory is flat; import paths resolve against it (Clause 7.2.2).
Every module compiles as a compilation root. The only cross-module imports
are `geolib` from `geolib_use.eko`, the free name `area` from
`geolib.eko` in `selective_import.eko` (Clause 7.2.3), and `std/time` in each
root that names a time type (Clause 9.1.3).

A test root is a module containing at least one binding in the `test` slot
(Clause 9.14.1). A test binding has the form `name: test =` followed by a
block. It has no arrow and does not enter the module interface (Clauses
9.14.1, 9.14.6). A module without a test binding is an ordinary
dependency under a test root; `geolib.eko` is the only such module.

The suite contains 81 test bindings. Each test is a block of ordinary
expressions; its value is discarded. Assertions are the standard library
functions `assert`, `assert_eq`, and `assert_ne` (Clause 9.14.3). `assert_eq`
locks both arguments to one type and reports failure through `inspect`
(Clauses 9.8.3, 9.14.3). A comparison of capabilities or functions is a
compile error, not a test failure (Clause 9.8.4).

| Condition | Requirement for a positive module |
|---|---|
| Compilation | no error diagnostic |
| Execution | every test binding in a test root runs |
| Result | no test binding panics |

A panic is captured per test (Clause D.4). The runner records the report name
`module.test_name`, marks that test failed, and continues with the remaining
tests in the module (Clause 9.14.6). A panic in one test does not fail the
compilation of the module.

Positive modules are self-checking: the expected result is the absence of a
panic. There are no expected-output files. A warning does not change a
program's meaning and does not reject it (Clause 2.4).

## D.3 Negative Tests

Negative tests are entries in `tests/negative/manifest.md`. Each entry is a
minimal program that MUST fail to compile; it specifies rejection, not a test
that executes.

Each entry has an identifier, a rejection description, and an expected
outcome. Where Annex B.3 specifies a code for the rejection, the expected
outcome is that code. Where no code is specified, the expected outcome names
the rejection and the violated clause.

| ID | Rejection | Expected outcome | Clause |
|---|---|---|---|
| N01 | nominal value into another nominal type | `E-nominal-mismatch` | 7.5.2 |
| N02 | wrong shape into an anonymous parameter | `E-shape-mismatch` | 7.5.5 |
| N03 | missing constructor arm | `E-exhaust` | 7.7.2 |
| N04 | bare panic as a case arm | compile error, no specified code | 6.7.1 |
| N05 | world bit under-declared | `E-world-bit` | 7.9.2 |
| N06 | world bit under-declared through a dispatcher | `E-world-bit` | 7.9.6 |
| N07 | world bit over-declared | `E-world-bit` | 7.9.2 |
| N08 | open bit not fed by a parameter | `E-open-bit-unfed` | 7.9.6 |
| N09 | result-only generic variable | `E-phantom` | 7.6.5 |
| N10 | phantom type binder | `E-phantom` | 7.6.2 |
| N11 | lowercase type name | `E-name-case` | 5.5.2 |
| N12 | import and top-level binding share a name | `E-name-conflict` | 7.4.6 |
| N13 | assertion on capabilities | `E-no-eq` | 9.8.4 |
| N14 | integer literal above `Int.max` | `E-literal-overflow` | 5.7.1 |
| N15 | raw tab as indentation | `E-tab` | 5.2.3 |
| N16 | chained comparison | parse error, no specified code | 6.5.5 |
| N17 | index syntax | parse error, no specified code | 6.10 |
| N18 | mixed numeric operands | compile error, no specified code | 7.8.4 |
| N19 | `:=` on an unannotated lambda | compile error, no specified code | 6.3.3, 7.6.3 |
| N20 | `?` in a container element position | `E-open-bit-position` | 7.9.6 |
| N21 | extern naming a reserved `eko_` symbol | `E-reserved-symbol` | 10.6 |
| N22 | `ctype` field outside machine words | `E-ctype-field` | 10.3.3 |
| N23 | extern parameter outside the marshalling whitelist | `E-extern-type` | 10.3.5 |
| N24 | type variable in an extern signature | `E-extern-type` | 10.2.3 |
| N25 | one name bound by two patterns of one case arm | `E-name-conflict` | 6.8.6 |
| N26 | floating literal that overflows the finite range | `E-literal-overflow` | 5.7.2 |
| N27 | two visible constructors and no anchor | `E-undetermined` | 7.4.6 |
| N28 | opened free name and a top-level binding share a name | `E-name-conflict` | 7.4.6 |
| N29 | two imports open the same free name | `E-name-conflict` | 7.4.6 |
| N30 | non-terminating generic instantiation | `E-instantiation-depth` | 8.7.3 |

For an entry with a specified code, the error diagnostic that rejects the
program MUST carry that code. For an entry without a specified code, the
compiler MUST reject the program, and the diagnostic that rejects it MUST
identify the violated clause. Message wording is not part of either
requirement. A warning is not a rejection (Clause 2.4).

A negative entry passes only when the program is rejected at compile time. A
runtime panic, a linker error, or a test failure is not a pass. The program
MUST be materialized as a standalone module. N12 requires `geolib` on the
module path, and N15 requires a raw tab character at the marked position.

Five entries have no specified code: N04, N16, N17, N18, and N19. For these,
the compiler MUST report an error diagnostic with an implementation-chosen
`E-` code and MUST identify the violated clause (Annex B.3). The snippets are
not added to the positive tree.

## D.4 Determinism

Positive tests execute without the world. The rules below are normative for
the suite; they make every assertion independent of host state, execution
order, and parallelism.

Test bindings have no parameter table, and capability construction is a host
right (Clauses 7.9.1, 9.14.5). A test therefore cannot obtain a real
capability. The derived constructors of Clause 9.14.5 are the only world-facing
values a test may use.

| World state | Prohibited dependency | Permitted substitute |
|---|---|---|
| Console | real output | `Out.null`, `Out.tee` |
| Input | real input | `In.empty`, `In.from_lines` |
| File system | real files or directories | `Fs.null`; pure data |
| Environment | process environment | `Env.frozen` |
| Arguments | process arguments | `Args.frozen` |
| Clock | wall clock or monotonic clock | `Clock.frozen` |
| Entropy | random source | `Rand.with_seed` |
| Scheduling | test order or parallelism | order-independent assertions |

A test MUST NOT depend on the wall clock, the process environment, the file
system, entropy, the argument vector, the network, or any other world state.
The constructors in the table are pure derivations over fixed inputs (Clause
9.14.5).

A test MUST NOT depend on the order in which tests execute or on concurrent
execution. Tests share no mutable state, and top-level bindings are pure
(Clauses 7.1.2, 9.14.6). A test MUST pass when executed alone, in any order, and
in parallel with other tests.

A test MUST NOT assert on an implementation-defined behavior listed in Clause
2.5. A test that compares a floating-point result of a Clause 9.13.2 function
MUST use the error bound the implementation documents (Clause 2.5), or compare
only exactly specified cases such as `NaN`, the infinities, and half-to-even
rounding. `Rand.with_seed` fixes the order of a
sequence, not the sequence itself; a test asserts properties, not specific
values (Clause 9.14.5).

The runner MUST capture a panic per test and MUST continue with the remaining
tests (Clause 9.14.6). A panic in one test MUST NOT terminate the run or
affect another test's result.

## D.5 Test-to-Clause Map

The table maps each positive module to the clauses it exercises. The map is by
module, not by individual assertion; a module may also exercise a clause
incidentally through a standard library call. The `Tests` column gives the
number of test bindings. `geolib.eko` is a fixture and has none.

| Module | Root | Tests | Clauses exercised | Coverage |
|---|---|---|---|---|
| `access_drilling.eko` | yes | 5 | 6.10, 7.4.3, 9.2.2, 9.2.3, 9.4.2, 9.10.1, 9.10.2 | `Option`-returning access on list, dictionary, and string; nested access; slice |
| `bindings.eko` | yes | 6 | 6.3.1, 6.3.2, 6.3.3, 6.3.4, 6.8.1, 8.2.1, 8.2.2, 8.2.3, 8.2.4, 9.2.1, 9.6.3 | initial and rebound bindings; capture snapshot; tuple split; discard; inverted chain |
| `cap_surface.eko` | yes | 7 | 9.1.1, 9.12, 9.14.5, 10.8 | capability handle types; derivations construct and forward; compiler-provided declarations; `File` family at a handle parameter |
| `case_patterns.eko` | yes | 8 | 6.7.1, 6.7.2, 6.7.3, 6.7.5, 6.8.2, 6.8.3, 6.8.4, 6.8.5, 7.7.2, 7.7.3, 8.3.1, 8.3.3, 8.3.4, 9.8.1 | guards; or-patterns; list and record patterns; literals; multiple scrutinees; single-line form |
| `ctor_anchor.eko` | yes | 1 | 7.4.6, 7.5.2 | same-named constructors of different types; the expected type selects one |
| `ctor_anchor_std.eko` | yes | 1 | 7.4.6, 7.6 | self-anchoring through the single visible constructor |
| `data_services.eko` | yes | 10 | 6.5.4, 7.8.6, 9.5.1, 9.5.2, 9.5.3, 9.6.1, 9.6.2, 9.6.3, 9.6.4, 9.7.1, 9.7.3, 9.8.1, 9.9.2, 9.9.3, 9.9.4, 9.9.5, 9.13.1 | `Bytes` round trips and append; JSON derivation and external tagging; numeric and parse round trips; frozen time; formatting |
| `derived_services.eko` | yes | 7 | 5.9.3, 7.5.6, 8.5.3, 9.8.1, 9.8.2, 9.8.3, 9.8.4, 9.8.5, 9.8.6, 9.8.7, 9.10.1 | `show` and `inspect`; override through interpolation; deep equality; declaration order; immutable `with`; stable `sort_by`; capability display |
| `generics.eko` | yes | 6 | 6.3.5, 6.9.4, 7.5.6, 7.6.1, 7.6.2, 7.6.3, 7.6.4, 7.7.2, 8.7.1, 8.8.2, 8.8.3, 9.2.2, 9.10.1 | implicit type variables; mutual recursion; recursive generic tree; self tail recursion; explicit comparator; instantiation by unification |
| `geolib.eko` | no | 0 | 6.3.5, 6.4.2, 7.2.1, 7.3.1, 7.3.2, 7.5.2, 7.7.2, 9.8.6 | module surface; public and private functions; nominal record and variant types |
| `geolib_use.eko` | yes | 3 | 6.5.2, 6.7.4, 6.8.1, 7.2.1, 7.2.2, 7.2.3, 7.4.4, 7.8.2, 8.3.2 | import opens types; constructors are anchored; qualified calls; UFCS extension; `if` |
| `lambdas_capture.eko` | yes | 4 | 6.4.1, 6.4.2, 7.9.2, 7.9.4, 7.9.5, 7.9.6, 8.2.1, 8.2.3, 9.10.1, 9.14.5 | lambdas without annotations; snapshot capture; pure factory returning a `!` closure; dispatcher bit |
| `namespaces.eko` | yes | 4 | 7.4.3, 7.9, 9.6, 9.10 | namespace rows in both spellings; arithmetic rows; a capability whose world bit flows from the row |
| `ranges.eko` | yes | 3 | 6.5.6, 9.8, 9.10.5 | both range operators over each endpoint type; compiler-supplied services over `Range`; wildcard arm |
| `result_chains.eko` | yes | 4 | 6.3.4, 6.8.1, 7.5.4, 8.2.4, 9.2.1, 9.2.2, 9.6.3, 9.10.1, 9.11.1, 9.14.4 | `<-` unwrapping; `to_result` and `map_err`; wildcard links over `Result _ e`; table-driven test |
| `selective_import.eko` | yes | 2 | 6.5.2, 7.2.3, 9.1.2 | selective import opens one free name; free-call forms of `show` and `panic` |
| `std_show.eko` | yes | 3 | 9.1.3, 9.7.4, 9.9.2 | `std/time` import; `Duration` decimal seconds; `Time` ISO 8601 UTC; `Json` minimal text |
| `types_composites.eko` | yes | 7 | 6.3.5, 6.6.1, 6.6.2, 6.6.3, 6.8.1, 6.9.2, 6.9.5, 7.5.2, 7.5.3, 7.5.5, 9.8.6, 9.10.2 | variants; records with nested partial update; newtype; tuples; dictionaries; shape adaptation |

The mapping is not exhaustive in the other direction: a clause may have no
dedicated module. The absence of a module is not permission to violate the
clause.

## D.6 Running the Suite

The suite is run by a test runner supplied by the toolchain (Clause 2.3).
Command names and option spellings are not specified by this document. The
conformance-relevant procedure is specified below.

| Step | Action | Pass condition |
|---|---|---|
| 1 | Add `tests/conformance/` to the module path as the project root (Clause 7.2.2). | `geolib` resolves for `geolib_use.eko` and for N12, and `std/time` resolves from the compiler (Clause 9.1.3). |
| 2 | Compile every positive module. | no error diagnostic; the fixture compiles without tests |
| 3 | Execute every test root. | every test binding runs; no panic; report `module.test_name` for each |
| 4 | Materialize each negative entry as a standalone module. | N15 contains a raw tab; N12 has `geolib` on the module path |
| 5 | Compile each negative module. | the program is rejected; the stated code or clause is identified on an error diagnostic |
| 6 | Summarize the run. | all positive tests pass and all negative entries are rejected |

A runner of the suite MUST NOT compile positive and negative modules in the
same root. A negative program is expected to fail; a positive module MUST NOT
be rejected. A compile of a positive module exits with status 0, and a compile
of a negative entry exits with status 2; no other status is a pass (Annex B.6).
A test-process status other than 0 is a test failure.

The runner MUST report a failure for an error diagnostic in a positive
module, a test binding that does not execute, a panic in a test, acceptance of
a negative entry, rejection of a negative entry with a different code or
clause, and a negative entry rejected at link time or at run time rather than
at compile time.

The runner SHOULD report the module name, the test name, and the diagnostic
code for each failure. The panic block format, where a panic is captured, is
specified in Annex B.6.

A run is complete only when every positive module has been compiled, every
test root has executed every test, and every negative entry has been compiled.
A partial run does not establish a suite result.

If a test conflicts with a normative clause, the clause prevails and the test
is an error in the suite (Clause 2.7). The suite is corrected by changing the
test, not the clause.

The suite is not part of a program build. Test bindings do not enter
interfaces, and a test root compiles independently of other roots (Clause
9.14.6).
