#include <stdarg.h>

#include "layout/internal.h"

/* Layout error codes: relative indentation that is not a multiple of
   four, a line at a column its position forbids, and a dedent
   matching no open block. */
#define CODE_STEP "E-indent-step"
#define CODE_COLUMN "E-indent-column"
#define CODE_DEDENT "E-indent-dedent"

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

    va_start(ap, fmt);
    ek_diag_emitv(c->diag, EK_SEV_ERROR, code, c->file,
                  l->tok[0].line, l->tok[0].col, fmt, ap);
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

/* Pops the indentation stack to col on a dedent, one DEDENT per pop; a
   col matching no stack entry is an error and recovers as if it
   matched. */
static void dedent_to(layout_ctx *c, int col, const line_run *l)
{
    int line = l->tok[0].line, at = l->tok[0].col;

    while (c->col_count > 1 && top_col(c) > col) {
        c->col_count--;
        lo_emit(c, EK_LTOK_DEDENT, line, at);
    }
    if (top_col(c) != col) {
        err_at(c, CODE_DEDENT, l,
               "dedent to column %d matches no open block", col);
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
    int line = l->tok[0].line, at = l->tok[0].col;

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
           "dedent to column %d matches no open block", col);
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
    int line = l->tok[0].line, at = l->tok[0].col;
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
        c->cb[c->cb_count - 1].member_col = col;
        c->cb[c->cb_count - 1].opened = 1;
    } else if (col == cb->member_col) {
        lo_emit(c, EK_LTOK_NEWLINE, line, at);
    } else if (col < cb->member_col) {
        err_at(c, CODE_COLUMN, l, "callback body line begins at column %d; "
               "its members begin in column %d", col, cb->member_col);
        lo_emit(c, EK_LTOK_NEWLINE, line, at);
    }
}

/* A line at bracket depth zero. Continuations splice: no token is
   emitted for the break. */
static void top_start(layout_ctx *c, const line_run *l, int col)
{
    if (l->tok[0].kind == EK_TOK_KW_ELSE) {
        else_dedent(c, col, l);
        return;
    }
    if (col == top_col(c)) {
        return;
    }
    if (col > top_col(c)) {
        if (c->expect_block) {
            if ((col - top_col(c)) % 4 != 0) {
                err_at(c, CODE_STEP, l, "indentation step of %d is not a "
                       "multiple of four", col - top_col(c));
            }
            lo_push_col(c, col, c->header_col);
            lo_emit(c, EK_LTOK_INDENT, l->tok[0].line, l->tok[0].col);
        } else if (l->tok[0].kind == EK_TOK_DOT && c->chain_col >= 0
                   && c->chain_col != l->tok[0].col) {
            err_at(c, CODE_COLUMN, l, "chain line begins at column %d; "
                   "the chain's first dot is at column %d",
                   l->tok[0].col, c->chain_col);
        }
        return;
    }
    dedent_to(c, col, l);
}

static void line_start(layout_ctx *c, const line_run *l)
{
    int col = l->tok[0].col - 1;

    if (c->depth > 0) {
        bracket_start(c, l, col);
    } else {
        top_start(c, l, col);
    }
}

/* Emits the line's tokens; closers first close the callback blocks
   their bracket ends, with DEDENTs before the bracket. */
static void emit_line(layout_ctx *c, const line_run *l)
{
    size_t i;

    for (i = 0; i < l->count; i++) {
        const ek_token *t = &l->tok[i];

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
   opens a callback block; a depth-zero line ending in ':' or '=' opens
   a block, and a dot fixes the chain column. */
static void line_state(layout_ctx *c, const line_run *l)
{
    const ek_token *last = &l->tok[l->count - 1];

    if (c->depth > 0) {
        if (l->count >= 2 && last->kind == EK_TOK_COLON
            && last[-1].kind == EK_TOK_RPAREN) {
            lo_push_cb(c, l->tok[0].col - 1, c->depth);
        }
        return;
    }
    c->expect_block = last->kind == EK_TOK_COLON || last->kind == EK_TOK_EQ;
    if (c->expect_block) {
        c->header_col = l->tok[0].col - 1;
    }
    if (l->tok[0].kind == EK_TOK_KW_IF) {
        c->last_if_col = l->tok[0].col - 1;
    }
    c->chain_col = first_dot_col(l);
}

/* The next significant line, or count when none follows. */
static size_t next_sig(const line_run *lines, size_t count, size_t idx)
{
    size_t j = idx + 1;

    while (j < count && lines[j].count == 0) {
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
    return lines[j].tok[0].col - 1 > top_col(c) && !c->expect_block;
}

/* A member line at depth zero ends with NEWLINE unless its break is
   suppressed because the line sits inside brackets or splices. */
static void line_terminator(layout_ctx *c, const line_run *lines,
                            size_t count, size_t idx)
{
    const line_run *l = &lines[idx];
    const ek_token *at;

    if (c->depth > 0 || next_is_splice(c, lines, count, idx)) {
        return;
    }
    at = l->nl ? l->nl : c->eof;
    lo_emit(c, EK_LTOK_NEWLINE, at->line, at->col);
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

        if (l->count == 0) {
            continue;
        }
        line_start(c, l);
        emit_line(c, l);
        line_state(c, l);
        line_terminator(c, lines, count, i);
    }
    finish(c);
}
