#include <string.h>

#include "lex/internal.h"

int lex_char_literal(lex_ctx *c, gbuf *out)
{
    int sline = c->line, scol = c->col;
    uint32_t value = 0;
    int count = 0;
    ek_token t;

    c->pos++; /* opening quote */
    c->col++;
    for (;;) {
        int b = ctx_byte(c, 0);

        if (b < 0 || b == '\n') {
            ctx_error(c, "E-unterminated-char", sline, scol,
                      "character literal is not terminated");
            return 1;
        }
        if (b == '\r') {
            int line = c->line, col = c->col;

            c->pos++;
            if (ctx_byte(c, 0) == '\n') {
                ctx_error(c, "E-unterminated-char", sline, scol,
                          "character literal is not terminated");
                return 1;
            }
            ctx_error(c, "E-stray-cr", line, col,
                      "carriage return outside a line terminator");
            continue;
        }
        if (b == '\'') {
            c->pos++;
            c->col++;
            break;
        }
        if (b == '\\') {
            uint32_t cp;
            esc_result r = scan_escape(c, 1, &cp);

            if (r == ESC_END) {
                ctx_error(c, "E-unterminated-char", sline, scol,
                          "character literal is not terminated");
                return 1;
            }
            if (r == ESC_OK) {
                value = cp;
                count++;
            }
            continue;
        }
        {
            ek_utf8_decoded d = ek_utf8_decode(c->src + c->pos,
                                               c->len - c->pos);
            if (d.len == 0) {
                return 1;
            }
            if (d.status != EK_UTF8_OK) {
                ctx_error(c, "E-utf8", c->line, c->col,
                          "source is not valid UTF-8");
            } else if (d.cp == 0xFEFF) {
                ctx_error(c, "E-stray-bom", c->line, c->col,
                          "byte order mark outside the start of the file");
            } else {
                value = d.cp;
                count++;
            }
            c->pos += d.len;
            c->col++;
        }
    }
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
