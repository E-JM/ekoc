# Design Record — Built-in Data Types and Services

Non-normative. Ruling clauses: 9.3–9.9. Rejected alternatives, prior art, and
rationale.

## 1. Char

Unicode has three layers — bytes, scalar values, grapheme clusters. `Char` is
one scalar value, because a scalar type needs a fixed domain; a grapheme
cluster is a sequence property and belongs to `String` functions. Swift's
variable-width `Character` and its indexing contortions are the cautionary
tale. `String.length` counts scalars: counting bytes hides a unit error; byte
length is `s.to_bytes().length()`.

## 2. Conversions

The three-way taxonomy (`Clause 9.2`) — input validation → `Result`, domain
check → `Option`, invariant violation → `panic` — means conversions never
panic, and yields two rulings. **Not saturating**: Rust's `as` maps NaN→0 and
infinity→MAX, silent misrepresentation; `Float.to_int`/`to_uint` return
`None`. **Negative domain first**: `Float.to_uint` checks negativity before
truncation (`-0.5 → None`), because truncating to 0 would discard the sign —
a second lie. Round trips are acceptance surfaces: `parse(show(x)) == Ok x`,
with Float NaN judged by NaN equivalence (identity preserved, payload bits not
promised).

## 3. Derived services

One overridable channel loses structural truth — Java's `toString` pitfall,
answered independently by Python's and Rust's second channels. Eko adopts both
from the start: `show` (display, overridable by a namespace declaration in the
owning module) and `inspect` (structural, always derived, never overridable,
"what you see can be written"). The override is the one place interpolation
requires non-local reasoning; the structural channel is the permanent
fallback.

`<` is derived **only for ADTs** — a declaration list is naturally an order.
Records have no magnitude (sort with `sort_by`), and `Bool`'s variant order is
enumeration convenience, so derivation is suppressed there. `Float` follows
IEEE (`NaN != NaN`); a total-order function may exist someday, never the
operator.

The dispatch set is eleven names: `show`, `inspect`, `eq`, `ne`, `lt`, `le`,
`gt`, `ge`, `with`, `to_json`, `from_json` (`Clause 9.8.7`). A declared
namespace member wins over the derived default, so the discipline is enforced
at the declaration, not at the call site.

Four names admit no declaration at all: `inspect` (`Clause 9.8.1`), the JSON
pair (`Clause 9.9.5`), and `with`, whose second parameter type is
inexpressible (`Clause 9.8.6`). Declaring one is `E-service-fixed`. The other
seven carry fixed signatures, pure arrows only — `show` is `(t) -> String`,
the equality and ordering family `(t, t) -> Bool` — and a wrong arrow is
`E-service-signature` at the declaration, because dispatch reads the member as
the service and a wrong arrow would detonate far from its cause. `with`'s
inexpressible signature is also why no second such built-in was allowed
(`Clause 6.10`): `Option.get` payload dispatch was rejected.

## 4. JSON

External tagging (serde-isomorphic): zero-payload variants map to strings,
payload variants to single-key objects — the simplest form for CLI
configuration. `Int` and `Float` stay separate variants, so `42` comes back as
`Int`; round-trip fidelity beats JSON's single number type. `Obj` preserves
insertion order, so text is reproducible and config diffs are stable.
Non-finite floats render as `NaN`/`Inf`/`-Inf` and parse back — an Eko-side
extension of strict JSON, honestly labeled; strict conversion is a library
concern.

## 5. Bytes

`Bytes` is a contiguous immutable block in the same family as `String`, with
**no byte literal**: construction goes through functions (`Clause 9.5`).

| Rejected | Reason |
|---|---|
| `[]Byte` | the list contract is sharing and inductive patterns; a byte block needs contiguity and offsets. One name for two layouts splits the cost model inside one type; a 1 MB file becomes a million nodes and pattern-rest O(n²). Haskell's `[Word8]` → `ByteString` is the same verdict |
| `Byte` element type | the safety is already `from_ints`' one `Option`; it has no second identity, and bytes are arithmetic daily (checksums, varints) — `b1 * 256 + b2` would become a code round trip. A dedicated identity is a user declaration |
| `String` as `[]Char` for symmetry | direction reversed: `String ≠ []Char` is the precedent (contiguous UTF-8 + `chars` bridge); `Bytes` repeats the act |
| early `UInt8` spelling | the machine-word family arrives whole with FFI; a byte is a natural number 0..255, the same move as `Char.to_code` |

Two gates: `File.read` keeps the text gate (`Err InvalidData` for non-UTF-8),
with `read_bytes` as the raw front door; `read_bytes` never reports
`InvalidData` — a short read is EOF.

## 6. Time

A timestamp (calendar), a measurement (order and length), and a duration
(difference) have mutually exclusive semantics; a bare `Int` leaves the unit
implicit, and the millisecond-versus-microsecond dispute is a real incident
source. A wall clock can jump backwards, a monotonic clock cannot, so the two
"now"s are typed apart. There is no operator sugar on time arithmetic — single-
variant payloads are not exposed, so every operation is named, which is the
credential of unit safety. UTC is the only zone; formatting and localization
are library domain. Leap seconds are not represented (a day is always 86400
seconds). The Int64-nanosecond payload spans roughly ±292 years; arithmetic
outside it panics. On a frozen clock `now()` still sets the world bit:
determinism is a property of values, the bit a property of signatures.

## 7. Rejected surface

| Area | Rejected | Reason (clause) |
|---|---|---|
| Char | grapheme clusters as `Char` | a sequence property belongs to `String` functions |
| Char | variable-width characters (Swift) | unfixed domain; indexing contortions |
| Char | byte length as `String.length` | hides a unit error; `to_bytes()` states it |
| Conversion | saturating conversion (Rust `as`) | NaN→0 and infinity→MAX are silent misrepresentation (`9.2`) |
| Conversion | truncate before the sign check | `-0.5 → 0` discards the sign (`9.2`) |
| JSON | format-string or query DSL | a second language for one tree; a `Json` value composes and greps |
| JSON | default pretty printing | `indent` is library domain (`9.9`) |
| JSON | silent non-string-key dictionary isomorphism | compile error; conversion is explicit |
| JSON | YAML/TOML in the prelude | outside the JSON service; library domain |
| JSON | runtime reflection for deriving | derivation is compile-time expansion |
| Time | bare `Int` for all three quantities | unit left implicit; ms/µs incidents |
| Time | operator sugar on time arithmetic | payloads are not exposed; named operations are the credential |
| Time | local time zones | determinism; formatting is library domain |
| Time | leap-second representation | a day is always 86400 seconds under POSIX epoch |
