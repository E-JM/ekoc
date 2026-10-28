#include <stdio.h>
#include <string.h>

#include "parse/internal.h"
#include "support/utf8.h"
#include "support/xid.h"

void *p_alloc(p_ctx *c, size_t n)
{
    void *p = ek_arena_alloc(c->arena, n);

    if (!p && !c->oom) {
        c->oom = 1;
        ek_diag_emit(c->diag, EK_SEV_ERROR, NULL, c->file, 0, 0,
                     "out of memory while parsing; results are partial");
    }
    return p;
}

ek_node *p_node(p_ctx *c, ek_node_kind kind, int line, int col)
{
    ek_node *n = p_alloc(c, sizeof *n);

    if (n) {
        memset(n, 0, sizeof *n);
        n->kind = kind;
        n->line = line;
        n->col = col;
        n->depth = 1;
    }
    return n;
}

ek_node *p_adopt(p_ctx *c, ek_node *parent, ek_node *child)
{
    int depth;

    if (!parent || !child) {
        return child;
    }
    depth = child->depth + 1;
    if (depth > EK_PARSE_MAX_DEPTH) {
        p_depth_limit(c);
        return NULL;
    }
    if (depth > parent->depth) {
        parent->depth = depth;
    }
    return child;
}

ek_node *p_node3(p_ctx *c, ek_node_kind kind, ek_node *pattern, ek_node *type,
                 ek_node *value)
{
    ek_node *n = p_node(c, kind, pattern ? pattern->line : 0,
                        pattern ? pattern->col : 0);

    if (!n) {
        return NULL;
    }
    n->u.bind.pattern = p_adopt(c, n, pattern);
    n->u.bind.type = p_adopt(c, n, type);
    n->u.bind.value = p_adopt(c, n, value);
    return n;
}

void p_list_init(ek_nlist *l)
{
    l->data = NULL;
    l->count = 0;
}

/* Children live in an arena growable array; growth copies, the spent
   block stays in the arena (no object management). */
static void *grow(p_ctx *c, void *data, size_t count, size_t unit)
{
    size_t cap = count ? count * 2 : 8;
    void *p = p_alloc(c, cap * unit);

    if (p && count > 0) {
        memcpy(p, data, count * unit);
    }
    return p;
}

void p_list_add(p_ctx *c, ek_node *owner, ek_nlist *l, ek_node *n)
{
    if (!n || c->oom || !p_adopt(c, owner, n)) {
        return;
    }
    if (l->count == 0 || (l->count & (l->count - 1)) == 0) {
        ek_node **grown = grow(c, l->data, l->count, sizeof *grown);

        if (!grown) {
            return;
        }
        l->data = grown;
    }
    l->data[l->count++] = n;
}

void p_path_init(ek_slist *l)
{
    l->data = NULL;
    l->count = 0;
}

void p_path_add(p_ctx *c, ek_slist *l, const char *s)
{
    if (!s || c->oom) {
        return;
    }
    if (l->count == 0 || (l->count & (l->count - 1)) == 0) {
        const char **grown = grow(c, l->data, l->count, sizeof *grown);

        if (!grown) {
            return;
        }
        l->data = grown;
    }
    l->data[l->count++] = s;
}

void p_error(p_ctx *c, const char *code, int line, int col,
             const char *fmt, ...)
{
    va_list ap;

    if (c->oom || c->fatal
        || (line == c->err_line && col == c->err_col)) {
        return;
    }
    c->err_line = line;
    c->err_col = col;
    va_start(ap, fmt);
    ek_diag_emitv(c->diag, EK_SEV_ERROR, code, c->file, line, col, fmt, ap);
    va_end(ap);
}

void p_error_tok(p_ctx *c, const char *code, const ek_layout_token *t,
                 const char *fmt, ...)
{
    va_list ap;

    if (c->oom || c->fatal
        || (t->line == c->err_line && t->col == c->err_col)) {
        return;
    }
    c->err_line = t->line;
    c->err_col = t->col;
    va_start(ap, fmt);
    ek_diag_emitv(c->diag, EK_SEV_ERROR, code, c->file, t->line, t->col,
                  fmt, ap);
    va_end(ap);
}

/* Recovers to the next member boundary: one module yields many
   diagnostics, so a failed member gives up its line and the enclosing
   loop resumes. Closing brackets and `else` stay for the enclosing
   parsers to consume. */
void p_sync(p_ctx *c)
{
    if (c->fatal) {
        while (!p_at_layout(c, EK_LTOK_EOF)) {
            p_take(c);
        }
        return;
    }
    for (;;) {
        const ek_layout_token *t = p_peek(c);

        if (t->kind != EK_LTOK_SOURCE) {
            return; /* NEWLINE, INDENT, DEDENT, EOF */
        }
        switch (t->tok.kind) {
        case EK_TOK_NEWLINE:
        case EK_TOK_SEMI:
        case EK_TOK_RPAREN:
        case EK_TOK_RBRACKET:
        case EK_TOK_RBRACE:
        case EK_TOK_KW_ELSE:
        case EK_TOK_EOF:
            return;
        default:
            p_take(c);
        }
    }
}

/* The ceiling was exceeded: diagnose once per parse and make recovery
   quiet for the rest of the stream. The deeper parse levels all sit
   above the limit, and a per-level diagnostic would bury the cause. */
void p_depth_limit(p_ctx *c)
{
    if (!c->depth_hit) {
        const ek_layout_token *t = p_peek(c);

        c->depth_hit = 1;
        c->err_line = t->line;
        c->err_col = t->col;
        ek_diag_emit(c->diag, EK_SEV_ERROR, "E-nesting-depth", c->file,
                     t->line, t->col,
                     "nesting exceeds %d levels; bind intermediate values "
                     "or split the construct", EK_PARSE_MAX_DEPTH);
    }
    c->fatal = 1;
}

int p_depth_enter(p_ctx *c)
{
    if (c->fatal) {
        return 0;
    }
    if (c->depth >= EK_PARSE_MAX_DEPTH) {
        p_depth_limit(c);
        return 0;
    }
    c->depth++;
    return 1;
}

void p_depth_leave(p_ctx *c)
{
    c->depth--;
}

const ek_layout_token *p_peek(p_ctx *c)
{
    if (c->pos >= c->count) {
        return &c->toks[c->count - 1]; /* the trailing EOF */
    }
    return &c->toks[c->pos];
}

const ek_layout_token *p_peek_at(p_ctx *c, size_t off)
{
    size_t i = c->pos + off;

    if (i >= c->count) {
        return &c->toks[c->count - 1];
    }
    return &c->toks[i];
}

int p_at(p_ctx *c, ek_token_kind k)
{
    const ek_layout_token *t = p_peek(c);

    return t->kind == EK_LTOK_SOURCE && t->tok.kind == k;
}

/* The lexical kind of a stream slot: markers read as EOF, which no
   grammar position admits, so tok.kind is never read unguarded. */
ek_token_kind p_src_kind(const ek_layout_token *t)
{
    return t->kind == EK_LTOK_SOURCE ? t->tok.kind : EK_TOK_EOF;
}

int p_at_layout(p_ctx *c, ek_layout_kind k)
{
    return p_peek(c)->kind == k;
}

const ek_layout_token *p_take(p_ctx *c)
{
    const ek_layout_token *t = p_peek(c);

    if (c->pos < c->count - 1) {
        c->pos++;
    }
    return t;
}

int p_got(p_ctx *c, ek_token_kind k)
{
    if (p_at(c, k)) {
        p_take(c);
        return 1;
    }
    return 0;
}

int p_got_layout(p_ctx *c, ek_layout_kind k)
{
    if (p_at_layout(c, k)) {
        p_take(c);
        return 1;
    }
    return 0;
}

const ek_layout_token *p_expect(p_ctx *c, ek_token_kind k, const char *what)
{
    if (p_at(c, k)) {
        return p_take(c);
    }
    p_error_tok(c, "E-syntax", p_peek(c), "expected %s", what);
    return NULL;
}

void p_skip_newlines(p_ctx *c)
{
    while (p_at_layout(c, EK_LTOK_NEWLINE)) {
        p_take(c);
    }
}

int p_expr_start(ek_token_kind k)
{
    switch (k) {
    case EK_TOK_INT:
    case EK_TOK_FLOAT:
    case EK_TOK_CHAR:
    case EK_TOK_STRING:
    case EK_TOK_IDENT:
    case EK_TOK_LPAREN:
    case EK_TOK_LBRACE:
    case EK_TOK_MINUS:
    case EK_TOK_KW_CASE:
    case EK_TOK_KW_IF:
        return 1;
    default:
        return 0;
    }
}

int p_apply_start(ek_token_kind k)
{
    return p_expr_start(k) && k != EK_TOK_MINUS;
}

int p_pattern_start(ek_token_kind k)
{
    switch (k) {
    case EK_TOK_INT:
    case EK_TOK_FLOAT:
    case EK_TOK_CHAR:
    case EK_TOK_STRING:
    case EK_TOK_IDENT:
    case EK_TOK_LPAREN:
    case EK_TOK_LBRACE:
    case EK_TOK_MINUS:
        return 1;
    default:
        return 0;
    }
}

int p_atom_start(ek_token_kind k)
{
    return k == EK_TOK_IDENT || k == EK_TOK_LPAREN || k == EK_TOK_LBRACKET;
}

/* Name case classes, by spelling alone. The class rides on the first
   scalar after the privacy prefix: a PascalCase name opens with an
   uppercase or titlecase letter and the remainder carries no `_`. */
static int no_underscore(const char *s)
{
    for (; *s; s++) {
        if (*s == '_') {
            return 0;
        }
    }
    return 1;
}

int p_is_pascal(const char *s)
{
    ek_utf8_decoded d;

    while (*s == '_') {
        s++;
    }
    d = ek_utf8_decode(s, strlen(s));
    if (d.status != EK_UTF8_OK || d.len == 0) {
        return 0;
    }
    return ek_xid_pascal_start(d.cp) && no_underscore(s);
}

int p_is_snake(const char *s)
{
    return !p_is_pascal(s);
}
