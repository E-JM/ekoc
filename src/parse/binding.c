#include <string.h>

#include "parse/internal.h"

/* Member bodies. A block body is NEWLINE INDENT { Member } DEDENT; a
   `;`-separated single-line suite is the same member sequence on one
   line, so both produce EK_N_BLOCK. */

ek_node *p_member_list(p_ctx *c)
{
    const ek_layout_token *first = p_peek(c);
    ek_node *block = p_node(c, EK_N_BLOCK, first->line, first->col);

    if (!block) {
        return NULL;
    }
    p_list_init(&block->u.kids);
    while (!c->oom) {
        size_t before = c->pos;
        ek_node *m;

        if (p_got_layout(c, EK_LTOK_DEDENT)
            || p_at_layout(c, EK_LTOK_EOF)) {
            break;
        }
        if (p_got_layout(c, EK_LTOK_NEWLINE) || p_got(c, EK_TOK_SEMI)) {
            continue;
        }
        m = p_member(c, 0);
        if (m) {
            p_list_add(c, &block->u.kids, m);
        }
        if (c->pos == before) {
            p_error_tok(c, "E-syntax", p_peek(c),
                        "member is malformed; skipped");
            p_take(c);
        }
    }
    return block;
}

ek_node *p_block_suite(p_ctx *c)
{
    if (!p_got_layout(c, EK_LTOK_NEWLINE)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected a block body (Clause 6.2.2)");
        return NULL;
    }
    if (!p_got_layout(c, EK_LTOK_INDENT)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected an indented block");
        return NULL;
    }
    return p_member_list(c);
}

ek_node *p_inline_suite(p_ctx *c)
{
    ek_node *first = p_expr(c);
    ek_node *block;

    if (!first) {
        return NULL;
    }
    if (!p_at(c, EK_TOK_SEMI)) {
        return first;
    }
    block = p_node(c, EK_N_BLOCK, first->line, first->col);
    if (!block) {
        return NULL;
    }
    p_list_init(&block->u.kids);
    p_list_add(c, &block->u.kids, first);
    while (p_got(c, EK_TOK_SEMI)) {
        ek_node *e = p_expr(c);

        if (!e) {
            break;
        }
        p_list_add(c, &block->u.kids, e);
    }
    return block;
}

/* The body after a lambda, `if`, or `else` header. */
ek_node *p_body_suite(p_ctx *c)
{
    if (p_at_layout(c, EK_LTOK_NEWLINE)) {
        return p_block_suite(c);
    }
    return p_inline_suite(c);
}

/* A binding right-hand side: one expression, or a block on the
   following lines. */
ek_node *p_binding_rhs(p_ctx *c)
{
    if (p_at_layout(c, EK_LTOK_NEWLINE)) {
        return p_block_suite(c);
    }
    return p_expr(c);
}

/* A line whose pre-scan found a binding operator. */
ek_node *p_binding_line(p_ctx *c, int top_level)
{
    if (p_at(c, EK_TOK_LARROW)) {
        return p_invert_bare(c);
    }
    if (p_at(c, EK_TOK_LPAREN) || p_at(c, EK_TOK_LBRACE)) {
        ek_node *pat = p_pattern(c);

        if (!pat) {
            p_sync(c);
            return NULL;
        }
        return p_binding_after_pattern(c, pat);
    }
    if (p_at(c, EK_TOK_IDENT)) {
        return p_binding_name_head(c, top_level);
    }
    p_error_tok(c, "E-syntax", p_peek(c), "expected a binding");
    p_sync(c);
    return NULL;
}

/* A binding line that begins with a pattern group or a brace pattern. */
ek_node *p_binding_after_pattern(p_ctx *c, ek_node *pat)
{
    if (p_got(c, EK_TOK_COLONEQ)) {
        ek_node *v = p_expr(c);

        if (!v) {
            return NULL;
        }
        return p_node3(c, EK_N_REBIND, pat, NULL, v);
    }
    if (p_at(c, EK_TOK_LARROW)) {
        ek_node *call;

        p_check_invert_pattern(c, pat);
        p_take(c);
        call = p_expr(c);
        if (!call) {
            return NULL;
        }
        if (call->kind != EK_N_CALL) {
            p_error(c, "E-invert-call", call->line, call->col,
                    "the right-hand side of `<-` is a call (Clause 6.3.1)");
        }
        return p_node3(c, EK_N_INVERT, pat, NULL, call);
    }
    if (p_got(c, EK_TOK_EQ)) {
        ek_node *v = p_binding_rhs(c);

        if (!v) {
            return NULL;
        }
        return p_node3(c, EK_N_LET, pat, NULL, v);
    }
    p_error_tok(c, "E-syntax", p_peek(c),
                "expected `:=`, `<-` or `=` after the pattern");
    p_sync(c);
    return NULL;
}

/* `Name := Expression`. */
static ek_node *rebind_tail(p_ctx *c, const ek_slist *path,
                            const ek_layout_token *head)
{
    ek_node *pat = p_path_pattern(c, path, head);
    ek_node *v;

    if (!pat) {
        return NULL;
    }
    v = p_expr(c);
    if (!v) {
        return NULL;
    }
    return p_node3(c, EK_N_REBIND, pat, NULL, v);
}

/* `Pattern <- Call`; the pattern must be an admitted shape. */
static ek_node *invert_tail(p_ctx *c, const ek_slist *path,
                            const ek_layout_token *head)
{
    ek_node *pat = p_path_pattern(c, path, head);
    ek_node *call;

    if (!pat) {
        return NULL;
    }
    p_check_invert_pattern(c, pat);
    p_take(c);
    call = p_expr(c);
    if (!call) {
        return NULL;
    }
    if (call->kind != EK_N_CALL) {
        p_error(c, "E-invert-call", call->line, call->col,
                "the right-hand side of `<-` is a call (Clause 6.3.1)");
    }
    return p_node3(c, EK_N_INVERT, pat, NULL, call);
}

/* `Name = BindingRHS` without an annotation. */
static ek_node *plain_let_tail(p_ctx *c, const ek_slist *path,
                               const ek_layout_token *head)
{
    ek_node *pat = p_path_pattern(c, path, head);
    ek_node *v;

    if (!pat) {
        return NULL;
    }
    v = p_binding_rhs(c);
    if (!v) {
        return NULL;
    }
    return p_node3(c, EK_N_LET, pat, NULL, v);
}

/* A binding line that begins with a name: `:=`, `<-`, `:` with an
   annotation or contextual word, or plain `=`. */
ek_node *p_binding_name_head(p_ctx *c, int top_level)
{
    ek_slist path;
    const ek_layout_token *head;

    p_name_path(c, &path, &head);
    if (!head) {
        return NULL;
    }
    if (p_got(c, EK_TOK_COLONEQ)) {
        return rebind_tail(c, &path, head);
    }
    if (p_at(c, EK_TOK_LARROW)) {
        return invert_tail(c, &path, head);
    }
    if (p_got(c, EK_TOK_COLON)) {
        return p_after_colon(c, top_level, &path, head);
    }
    if (p_got(c, EK_TOK_EQ)) {
        return plain_let_tail(c, &path, head);
    }
    p_error_tok(c, "E-syntax", p_peek(c),
                "expected `:=`, `<-`, `:` or `=` after the name");
    p_sync(c);
    return NULL;
}

/* The word in the annotation slot names a declaration; the name and
   its top-level position must fit the form. */
static const char *slot_word(p_ctx *c)
{
    const ek_layout_token *w = p_peek(c);

    if (w->kind != EK_LTOK_SOURCE || w->tok.kind != EK_TOK_IDENT) {
        return NULL;
    }
    return w->tok.text;
}

static ek_node *word_decl(p_ctx *c, const char *word, int clause,
                          int top_level, const ek_slist *path,
                          const ek_layout_token *head)
{
    if (!top_level) {
        p_error(c, "E-syntax", head->line, head->col,
                "a %s declaration is top-level (Clause 6.3.%d)", word,
                clause);
    }
    if (path->count != 1) {
        p_error(c, "E-syntax", head->line, head->col,
                "a %s declaration is named by one name (Clause 6.3.%d)",
                word, clause);
        p_sync(c);
        return NULL;
    }
    if (strcmp(word, "type") == 0) {
        return p_type_decl(c, path->data[0], head);
    }
    if (strcmp(word, "ctype") == 0) {
        return p_ctype_decl(c, path->data[0], head);
    }
    return p_test_decl(c, path->data[0], head);
}

static ek_node *annotated_let(p_ctx *c, const ek_slist *path,
                              const ek_layout_token *head, ek_node *ann);

/* Everything after `Name ':'`: the contextual words type, ctype, and
   test, or a type annotation followed by `=`. */
ek_node *p_after_colon(p_ctx *c, int top_level, const ek_slist *path,
                       const ek_layout_token *head)
{
    const char *word = slot_word(c);
    ek_node *ann;

    if (word && (strcmp(word, "type") == 0 || strcmp(word, "ctype") == 0
                 || strcmp(word, "test") == 0)) {
        return word_decl(c, word,
                         strcmp(word, "type") == 0 ? 5 :
                         strcmp(word, "ctype") == 0 ? 6 : 7,
                         top_level, path, head);
    }
    ann = p_type(c);
    if (!ann) {
        p_sync(c);
        return NULL;
    }
    if (!p_got(c, EK_TOK_EQ)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `=` after the annotation");
        p_sync(c);
        return NULL;
    }
    if (p_at(c, EK_TOK_IDENT) && strcmp(p_peek(c)->tok.text, "extern") == 0
        && p_peek_at(c, 1)->kind == EK_LTOK_SOURCE
        && p_peek_at(c, 1)->tok.kind == EK_TOK_STRING) {
        return p_extern_tail(c, path->data[path->count - 1], head, ann);
    }
    if (path->count >= 2) {
        return p_namespace_tail(c, path, head, ann);
    }
    return annotated_let(c, path, head, ann);
}

/* `Pattern : Type = BindingRHS`. */
static ek_node *annotated_let(p_ctx *c, const ek_slist *path,
                              const ek_layout_token *head, ek_node *ann)
{
    ek_node *pat = p_path_pattern(c, path, head);
    ek_node *v;

    if (!pat) {
        return NULL;
    }
    v = p_binding_rhs(c);
    if (!v) {
        return NULL;
    }
    return p_node3(c, EK_N_LET, pat, ann, v);
}

/* A namespace binding: a Pascal owner chain, a snake_case name,
   ':' type '=' binding right-hand side. */
ek_node *p_namespace_tail(p_ctx *c, const ek_slist *path,
                          const ek_layout_token *head, ek_node *ann)
{
    ek_node *n = p_node(c, EK_N_NAMESPACE, head->line, head->col);
    size_t i;

    if (!p_is_pascal(path->data[0])) {
        p_error(c, "E-name-case", head->line, head->col,
                "a namespace binding is headed by a type name (Clause 6.3.1)");
    }
    if (!n) {
        return NULL;
    }
    p_path_init(&n->u.namespace.owner);
    for (i = 0; i + 1 < path->count; i++) {
        p_path_add(c, &n->u.namespace.owner, path->data[i]);
    }
    n->u.namespace.name = path->data[path->count - 1];
    n->u.namespace.type = ann;
    n->u.namespace.value = p_binding_rhs(c);
    return n;
}
