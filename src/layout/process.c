#include "layout/internal.h"

/* Documentation tokens are trivia for every line-shape decision. */
static int is_doc(const ek_token *t)
{
    return t->kind == EK_TOK_DOC;
}

static size_t sig_count(const line_run *l)
{
    size_t i, n = 0;

    for (i = 0; i < l->count; i++) {
        if (!is_doc(&l->tok[i])) {
            n++;
        }
    }
    return n;
}

const ek_token *sig_first(const line_run *l)
{
    size_t i;

    for (i = 0; i < l->count; i++) {
        if (!is_doc(&l->tok[i])) {
            return &l->tok[i];
        }
    }
    return NULL;
}

static const ek_token *sig_last(const line_run *l)
{
    size_t i;

    for (i = l->count; i > 0; i--) {
        if (!is_doc(&l->tok[i - 1])) {
            return &l->tok[i - 1];
        }
    }
    return NULL;
}

/* A document attaches to a binding line, so the run is handed over only
   when the line can begin one — a name, a group, a brace, or the
   literal heads a pattern binding may carry. A document before any
   other construct is dropped silently. */
static int binding_line(const line_run *l)
{
    const ek_token *t = sig_first(l);

    if (!t) {
        return 0;
    }
    switch (t->kind) {
    case EK_TOK_IDENT:
    case EK_TOK_LPAREN:
    case EK_TOK_LBRACE:
    case EK_TOK_LARROW:
    case EK_TOK_INT:
    case EK_TOK_FLOAT:
    case EK_TOK_CHAR:
    case EK_TOK_STRING:
    case EK_TOK_MINUS:
        return 1;
    default:
        return 0;
    }
}

static void collect_docs(layout_ctx *c, const line_run *l)
{
    size_t i;

    for (i = 0; i < l->count; i++) {
        if (is_doc(&l->tok[i])) {
            lo_push_doc(c, &l->tok[i]);
        }
    }
}

static int first_dot_col(const line_run *l)
{
    size_t i;

    for (i = 0; i < l->count; i++) {
        if (l->tok[i].kind == EK_TOK_DOT) {
            return l->tok[i].col;
        }
    }
    return -1;
}

/* Whether a line ending in this token opens a block for the lines
   indented under it. */
static int opens_block(ek_token_kind kind)
{
    return kind == EK_TOK_COLON || kind == EK_TOK_EQ
           || kind == EK_TOK_ARROW;
}

/* Whether a line ending in this token leaves the expression open, so a
   deeper line continues it.  A lone `!` or `?` ends a member. */
static int continues_member(ek_token_kind kind)
{
    switch (kind) {
    case EK_TOK_PLUS:
    case EK_TOK_MINUS:
    case EK_TOK_STAR:
    case EK_TOK_SLASH:
    case EK_TOK_PERCENT:
    case EK_TOK_PLUSPLUS:
    case EK_TOK_EQEQ:
    case EK_TOK_BANGEQ:
    case EK_TOK_LT:
    case EK_TOK_LTEQ:
    case EK_TOK_GT:
    case EK_TOK_GTEQ:
    case EK_TOK_ANDAND:
    case EK_TOK_OROR:
    case EK_TOK_LARROW:
    case EK_TOK_DOTDOT:
    case EK_TOK_DOTDOTEQ:
    case EK_TOK_COMMA:
    case EK_TOK_COLONEQ:
    case EK_TOK_DOT:
    case EK_TOK_PIPE:
        return 1;
    default:
        return 0;
    }
}

/* Whether the line holds a ':' outside brackets, which ends a case
   header. */
static int has_top_colon(const line_run *l)
{
    int depth = 0;
    size_t i;

    for (i = 0; i < l->count; i++) {
        switch (l->tok[i].kind) {
        case EK_TOK_LPAREN:
        case EK_TOK_LBRACKET:
        case EK_TOK_LBRACE:
            depth++;
            break;
        case EK_TOK_RPAREN:
        case EK_TOK_RBRACKET:
        case EK_TOK_RBRACE:
            if (depth > 0) {
                depth--;
            }
            break;
        case EK_TOK_COLON:
            if (depth == 0) {
                return 1;
            }
            break;
        default:
            break;
        }
    }
    return 0;
}

/* Whether a deeper line after prev holds a continuation license: a
   chain link, a continued case scrutinee, or an expression the
   previous line left open. */
int deeper_licensed(const layout_ctx *c, const line_run *prev,
                    const line_run *cur)
{
    if (sig_first(cur)->kind == EK_TOK_DOT) {
        return 1;
    }
    if (c->case_pending) {
        return 1;
    }
    return continues_member(sig_last(prev)->kind);
}

/* Emits the line's tokens; closers first close the callback blocks
   their bracket ends, with DEDENTs before the bracket. */
static void emit_line(layout_ctx *c, const line_run *l)
{
    size_t i;

    for (i = 0; i < l->count; i++) {
        const ek_token *t = &l->tok[i];

        if (is_doc(t)) {
            continue;
        }
        if (t->kind == EK_TOK_LPAREN || t->kind == EK_TOK_LBRACKET
            || t->kind == EK_TOK_LBRACE) {
            lo_emit_source(c, t);
            c->depth++;
        } else if (t->kind == EK_TOK_RPAREN || t->kind == EK_TOK_RBRACKET
                   || t->kind == EK_TOK_RBRACE) {
            close_cb_depth(c, c->depth, t->line, t->col);
            lo_emit_source(c, t);
            if (c->depth > 0) {
                c->depth--;
            }
        } else {
            lo_emit_source(c, t);
        }
    }
}

/* End-of-line state: a parameter group followed by ':' inside brackets
   opens a callback block; a depth-zero line ending in a block opener
   opens a block, and a dot fixes the chain column. */
static void line_state(layout_ctx *c, const line_run *l)
{
    const ek_token *first = sig_first(l);
    const ek_token *last = sig_last(l);

    if (c->depth > 0) {
        if (last > l->tok && last->kind == EK_TOK_COLON
            && last[-1].kind == EK_TOK_RPAREN) {
            lo_push_cb(c, first->col - 1, c->depth);
        }
        return;
    }
    c->expect_block = opens_block(last->kind);
    if (c->expect_block) {
        c->header_col = first->col - 1;
    }
    if (first->kind == EK_TOK_KW_IF) {
        c->last_if_col = first->col - 1;
    }
    if (first->kind == EK_TOK_KW_CASE) {
        c->case_pending = 1;
    }
    if (c->case_pending && has_top_colon(l)) {
        c->case_pending = 0;
    }
    c->chain_col = first_dot_col(l);
}

/* The next significant line, or count when none follows. */
static size_t next_sig(const line_run *lines, size_t count, size_t idx)
{
    size_t j = idx + 1;

    while (j < count && sig_count(&lines[j]) == 0) {
        j++;
    }
    return j;
}

/* The next line is deeper with no block expected, so it continues or
   splices and the previous line's NEWLINE is suppressed. */
static int next_is_splice(const layout_ctx *c, const line_run *lines,
                          size_t count, size_t idx)
{
    size_t j = next_sig(lines, count, idx);

    if (j == count) {
        return 0;
    }
    if (sig_first(&lines[j])->col - 1 <= top_col(c) || c->expect_block) {
        return 0;
    }
    if (c->stray_base >= 0) {
        return 0;
    }
    return deeper_licensed(c, &lines[idx], &lines[j]);
}

/* A member line at depth zero ends with NEWLINE unless its break is
   suppressed because the line sits inside brackets or splices. */
static void line_terminator(layout_ctx *c, const line_run *lines,
                            size_t count, size_t idx)
{
    const line_run *l = &lines[idx];
    const ek_token *at;

    c->after_break = 0;
    if (c->depth > 0 || next_is_splice(c, lines, count, idx)) {
        return;
    }
    at = l->nl ? l->nl : c->eof;
    lo_emit(c, EK_LTOK_NEWLINE, at->line, at->col);
    c->after_break = 1;
}

/* End of file closes every open level, callback blocks first. */
static void finish(layout_ctx *c)
{
    while (c->cb_count > 0) {
        int opened = c->cb[--c->cb_count].opened;

        if (opened) {
            lo_emit(c, EK_LTOK_DEDENT, c->eof->line, c->eof->col);
        }
    }
    while (c->col_count > 1) {
        c->col_count--;
        lo_emit(c, EK_LTOK_DEDENT, c->eof->line, c->eof->col);
    }
    lo_emit(c, EK_LTOK_EOF, c->eof->line, c->eof->col);
}

void lo_process(layout_ctx *c, const line_run *lines, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        const line_run *l = &lines[i];

        if (sig_count(l) == 0) {
            if (l->count == 0) {
                lo_clear_docs(c);
            } else {
                collect_docs(c, l);
            }
            continue;
        }
        line_start(c, l);
        if (c->after_break && binding_line(l)) {
            lo_flush_docs(c);
        } else {
            lo_clear_docs(c);
        }
        emit_line(c, l);
        collect_docs(c, l);
        line_state(c, l);
        line_terminator(c, lines, count, i);
    }
    finish(c);
}
