# src/parse

Clause 6 + Annex A.3 — layout tokens to AST

ek_parse is a pure entry point, (input, arena, diag) -> output. A
module that fails to parse still yields a partial AST plus error
diagnostics; the parser never aborts. Nodes are dumb data: a kind, a
source position, and children. No parent pointers, no semantic
fields; everything is arena-allocated.

Representation notes for consumers:

- Dotted references and receiver chains share EK_N_FIELD and
  EK_N_CALL. `Math.pi` is FIELD(NAME "Math", "pi"); `p.f(x)` is
  CALL(FIELD(NAME "p", "f"), [x]); `a.Mod.f(b)` nests the same way.
  Which links are module accesses, namespace members, or record
  fields is not decided here.
- EK_N_APPLY is constructor juxtaposition (`Ok x`, `Mod.Name x`).
  The head is an all-Pascal name chain; the argument is one postfix
  expression, since juxtaposition binds tighter than every operator.
- EK_N_STRING_LIT carries its parts in source order, one ek_str_seg
  per literal segment (text/text_len set, expr NULL) and one per
  interpolation slot (expr set, text NULL). A slot's token run is
  parsed by the same expression grammar, so a slot is a full
  expression node and slots nest through EK_N_STRING_LIT.
- EK_N_BLOCK holds the members of an indented block body, of a
  `;`-separated single-line suite, and of a case arm body; the three
  are the same node. A body of one expression is stored bare, so
  `if c: a else: b` has expressions, not blocks, as its branches, and
  an arm whose body sits on the arm's line does too.
- EK_N_P_BRACE covers the brace patterns that share one syntax
  (tuple, record, list). The expected type decides the family, so
  the node records items as written: a bare-identifier item is
  EK_N_P_VAR, which a record reading reinterprets as the shorthand
  field. A `:` item is EK_N_P_FIELD.
- EK_N_T_FN with result NULL is one of the elided forms `(a)`,
  `(a) !`, `(a) ?`; a written unit result is an EK_N_T_NAME "_"
  node. EK_N_T_PARAM reuses the t_field member and carries a NULL
  name for an unlabeled parameter.
- EK_N_P_VAR and EK_N_P_CONS are classified by spelling alone:
  snake_case is a variable, PascalCase a constructor.
- EK_N_IMPORT carries its module path and, when the line ends in a
  selective list, the listed free names; an empty names list means
  the import carries no list. The list's braces are plain tokens on
  the import's own line, so layout never sees them.
- A binding node carries `deprecated` when the document before it has
  a line beginning `@deprecated`. The document's text is not stored;
  extraction and rendering are toolchain work.
- The record-update form `p.with({...})` has no node of its own: the
  grammar defines it as an ordinary postfix member call whose one
  argument is a composite literal (annex A.3.5 Postfix; Clauses
  6.6.3, 9.8.6), so it parses to CALL(FIELD(p, "with"), [COMPOSITE])
  and the partial-literal reading is check's concern. A member or
  field actually named `with` is indistinguishable here by design.
