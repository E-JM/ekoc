#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "lex/internal.h"

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

static int digit_value(int b, int base)
{
    if (base == 16) {
        return hex_value(b);
    }
    if (b < '0' || b > '9') {
        return -1;
    }
    if (base == 2 && b > '1') {
        return -1;
    }
    return b - '0';
}

/* Accumulates one digit; sets *overflow when the value outgrows 64 bits
   and keeps counting modulo 2^64 (the text stays authoritative). */
static void take_digit(lex_ctx *c, int base, unsigned long long *mag,
                       int *overflow)
{
    int d = digit_value(ctx_byte(c, 0), base);
    unsigned long long dig = (unsigned long long)d;

    if (*mag > (ULLONG_MAX - dig) / (unsigned long long)base) {
        *overflow = 1;
    }
    *mag = *mag * (unsigned long long)base + dig;
    c->pos++;
    c->col++;
}

/* Digit { ['_'] Digit }, Clause A.2.4. A stranded underscore is
   diagnosed and ends the literal; the next token starts there. */
static void scan_digits(lex_ctx *c, int base, unsigned long long *mag,
                        int *overflow)
{
    take_digit(c, base, mag, overflow);
    for (;;) {
        if (ctx_byte(c, 0) == '_') {
            if (digit_value(ctx_byte(c, 1), base) < 0) {
                ctx_error(c, "E-digit-grouping", c->line, c->col,
                          "underscore must lie between two digits");
                return;
            }
            c->pos++;
            c->col++;
        } else if (digit_value(ctx_byte(c, 0), base) < 0) {
            return;
        }
        take_digit(c, base, mag, overflow);
    }
}

/* Exponent := 'e' [ '+' | '-' ] DecDigit { DecDigit }, Clause A.2.5.
   Returns 1 when the marker joined the literal; a bare 'e', or a sign
   without digits, is left for the next token. */
static int scan_exponent(lex_ctx *c)
{
    int s1 = ctx_byte(c, 1), s2 = ctx_byte(c, 2);
    int b;

    if (!(s1 >= '0' && s1 <= '9')
        && !((s1 == '+' || s1 == '-') && s2 >= '0' && s2 <= '9')) {
        return 0;
    }
    c->pos++;
    c->col++;
    if (ctx_byte(c, 0) == '+' || ctx_byte(c, 0) == '-') {
        c->pos++;
        c->col++;
    }
    while ((b = ctx_byte(c, 0)) >= '0' && b <= '9') {
        c->pos++;
        c->col++;
    }
    return 1;
}

/* Decimal digits, then fraction and exponent per Clause A.2.5: a '.'
   joins the literal only between digits, and the exponent admits no
   grouping underscores. */
static void scan_decimal(lex_ctx *c, unsigned long long *mag, int *overflow,
                         int *is_float)
{
    scan_digits(c, 10, mag, overflow);
    if (ctx_byte(c, 0) == '.' && ctx_byte(c, 1) >= '0'
        && ctx_byte(c, 1) <= '9') {
        c->pos++;
        c->col++;
        *is_float = 1;
        scan_digits(c, 10, mag, overflow);
    }
    if (ctx_byte(c, 0) == 'e' && scan_exponent(c)) {
        *is_float = 1;
    }
}

/* HexInteger | BinaryInteger of A.2.4, called on '0x' or '0b'. Sets
   *base when the prefix joins the literal; a stranded '_' after the
   prefix is diagnosed, and the lone '0' is consumed either way so the
   rest lexes as its own tokens (Clause 5.10.3). */
static void scan_prefix(lex_ctx *c, unsigned long long *mag, int *overflow,
                        int *base)
{
    int pbase = ctx_byte(c, 1) == 'x' ? 16 : 2;

    if (digit_value(ctx_byte(c, 2), pbase) >= 0) {
        *base = pbase;
        c->pos += 2;
        c->col += 2;
        scan_digits(c, pbase, mag, overflow);
        return;
    }
    if (ctx_byte(c, 2) == '_') {
        ctx_error(c, "E-digit-grouping", c->line, c->col + 2,
                  "underscore must lie between two digits");
    }
    c->pos++;
    c->col++;
}

static char *strip_underscores(lex_ctx *c, const char *text, size_t len)
{
    char *clean = ek_arena_alloc(c->arena, len + 1);
    size_t i, n = 0;

    if (!clean) {
        ctx_oom(c);
        return NULL;
    }
    for (i = 0; i < len; i++) {
        if (text[i] != '_') {
            clean[n++] = text[i];
        }
    }
    clean[n] = '\0';
    return clean;
}

/* Emits the token: INT carries magnitude, base, and the overflow mark
   (Clause 5.7.4 defers E-literal-overflow to check); FLOAT carries the
   value of the cleaned spelling. */
static int emit_number(lex_ctx *c, gbuf *out, size_t start, int sline,
                       int scol, unsigned long long mag, int overflow,
                       int is_float, int base)
{
    const char *text = ctx_slice(c, start, c->pos);
    ek_token t;
    double fvalue = 0.0;

    if (!text) {
        return 0;
    }
    if (is_float) {
        char *clean = strip_underscores(c, text, c->pos - start);

        if (!clean) {
            return 0;
        }
        fvalue = strtod(clean, NULL);
    }
    memset(&t, 0, sizeof t);
    t.kind = is_float ? EK_TOK_FLOAT : EK_TOK_INT;
    t.line = sline;
    t.col = scol;
    t.text = text;
    t.len = c->pos - start;
    t.magnitude = mag;
    t.overflow = overflow;
    t.base = base;
    t.fvalue = fvalue;
    return emit_token(c, out, &t);
}

int lex_number(lex_ctx *c, gbuf *out)
{
    size_t start = c->pos;
    int sline = c->line, scol = c->col;
    unsigned long long mag = 0;
    int overflow = 0, is_float = 0, base = 10;

    if (ctx_byte(c, 0) == '0'
        && (ctx_byte(c, 1) == 'x' || ctx_byte(c, 1) == 'b')) {
        scan_prefix(c, &mag, &overflow, &base);
    } else {
        scan_decimal(c, &mag, &overflow, &is_float);
    }
    return emit_number(c, out, start, sline, scol, mag, overflow, is_float,
                       base);
}
