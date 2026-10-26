#ifndef EK_PARSE_INTERNAL_H
#define EK_PARSE_INTERNAL_H

#include <stdarg.h>

#include "parse/parse.h"

/* Parser state. The cursor walks the layout token stream; errors are
   emitted once per position and never abort. */
typedef struct {
    const char *file;
    ek_arena *arena;
    ek_diag *diag;
    const ek_layout_token *toks;
    size_t count;
    size_t pos;
    int oom;
    int err_line; /* last diagnosed position; suppresses repeats */
    int err_col;
    int depth; /* live recursion levels of the expression, type, and
                  pattern parsers; bounded by EK_PARSE_MAX_DEPTH */
    int depth_hit; /* the ceiling was diagnosed once on this parse */
    int fatal;     /* the ceiling was reached: recovery drains to EOF */
    size_t *match; /* memoized bracket partners, built on first use */
    size_t *match_stack;
} p_ctx;

/* The height ceiling of the parsed tree. It bounds both the parsers'
   live recursion and the depth of any chain the parsers build
   iteratively, so later stages can walk the tree with the default
   stack. The full pipeline overflows only around 58,000 chain levels
   under the default 8 MB stack. Real modules nest under a few dozen
   levels; 1000 leaves more than an order of magnitude of headroom
   above them while a runaway input recovers with one diagnostic
   instead of crashing. */
#define EK_PARSE_MAX_DEPTH 1000

/* util.c: allocation, node construction, diagnostics, cursor. */
void *p_alloc(p_ctx *c, size_t n);
ek_node *p_node(p_ctx *c, ek_node_kind kind, int line, int col);
ek_node *p_node3(p_ctx *c, ek_node_kind kind, ek_node *pattern, ek_node *type,
                 ek_node *value); /* the u.bind shape */
void p_list_init(ek_nlist *l);
/* owner carries the list's height; NULL marks the compilation-unit
   list, whose root adds no nesting level. */
void p_list_add(p_ctx *c, ek_node *owner, ek_nlist *l, ek_node *n);
void p_path_init(ek_slist *l);
void p_path_add(p_ctx *c, ek_slist *l, const char *s);
void p_error(p_ctx *c, const char *code, int line, int col,
             const char *fmt, ...);
void p_error_tok(p_ctx *c, const char *code, const ek_layout_token *t,
                 const char *fmt, ...);
void p_sync(p_ctx *c);

/* p_adopt records child as one level under parent and returns child.
   A child that would put parent past EK_PARSE_MAX_DEPTH is refused:
   the ceiling is diagnosed once and the caller unwinds with NULL, so
   no over-deep tree reaches later stages. */
ek_node *p_adopt(p_ctx *c, ek_node *parent, ek_node *child);

/* The height guard. p_depth_enter counts one live recursion level and
   fails past EK_PARSE_MAX_DEPTH; every guarded entry that sees the
   failure returns NULL and the frames unwind. p_depth_leave drops one
   level. Guarded are the functions whose mutual recursion follows
   input nesting: the expression, type, and pattern entries,
   constructor application, constructor-payload patterns, and nested
   lambda parameter groups. */
int p_depth_enter(p_ctx *c);
void p_depth_leave(p_ctx *c);
void p_depth_limit(p_ctx *c);

const ek_layout_token *p_peek(p_ctx *c);
const ek_layout_token *p_peek_at(p_ctx *c, size_t off);
ek_token_kind p_src_kind(const ek_layout_token *t);
/* p_at: current token is a source token of kind k. p_at_layout: current
   token is the layout marker k. */
int p_at(p_ctx *c, ek_token_kind k);
int p_at_layout(p_ctx *c, ek_layout_kind k);
const ek_layout_token *p_take(p_ctx *c);
int p_got(p_ctx *c, ek_token_kind k);
int p_got_layout(p_ctx *c, ek_layout_kind k);
const ek_layout_token *p_expect(p_ctx *c, ek_token_kind k, const char *what);
void p_skip_newlines(p_ctx *c);
int p_expr_start(ek_token_kind k);  /* begins an expression or operand */
int p_apply_start(ek_token_kind k); /* begins a juxtaposition argument */
int p_pattern_start(ek_token_kind k);
int p_atom_start(ek_token_kind k);  /* begins a type atom */
int p_is_pascal(const char *s);
int p_is_snake(const char *s);

/* parse.c: module, members, dispatch. */
ek_node *p_module(p_ctx *c);
ek_node *p_member(p_ctx *c, int top_level);
int p_binding_op_ahead(p_ctx *c);      /* 1 :=, 2 <-, 3 =, 0 none */
void p_name_path(p_ctx *c, ek_slist *path, const ek_layout_token **head);
ek_node *p_path_pattern(p_ctx *c, const ek_slist *path,
                        const ek_layout_token *head);
void p_check_invert_pattern(p_ctx *c, const ek_node *pat);

/* binding.c: binding forms and member/suite bodies. */
ek_node *p_binding_line(p_ctx *c, int top_level);
ek_node *p_invert_bare(p_ctx *c); /* `<-` without a pattern */
ek_node *p_binding_after_pattern(p_ctx *c, ek_node *pat);
ek_node *p_binding_name_head(p_ctx *c, int top_level);
ek_node *p_member_list(p_ctx *c);      /* members until DEDENT (consumed) */
ek_node *p_block_suite(p_ctx *c);      /* NEWLINE INDENT members DEDENT */
ek_node *p_inline_suite(p_ctx *c);     /* expr {';' expr} -> expr | BLOCK */
ek_node *p_body_suite(p_ctx *c);       /* ':' seen: block or inline suite */
ek_node *p_binding_rhs(p_ctx *c);      /* after '=': block or one expression */
ek_node *p_after_colon(p_ctx *c, int top_level, const ek_slist *path,
                       const ek_layout_token *head);
ek_node *p_namespace_tail(p_ctx *c, const ek_slist *path,
                          const ek_layout_token *head, ek_node *ann);

/* decl.c */
void p_check_case(p_ctx *c, const char *name, const ek_layout_token *t,
                  int want_pascal, const char *what);
ek_node *p_type_decl(p_ctx *c, const char *name, const ek_layout_token *head);
ek_node *p_ctype_decl(p_ctx *c, const char *name, const ek_layout_token *head);
ek_node *p_test_decl(p_ctx *c, const char *name, const ek_layout_token *head);
ek_node *p_extern_tail(p_ctx *c, const char *name,
                       const ek_layout_token *head, ek_node *sig);
ek_node *p_variant(p_ctx *c);

/* expr.c */
ek_node *p_expr(p_ctx *c);

/* postfix.c: the operand layer of the expression ladder. */
ek_node *p_postfix(p_ctx *c);

/* literal.c */
ek_node *p_literal(p_ctx *c, const ek_layout_token *t);
ek_node *p_paren_primary(p_ctx *c); /* grouping or lambda */
ek_node *p_composite(p_ctx *c);     /* '{' consumed */
ek_node *p_slot_expr(p_ctx *c, const ek_token *toks, size_t n);

/* ctl.c */
ek_node *p_case_expr(p_ctx *c); /* cursor at 'case' */
ek_node *p_if_expr(p_ctx *c);   /* cursor at 'if' */

/* pattern.c */
ek_node *p_pattern(p_ctx *c);

/* type.c */
ek_node *p_type(p_ctx *c);
ek_node *p_type_atom(p_ctx *c);

#endif
