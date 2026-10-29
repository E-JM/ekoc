#include "harness.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

#include "ekort.h"

/* The display channel: Int, UInt, Bool, and Char print in their bare
   forms, Float prints the shortest round-trip form with the pinned
   shapes (3.0 keeps its fractional point, the non-finites print as
   NaN, Inf, and -Inf), and every finite float reads back through the
   parser as the same double. */

static void check_int(const char *want, int64_t n)
{
    ek_obj *s = ek_show_int(ek_val_int(n));

    EK_CHECK_STR(ek_str_cstr(s), want);
    ek_release(ek_val_ref(s));
}

static void check_uint(const char *want, uint64_t n)
{
    ek_obj *s = ek_show_uint(ek_val_uint(n));

    EK_CHECK_STR(ek_str_cstr(s), want);
    ek_release(ek_val_ref(s));
}

static void check_float(const char *want, double x)
{
    ek_obj *s = ek_show_float(ek_val_float(x));

    EK_CHECK_STR(ek_str_cstr(s), want);
    ek_release(ek_val_ref(s));
}

static void check_bool(const char *want, int b)
{
    ek_obj *s = ek_show_bool(ek_bool(b));

    EK_CHECK_STR(ek_str_cstr(s), want);
    ek_release(ek_val_ref(s));
}

static void check_char(const char *want, uint32_t code)
{
    ek_obj *s = ek_show_char(ek_val_char(code));

    EK_CHECK_STR(ek_str_cstr(s), want);
    ek_release(ek_val_ref(s));
}

static void int_prints_the_full_value(void)
{
    check_int("0", 0);
    check_int("42", 42);
    check_int("-42", -42);
    check_int("9223372036854775807", INT64_MAX);
    check_int("-9223372036854775808", INT64_MIN);
}

static void uint_prints_the_full_value(void)
{
    check_uint("0", 0);
    check_uint("42", 42);
    check_uint("18446744073709551615", UINT64_MAX);
}

static void bool_prints_the_constructors(void)
{
    check_bool("False", 0);
    check_bool("True", 1);
    check_bool("True", -2);
}

static void char_prints_the_bare_character(void)
{
    check_char("a", 'a');
    check_char("\xc3\xa9", 0xE9);
    check_char("\xf0\x9f\x98\x80", 0x1F600);
}

static void float_prints_the_pinned_shapes(void)
{
    check_float("3.0", 3.0);
    check_float("0.5", 0.5);
    check_float("3.5", 3.5);
    check_float("-2.0", -2.0);
    check_float("0.0", 0.0);
    check_float("-0.0", -0.0);
    check_float("NaN", NAN);
    check_float("Inf", INFINITY);
    check_float("-Inf", -INFINITY);
}

/* The positions the magnitude picks: fixed from 1e-4 to 1e16, the
   e-notation outside, always the shortest digit run that reads
   back. */
static void float_prints_shortest_forms(void)
{
    check_float("0.1", 0.1);
    check_float("100.0", 100.0);
    check_float("0.25", 0.25);
    check_float("7.125", 7.125);
    check_float("0.0001", 0.0001);
    check_float("1e-05", 1e-5);
    check_float("1000000000000000.0", 1e15);
    check_float("1e+16", 1e16);
    check_float("1e+300", 1e300);
    check_float("5e-324", 5e-324);
}

/* Show followed by parse returns the same double, for the boundary
   magnitudes and the values whose nearest neighbors are close. */
static void float_show_round_trips(double x)
{
    ek_obj *s = ek_show_float(ek_val_float(x));
    double back;

    EK_CHECK(ek_parse_float(s, &back) == EK_PARSE_OK);
    EK_CHECK(back == x);
    ek_release(ek_val_ref(s));
}

static void float_show_and_parse_are_inverses(void)
{
    float_show_round_trips(0.0);
    float_show_round_trips(-0.0);
    float_show_round_trips(1.0);
    float_show_round_trips(-7.125);
    float_show_round_trips(0.1);
    float_show_round_trips(1.0 / 3.0);
    float_show_round_trips(2.5e-10);
    float_show_round_trips(1e15);
    float_show_round_trips(1e16);
    float_show_round_trips(1e300);
    float_show_round_trips(DBL_MIN);
    float_show_round_trips(DBL_MAX);
    float_show_round_trips(5e-324);
    float_show_round_trips(-DBL_MIN);
}

/* The rendered form of an integral float parses back with its
   fractional point intact. */
static void integral_floats_keep_their_point(void)
{
    check_float("9.0", 9.0);
}

int main(void)
{
    int_prints_the_full_value();
    uint_prints_the_full_value();
    bool_prints_the_constructors();
    char_prints_the_bare_character();
    float_prints_the_pinned_shapes();
    float_prints_shortest_forms();
    float_show_and_parse_are_inverses();
    integral_floats_keep_their_point();
    return ek_test_report("rt/fmt");
}
