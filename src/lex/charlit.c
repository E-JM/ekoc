#include <string.h>

#include "lex/internal.h"

/* One raw scalar of the literal. Returns 0 at end of input; a faulty
   scalar is diagnosed and skipped, contributing no value. */
static int char_scalar(lex_ctx *c, uint32_t *value, int *count)
{
    ek_utf8_decoded d = ek_utf8_decode(c->src + c->pos, c->len - c->pos);

    if (d.len == 0) {
        return 0;
    }
    if (d.status != EK_UTF8_OK) {
        ctx_error(c, "E-encoding", c->line, c->col,
                  "source is not valid UTF-8");
    } else if (d.cp == 0xFEFF) {
        ctx_error(c, "E-stray-bom", c->line, c->col,
                  "byte order mark outside the start of the file");
    } else {
        *value = d.cp;
        (*count)++;
    }
    c->pos += d.len;
    c->col++;
    return 1;
}

/* Emits the CHAR token for exactly one cooked scalar; otherwise the
   one-scalar rule is diagnosed and no token is emitted. */
static int emit_char(lex_ctx *c, gbuf *out, uint32_t value, int count,
                     int sline, int scol)
{
    ek_token t;

    if (count == 1) {
        memset(&t, 0, sizeof t);
        t.kind = EK_TOK_CHAR;
        t.line = sline;
        t.col = scol;
        t.scalar = value;
        return emit_token(c, out, &t);
    }
    if (count == 0) {
        ctx_error(c, "E-char-literal", sline, scol,
                  "character literal holds no scalar value");
    } else {
        ctx_error(c, "E-char-literal", sline, scol,
                  "character literal holds more than one scalar value");
    }
    return 1;
}

/* Diagnoses the span and puts the scan back at the first bracket of
   the literal, so the recovered stream keeps a closer that sits past
   the defect. A span already diagnosed at or past the bracket stays
   put: rewinding would scan that material again and report its faults
   twice. */
static int unterminated(lex_ctx *c, int sline, int scol, int have_bound,
                        size_t bound_pos, int bound_col)
{
    if (have_bound && !(c->saw_fault && c->last_fault_pos >= bound_pos)) {
        c->pos = bound_pos;
        c->col = bound_col;
    }
    ctx_error(c, "E-unterminated-char", sline, scol,
              "character literal is not terminated");
    return 1;
}

int lex_char_literal(lex_ctx *c, gbuf *out)
{
    int sline = c->line, scol = c->col;
    uint32_t value = 0;
    int count = 0;
    size_t bound_pos = 0;
    int bound_col = 0, have_bound = 0;

    c->pos++; /* opening quote */
    c->col++;
    for (;;) {
        int b = ctx_byte(c, 0);

        if (b < 0 || b == '\n') {
            return unterminated(c, sline, scol, have_bound, bound_pos,
                                bound_col);
        }
        if (!have_bound && lex_is_bracket(b)) {
            have_bound = 1;
            bound_pos = c->pos;
            bound_col = c->col;
        }
        if (b == '\r') {
            if (lex_consume_cr(c)) {
                return unterminated(c, sline, scol, have_bound, bound_pos,
                                    bound_col);
            }
            continue;
        }
        if (b == '\'') {
            c->pos++;
            c->col++;
            return emit_char(c, out, value, count, sline, scol);
        }
        if (b == '\\') {
            uint32_t cp;
            esc_result r = scan_escape(c, 1, &cp);

            if (r == ESC_END) {
                return unterminated(c, sline, scol, have_bound, bound_pos,
                                    bound_col);
            }
            if (r == ESC_STOP) {
                return 1; /* the escape already diagnosed the span */
            }
            if (r == ESC_OK) {
                value = cp;
                count++;
            }
            continue;
        }
        if (!char_scalar(c, &value, &count)) {
            return 1;
        }
    }
}
