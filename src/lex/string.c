#include <string.h>

#include "lex/internal.h"

static void unterminated(lex_ctx *c, int sline, int scol)
{
    ctx_error(c, "E-unterminated-string", sline, scol,
              "string literal is not terminated");
}

static int flush_literal(lex_ctx *c, gbuf *seg, gbuf *parts)
{
    ek_string_part p;

    if (seg->len == 0) {
        return 1;
    }
    memset(&p, 0, sizeof p);
    p.text = seg->data;
    p.text_len = seg->len;
    if (!gbuf_put(parts, &p, sizeof p)) {
        ctx_oom(c);
        return 0;
    }
    gbuf_init(seg, seg->arena); /* the spent block lives on in the arena */
    return 1;
}

/* Interpolation slot, Clause 5.9.3. Returns 1 when the slot closed at
   its matching brace, -1 when the string died inside the slot, 0 when
   the arena is exhausted. An empty run is E-empty-slot; braces inside
   the run balance through the LBRACE/RBRACE token count. A surplus of
   braces always swallows the closing quote, so the failure surfaces as
   E-unterminated-string and needs no code of its own. */
static int scan_slot(lex_ctx *c, gbuf *parts, int sline, int scol)
{
    int bline = c->line, bcol = c->col;
    gbuf run;
    int depth = 0;

    gbuf_init(&run, c->arena);
    c->pos++; /* '{' */
    c->col++;
    for (;;) {
        int b = ctx_byte(c, 0);

        if (b == ' ' || b == '\t') {
            c->pos++;
            c->col++;
            continue;
        }
        if (b == '\r') {
            int line = c->line, col = c->col;

            c->pos++;
            if (ctx_byte(c, 0) == '\n') {
                unterminated(c, sline, scol);
                return -1;
            }
            ctx_error(c, "E-stray-cr", line, col,
                      "carriage return outside a line terminator");
            continue;
        }
        if (b < 0 || b == '\n') {
            unterminated(c, sline, scol);
            return -1;
        }
        if (b == '/' && ctx_byte(c, 1) == '/') {
            lex_skip_line_comment(c);
            continue;
        }
        if (b == '/' && ctx_byte(c, 1) == '*') {
            ctx_error(c, "E-block-comment", c->line, c->col,
                      "block comments do not exist; use //");
            c->pos += 2;
            c->col += 2;
            continue;
        }
        if (b == '}' && depth == 0) {
            ek_string_part p;

            c->pos++;
            c->col++;
            if (run.len == 0) {
                ctx_error(c, "E-empty-slot", bline, bcol,
                          "interpolation slot is empty");
                return 1;
            }
            memset(&p, 0, sizeof p);
            p.tokens = (ek_token *)run.data;
            p.token_count = run.len / sizeof(ek_token);
            if (!gbuf_put(parts, &p, sizeof p)) {
                ctx_oom(c);
                return 0;
            }
            return 1;
        }
        {
            size_t before = run.len;
            ek_token_kind k;

            if (!lex_step(c, &run, lex_token)) {
                return 0;
            }
            if (run.len > before) {
                k = ((const ek_token *)(run.data + run.len
                                        - sizeof(ek_token)))->kind;
                if (k == EK_TOK_LBRACE) {
                    depth++;
                } else if (k == EK_TOK_RBRACE) {
                    depth--;
                }
            }
        }
    }
}

static int cook_scalar(lex_ctx *c, gbuf *seg)
{
    ek_utf8_decoded d = ek_utf8_decode(c->src + c->pos, c->len - c->pos);

    if (d.len == 0) {
        return 0;
    }
    if (d.status != EK_UTF8_OK) {
        ctx_error(c, "E-utf8", c->line, c->col, "source is not valid UTF-8");
    } else if (d.cp == 0xFEFF) {
        ctx_error(c, "E-stray-bom", c->line, c->col,
                  "byte order mark outside the start of the file");
    } else if (!gbuf_put(seg, c->src + c->pos, d.len)) {
        ctx_oom(c);
        return 0;
    }
    c->pos += d.len;
    c->col++;
    return 1;
}

int lex_string_literal(lex_ctx *c, gbuf *out)
{
    int sline = c->line, scol = c->col;
    gbuf seg, parts;
    ek_token t;

    gbuf_init(&seg, c->arena);
    gbuf_init(&parts, c->arena);
    c->pos++; /* opening quote */
    c->col++;
    for (;;) {
        int b = ctx_byte(c, 0);

        if (b < 0 || b == '\n') {
            unterminated(c, sline, scol);
            return 1;
        }
        if (b == '\r') {
            int line = c->line, col = c->col;

            c->pos++;
            if (ctx_byte(c, 0) == '\n') {
                unterminated(c, sline, scol);
                return 1;
            }
            ctx_error(c, "E-stray-cr", line, col,
                      "carriage return outside a line terminator");
            continue;
        }
        if (b == '"') {
            c->pos++;
            c->col++;
            break;
        }
        if (b == '\\') {
            uint32_t cp;
            esc_result r = scan_escape(c, 0, &cp);
            char buf[4];
            size_t n;

            if (r == ESC_END) {
                unterminated(c, sline, scol);
                return 1;
            }
            if (r == ESC_OK) {
                n = ek_utf8_encode(cp, buf);
                if (!gbuf_put(&seg, buf, n)) {
                    ctx_oom(c);
                    return 0;
                }
            }
            continue;
        }
        if (b == '{') {
            if (!flush_literal(c, &seg, &parts)) {
                return 0;
            }
            if (c->depth >= EK_LEX_MAX_SLOT_DEPTH) {
                ctx_error(c, "E-slot-depth", c->line, c->col,
                          "interpolation slots are nested too deeply");
                c->pos++;
                c->col++;
                continue;
            }
            c->depth++;
            {
                int r = scan_slot(c, &parts, sline, scol);
                c->depth--;
                if (r <= 0) {
                    return r == 0 ? 0 : 1;
                }
            }
            continue;
        }
        if (b == '}') {
            if (!gbuf_char(&seg, '}')) {
                ctx_oom(c);
                return 0;
            }
            c->pos++;
            c->col++;
            continue;
        }
        if (!cook_scalar(c, &seg)) {
            return 0;
        }
    }
    if (!flush_literal(c, &seg, &parts)) {
        return 0;
    }
    memset(&t, 0, sizeof t);
    t.kind = EK_TOK_STRING;
    t.line = sline;
    t.col = scol;
    t.parts = (ek_string_part *)parts.data;
    t.part_count = parts.len / sizeof(ek_string_part);
    return emit_token(c, out, &t);
}
