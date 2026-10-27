#include <stdarg.h>

#include "layout/internal.h"

/* Layout error codes: relative indentation that is not a multiple of
   four, a line at a column its position forbids, a dedent matching no
   open block, and an indent no continuation licenses. */
#define CODE_STEP "E-indent-step"
#define CODE_COLUMN "E-indent-column"
#define CODE_DEDENT "E-indent-dedent"
#define CODE_CONTINUATION "E-syntax"

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

static const ek_token *sig_first(const line_run *l)
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
   when the line can begin one. A document before any other construct is
   dropped silently. */
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

/* The stack is empty only while an allocation failure drains the run;
   the bottom column 0 is the safe answer then. */
static int top_col(const layout_ctx *c)
{
    return c->col_count ? c->cols[c->col_count - 1] : 0;
}

static void err_at(layout_ctx *c, const char *code, const line_run *l,
                   const char *fmt, ...)
{
    va_list ap;

    const ek_token *t = sig_first(l);

    va_start(ap, fmt);
    ek_diag_emitv(c->diag, EK_SEV_ERROR, code, c->file,
                  t->line, t->col, fmt, ap);
    va_end(ap);
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
static int deeper_licensed(const layout_ctx *c, const line_run *prev,
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

/* Pops the indentation stack to col on a dedent, one DEDENT per pop; a
   col matching no stack entry is an error and recovers as if it
   matched. */
static void dedent_to(layout_ctx *c, int col, const line_run *l)
{
    int line = sig_first(l)->line, at = sig_first(l)->col;

    while (c->col_count > 1 && top_col(c) > col) {
        c->col_count--;
        lo_emit(c, EK_LTOK_DEDENT, line, at);
    }
    if (top_col(c) != col) {
        err_at(c, CODE_DEDENT, l,
               "dedent to column %d matches no open block", col + 1);
        lo_push_col(c, col, -1);
    }
}

/* An else line continues the if whose header begins in its column.  The
   if's branch block is the level the dedent closes, so a match on that
   level's header column ends the dedent even when the if's line was a
   continuation that never pushed its own level.  An if with an inline
   suite opened no level at all; its else is still a continuation. */
static void else_dedent(layout_ctx *c, int col, const line_run *l)
{
    int line = sig_first(l)->line, at = sig_first(l)->col;

    while (c->col_count > 1 && top_col(c) > col) {
        int header = c->hdr[c->col_count - 1];

        c->col_count--;
        lo_emit(c, EK_LTOK_DEDENT, line, at);
        if (header == col) {
            return;
        }
    }
    if (top_col(c) == col) {
        return;
    }
    if (col > top_col(c) && c->last_if_col == col) {
        return;
    }
    err_at(c, CODE_DEDENT, l,
           "dedent to column %d matches no open block", col + 1);
    lo_push_col(c, col, -1);
}

/* At a line start, closes every callback block whose header sits at
   or right of col, emitting the DEDENTs owed for opened blocks. */
static void close_cb_col(layout_ctx *c, int col, int line, int at)
{
    while (c->cb_count > 0 && c->cb[c->cb_count - 1].header_col >= col) {
        int opened = c->cb[--c->cb_count].opened;

        if (opened) {
            lo_emit(c, EK_LTOK_DEDENT, line, at);
        }
    }
}

/* At a closing bracket, a bracket closing from depth d closes every
   callback block opened at depth d or deeper. */
static void close_cb_depth(layout_ctx *c, int depth, int line, int at)
{
    while (c->cb_count > 0 && c->cb[c->cb_count - 1].header_depth >= depth) {
        int opened = c->cb[--c->cb_count].opened;

        if (opened) {
            lo_emit(c, EK_LTOK_DEDENT, line, at);
        }
    }
}

/* A line inside brackets. Only callback blocks impose structure here;
   every other line break is free formatting. */
static void bracket_start(layout_ctx *c, const line_run *l, int col)
{
    int line = sig_first(l)->line, at = sig_first(l)->col;
    const cb_block *cb;

    close_cb_col(c, col, line, at);
    if (c->cb_count == 0) {
        return;
    }
    cb = &c->cb[c->cb_count - 1];
    if (cb->member_col == 0) {
        if ((col - cb->header_col) % 4 != 0) {
            err_at(c, CODE_STEP, l, "indentation step of %d from the "
                   "callback header is not a multiple of four",
                   col - cb->header_col);
        }
        lo_emit(c, EK_LTOK_NEWLINE, line, at);
        lo_emit(c, EK_LTOK_INDENT, line, at);
        c->after_break = 1;
        c->cb[c->cb_count - 1].member_col = col;
        c->cb[c->cb_count - 1].opened = 1;
    } else if (col == cb->member_col) {
        lo_emit(c, EK_LTOK_NEWLINE, line, at);
        c->after_break = 1;
    } else if (col < cb->member_col) {
        err_at(c, CODE_COLUMN, l, "callback body line begins at column %d; "
               "its members begin in column %d", col + 1,
               cb->member_col + 1);
        lo_emit(c, EK_LTOK_NEWLINE, line, at);
        c->after_break = 1;
    }
}

/* A line at bracket depth zero. Continuations splice: no token is
   emitted for the break. */
static void top_start(layout_ctx *c, const line_run *l, int col)
{
    if (sig_first(l)->kind == EK_TOK_KW_ELSE) {
        else_dedent(c, col, l);
        return;
    }
    if (top_col(c) == col) {
        return;
    }
    if (col > top_col(c)) {
        if (c->expect_block) {
            int base = c->stray_base >= 0 ? c->header_col : top_col(c);

            if ((col - base) % 4 != 0) {
                err_at(c, CODE_STEP, l, "indentation step of %d is not a "
                       "multiple of four", col - base);
            }
            lo_push_col(c, col, c->header_col);
            lo_emit(c, EK_LTOK_INDENT, sig_first(l)->line,
                    sig_first(l)->col);
        } else if (c->stray_base >= 0) {
            return;
        } else if (!c->seen) {
            return;
        } else if (deeper_licensed(c, c->prev, l)) {
            if (sig_first(l)->kind == EK_TOK_DOT && c->chain_col >= 0
                && c->chain_col != sig_first(l)->col) {
                err_at(c, CODE_COLUMN, l, "chain line begins at column %d; "
                       "the chain's first dot is at column %d",
                       sig_first(l)->col, c->chain_col);
            }
        } else {
            err_at(c, CODE_CONTINUATION, l, "indentation to column %d "
                   "follows a complete member with no continuation "
                   "license", col + 1);
            c->stray_base = top_col(c);
        }
        return;
    }
    dedent_to(c, col, l);
}

static void line_start(layout_ctx *c, const line_run *l)
{
    int col = sig_first(l)->col - 1;

    if (c->stray_base >= 0 && col <= c->stray_base) {
        c->stray_base = -1;
    }
    if (c->depth > 0) {
        bracket_start(c, l, col);
    } else {
        top_start(c, l, col);
    }
    c->seen = 1;
    c->prev = l;
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
