#include "ekort.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

/* The display channel over the scalar tags. Int, UInt, and Bool print
   through the C formatters; Char encodes its code point; Float prints
   the shortest decimal form that reads back as the same double. */

ek_obj *ek_show_int(ek_val v)
{
    char buf[32];

    snprintf(buf, sizeof buf, "%" PRId64, v.as.i);
    return ek_str_from_cstr(buf);
}

ek_obj *ek_show_uint(ek_val v)
{
    char buf[32];

    snprintf(buf, sizeof buf, "%" PRIu64, v.as.u);
    return ek_str_from_cstr(buf);
}

ek_obj *ek_show_bool(ek_val v)
{
    return ek_str_from_cstr(v.as.i != 0 ? "True" : "False");
}

ek_obj *ek_show_char(ek_val v)
{
    char buf[8];
    size_t len = ek_utf8_encode(v.as.c, buf);

    return ek_str_new(buf, len);
}

/* The shortest scientific form with sig significant digits that reads
   back as x, written into out; %.*e always has one digit before the
   point, and seventeen significant digits always round-trip. */
static void shortest_scientific(double x, char *out, size_t cap)
{
    int sig;

    for (sig = 1; sig <= 17; sig++) {
        snprintf(out, cap, "%.*e", sig - 1, x);
        if (strtod(out, NULL) == x)
            return;
    }
}

/* The fixed-position rendering of digits * 10^exp with one digit
   before the point: the integer part takes exp+1 digits, padded with
   zeros, and the fractional part keeps the remaining digits, or a
   single 0 when the value is integral. */
static void write_fixed(char *out, const char *digits, size_t n, int exp)
{
    size_t pos = 0;
    size_t int_len = (size_t)exp + 1;
    size_t i;

    if (exp >= 0) {
        for (i = 0; i < int_len; i++)
            out[pos++] = i < n ? digits[i] : '0';
        out[pos++] = '.';
        for (; i < n; i++)
            out[pos++] = digits[i];
        if (n <= int_len)
            out[pos++] = '0';
    } else {
        out[pos++] = '0';
        out[pos++] = '.';
        for (i = 0; i < (size_t)(-exp - 1); i++)
            out[pos++] = '0';
        for (i = 0; i < n; i++)
            out[pos++] = digits[i];
    }
    out[pos] = '\0';
}

/* The e-notation rendering: one digit, then the fraction when the
   shortest form carries one, and the caller appends the exponent. */
static size_t write_scientific(char *out, const char *digits, size_t n)
{
    size_t pos = 0;
    size_t i;

    out[pos++] = digits[0];
    if (n > 1) {
        out[pos++] = '.';
        for (i = 1; i < n; i++)
            out[pos++] = digits[i];
    }
    return pos;
}

/* Renders x in the position the magnitude asks for: fixed notation
   from 1e-4 up to but not including 1e16, e-notation outside, so the
   integral values keep their fractional point (3.0, 0.5, 1e+300). */
static void render_float(char *out, double x)
{
    char sci[40];
    char digits[20] = {0};
    const char *exp_text;
    size_t n = 0;
    size_t i;
    int exp;

    shortest_scientific(x, sci, sizeof sci);
    exp_text = strchr(sci, 'e');
    exp = (int)strtol(exp_text + 1, NULL, 10);
    for (i = 0; sci[i] != 'e'; i++)
        if (sci[i] >= '0' && sci[i] <= '9')
            digits[n++] = sci[i];
    if (exp >= -4 && exp < 16)
        write_fixed(out, digits, n, exp);
    else
        strcpy(out + write_scientific(out, digits, n), exp_text);
}

ek_obj *ek_show_float(ek_val v)
{
    double x = v.as.f;
    char buf[48];
    int neg = signbit(x);

    if (isnan(x))
        return ek_str_from_cstr("NaN");
    if (isinf(x))
        return ek_str_from_cstr(neg ? "-Inf" : "Inf");
    render_float(buf, neg ? -x : x);
    if (neg) {
        char signed_buf[48];

        signed_buf[0] = '-';
        strcpy(signed_buf + 1, buf);
        return ek_str_from_cstr(signed_buf);
    }
    return ek_str_from_cstr(buf);
}

/* The radix channel over the integer slots. The digits carry the
   unsigned magnitude of the 64-bit slot word: an Int's negative values
   render in two's-complement form, and a UInt renders its own word.
   The form is lowercase and shortest -- no leading zeros, the plain
   zero printing as one zero digit. The radix is 2, 8, or 16, and a
   value of another tag is undefined. The caller owns the returned
   object. */

/* Writes the digits least significant first and returns the count. */
static size_t radix_digits(unsigned long long mag, int radix, char *out)
{
    static const char digits[] = "0123456789abcdef";
    size_t n = 0;

    do {
        out[n++] = digits[mag % (unsigned long long)radix];
        mag /= (unsigned long long)radix;
    } while (mag > 0);
    return n;
}

/* The magnitude of the slot word: a UInt is its own word, every other
   slot reads as the signed word whose two's complement is the value. */
static unsigned long long radix_magnitude(ek_val v)
{
    return v.tag == EK_VAL_UINT ? v.as.u : (unsigned long long)v.as.i;
}

ek_obj *ek_show_radix(ek_val v, int radix)
{
    char digits[65];
    char out[65];
    size_t n = radix_digits(radix_magnitude(v), radix, digits);
    size_t i;

    for (i = 0; i < n; i++)
        out[i] = digits[n - 1 - i];
    out[n] = '\0';
    return ek_str_from_cstr(out);
}

/* Fixed-width hexadecimal over the same slot words: the radix-16 form
   zero-padded on the left to at least width digits and never
   truncated. The width travels as a UInt value; a value of another tag
   is undefined. The caller owns the returned object. */
ek_obj *ek_show_hex_pad(ek_val v, ek_val width)
{
    char digits[65];
    size_t n = radix_digits(radix_magnitude(v), 16, digits);
    size_t pad = width.as.u > n ? (size_t)(width.as.u - n) : 0;
    char *buf = ek_rt_alloc_zeroed(n + pad);
    ek_obj *out;
    size_t i;

    for (i = 0; i < pad; i++)
        buf[i] = '0';
    for (i = 0; i < n; i++)
        buf[pad + i] = digits[n - 1 - i];
    out = ek_str_new(buf, n + pad);
    free(buf);
    return out;
}

/* The decimal text channel: fixed point and scientific notation over
   a requested digit count, the non-finite values rendered as
   themselves. The digit count travels as a UInt value; a value of
   another tag is undefined, and a count beyond the exact form's
   reach stops at the reach. */

/* The largest count the exact form carries: the smallest subnormal's
   expansion ends 1074 fractional digits past the point, and the
   largest finite's integer part fits in 309 digits. */
#define EK_FMT_MAX_DIGITS 1074

static ek_obj *show_fixed_or_exp(ek_val v, ek_val digits, int scientific)
{
    char buf[309 + EK_FMT_MAX_DIGITS + 4];
    int nd = digits.as.u > EK_FMT_MAX_DIGITS ? EK_FMT_MAX_DIGITS
                                             : (int)digits.as.u;
    int neg = signbit(v.as.f);

    if (isnan(v.as.f))
        return ek_str_from_cstr("NaN");
    if (isinf(v.as.f))
        return ek_str_from_cstr(neg ? "-Inf" : "Inf");
    if (scientific) {
        snprintf(buf, sizeof buf, "%.*e", nd > 0 ? nd - 1 : 0,
                 neg ? -v.as.f : v.as.f);
    } else {
        snprintf(buf, sizeof buf, "%.*f", nd, neg ? -v.as.f : v.as.f);
    }
    if (neg) {
        char signed_buf[sizeof buf];

        signed_buf[0] = '-';
        strcpy(signed_buf + 1, buf);
        return ek_str_from_cstr(signed_buf);
    }
    return ek_str_from_cstr(buf);
}

/* Fixed point with the requested fractional digits. */
ek_obj *ek_show_fixed(ek_val v, ek_val digits)
{
    return show_fixed_or_exp(v, digits, 0);
}

/* Scientific notation with the requested mantissa digits. */
ek_obj *ek_show_exp(ek_val v, ek_val digits)
{
    return show_fixed_or_exp(v, digits, 1);
}
