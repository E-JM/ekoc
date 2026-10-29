#include "ekort.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The parse family over String objects. Each entry validates the
   whole text before the value is read, so a rejected input never
   reaches the numeric conversion. Only the system cc builds this
   file, so the checked-arithmetic builtins carry the accumulation. */

/* The digit value of c in the radix, or -1 when c is not a digit of
   it. The hexadecimal letters read in either case. */
static int digit_value(char c, int radix)
{
    int v;

    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'a' && c <= 'f')
        v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        v = c - 'A' + 10;
    else
        return -1;
    return v < radix ? v : -1;
}

/* The unsigned magnitude of len digits, or INVALID on a stray
   character and RANGE when the accumulation leaves the 64-bit
   domain. */
static ek_parse_status parse_magnitude(const char *p, size_t len, int radix,
                                       uint64_t *out)
{
    uint64_t mag = 0;
    size_t i;

    for (i = 0; i < len; i++) {
        int d = digit_value(p[i], radix);

        if (d < 0)
            return EK_PARSE_INVALID;
        if (__builtin_mul_overflow(mag, (uint64_t)radix, &mag)
            || __builtin_add_overflow(mag, (uint64_t)d, &mag))
            return EK_PARSE_RANGE;
    }
    *out = mag;
    return EK_PARSE_OK;
}

/* The signed build: a negative value may reach one step below the
   positive maximum, which is where the minimum lives. */
static ek_parse_status finish_signed(uint64_t mag, int negative, int64_t *out)
{
    uint64_t limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;

    if (mag > limit)
        return EK_PARSE_RANGE;
    if (negative)
        *out = mag == limit ? INT64_MIN : -(int64_t)mag;
    else
        *out = (int64_t)mag;
    return EK_PARSE_OK;
}

/* An optional sign, then one or more digits, then nothing. */
static ek_parse_status parse_int_text(const char *p, size_t len, int radix,
                                      int64_t *out)
{
    size_t i = 0;
    int negative = 0;
    ek_parse_status st;
    uint64_t mag;

    if (len == 0)
        return EK_PARSE_EMPTY;
    if (p[0] == '+' || p[0] == '-') {
        negative = p[0] == '-';
        i = 1;
    }
    if (i == len)
        return EK_PARSE_INVALID;
    st = parse_magnitude(p + i, len - i, radix, &mag);
    if (st != EK_PARSE_OK)
        return st;
    return finish_signed(mag, negative, out);
}

/* The unsigned build: the sign is read for the grammar, and a minus
   denotes a value below zero, which is outside the limits. */
static ek_parse_status parse_uint_text(const char *p, size_t len, int radix,
                                       uint64_t *out)
{
    size_t i = 0;
    ek_parse_status st;
    uint64_t mag;

    if (len == 0)
        return EK_PARSE_EMPTY;
    if (p[0] == '+' || p[0] == '-')
        i = 1;
    if (i == len)
        return EK_PARSE_INVALID;
    st = parse_magnitude(p + i, len - i, radix, &mag);
    if (st != EK_PARSE_OK)
        return st;
    if (p[0] == '-')
        return EK_PARSE_RANGE;
    *out = mag;
    return EK_PARSE_OK;
}

ek_parse_status ek_parse_int(ek_obj *s, int64_t *out)
{
    return parse_int_text(ek_str_cstr(s), ek_str_len(s), 10, out);
}

ek_parse_status ek_parse_uint(ek_obj *s, uint64_t *out)
{
    return parse_uint_text(ek_str_cstr(s), ek_str_len(s), 10, out);
}

ek_parse_status ek_parse_int_radix(ek_obj *s, int radix, int64_t *out)
{
    return parse_int_text(ek_str_cstr(s), ek_str_len(s), radix, out);
}

ek_parse_status ek_parse_uint_radix(ek_obj *s, int radix, uint64_t *out)
{
    return parse_uint_text(ek_str_cstr(s), ek_str_len(s), radix, out);
}

/* Nonzero when the text is exactly the spelling. */
static int spell(const char *p, size_t len, const char *word)
{
    return strlen(word) == len && memcmp(p, word, len) == 0;
}

/* A decimal digit run: the start index sits on a digit and the scan
   moves past the run. */
static size_t digit_run(const char *p, size_t len, size_t i)
{
    while (i < len && p[i] >= '0' && p[i] <= '9')
        i++;
    return i;
}

/* An optional sign, then either the four non-finite spellings or a
   leading digit with an optional fraction and exponent, then
   nothing. */
ek_parse_status ek_parse_float(ek_obj *s, double *out)
{
    const char *p = ek_str_cstr(s);
    size_t len = ek_str_len(s);
    size_t i = 0;

    if (len == 0)
        return EK_PARSE_EMPTY;
    if (spell(p, len, "NaN")) {
        *out = NAN;
        return EK_PARSE_OK;
    }
    if (spell(p, len, "Inf") || spell(p, len, "+Inf")) {
        *out = INFINITY;
        return EK_PARSE_OK;
    }
    if (spell(p, len, "-Inf")) {
        *out = -INFINITY;
        return EK_PARSE_OK;
    }
    if (p[0] == '+' || p[0] == '-')
        i = 1;
    if (i >= len || p[i] < '0' || p[i] > '9')
        return EK_PARSE_INVALID;
    i = digit_run(p, len, i);
    if (i < len && p[i] == '.') {
        i++;
        if (i >= len || p[i] < '0' || p[i] > '9')
            return EK_PARSE_INVALID;
        i = digit_run(p, len, i);
    }
    if (i < len && (p[i] == 'e' || p[i] == 'E')) {
        i++;
        if (i < len && (p[i] == '+' || p[i] == '-'))
            i++;
        if (i >= len || p[i] < '0' || p[i] > '9')
            return EK_PARSE_INVALID;
        i = digit_run(p, len, i);
    }
    if (i != len)
        return EK_PARSE_INVALID;
    *out = strtod(p, NULL);
    return EK_PARSE_OK;
}
