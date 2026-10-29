#include "harness.h"

#include <math.h>
#include <stdint.h>

#include "ekort.h"

/* The parse family: the grammar is an optional sign over digits of
   the radix and nothing else, the four kinds separate a missing value
   from a malformed one and an out-of-limits one, the UInt entries
   reject a minus sign as out of range, and Float reads the non-finite
   spellings plus a leading digit with optional fraction and
   exponent. */

/* Each wrapper releases the string it builds, so a suite run leaves
   nothing behind. */

static ek_parse_status p_int(const char *s, int64_t *out)
{
    ek_obj *o = ek_str_from_cstr(s);
    ek_parse_status st = ek_parse_int(o, out);

    ek_release(ek_val_ref(o));
    return st;
}

static ek_parse_status p_uint(const char *s, uint64_t *out)
{
    ek_obj *o = ek_str_from_cstr(s);
    ek_parse_status st = ek_parse_uint(o, out);

    ek_release(ek_val_ref(o));
    return st;
}

static ek_parse_status p_int_radix(const char *s, int radix, int64_t *out)
{
    ek_obj *o = ek_str_from_cstr(s);
    ek_parse_status st = ek_parse_int_radix(o, radix, out);

    ek_release(ek_val_ref(o));
    return st;
}

static ek_parse_status p_uint_radix(const char *s, int radix, uint64_t *out)
{
    ek_obj *o = ek_str_from_cstr(s);
    ek_parse_status st = ek_parse_uint_radix(o, radix, out);

    ek_release(ek_val_ref(o));
    return st;
}

static ek_parse_status p_float(const char *s, double *out)
{
    ek_obj *o = ek_str_from_cstr(s);
    ek_parse_status st = ek_parse_float(o, out);

    ek_release(ek_val_ref(o));
    return st;
}

static ek_parse_status p_float_raw(const char *bytes, size_t len, double *out)
{
    ek_obj *o = ek_str_new(bytes, len);
    ek_parse_status st = ek_parse_float(o, out);

    ek_release(ek_val_ref(o));
    return st;
}

static void int_accepts_signed_decimal_digits(void)
{
    int64_t out = 0;

    EK_CHECK(p_int("0", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 0);
    EK_CHECK(p_int("42", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 42);
    EK_CHECK(p_int("+7", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 7);
    EK_CHECK(p_int("-7", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, -7);
    EK_CHECK(p_int("007", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 7);
    EK_CHECK(p_int("-0", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 0);
    EK_CHECK(p_int("9223372036854775807", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, INT64_MAX);
    EK_CHECK(p_int("-9223372036854775808", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, (unsigned long long)INT64_MIN);
}

static void int_separates_the_failure_kinds(void)
{
    int64_t out = 99;

    EK_CHECK(p_int("", &out) == EK_PARSE_EMPTY);
    EK_CHECK(p_int("-", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int("+", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int("12x", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int(" 1", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int("1 2", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int("1_000", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int("0x10", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int("9223372036854775808", &out) == EK_PARSE_RANGE);
    EK_CHECK(p_int("-9223372036854775809", &out) == EK_PARSE_RANGE);
    EK_CHECK_U64(out, 99); /* a rejected input writes nothing */
}

static void uint_accepts_signed_nonnegative_digits(void)
{
    uint64_t out = 0;

    EK_CHECK(p_uint("0", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 0);
    EK_CHECK(p_uint("+5", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 5);
    EK_CHECK(p_uint("18446744073709551615", &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, UINT64_MAX);
    EK_CHECK(p_uint("-1", &out) == EK_PARSE_RANGE);
    EK_CHECK(p_uint("-0", &out) == EK_PARSE_RANGE);
    EK_CHECK(p_uint("18446744073709551616", &out) == EK_PARSE_RANGE);
    EK_CHECK(p_uint("", &out) == EK_PARSE_EMPTY);
    EK_CHECK(p_uint("3.5", &out) == EK_PARSE_INVALID);
}

static void radix_parsers_read_no_prefix(void)
{
    int64_t out = 0;
    uint64_t uout = 0;

    EK_CHECK(p_int_radix("ff", 16, &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 255);
    EK_CHECK(p_int_radix("FF", 16, &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 255);
    EK_CHECK(p_int_radix("-80", 16, &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, -128);
    EK_CHECK(p_int_radix("777", 8, &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 511);
    EK_CHECK(p_int_radix("101", 2, &out) == EK_PARSE_OK);
    EK_CHECK_U64(out, 5);
    EK_CHECK(p_int_radix("0x10", 16, &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int_radix("18", 8, &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int_radix("102", 2, &out) == EK_PARSE_INVALID);
    EK_CHECK(p_int_radix("9223372036854775808", 16, &out)
             == EK_PARSE_RANGE);
    EK_CHECK(p_uint_radix("ff", 16, &uout) == EK_PARSE_OK);
    EK_CHECK_U64(uout, 255);
    EK_CHECK(p_uint_radix("-1", 16, &uout) == EK_PARSE_RANGE);
}

static void float_accepts_the_number_grammar(void)
{
    double out = 0;

    EK_CHECK(p_float("0.5", &out) == EK_PARSE_OK);
    EK_CHECK(out == 0.5);
    EK_CHECK(p_float("3.0", &out) == EK_PARSE_OK);
    EK_CHECK(out == 3.0);
    EK_CHECK(p_float("5", &out) == EK_PARSE_OK);
    EK_CHECK(out == 5.0);
    EK_CHECK(p_float("-0.25", &out) == EK_PARSE_OK);
    EK_CHECK(out == -0.25);
    EK_CHECK(p_float("+2.5e3", &out) == EK_PARSE_OK);
    EK_CHECK(out == 2500.0);
    EK_CHECK(p_float("1E-3", &out) == EK_PARSE_OK);
    EK_CHECK(out == 0.001);
    EK_CHECK(p_float("1e+3", &out) == EK_PARSE_OK);
    EK_CHECK(out == 1000.0);
}

/* A leading digit is required, and both the fraction and the exponent
   carry at least one digit of their own. */
static void float_rejects_malformed_text(void)
{
    double out = 0;

    EK_CHECK(p_float("", &out) == EK_PARSE_EMPTY);
    EK_CHECK(p_float(".5", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float("5.", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float("5e", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float("5e+", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float("e5", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float("1x", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float("1.2.3", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float(" 1", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float("+NaN", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float("inf", &out) == EK_PARSE_INVALID);
    EK_CHECK(p_float("nan", &out) == EK_PARSE_INVALID);
}

static void float_reads_the_nonfinite_spellings(void)
{
    double out = 0;

    EK_CHECK(p_float("NaN", &out) == EK_PARSE_OK);
    EK_CHECK(isnan(out));
    EK_CHECK(p_float("Inf", &out) == EK_PARSE_OK);
    EK_CHECK(out == INFINITY);
    EK_CHECK(p_float("+Inf", &out) == EK_PARSE_OK);
    EK_CHECK(out == INFINITY);
    EK_CHECK(p_float("-Inf", &out) == EK_PARSE_OK);
    EK_CHECK(out == -INFINITY);
    /* the infinities are values of the type, so the exponent may run
       past the finite range and read as one */
    EK_CHECK(p_float("1e999", &out) == EK_PARSE_OK);
    EK_CHECK(out == INFINITY);
    EK_CHECK(p_float("1e-999", &out) == EK_PARSE_OK);
    EK_CHECK(out == 0.0);
}

static void parsers_accept_only_the_whole_text(void)
{
    double f = 0;
    int64_t i = 0;

    /* an embedded NUL is an ordinary byte, not an end of text */
    EK_CHECK(p_float_raw("1\0", 2, &f) == EK_PARSE_INVALID);
    EK_CHECK(p_int("1\n", &i) == EK_PARSE_INVALID);
}

int main(void)
{
    int_accepts_signed_decimal_digits();
    int_separates_the_failure_kinds();
    uint_accepts_signed_nonnegative_digits();
    radix_parsers_read_no_prefix();
    float_accepts_the_number_grammar();
    float_rejects_malformed_text();
    float_reads_the_nonfinite_spellings();
    parsers_accept_only_the_whole_text();
    return ek_test_report("rt/parse");
}
