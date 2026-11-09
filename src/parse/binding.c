#include <string.h>

#include "parse/internal.h"

/* `Pattern [= BindingRHS]` under a LET node; the annotation may be
   NULL. */
static ek_node *let_tail(p_ctx *c, ek_node *pat, ek_node *ann, int top_level);

/* A line whose pre-scan found a binding operator. Any pattern may
   stand on the left; the refutable families are parse-legal and the
   check stage rejects them. */
ek_node *p_binding_line(p_ctx *c, int top_level)
{
    const ek_layout_token *t = p_peek(c);
    ek_node *pat;

    if (p_at(c, EK_TOK_LARROW)) {
        return p_invert_bare(c);
    }
    if (p_at(c, EK_TOK_IDENT)) {
        return p_binding_name_head(c, top_level);
    }
    if (p_src_kind(t) != EK_TOK_LPAREN && p_src_kind(t) != EK_TOK_LBRACE
        && !p_pattern_start(p_src_kind(t))) {
        p_error_tok(c, "E-syntax", t, "expected a binding");
        p_sync(c);
        return NULL;
    }
    pat = p_pattern(c);
    if (!pat) {
        p_sync(c);
        return NULL;
    }
    return p_binding_after_pattern(c, pat, top_level);
}

/* A binding line that begins with a pattern group, a brace pattern, or
   a literal pattern. */
ek_node *p_binding_after_pattern(p_ctx *c, ek_node *pat, int top_level)
{
    ek_node *ann;

    if (p_at(c, EK_TOK_COLON)) {
        p_take(c);
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
        return let_tail(c, pat, ann, top_level);
    }
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
                    "the right-hand side of `<-` must be a call "
                    "(Clause 6.3.1)");
        }
        return p_node3(c, EK_N_INVERT, pat, NULL, call);
    }
    if (p_got(c, EK_TOK_EQ)) {
        return let_tail(c, pat, NULL, top_level);
    }
    p_error_tok(c, "E-syntax", p_peek(c),
                "expected `:`, `:=`, `<-` or `=` after the pattern");
    p_sync(c);
    return NULL;
}

/* A nested annotated binding whose annotation is a function type and
   whose value is neither a lambda nor a bare name reads as the
   declaration body: the annotation's parameter names bind over the
   value (Clause 6.4.2), as a module member reads. A bare name keeps
   the value reading -- a local binding at a function type takes the
   named function read as a value (Annex D.5). The synthesized lambda
   carries no written source, so its position is the value's. */
static ek_node *fn_body_desugar(p_ctx *c, ek_node *ann, ek_node *v)
{
    ek_node *params;
    ek_node *lam;
    size_t i;

    if (!ann || ann->kind != EK_N_T_FN || !v || v->kind == EK_N_LAMBDA
        || v->kind == EK_N_NAME) {
        return v;
    }
    params = p_node(c, EK_N_LAMBDA_PARAMS, v->line, v->col);
    if (!params) {
        return NULL;
    }
    p_list_init(&params->u.kids);
    for (i = 0; i < ann->u.t_fn.params.count; i++) {
        const ek_node *p = ann->u.t_fn.params.data[i];
        const char *name = p && p->kind == EK_N_T_PARAM
                               ? p->u.t_field.name : NULL;
        ek_node *kid = p_node(c, EK_N_NAME, v->line, v->col);

        if (!kid) {
            return NULL;
        }
        p_path_init(&kid->u.path);
        p_path_add(c, &kid->u.path, name ? name : "_");
        p_list_add(c, params, &params->u.kids, kid);
    }
    lam = p_node(c, EK_N_LAMBDA, v->line, v->col);
    if (!lam) {
        return NULL;
    }
    lam->u.pair.a = p_adopt(c, lam, params);
    lam->u.pair.b = p_adopt(c, lam, v);
    if (!lam->u.pair.a || !lam->u.pair.b) {
        return NULL;
    }
    return lam;
}

/* `Pattern [= BindingRHS]` under a LET node; the annotation may be
   NULL. */
static ek_node *let_tail(p_ctx *c, ek_node *pat, ek_node *ann, int top_level)
{
    ek_node *v = p_binding_rhs(c);

    if (!v) {
        return NULL;
    }
    if (!top_level) {
        v = fn_body_desugar(c, ann, v);
        if (!v) {
            return NULL;
        }
    }
    return p_node3(c, EK_N_LET, pat, ann, v);
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
                "the right-hand side of `<-` must be a call "
                "(Clause 6.3.1)");
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
   annotation or contextual word, plain `=`, or — when the name is
   constructor-shaped and a pattern follows it — a constructor pattern
   with its payload. */
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
    if (p_pattern_start(p_src_kind(p_peek(c)))
        && (path.count > 1 || p_is_pascal(path.data[path.count - 1]))) {
        return p_binding_after_pattern(c, p_ctor_pattern(c, &path, head),
                                       top_level);
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
                              const ek_layout_token *head, ek_node *ann,
                              int top_level);

/* Whether the extern tail, `extern "name"`, begins off tokens ahead. */
static int extern_ahead(p_ctx *c, size_t off)
{
    const ek_layout_token *w = p_peek_at(c, off);

    return w->kind == EK_LTOK_SOURCE && w->tok.kind == EK_TOK_IDENT
           && strcmp(w->tok.text, "extern") == 0
           && p_peek_at(c, off + 1)->kind == EK_LTOK_SOURCE
           && p_peek_at(c, off + 1)->tok.kind == EK_TOK_STRING;
}

/* The extern form is a compilation-unit item. */
static void extern_top_level(p_ctx *c, int top_level,
                             const ek_layout_token *head)
{
    if (!top_level) {
        p_error(c, "E-syntax", head->line, head->col,
                "an extern declaration is top-level (Clause 6.3.8)");
    }
}

/* The extern tail after `=`: `extern "name"` on the binding's own
   line, or on a line of its own under the binding. Returns 1 when the
   binding continues as the extern declaration and *out carries the
   node, NULL after a diagnostic. */
static int extern_slot(p_ctx *c, int top_level, const ek_slist *path,
                       const ek_layout_token *head, ek_node *ann,
                       ek_node **out)
{
    size_t off;

    if (extern_ahead(c, 0)) {
        extern_top_level(c, top_level, head);
        *out = p_extern_tail(c, path->data[path->count - 1], head, ann);
        return 1;
    }
    if (!p_at_layout(c, EK_LTOK_NEWLINE)) {
        return 0;
    }
    /* The tail may sit on its own line, under an opened indent pair. */
    off = p_peek_at(c, 1)->kind == EK_LTOK_INDENT ? 2 : 1;
    if (!extern_ahead(c, off)) {
        return 0;
    }
    extern_top_level(c, top_level, head);
    p_take(c);
    if (off == 2) {
        p_take(c);
    }
    *out = p_extern_tail(c, path->data[path->count - 1], head, ann);
    /* the tail's line end and the indent's closing dedent stay with
       the opened pair */
    p_got_layout(c, EK_LTOK_NEWLINE);
    p_got_layout(c, EK_LTOK_DEDENT);
    return 1;
}

/* Everything after `Name ':'`: the contextual words type, ctype, and
   test, or a type annotation followed by `=`. */
ek_node *p_after_colon(p_ctx *c, int top_level, const ek_slist *path,
                       const ek_layout_token *head)
{
    const char *word = slot_word(c);
    ek_node *ann, *n;

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
    if (extern_slot(c, top_level, path, head, ann, &n)) {
        return n;
    }
    if (path->count >= 2) {
        return p_namespace_tail(c, path, head, ann);
    }
    return annotated_let(c, path, head, ann, top_level);
}

/* `Pattern : Type = BindingRHS`. */
static ek_node *annotated_let(p_ctx *c, const ek_slist *path,
                              const ek_layout_token *head, ek_node *ann,
                              int top_level)
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
    if (!top_level) {
        v = fn_body_desugar(c, ann, v);
        if (!v) {
            return NULL;
        }
    }
    return p_node3(c, EK_N_LET, pat, ann, v);
}

/* A namespace binding: an owner chain whose last segment names the
   owner type, a snake_case member, ':' type '=' binding right-hand
   side. */
ek_node *p_namespace_tail(p_ctx *c, const ek_slist *path,
                          const ek_layout_token *head, ek_node *ann)
{
    ek_node *n = p_node(c, EK_N_NAMESPACE, head->line, head->col);
    size_t i;

    if (!p_is_pascal(path->data[path->count - 2])) {
        p_error(c, "E-name-case", head->line, head->col,
                "a namespace owner chain ends in a type name "
                "(Clause 6.3.1)");
    }
    if (!n) {
        return NULL;
    }
    p_path_init(&n->u.namespace.owner);
    for (i = 0; i + 1 < path->count; i++) {
        p_path_add(c, &n->u.namespace.owner, path->data[i]);
    }
    n->u.namespace.name = path->data[path->count - 1];
    n->u.namespace.type = p_adopt(c, n, ann);
    n->u.namespace.value = p_adopt(c, n, p_binding_rhs(c));
    return n;
}
