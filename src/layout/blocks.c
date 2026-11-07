#include <stdarg.h>

#include "layout/internal.h"

/* Layout error codes: relative indentation that is not a multiple of
   four, a line at a column its position forbids, a dedent matching no
   open block, and an indent no continuation licenses. */
#define CODE_STEP "E-indent-step"
#define CODE_COLUMN "E-indent-column"
#define CODE_DEDENT "E-indent-dedent"
#define CODE_CONTINUATION "E-syntax"

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

/* The stack is empty only while an allocation failure drains the run;
   the bottom column 0 is the safe answer then. */
int top_col(const layout_ctx *c)
{
    return c->col_count ? c->cols[c->col_count - 1] : 0;
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
void close_cb_depth(layout_ctx *c, int depth, int line, int at)
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

void line_start(layout_ctx *c, const line_run *l)
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
