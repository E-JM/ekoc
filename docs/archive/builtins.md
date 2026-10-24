# Design Record — Built-in Data Types and Services

Non-normative. Rulings: `08-builtins.md`. Rejected alternatives, prior art,
and rationale.

## 1. Char: the scalar layer of Unicode

Unicode has three layers — bytes, scalar values, grapheme clusters. `Char`
is one scalar value, because a scalar type needs a fixed domain; a grapheme
cluster is a sequence property and belongs to `String` functions (library
domain). Swift's variable-width `Character` and its indexing contortions
are the recorded cautionary tale. `String.length` counts scalar values,
not bytes — counting bytes hides a unit error (the Rust trap, not set);
byte length goes through `s.to_bytes().length()`.

## 2. Conversions: honesty against silent misrepresentation

The three-way failure taxonomy (`08-builtins.md §4.1`) — input validation
→ `Result`, domain check → `Option`, invariant violation → `panic`,
conversions never panic — produces two anti-saturation rulings:

- **Not saturating.** Rust's `as` maps NaN→0 and infinity→MAX: silent
  misrepresentation. `Float.to_int`/`to_uint` return `None` instead.
- **Negative-domain first.** `Float.to_uint` checks the negative domain
  *before* truncation (`-0.5 → None`): truncating to 0 would discard the
  sign, a second lie.

Round trips are acceptance surfaces: `parse(show(x)) == Ok x`, with the
Float NaN exception judged by NaN equivalence (identity preserved, payload
bits not promised).

## 3. Derived services: the dual channel

A single overridable channel loses structural truth — Java's `toString`
pitfall; Python and Rust each independently grew a second channel. Eko
adopts both from the start: `show` (display, overridable by a namespace
declaration in the owning module) and `inspect` (structural, always
derived, never overridable — "what you see can be written", zero
exceptions). The override is the one place interpolation requires
non-local reasoning; the cost is recorded and the structural channel is
always present as the fallback.

- `<` is derived **only for ADTs**: a declaration list is naturally an
  order, so derivation is faithful. Records have no magnitude semantics
  (sort with `sort_by`); `Bool`'s variant order is enumeration convenience,
  not magnitude — derivation deliberately suppressed.
- `Float` follows IEEE (`NaN != NaN`); a total-order function may exist
  someday, never the operator.
- `with`'s parameter type is **inexpressible** in Eko's type syntax — the
  first compiler built-in service whose type cannot be written. The
  exception is explicit, localized, and the reason no second one
  (`Option.get` payload dispatch) was allowed (`05-names.md §12`).

## 4. JSON: external tagging and round-trip fidelity

- **External tagging** (serde-isomorphic): zero-payload variants map to
  strings, payload variants to single-key objects — the simplest form for
  CLI configuration, where zero-payload enums read and write directly.
- **`Int` and `Float` are separate variants**: `42` goes in and comes out
  as `Int`. Round-trip fidelity takes precedence over the surface
  uniformity of JSON's single number type.
- `Obj` preserves insertion order, so JSON text is reproducible and
  configuration diffs are stable.
- Non-finite floats render as `NaN`/`Inf`/`-Inf` and parse back — an
  Eko-side extension of strict JSON, honestly labeled; converting for
  strict external consumers is a library concern.

Rejected: a format-string or query DSL, default pretty printing (`indent`
is library domain), silent non-string-key dictionary isomorphism (compile
error, explicit conversion), YAML/TOML in the prelude, and runtime
reflection (derivation is compile-time expansion).

## 5. Bytes: the identity decision

`Bytes` is a contiguous immutable block, in the same family as `String`,
with **no byte literal** — construction goes through functions. The full
rejection table behind `08-builtins.md §3`:

| Rejected | Reason |
|---|---|
| `[]Byte` (bytes as a list) | The list contract is structural sharing and inductive patterns (`{h, ..t}` O(1) tails); a byte block needs contiguity and offsets. Sharing one name for both means same-type-different-layout — the cost model splits *inside* one type. Per-byte nodes also multiply memory and counts (a 1 MB file is a million heap objects) and make pattern-rest O(n) per step (O(n²) protocol parsing). Haskell's `[Word8]` → packed `ByteString` is history's own answer to this experiment |
| `Byte` element type | The safety it buys is already `from_ints`'s single `Option` check; it has no second identity like `Char` earned (no literal, no self-display), and bytes are *arithmetic* daily (checksums, varints) — an element type turns `b1 * 256 + b2` into a to_code/from_code round trip. A dedicated identity is available as a user single-variant declaration |
| `String` as `[]Char` (for symmetry) | Direction reversed: `String ≠ []Char` is the established precedent (contiguous UTF-8 + `chars` bridge); `Bytes` is the same design act a second time, not a new exception |
| Early `UInt8` element spelling | The machine-word family arrives as a whole with the FFI track; a single early member leaks the family boundary and drags in its overflow and conversion rulings. A byte is a natural number 0..255 — the same move as `Char.to_code` — and machine words hold and ferry, never compute |

The two gates: `File.read` keeps the text gate (`Err InvalidData` for
non-UTF-8) — with `read_bytes` as the raw-byte front door, the refusal is
a guard, not a dead end — and `read_bytes` never reports `InvalidData`; a
short read means EOF.

## 6. Time: three quantities, three types

A timestamp (human calendar), a measurement (order and length), a duration
(difference) have mutually exclusive semantics; a bare `Int` leaves the
unit implicit, and the millisecond-versus-microsecond dispute is a real
incident source — sealing the unit into a nominal type rules it out.
A wall clock can jump (a correction may move it backwards); a monotonic
clock cannot; the two "now"s are typed apart so misuse cannot meet
(`measure with mono, timestamp with now`).

- **No operator sugar on time arithmetic**: single-variant payloads are
  not exposed, so operators cannot reach them; every operation is named,
  which is both the price and the credential of unit safety.
- **UTC is the only zone** — a local timezone undermines determinism;
  formatting and localization are library domain.
- Leap seconds are not represented (POSIX epoch maps directly; a day is
  always 86400 seconds); alignment policy is host domain.
- The Int64-nanosecond payload spans roughly ±292 years; arithmetic outside
  the domain panics under the existing rules.
- On a frozen clock, `now()` **still sets the world bit**: determinism is a
  property of the values; the bit is a property of the signature — each
  pays its own bill.
