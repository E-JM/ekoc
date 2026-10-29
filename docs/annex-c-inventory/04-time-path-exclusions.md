# Annex C. Standard Library Inventory

This file inventories the JSON surface, the path namespace, and the library
exclusions. The `std/time` companion is inventoried in C.13.

## C.10 Json

| Namespace | Member | Signature | Clause |
|---|---|---|---|
| `Json` | `Json.parse` | `(String) -> Result Json ParseError` | Clause 9.9.2 |
| `Json` | `Json.show` | `(Json) -> String` | Clause 9.9.2 |

`Json` is an ADT with seven variants (Clause 9.9.1). `parse` is input
validation and reuses `ParseError` (Clause 9.9.2). `show` is minimal and emits
no whitespace; `inspect` is the constructor form (Clause 9.9.2). `Obj`
preserves insertion order, so JSON text is reproducible (Clause 9.9.1). Number
text follows the variant: `Int` and `Float` are separate variants, and a
`Float` uses the shortest round trip (Clause 9.9.2). The derived `to_json` and
`from_json` are the type-namespace members of Annex C.2 (Clause 9.9.3).

## C.11 Path

`Path` is a pure namespace over `String`; path vocabulary is lexical and does
not touch the file system (Clause 9.11.2).

| Member | Signature | Clause |
|---|---|---|
| `Path.join` | `(String, String) -> String` | Clause 9.11.2 |
| `Path.normalize` | `(String) -> String` | Clause 9.11.2 |
| `Path.is_absolute` | `(String) -> Bool` | Clause 9.11.2 |

`join` uses the host separator. `normalize` collapses separators and `.` and
resolves `..` lexically. `is_absolute` applies the platform rule. Resolution
through the file system is `Fs.realpath` (Clause 9.12.6).

## C.12 Not in the Library

The exclusions are specified in Clause 9.15 and are not restated here. Their
inventory consequence is that no member of this annex falls in a Clause 9.15
category: the surface carries no world-touching free function, no `Rights`, no
`Date` or timezone, no `Set` notation or `Set.empty`, no `Bytes.new` or
`byte_length`, no regular expression, and no index syntax (Clause 6.10).

Additions to this inventory are specification changes. The standard library is
versioned with the compiler, and its opened names are part of its interface
(Clause 9.1.4).
