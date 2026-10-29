# 9. Standard Library

## 9.10 Collections

### 9.10.1 List

A list is a homogeneous, immutable sequence with structural sharing. Patterns
are inductive (`{h, ..t}`), and `List.get` is O(1). `List.append` is O(|xs|)
new nodes with the right operand shared (Clause 8.10). `List.map` preserves
order and length, `List.filter` preserves order, and `List.fold` proceeds left
to right. `List.each` and `List.each_with_index` visit in order, and
`List.length` counts elements. `List.any` and `List.all` short-circuit.
`List.reverse` is total. `List.take` and `List.drop` truncate: a count beyond
the length takes or drops everything, and a negative count counts as zero.
`List.map`, `List.filter`, `List.fold`, `List.each`, `List.each_with_index`,
`List.any`, `List.all`, `List.find_by`, `List.take_while`, `List.drop_while`,
`List.sort_by`, and `List.zip_with` are open-bit dispatchers; a pure callback
makes the call pure, and a world-touching callback makes it world-touching
(Clause 7.9). Iteration combinators and tail positions follow Clause 8.8.
`List.slice` takes a start and a length and yields `Option []t`. It never
clamps: a request that does not lie entirely inside the list yields `None`.
`List.join` concatenates the derived `show` forms with a separator; an element
type without `show` is a compile error, by the same rule as interpolation.
`List.sort` uses the derived `<` ordering; a record has no `<`, so
instantiating `List.sort` on a record is a compile error. `List.sort_by` takes
an explicit less-than comparator. Both sorts are stable, and `List.sort_by`
runs in O(n log n). An inconsistent comparator terminates with an
implementation-defined order (Clause 2.5).

### 9.10.2 Dict

A dictionary is an immutable, persistent mapping from a key domain to values
(Clause 9.10.4). `Dict.get` is O(1) and yields `Option v`; `Dict.length`
counts keys. `Dict.insert` and `Dict.remove` return a new dictionary; removing
an absent key returns the original. `Dict.merge` and `++` return a dictionary
in which the right operand wins on duplicate keys. `Dict.map`, `Dict.filter`,
`Dict.fold`, and `Dict.each` dispatch on the key and value in insertion order.
Iteration over a dictionary follows insertion order; this order is part of the
observable result of `to_list`, `show`, and JSON rendering of a string-keyed
dictionary. Dictionaries are persistent structures. The magnitude of
`Dict.merge` is implementation-defined (Clause 8.10).

### 9.10.3 Set

`Set t` is an ordinary nominal standard-library generic type. It has no
notation and no literal; the four composite forms are unchanged. `Set.of`
constructs a set from a list and deduplicates, keeping the first occurrence.
`Set.to_list` yields insertion order. `Set.contains` is a domain check.
`Set.insert` and `Set.remove` never mutate the receiver; a no-op returns the
original set. `Set.union` preserves the left order first and appends new
right-set elements in right order. `Set.intersect` and `Set.diff` preserve
left order. `Set.length` counts elements. `Set` derives `eq` structurally. It
has no `<`; structural equality carries no ordering, by the same rule as for
`Dict`. The empty set is `Set.of({})` with an expected type supplying the
element type. `Set.empty: ()
-> Set t` violates the result-only prohibition on type variables (Clause
7.6.5); no such member exists.

### 9.10.4 Key Domains

The key domain is the set of data types that have `eq`. Structural equality is
key equality. Lists, records, tuples, ADTs, `Bytes`, `Set`, and dictionaries
themselves are admissible keys; the type `[[]String]Int` is well formed.
Capabilities and functions are excluded; their equality derivation halts
(Clause 9.8.4). A `Float` key follows IEEE equality. Since `NaN != NaN`, a
`NaN` key always misses. `0.0` and `-0.0` compare equal and are the same key.
The element domain of `Set` is the same key domain. A dictionary with a
non-string key domain has no default JSON mapping (`E-json-shape`, Annex B).

### 9.10.5 Range

`Range t` is a compiler-supplied nominal type (Clause 9.1.1). It has no
constructor and no pattern form; the range operators `..` and `..=` are its
only construction (Clause 6.5.6). `Range` is the namespace of `Range t`, and
every member of the namespace is compiler-supplied. The iteration members take
the range first with the callback shapes of `List` (Clause 9.10.1).
`Range.to_list` materializes the elements in ascending order and is empty when
the start exceeds the end; `Range.length` counts them; `Range.each` and
`Range.each_with_index` visit in ascending order; `Range.map` yields `[]u`;
`Range.fold` folds in iteration order. `Range.eq` is structural equality over
the two endpoints and the closed flag. `Range.lt` and its family (`le`, `gt`,
`ge`) order lexicographically over the endpoints, the closed flag last.
`Range.show` is the operator form, `1..5` and `1..=5`, so neither `show` nor
`inspect` prints a constructor name.
A member is world-touching exactly when its callback is (Clause 7.9).

## 9.11 Text Utilities

### 9.11.1 String

`String.contains`, `String.starts_with`, and `String.ends_with` test
substring, prefix, and suffix presence. The empty string is contained in every
string. `String.replace` replaces every occurrence; `String.replace_first`
replaces the first occurrence only. `String.slice` takes a start and a length
in the Unicode scalar domain, yields `Option String`, and never clamps; a
request that does not lie entirely inside the string yields `None`. Its cost
is O(n). `List.take` and `List.drop` truncate; `String.slice` does not.
`String.trim`, `String.trim_start`, and `String.trim_end` remove Unicode
`White_Space` at the requested ends. `String.split(s, sep)` splits on every
occurrence of `sep` and keeps empty fields, whether leading, trailing, or
between adjacent separators. The separator MUST be non-empty; an empty
separator is a panic (Clause 9.2.3). `String.lines` splits on `'\n'` and drops
the final empty element when the input ends with `'\n'`.

```eko
"a,b".split(",")       // {"a", "b"}
",,x,".split(",")      // {"", "", "x", ""}
"a\nb\n".lines()       // {"a", "b"}
```

Unicode case mapping and regular expressions are not part of the standard
library.

### 9.11.2 Path

Paths are `String` values. `Path` is a non-type namespace of `std`: its
members are free functions over strings, reached by the qualified head
`Path.f` or by the receiver form, and it declares no type. The path vocabulary
is pure and lexical:
`Path.join` joins two components with the host separator; `Path.normalize`
collapses separators and `.`, and resolves `..` lexically; `Path.is_absolute`
applies the platform rule. `Path.normalize` is syntax, not semantics: it does
not consult the file system and cannot know whether a component is a symbolic
link. Resolution against the working directory and through symbolic links is
`Fs.realpath` (Clause 9.12.6). No `Path` nominal type exists.
