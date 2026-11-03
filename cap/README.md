# cap — the embedded modules

Eko source, embedded into the compiler binary at build (Clauses 9.1.1, 11.1).

- `sys.eko` — the system vocabulary: the handle and data types of the
  capability surfaces (`File`, `OpenMode`, `FileInfo`, `IoError`, `Scope`,
  `Task t`). Ordinary source with one import, `std/time`; no privilege. The
  runtime names only this module's type identities.
- `cap.eko` — the capability module: the capability namespace members, the
  bundle records, and the pure derivations, composed over the injected
  wrap/unwrap pairs (Clause 10.8). The only source that touches the pairs.

Neither has an import path; both reach every file beneath `std`. The embedded
tables under `src/resolve/` (`sys_source.c`, `cap_source.c`) are regenerated
by `tools/gen_embedded_source.py` and committed.
