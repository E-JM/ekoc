# Annex C. Standard Library Inventory

## C.13 The std/time Companion

The time family is delivered by the companion module `std/time` (Clause 9.1.3),
not by the `std` root. The import opens the companion's public types and
constructors unqualified (Clause 7.2.3), so after `import std/time` the types
are named `Time`, `Mono`, and `Duration` and a constructor form is `Time 0`;
the qualified spellings `time.Time`, `time.Mono`, and `time.Duration` are
legal alternatives, and a member is `time.Duration.secs`. Receiver calls on
time values resolve through the type namespace and require no import (Clause
7.4.3). The companion source is compiler-embedded and versioned with the
compiler. Its inventory is listed here because it ships with the compiler;
future companions follow the same rule (Clause 9.1.4).

| Namespace | Member | Signature | Clause |
|---|---|---|---|
| `Time` | `Time.sub` | `(Time, Time) -> Duration` | Clause 9.7.1 |
| `Time` | `Time.add` | `(Time, Duration) -> Time` | Clause 9.7.1 |
| `Time` | `Time.to_ns` | `(Time) -> Int` | Clause 9.7.1 |
| `Time` | `Time.show` | `(Time) -> String` | Clause 9.7.4 |
| `Time` | `Time.parse` | `(String) -> Result Time ParseError` | Clause 9.7.4 |
| `Mono` | `Mono.sub` | `(Mono, Mono) -> Duration` | Clause 9.7.1 |
| `Mono` | `Mono.add` | `(Mono, Duration) -> Mono` | Clause 9.7.1 |
| `Mono` | `Mono.to_ns` | `(Mono) -> Int` | Clause 9.7.1 |
| `Duration` | `Duration.add` | `(Duration, Duration) -> Duration` | Clause 9.7.1 |
| `Duration` | `Duration.mul` | `(Duration, Int) -> Duration` | Clause 9.7.1 |
| `Duration` | `Duration.neg` | `(Duration) -> Duration` | Clause 9.7.1 |
| `Duration` | `Duration.secs`, `Duration.millis` | `(Int) -> Duration` | Clause 9.7.1 |
| `Duration` | `Duration.to_ns` | `(Duration) -> Int` | Clause 9.7.1 |
| `Duration` | `Duration.show` | `(Duration) -> String` | Clause 9.7.4 |
| `Duration` | `Duration.parse` | `(String) -> Result Duration ParseError` | Clause 9.7.4 |

`Time`, `Mono`, and `Duration` are nominal single-variant types with an `Int`
nanosecond payload (Clause 9.7.1). All their members are pure. `Time.show` is
ISO 8601 in UTC, `Duration.show` is decimal seconds, and `Mono.show` is the
default form, identical to `inspect`; `inspect` is the constructor form for all
three (Clause 9.7.4).
The payload is Int64 nanoseconds, a span of about ±292 years; arithmetic
outside the domain panics under the arithmetic rule (Clause 9.7.5). There is
no operator sugar: every operation is a named function (Clause 9.7.1).
`Time.parse` accepts the ISO 8601 UTC forms that `Time.show` produces, and
`Duration.parse` accepts decimal seconds (Clause 9.7.4). The two parsers are
compiler-supplied intrinsic rows of the companion. `ParseError` remains in
the `std` root (Clause 9.6.3).
