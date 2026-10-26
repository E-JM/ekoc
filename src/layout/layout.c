#include <string.h>

#include "layout/internal.h"

void lo_oom(layout_ctx *c)
{
    if (c->oom) {
        return;
    }
    c->oom = 1;
    ek_diag_emit(c->diag, EK_SEV_ERROR, NULL, c->file, 1, 1,
                 "out of memory while laying out; results are partial");
}

/* Doubles a growable arena array, copying count items to the new block.
   Old blocks stay in the arena and are freed with it. */
static void *grow(layout_ctx *c, void *data, size_t count, size_t *cap,
                  size_t item)
{
    size_t want = *cap ? *cap * 2 : 64;
    void *p = ek_arena_alloc(c->arena, want * item);

    if (!p) {
        lo_oom(c);
        return data;
    }
    if (count > 0) {
        memcpy(p, data, count * item);
    }
    *cap = want;
    return p;
}

void lo_emit(layout_ctx *c, ek_layout_kind kind, int line, int col)
{
    ek_layout_token t;

    if (c->oom) {
        return;
    }
    if (c->out.len == c->out.cap) {
        ek_layout_token *p = grow(c, c->out.data, c->out.len, &c->out.cap,
                                  sizeof *p);
        if (c->oom) {
            return;
        }
        c->out.data = p;
    }
    memset(&t, 0, sizeof t);
    t.kind = kind;
    t.line = line;
    t.col = col;
    c->out.data[c->out.len++] = t;
}

void lo_emit_source(layout_ctx *c, const ek_token *t)
{
    ek_layout_token lt;

    if (c->oom) {
        return;
    }
    if (c->out.len == c->out.cap) {
        ek_layout_token *p = grow(c, c->out.data, c->out.len, &c->out.cap,
                                  sizeof *p);
        if (c->oom) {
            return;
        }
        c->out.data = p;
    }
    memset(&lt, 0, sizeof lt);
    lt.kind = EK_LTOK_SOURCE;
    lt.line = t->line;
    lt.col = t->col;
    lt.tok = *t;
    c->out.data[c->out.len++] = lt;
}

/* Pushes one indentation level: the column its members begin in and
   the column of the header line that opened it (-1 when unknown). */
void lo_push_col(layout_ctx *c, int col, int hdr)
{
    if (c->oom) {
        return;
    }
    if (c->col_count == c->col_cap) {
        size_t want = c->col_cap ? c->col_cap * 2 : 64;
        int *cols = ek_arena_alloc(c->arena, want * sizeof *cols);
        int *hdr_cols = ek_arena_alloc(c->arena, want * sizeof *hdr_cols);

        if (!cols || !hdr_cols) {
            lo_oom(c);
            return;
        }
        if (c->col_count > 0) {
            memcpy(cols, c->cols, c->col_count * sizeof *cols);
            memcpy(hdr_cols, c->hdr, c->col_count * sizeof *hdr_cols);
        }
        c->cols = cols;
        c->hdr = hdr_cols;
        c->col_cap = want;
    }
    c->cols[c->col_count] = col;
    c->hdr[c->col_count] = hdr;
    c->col_count++;
}

void lo_push_cb(layout_ctx *c, int header_col, int header_depth)
{
    if (c->oom) {
        return;
    }
    if (c->cb_count == c->cb_cap) {
        cb_block *p = grow(c, c->cb, c->cb_count, &c->cb_cap, sizeof *p);
        if (c->oom) {
            return;
        }
        c->cb = p;
    }
    c->cb[c->cb_count].header_col = header_col;
    c->cb[c->cb_count].header_depth = header_depth;
    c->cb[c->cb_count].member_col = 0;
    c->cb[c->cb_count].opened = 0;
    c->cb_count++;
}

/* Splits the token stream at NEWLINE and EOF terminators. The trailing
   input EOF is not a line. */
static line_run *split_lines(layout_ctx *c, const ek_token *tokens,
                             size_t count, size_t *out_count)
{
    line_run *lines = NULL;
    size_t cap = 0, n = 0, i = 0;

    while (i < count && tokens[i].kind != EK_TOK_EOF) {
        size_t start = i;

        while (i < count && tokens[i].kind != EK_TOK_NEWLINE
               && tokens[i].kind != EK_TOK_EOF) {
            i++;
        }
        if (n == cap) {
            line_run *p = grow(c, lines, n, &cap, sizeof *p);
            if (c->oom) {
                *out_count = n;
                return lines;
            }
            lines = p;
        }
        lines[n].tok = tokens + start;
        lines[n].count = i - start;
        lines[n].nl = i < count && tokens[i].kind == EK_TOK_NEWLINE
                          ? &tokens[i] : NULL;
        n++;
        if (i < count && tokens[i].kind == EK_TOK_NEWLINE) {
            i++;
        }
    }
    *out_count = n;
    return lines;
}

ek_layout_result ek_layout(const ek_token *tokens, size_t count, const char *file,
                           ek_arena *arena, ek_diag *diag)
{
    layout_ctx ctx;
    ek_token fallback_eof;
    line_run *lines;
    size_t line_count = 0;
    ek_layout_result r;

    memset(&ctx, 0, sizeof ctx);
    ctx.file = file;
    ctx.arena = arena;
    ctx.diag = diag;
    ctx.chain_col = -1;
    ctx.last_if_col = -1;
    ctx.stray_base = -1;
    if (count > 0) {
        ctx.eof = &tokens[count - 1];
    } else {
        memset(&fallback_eof, 0, sizeof fallback_eof);
        fallback_eof.line = 1;
        fallback_eof.col = 1;
        ctx.eof = &fallback_eof;
    }
    lo_push_col(&ctx, 0, -1);
    lines = split_lines(&ctx, tokens, count, &line_count);
    if (lines) {
        lo_process(&ctx, lines, line_count);
    } else {
        lo_emit(&ctx, EK_LTOK_EOF, ctx.eof->line, ctx.eof->col);
    }
    r.tokens = ctx.out.data;
    r.count = ctx.out.len;
    return r;
}
