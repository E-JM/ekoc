#include "lex/internal.h"

/* Escapes shared by string and character literals: every string escape
   also cooks inside a character literal, so \{ is a literal brace in
   both. \' is a character-literal escape only. */

static int hex_value(int b)
{
    if (b >= '0' && b <= '9') {
        return b - '0';
    }
    if (b >= 'a' && b <= 'f') {
        return b - 'a' + 10;
    }
    if (b >= 'A' && b <= 'F') {
        return b - 'A' + 10;
    }
    return -1;
}

/* The braced digits of \u, entered on '{'. Returns ESC_OK with the
   value and the digit count; ESC_ERROR after diagnosing, leaving a
   line terminator or end of input for the caller. */
static esc_result scan_unicode_braces(lex_ctx *c, int line, int col,
                                      uint32_t *out, int *digits)
{
    uint32_t value = 0;

    *digits = 0;
    c->pos++; /* '{' */
    c->col++;
    for (;;) {
        int b = ctx_byte(c, 0);
        int d = hex_value(b);

        if (d >= 0) {
            if (*digits == 6) {
                ctx_error(c, "E-escape", line, col,
                          "escape \\u takes at most six digits");
                return ESC_ERROR;
            }
            value = value * 16 + (uint32_t)d;
            (*digits)++;
            c->pos++;
            c->col++;
            continue;
        }
        if (b == '}') {
            c->pos++;
            c->col++;
            *out = value;
            return ESC_OK;
        }
        if (b < 0 || b == '\n' || b == '\r') {
            ctx_error(c, "E-escape", line, col,
                      "escape \\u is not terminated");
            return ESC_STOP; /* the terminator belongs to the caller */
        }
        ctx_error(c, "E-escape", line, col,
                  "escape \\u has a non-hexadecimal digit");
        c->pos++;
        c->col++;
        return ESC_ERROR;
    }
}

static esc_result scan_unicode(lex_ctx *c, int line, int col, uint32_t *out)
{
    uint32_t value = 0;
    int digits = 0;
    esc_result r;

    if (ctx_byte(c, 0) != '{') {
        ctx_error(c, "E-escape", line, col,
                  "escape \\u expects a braced scalar value");
        return ESC_ERROR;
    }
    r = scan_unicode_braces(c, line, col, &value, &digits);
    if (r != ESC_OK) {
        return r; /* ESC_STOP keeps the unterminated span distinct */
    }
    if (digits == 0) {
        ctx_error(c, "E-escape", line, col, "escape \\u has no digits");
        return ESC_ERROR;
    }
    if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        ctx_error(c, "E-escape", line, col,
                  "escape \\u is not a Unicode scalar value");
        return ESC_ERROR;
    }
    *out = value;
    return ESC_OK;
}

esc_result scan_escape(lex_ctx *c, int in_char, uint32_t *out)
{
    int line = c->line, col = c->col;
    int b;

    c->pos++; /* backslash */
    c->col++;
    b = ctx_byte(c, 0);
    if (b < 0 || b == '\n' || b == '\r') {
        return ESC_END; /* the literal ends here, unconsumed */
    }
    c->pos++;
    c->col++;
    switch (b) {
    case 'n': *out = 0x0A; return ESC_OK;
    case 't': *out = 0x09; return ESC_OK;
    case 'r': *out = 0x0D; return ESC_OK;
    case '\\': *out = 0x5C; return ESC_OK;
    case '"': *out = 0x22; return ESC_OK;
    case '\'':
        if (in_char) {
            *out = 0x27;
            return ESC_OK;
        }
        ctx_error(c, "E-escape", line, col,
                  "escape \\' is not valid in a string literal");
        return ESC_ERROR;
    case '{': *out = 0x7B; return ESC_OK;
    case 'u': return scan_unicode(c, line, col, out);
    default:
        ctx_error(c, "E-escape", line, col, "unknown escape sequence");
        return ESC_ERROR;
    }
}
