# src/resolve

Clauses 7.2-7.4 — AST to symbols and bindings

ek_resolve is a pure entry point, (input, arena, diag) -> output. It
resolves one compilation root -- the module AST, its file, and the
module path directories an import searches -- and loads explicit imports
recursively through ek_lex/ek_layout/ek_parse; the dependency graph must
be acyclic. Errors go to diag, never abort.

The output is dumb data: every resolved reference site carries the
symbol it denotes and the lookup order that produced it. refs are
sorted by node address, so check can binary search the table by AST
node. All storage lives in the arena and is valid until it is
destroyed.

A constructor reference additionally carries every visible
same-named constructor as a candidate set. Constructors do not
conflict by visibility: which one a reference denotes is decided by
the expected type in value position or by the scrutinee type in
pattern position, so the selection belongs to check and resolve
hands the whole set over. symbol names the first candidate, so a
reader that only wants one symbol keeps a sensible answer.

Receiver-form links whose reading needs the static type of the
receiver come back as EK_SYM_MEMBER: resolve verified every
name-level fact (the link is not a lexical binding, a module segment
names a real module, module-path members exist and are public);
check selects the field, namespace, or current-module reading from
the receiver's type.

STD SURFACE. resolve answers the implicit `import std` from the
compiler-embedded lib/std.eko: the source is parsed and its surface
built like any other module's, then extended with the placeholder
rows the file does not own yet. The file declares the core types
(Bool, Option, Result) with their constructors and the three
opened free functions; the placeholders that remain are the scalar,
container, capability, and time type names (Char, String, Bytes, Int,
UInt, Float, Set, List — the head of the []t namespace — Dict — the
head of the [k]v namespace — Path, Json, Time, Duration, the
capability types) and the Time and Duration constructors. The
capability module is embedded the same way from cap/cap.eko (Clause
9.1.1): its surface — the handle and data types File, OpenMode,
FileInfo, IoError, Scope, and Task with their constructors, the
capability members, and the injected pairs of the seven bundled
capabilities — is the fallback beneath std, and no import path names
it. A real-file row wins over a placeholder
row of the same kind and name; the placeholders are the fallbacks.
A std or cap name no row declares resolves to EK_SYM_EXTERNAL
with no diagnostic (the bridge), so a name the embedded sources do
not declare yet keeps its bridged reading. Explicit imports of user
modules resolve for real and are diagnosed.
