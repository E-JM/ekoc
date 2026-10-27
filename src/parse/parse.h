#ifndef EK_PARSE_PARSE_H
#define EK_PARSE_PARSE_H

#include <stddef.h>
#include <stdint.h>

#include "layout/layout.h"
#include "support/arena.h"
#include "support/diag.h"

/*
 * Layout token stream -> AST.
 *
 * ek_parse is a pure entry point, (input, arena, diag) -> output. A
 * module that fails to parse still yields a partial AST plus error
 * diagnostics; the parser never aborts. Nodes are dumb data: a kind, a
 * source position, and children. No parent pointers, no semantic
 * fields; everything is arena-allocated.
 *
 * Representation notes for consumers:
 *
 * - Dotted references and receiver chains share EK_N_FIELD and
 *   EK_N_CALL. `Math.pi` is FIELD(NAME "Math", "pi"); `p.f(x)` is
 *   CALL(FIELD(NAME "p", "f"), [x]); `a.Mod.f(b)` nests the same way.
 *   Which links are module accesses, namespace members, or record
 *   fields is not decided here.
 * - EK_N_APPLY is constructor juxtaposition (`Ok x`, `Mod.Name x`).
 *   The head is an all-Pascal name chain; the argument is one postfix
 *   expression, since juxtaposition binds tighter than every operator.
 * - EK_N_STRING_LIT carries its parts in source order, one ek_str_seg
 *   per literal segment (text/text_len set, expr NULL) and one per
 *   interpolation slot (expr set, text NULL). A slot's token run is
 *   parsed by the same expression grammar, so a slot is a full
 *   expression node and slots nest through EK_N_STRING_LIT.
 * - EK_N_BLOCK holds the members of an indented block body, of a
 *   `;`-separated single-line suite, and of a case arm body; the three
 *   are the same node. A body of one expression is stored bare, so
 *   `if c: a else: b` has expressions, not blocks, as its branches, and
 *   an arm whose body sits on the arm's line does too.
 * - EK_N_P_BRACE covers the brace patterns that share one syntax
 *   (tuple, record, list). The expected type decides the family, so
 *   the node records items as written: a bare-identifier item is
 *   EK_N_P_VAR, which a record reading reinterprets as the shorthand
 *   field. A `:` item is EK_N_P_FIELD.
 * - EK_N_T_FN with result NULL is one of the elided forms `(a)`,
 *   `(a) !`, `(a) ?`; a written unit result is an EK_N_T_NAME "_"
 *   node. EK_N_T_PARAM reuses the t_field member and carries a NULL
 *   name for an unlabeled parameter.
 * - EK_N_P_VAR and EK_N_P_CONS are classified by spelling alone:
 *   snake_case is a variable, PascalCase a constructor.
 * - EK_N_IMPORT carries its module path and, when the line ends in a
 *   selective list, the listed free names; an empty names list means
 *   the import carries no list. The list's braces are plain tokens on
 *   the import's own line, so layout never sees them.
 * - A binding node carries `deprecated` when the document before it has
 *   a line beginning `@deprecated`. The document's text is not stored;
 *   extraction and rendering are toolchain work.
 * - The record-update form `p.with({...})` has no node of its own: the
 *   grammar defines it as an ordinary postfix member call whose one
 *   argument is a composite literal (annex A.3.5 Postfix; Clauses
 *   6.6.3, 9.8.6), so it parses to CALL(FIELD(p, "with"), [COMPOSITE])
 *   and the partial-literal reading is check's concern. A member or
 *   field actually named `with` is indistinguishable here by design.
 */

typedef enum {
    EK_N_MODULE, EK_N_IMPORT,
    EK_N_LET, EK_N_REBIND, EK_N_INVERT, EK_N_NAMESPACE,
    EK_N_TYPE_DECL, EK_N_VARIANT, EK_N_CTYPE_DECL, EK_N_TEST_DECL,
    EK_N_EXTERN_DECL,
    EK_N_INT_LIT, EK_N_FLOAT_LIT, EK_N_CHAR_LIT, EK_N_STRING_LIT,
    EK_N_NAME, EK_N_FIELD, EK_N_CALL, EK_N_APPLY, EK_N_PAREN,
    EK_N_UNARY, EK_N_BINARY, EK_N_LAMBDA, EK_N_LAMBDA_PARAMS,
    EK_N_IF, EK_N_CASE, EK_N_ARM, EK_N_BLOCK,
    EK_N_COMPOSITE, EK_N_COMPOSITE_ITEM,
    EK_N_P_WILDCARD, EK_N_P_VAR, EK_N_P_LIT, EK_N_P_CONS,
    EK_N_P_BRACE, EK_N_P_PAREN, EK_N_P_FIELD, EK_N_P_OR,
    EK_N_T_NAME, EK_N_T_APPLY, EK_N_T_LIST, EK_N_T_DICT, EK_N_T_TUPLE,
    EK_N_T_RECORD, EK_N_T_FIELD, EK_N_T_PARAM, EK_N_T_PAREN, EK_N_T_FN
} ek_node_kind;

/* Operator of EK_N_BINARY; range operators are binary too. */
typedef enum {
    EK_BIN_ADD, EK_BIN_SUB, EK_BIN_MUL, EK_BIN_DIV, EK_BIN_REM,
    EK_BIN_CONCAT, EK_BIN_EQ, EK_BIN_NE, EK_BIN_LT, EK_BIN_LE,
    EK_BIN_GT, EK_BIN_GE, EK_BIN_AND, EK_BIN_OR,
    EK_BIN_RANGE_EXC, EK_BIN_RANGE_INC
} ek_binop;

/* World bit of a function type. result NULL means the arrow is
   elided and the result is `_`. */
typedef enum {
    EK_ARROW_PURE, EK_ARROW_WORLD, EK_ARROW_OPEN
} ek_arrow;

/* Where the `..` rest of a list pattern sits. */
typedef enum {
    EK_REST_NONE, EK_REST_SUFFIX, EK_REST_PREFIX
} ek_rest;

/* Right-hand side shape of a type declaration. */
typedef enum {
    EK_TYPE_RHS_VARIANTS, EK_TYPE_RHS_RECORD, EK_TYPE_RHS_TUPLE
} ek_type_rhs;

typedef struct ek_node ek_node;

typedef struct {
    ek_node **data;
    size_t count;
} ek_nlist;

typedef struct {
    const char **data;
    size_t count;
} ek_slist;

/* One part of a string literal: a cooked literal segment or one
   interpolation slot parsed to an expression. */
typedef struct {
    const char *text;   /* literal segment; NULL when the part is a slot */
    size_t text_len;
    ek_node *expr;      /* slot expression; NULL when the part is literal */
} ek_str_seg;

struct ek_node {
    ek_node_kind kind;
    int line;  /* 1-based, of the construct's first token */
    int col;   /* 1-based, counted in Unicode scalar values */
    int depth; /* parse-time tree height; consumers ignore it */
    int deprecated; /* the document before the binding marks it deprecated */
    union {
        ek_nlist kids;    /* MODULE, BLOCK, LAMBDA_PARAMS, P_PAREN, P_OR,
                             T_TUPLE, T_RECORD */
        ek_slist path;    /* NAME, T_NAME, P_CONS head */
        struct { ek_slist path; ek_slist names; } imports; /* IMPORT */
        struct { ek_str_seg *segs; size_t count; } str;
        struct {
            const char *text;
            unsigned long long magnitude;
            int overflow;
            int base;
        } int_lit;
        struct { const char *text; double value; } float_lit;
        struct { uint32_t scalar; } char_lit;
        struct { ek_node *inner; } wrap; /* PAREN, UNARY, P_LIT, T_PAREN,
                                            T_LIST */
        struct { ek_node *a; ek_node *b; } pair; /* APPLY, LAMBDA,
                                                     COMPOSITE_ITEM, T_DICT */
        struct { ek_binop op; ek_node *lhs; ek_node *rhs; } binary;
        struct { ek_node *recv; const char *name; } field;
        struct { ek_node *pattern; ek_node *type; ek_node *value; } bind;
        struct {
            ek_slist owner; /* all segments but the last */
            const char *name;
            ek_node *type;
            ek_node *value;
        } namespace;
        struct {
            const char *name;
            ek_slist params;   /* type variable binders */
            ek_type_rhs rhs;
            ek_nlist variants; /* EK_N_VARIANT when rhs is VARIANTS */
            ek_node *type;     /* when rhs is RECORD or TUPLE */
        } type_decl;
        struct { const char *name; ek_node *payload; } variant;
        struct { ek_slist name; ek_node *payload; } cons; /* P_CONS */
        struct { const char *name; ek_node *body; } decl; /* TEST_DECL body,
                                                             CTYPE_DECL type */
        struct {
            const char *name;
            ek_node *type;
            const char *symbol;
            int pure;
        } extern_decl;
        struct { ek_node *callee; ek_nlist args; } call;
        struct { ek_node *cond; ek_node *then; ek_node *els; } ifexpr;
        struct { ek_nlist scrutinees; ek_nlist arms; } caseexpr;
        struct { ek_nlist patterns; ek_node *guard; ek_node *body; } arm;
        struct { const char *name; ek_node *pattern; } p_field;
        struct { const char *name; ek_node *type; } t_field; /* T_FIELD,
                                                                T_PARAM */
        struct { ek_rest rest; const char *rest_name; ek_nlist items; } p_brace;
        struct { ek_node *head; ek_nlist args; } t_apply;
        struct { ek_arrow arrow; ek_nlist params; ek_node *result; } t_fn;
    } u;
};

typedef struct {
    ek_node *module; /* EK_N_MODULE; NULL only on allocation failure */
} ek_parse_result;

/* Parses count layout tokens ending in EK_LTOK_EOF into a module AST.
   Errors go to diag, never abort; a module that fails to parse still
   comes back as a partial AST. file names the source in diagnostics
   and is borrowed. */
ek_parse_result ek_parse(const ek_layout_token *tokens, size_t count,
                         const char *file, ek_arena *arena, ek_diag *diag);

#endif
