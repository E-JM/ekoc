# Session plan (2026-11-06)

Scope: the nine-step execution order of `~/Work/ekoc-adjudications-2026-11.md`
plus the open items of `ekoc-followups.md` and `ekoc-dev-issues.md`. Steps 1-5
of the order are already on main (verified by probe below). This session
closes step 6 remainder, step 4 remainder, step 7, step 8, the open spec
gaps, and the report files. Step 9 (surface-table unification, Vector+HAMT,
Conc scheduler 4b, veneer implementation, R8 archive) is sequenced after P2
by the adjudication record itself — not silently skipped; its hooks stay in
the followups.

Verified state at session start (probes in /tmp/probe-batch):
- done: #1 dict keys + R5-1 (da21b82), #2 qualified composite ctor, F11/F12
  (15292c2), F15 (pinned), R1-2/R1-3/R1-4 code+spec, R1-5 impl (--link/--shim,
  atomic link, multiline E-emit), FFI diag (b) E-ctype-field spelling.
- open: F7/F16 SIGSEGV (reproduced, root cause found in capture.c:131),
  F17 (reproduced), R5-3 top-level `<-` accepted, List/String.slice overflow
  panic (reproduced), F13 diagnostic, R1-6 ekoveneer.h, R1-1/R1-5 spec
  sentences, FFI diag (a) extern newline, diag (c) world-bit remedy,
  R5-10 Dict.to_list, B.6 frames, all of step 8, F2/F9/F10 doc gaps.
- not reproducible: followups #10 UFCS/member-lambda fault (both forms pass;
  record evidence and close).

## Checklist

- [x] 0. Housekeeping: restore CONVENTIONS.md (uncommitted deletion removed
      the English/Build/Git sections; no adjudication backs it), re-apply the
      two deliberate d1acb88 edits cleanly (fix their typos), commit `conv:`.
- [x] 1. F7/F16: fix capture.c reading `u.kids` off an `EK_N_P_BRACE` (items
      live in `u.p_brace.items`; rest enum leaked as the 0x1 pointer).
      Conformance tests: both F16 forms, prefix-rest, multi-scrutinee, a
      mutual-recursion cluster. Corpus rebuild (example/*) as witness.
      Commit `lower:`.
- [x] 2. F17: expected type flows through a chained `<-` continuation into
      the tail constructor record. Repro pinned first; fix in check's invert
      continuation handling; conformance test. Commit `check:`.
- [x] 3. R5-3: top-level `<-` is a check error `E-top-arrow`; Annex B row,
      6.3.4 sentence, negative test. Commit `check:` (+ `spec:`/`docs:` bits).
- [x] 4. Slice overflow: List.slice guard without `start + len` overflow;
      String.slice inherits. Conformance test near Int max. F10 wording in
      9.10.1/9.13/C.6 (start+count, never [i,j)). Commits `lib:` + `spec:`.
- [x] 5. F13: local named-function binding diagnostic carries the remedy
      sentence. Probe current behavior first. Commit `check:`.
- [x] 6. FFI diagnostics (a) extern newline parse bug (probe, fix parse),
      (c) world-bit non-function-binding diagnostic remedy sentence
      (7.9.2 rule unchanged). Commit `parse:` / `check:`.
- [x] 7. R1-6 ekoveneer.h: rt/ekoveneer.h single source of truth with the
      normative prototypes (10.7.1 entries as C signatures, string bridge
      ownership, closure ctx borrow); 10.7.1 rewritten as the normative
      prototype list; tools/ check that spec table and header agree
      (Annex C discipline: committed tables, tool-run, never a build step).
      Commit `rt:` + `spec:` + `build:`(tool).
- [x] 8. R1-1/R1-5 spec sentences: portability layering (extern+veneer
      portable, shim hook documented per implementation) in 10.1/10.2.4
      neighborhood; 10.3.6 MUST sentence on documenting the hook mechanism
      in the conformance statement. Commit `spec:`.
- [x] 9. R5-10 Dict.to_list in std.eko over the insertion-order walk;
      Annex C.4 row + 9.10.2 sentence satisfied; conformance test.
      Commit `lib:`.
- [x] 10. B.6 frame capture + `#line` (followups #6 same batch, R5-8
      source-level names): emitted prologue/epilogue maintain a frame stack
      in rt (module.function, no instantiation suffix), panic walks it;
      lower emits `#line` at statement granularity. B.6/Annex B wording
      check; tests. Commits `rt:`/`lower:`.
- [x] 11. D3 combinators: List.fold_until, List.find_map, List.set,
      String.rep in std.eko; Annex C.3 + 9.10.1 rows; conformance tests.
      Commit `lib:`.
- [x] 12. D4 `"""` raw strings: lexer token (no escapes/interpolation,
      open delimiter must break line, strip common indent, no closing
      newline), 5.9.1 exception row + 5.2.2 layout note, Annex A touch if
      needed; conformance tests + negative (content may not contain `"""`).
      Commits `lex:` + `spec:`.
- [x] 13. R2-1 std/regex companion: ekogrep NFA (its core 399 lines) lifted
      to lib/std/regex.eko; embedded table + resolve wiring; 9.15 regex row
      deleted; 9.1.3 companion list + 9.11.1 sentence rewritten; lazy-
      quantifier limit promoted from comment to clause; ekogrep import swap
      verified. Commits `lib:` + `resolve:` + `spec:`.
- [x] 14. R2-2 Char predicates: is_alpha/is_digit/is_alnum/is_space/
      is_upper/is_lower/is_xid_start/is_xid_continue — rt tables (XID same
      source as tools/gen_xid.py), intrinsic routing, nsrows, 9.4.3 +
      Annex C.6 rows, tests. Commits `rt:` + `lower:`/`check:` + `spec:`.
- [x] 15. R6-1(b) reserved stems: 9.1.3 declarative rule (each companion's
      stem reserved, list follows the companion); check enforces stems from
      the companion set. R6-1(e) 7.5.7 MUST accept >=64 nesting; test at 64.
      Commits `spec:` (+ `check:` if enforcement lands).
- [x] 16. F2/F9 spec gaps: 7.4.6 note — std root types have no qualified
      constructor spelling; 6.9.4 sentence — type-application arguments are
      juxtaposed nominals, tuple workaround named. Commit `spec:`.
- [x] 17. Verify sweep sentences landed (9.13.1 pad panic, 11.4 version
      gate); regenerate Annex D after suite changes (`--check` discipline).
- [x] 18. Reports: update ~/Work/README.md open-items, followups (#1 #2 #10
      closures, UFCS note), dev-issues (F7/F11/F12/F13/F15/F16/F17 status),
      adjudication record untouched (closed record).
- [x] 19. Final gate: make test, make conformance, negative suite, example
      corpus builds; ordered commits throughout; push at end; final log.


## Beyond the checklist (debt closed on the way)

- [x] followups #3: `rt/capval.c` + `capenv.c` split into `bundle.c`, `env.c`,
      `args.c`, `genval.c`, `genio.c`, `gentime.c`, `geninfo.c`; the lower
      capability table narrowed per capability. Probes: Out/Args/Rand link no
      family, Clock only Time/Mono (generated unit 825 -> 122 lines), In only
      status + IoError, Fs the full set.
- [x] followups #4: the FileInfo record got `ekort.h` slot enumeration, rt
      `offsetof` checks, and an emitted `ekB_sys$FileInfo` check in the unit;
      the one-payload shape is pinned once.
- [x] 7.4.3 station 3 resolved over structural receivers too (list, record,
      function), with a check unit test and a conformance binding.
- [x] followups #10 UFCS/member-lambda fault: four probe forms pass; closed
      with evidence (the one real gap found beside it is the station 3 fix).
- [x] dev-issues F18 recorded (new P1: a generic service call is not
      re-checked at instantiation; `assert_eq` over a function-carrying type is
      silently false). Root cause and fix direction are in the report; the fix
      needs an adjudicated design (scheme constraints at instantiation).

## Log

- Session start: read README + three reports + conventions + skill; built
  tree, unit tests pass; probed every open suspect (see state above);
  gdb+ASan located F16's wild pointer: capture.c:131 groups P_BRACE into
  the u.kids case, but the brace pattern's items live in u.p_brace.items;
  with a suffix rest the enum word 1 becomes the data pointer 0x1.

- Step 12: raw string token, indent stripping, one-diagnostic recovery; unit
  and conformance tests. Blank lines now carry empty content, as 5.9.4 says.
- Step 13: `std/regex` companion over a closure-carried NFA; companion table
  in the resolver; generator check mode; 9.11.3 + C.14; ekogrep 49/49 with
  its 399-line engine removed (1170 -> 765 lines).
- Step 14: rt XID tables from the lexer's generator, eight Char predicates,
  check/lower rows, 9.4.4 + C.7; std trim drops its private White_Space copy.
- Step 15: reserved-stem test covers both companions; 64-level nesting test
  pins the floor, the 1001-level guard is a named rejection.
- Step 16: 7.4.6 root-constructor note; 6.9.4 composite argument set.
- Step 17: R3-1/R3-2/R4-1/R4-2 sentences had not landed despite the session
  start claim; written now, and `with` over a type variable moved to check.
  Annex D regenerated twice; `--check` green.
- Step 18: README, followups, dev-issues status tables; adjudication record
  untouched.
- Step 19: clean build, unit suite (`make test`, 3m04s), conformance,
  negative entries N34-N37 probed, all five embedded sources + XID tables +
  Annex D + veneer prototypes `--check` current, 17 corpus programs
  test+build, todolist 15/15, ekogit CLI script ok; `temp` needs raylib and
  a shim that the corpus does not carry (environmental, pre-existing).
