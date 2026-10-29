#include "harness.h"

#include <math.h>
#include <stdint.h>

#include "ekort.h"

/* Arithmetic and comparison over the domain words: round-trips and
   truncation for the three widths, the boundary shapes that stay
   defined, the IEEE behavior of the Float path, and the unsigned
   ordering above the signed range. The terminating cases live in the
   fork suite. */

static ek_val wi(int64_t n)
{
    return ek_val_int(n);
}

static ek_val wu(uint64_t n)
{
    return ek_val_uint(n);
}

static ek_val wf(double x)
{
    return ek_val_float(x);
}

/* A comparison result must be a Bool: an Int of exactly 0 or 1. */
static void check_true(ek_val v)
{
    EK_CHECK(ek_val_is_int(v));
    EK_CHECK_U64(v.as.i, 1);
}

static void check_false(ek_val v)
{
    EK_CHECK(ek_val_is_int(v));
    EK_CHECK_U64(v.as.i, 0);
}

static void bool_constructor_normalizes(void)
{
    EK_CHECK(ek_val_is_int(ek_bool(0)));
    EK_CHECK(ek_val_is_int(ek_bool(7)));
    EK_CHECK_U64(ek_bool(0).as.i, 0);
    EK_CHECK_U64(ek_bool(7).as.i, 1);
    EK_CHECK_U64(ek_bool(-1).as.i, 1);
}

static void int_results_carry_the_int_tag(void)
{
    ek_val r = ek_num_add(wi(2), wi(3));

    EK_CHECK(ek_val_is_int(r));
    EK_CHECK(ek_val_is_int(ek_num_mul(wi(-4), wi(6))));
    EK_CHECK(ek_val_is_int(ek_num_div(wi(9), wi(4))));
    EK_CHECK(ek_val_is_int(ek_num_neg(wi(9))));
}

static void int_arithmetic_round_trips(void)
{
    EK_CHECK_U64(ek_num_add(wi(7), wi(2)).as.i, 9);
    EK_CHECK_U64(ek_num_sub(wi(7), wi(2)).as.i, 5);
    EK_CHECK_U64(ek_num_mul(wi(7), wi(2)).as.i, 14);
    EK_CHECK_U64(ek_num_div(wi(7), wi(2)).as.i, 3);
    EK_CHECK_U64(ek_num_mod(wi(7), wi(2)).as.i, 1);
}

/* The division pair truncates toward zero exactly as C does: the
   remainder takes the sign of the dividend. */
static void int_division_truncates_toward_zero(void)
{
    EK_CHECK_U64(ek_num_div(wi(-7), wi(2)).as.i, -3);
    EK_CHECK_U64(ek_num_mod(wi(-7), wi(2)).as.i, -1);
    EK_CHECK_U64(ek_num_div(wi(7), wi(-2)).as.i, -3);
    EK_CHECK_U64(ek_num_mod(wi(7), wi(-2)).as.i, 1);
    EK_CHECK_U64(ek_num_div(wi(-7), wi(-2)).as.i, 3);
    EK_CHECK_U64(ek_num_mod(wi(-7), wi(-2)).as.i, -1);
}

/* Boundaries one step inside the checked paths: the results below are
   representable, so no entry may panic here. */
static void int_boundaries_stay_defined(void)
{
    EK_CHECK_U64(ek_num_add(wi(INT64_MAX - 1), wi(1)).as.i, INT64_MAX);
    EK_CHECK_U64(ek_num_add(wi(INT64_MIN), wi(0)).as.i, INT64_MIN);
    EK_CHECK_U64(ek_num_sub(wi(INT64_MIN + 1), wi(1)).as.i, INT64_MIN);
    EK_CHECK_U64(ek_num_mul(wi((int64_t)1 << 31), wi((int64_t)1 << 31)).as.i,
                 (int64_t)1 << 62);
    EK_CHECK_U64(ek_num_div(wi(INT64_MIN), wi(2)).as.i, -((int64_t)1 << 62));
    EK_CHECK_U64(ek_num_mod(wi(INT64_MIN), wi(2)).as.i, 0);
    EK_CHECK_U64(ek_num_neg(wi(INT64_MAX)).as.i, -INT64_MAX);
    EK_CHECK_U64(ek_num_sub(wi(INT64_MAX), wi(INT64_MAX)).as.i, 0);
}

static void uint_results_carry_the_uint_tag(void)
{
    ek_val r = ek_num_add(wu(2), wu(3));

    EK_CHECK(ek_val_is_uint(r));
    EK_CHECK(ek_val_is_uint(ek_num_mul(wu(4), wu(6))));
    EK_CHECK(ek_val_is_uint(ek_num_mod(wu(9), wu(4))));
}

static void uint_arithmetic_round_trips(void)
{
    EK_CHECK_U64(ek_num_add(wu(7), wu(2)).as.u, 9);
    EK_CHECK_U64(ek_num_sub(wu(7), wu(2)).as.u, 5);
    EK_CHECK_U64(ek_num_mul(wu(7), wu(2)).as.u, 14);
    EK_CHECK_U64(ek_num_div(wu(7), wu(2)).as.u, 3);
    EK_CHECK_U64(ek_num_mod(wu(7), wu(2)).as.u, 1);
}

static void uint_boundaries_stay_defined(void)
{
    EK_CHECK_U64(ek_num_add(wu(UINT64_MAX - 1), wu(1)).as.u, UINT64_MAX);
    EK_CHECK_U64(ek_num_div(wu(UINT64_MAX), wu(2)).as.u, UINT64_MAX / 2);
    EK_CHECK_U64(ek_num_mod(wu(UINT64_MAX), wu(2)).as.u, 1);
    EK_CHECK_U64(ek_num_div(wu(UINT64_MAX), wu(UINT64_MAX)).as.u, 1);
    EK_CHECK_U64(ek_num_sub(wu(UINT64_MAX), wu(UINT64_MAX)).as.u, 0);
    EK_CHECK_U64(ek_num_mul(wu(UINT64_MAX), wu(1)).as.u, UINT64_MAX);
}

static void float_results_carry_the_float_tag(void)
{
    ek_val r = ek_num_add(wf(1.5), wf(2.0));

    EK_CHECK(ek_val_is_float(r));
    EK_CHECK(ek_val_is_float(ek_num_mul(wf(1.5), wf(2.0))));
    EK_CHECK(ek_val_is_float(ek_num_neg(wf(1.5))));
}

static void float_arithmetic_round_trips(void)
{
    EK_CHECK(ek_num_add(wf(1.5), wf(2.25)).as.f == 3.75);
    EK_CHECK(ek_num_sub(wf(0.5), wf(1.25)).as.f == -0.75);
    EK_CHECK(ek_num_mul(wf(1.5), wf(2.0)).as.f == 3.0);
    EK_CHECK(ek_num_div(wf(7.0), wf(2.0)).as.f == 3.5);
}

/* % is C fmod: the result takes the sign of the dividend. */
static void float_mod_takes_the_dividend_sign(void)
{
    EK_CHECK(ek_num_mod(wf(7.5), wf(2.0)).as.f == 1.5);
    EK_CHECK(ek_num_mod(wf(-7.5), wf(2.0)).as.f == -1.5);
    EK_CHECK(ek_num_mod(wf(7.5), wf(-2.0)).as.f == 1.5);
    EK_CHECK(ek_num_mod(wf(-7.5), wf(-2.0)).as.f == -1.5);
}

/* The Float path never checks: the operations overflow to the IEEE
   infinities and division by zero follows the IEEE rules. */
static void float_overflows_to_infinity(void)
{
    EK_CHECK(ek_num_add(wf(INFINITY), wf(1.0)).as.f == INFINITY);
    EK_CHECK(ek_num_mul(wf(INFINITY), wf(2.0)).as.f == INFINITY);
    EK_CHECK(ek_num_div(wf(1.0), wf(0.0)).as.f == INFINITY);
    EK_CHECK(ek_num_div(wf(-1.0), wf(0.0)).as.f == -INFINITY);
    EK_CHECK(ek_num_add(wf(-INFINITY), wf(1.0)).as.f == -INFINITY);
    EK_CHECK(ek_num_sub(wf(1.0), wf(INFINITY)).as.f == -INFINITY);
}

static void float_negation(void)
{
    EK_CHECK(ek_num_neg(wf(2.5)).as.f == -2.5);
    /* negation flips the zero's sign, which division can observe */
    EK_CHECK(ek_num_neg(wf(0.0)).as.f == 0.0);
    EK_CHECK(1.0 / ek_num_neg(wf(0.0)).as.f == -INFINITY);
    EK_CHECK(1.0 / ek_num_neg(wf(-0.0)).as.f == INFINITY);
}

static void int_comparisons_are_signed(void)
{
    check_true(ek_num_lt(wi(-3), wi(-2)));
    check_true(ek_num_le(wi(-2), wi(-2)));
    check_true(ek_num_gt(wi(-1), wi(-2)));
    check_true(ek_num_ge(wi(-2), wi(-2)));
    check_true(ek_num_eq(wi(42), wi(42)));
    check_true(ek_num_ne(wi(-42), wi(42)));
    check_true(ek_num_lt(wi(INT64_MIN), wi(0)));
    check_true(ek_num_gt(wi(INT64_MAX), wi(0)));
    check_false(ek_num_lt(wi(-2), wi(-3)));
    check_false(ek_num_eq(wi(42), wi(43)));
}

/* UInt values above the signed range order by their unsigned
   magnitude, not by the reinterpreted sign bit. */
static void uint_comparisons_are_unsigned(void)
{
    ek_val big = wu((uint64_t)1 << 63);

    check_true(ek_num_gt(big, wu(INT64_MAX)));
    check_true(ek_num_ge(big, wu((uint64_t)1 << 63)));
    check_true(ek_num_gt(wu(UINT64_MAX), wu(1)));
    check_true(ek_num_lt(wu(1), wu(UINT64_MAX)));
    check_true(ek_num_eq(wu(UINT64_MAX), wu(UINT64_MAX)));
    check_true(ek_num_ne(wu(UINT64_MAX - 1), wu(UINT64_MAX)));
    check_false(ek_num_lt(wu(UINT64_MAX), wu(1)));
    check_false(ek_num_gt(wu(1), wu(UINT64_MAX)));
}

static void float_comparisons_follow_ieee(void)
{
    check_true(ek_num_lt(wf(2.5), wf(3.5)));
    check_true(ek_num_le(wf(2.5), wf(2.5)));
    check_true(ek_num_gt(wf(3.5), wf(2.5)));
    check_true(ek_num_ge(wf(3.5), wf(3.5)));
    check_true(ek_num_eq(wf(2.5), wf(2.5)));
    check_true(ek_num_ne(wf(2.5), wf(3.5)));
    check_false(ek_num_eq(wf(2.5), wf(3.5)));
    check_false(ek_num_gt(wf(2.5), wf(3.5)));
}

static void nan_is_unordered(void)
{
    ek_val nan = wf(NAN);

    check_false(ek_num_eq(nan, nan));
    check_true(ek_num_ne(nan, nan));
    check_false(ek_num_lt(nan, nan));
    check_false(ek_num_le(nan, nan));
    check_false(ek_num_gt(nan, nan));
    check_false(ek_num_ge(nan, nan));
    check_false(ek_num_lt(nan, wf(1.0)));
    check_false(ek_num_gt(nan, wf(1.0)));
}

static void negative_zero_equals_zero(void)
{
    check_true(ek_num_eq(wf(-0.0), wf(0.0)));
    check_false(ek_num_lt(wf(-0.0), wf(0.0)));
    check_false(ek_num_gt(wf(-0.0), wf(0.0)));
}

int main(void)
{
    bool_constructor_normalizes();
    int_results_carry_the_int_tag();
    int_arithmetic_round_trips();
    int_division_truncates_toward_zero();
    int_boundaries_stay_defined();
    uint_results_carry_the_uint_tag();
    uint_arithmetic_round_trips();
    uint_boundaries_stay_defined();
    float_results_carry_the_float_tag();
    float_arithmetic_round_trips();
    float_mod_takes_the_dividend_sign();
    float_overflows_to_infinity();
    float_negation();
    int_comparisons_are_signed();
    uint_comparisons_are_unsigned();
    float_comparisons_follow_ieee();
    nan_is_unordered();
    negative_zero_equals_zero();
    return ek_test_report("rt/num");
}
