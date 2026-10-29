#include "harness.h"

#include <math.h>
#include <stdint.h>

#include "ekort.h"

/* The conversion family: the narrowing rows report the range through
   the status, Float truncates toward zero and checks NaN, the
   infinities, and -0.5 before -0.0, the machine-word rows check the
   named width, and the total rows widen without a miss. */

static int try_i8(double f, int64_t *out)
{
    return ek_try_to_i8(ek_val_float(f), out);
}

static void int_to_uint_checks_the_sign(void)
{
    uint64_t out = 7;

    EK_CHECK(ek_try_to_uint(ek_val_int(5), &out));
    EK_CHECK_U64(out, 5);
    EK_CHECK(ek_try_to_uint(ek_val_int(0), &out));
    EK_CHECK_U64(out, 0);
    EK_CHECK(!ek_try_to_uint(ek_val_int(-1), &out));
    EK_CHECK_U64(out, 0); /* a miss leaves the out-param untouched */
}

static void uint_to_int_checks_the_top(void)
{
    int64_t out = 7;

    EK_CHECK(ek_try_to_int(ek_val_uint(5), &out));
    EK_CHECK_U64(out, 5);
    EK_CHECK(ek_try_to_int(ek_val_uint((uint64_t)INT64_MAX), &out));
    EK_CHECK_U64(out, INT64_MAX);
    EK_CHECK(!ek_try_to_int(ek_val_uint((uint64_t)INT64_MAX + 1), &out));
}

static void float_to_int_truncates_and_checks(void)
{
    int64_t out = 0;

    EK_CHECK(ek_try_to_int(ek_val_float(3.99), &out));
    EK_CHECK_U64(out, 3);
    EK_CHECK(ek_try_to_int(ek_val_float(-3.99), &out));
    EK_CHECK_U64(out, -3);
    EK_CHECK(!ek_try_to_int(ek_val_float(NAN), &out));
    EK_CHECK(!ek_try_to_int(ek_val_float(INFINITY), &out));
    EK_CHECK(!ek_try_to_int(ek_val_float(-INFINITY), &out));
    /* 2^63 itself is one step past the maximum; the largest double
       below it still truncates in */
    EK_CHECK(!ek_try_to_int(ek_val_float(9223372036854775808.0), &out));
    EK_CHECK(ek_try_to_int(ek_val_float(9223372036854774784.0), &out));
    EK_CHECK_U64(out, 9223372036854774784LL);
    EK_CHECK(ek_try_to_int(ek_val_float(-9223372036854775808.0), &out));
    EK_CHECK_U64(out, (unsigned long long)INT64_MIN);
}

/* The sign of a fractional negative refuses the unsigned target
   before truncation, while negative zero truncates to zero. */
static void float_to_uint_checks_the_sign_first(void)
{
    uint64_t out = 9;

    EK_CHECK(!ek_try_to_uint(ek_val_float(-0.5), &out));
    EK_CHECK(ek_try_to_uint(ek_val_float(-0.0), &out));
    EK_CHECK_U64(out, 0);
    EK_CHECK(ek_try_to_uint(ek_val_float(0.5), &out));
    EK_CHECK_U64(out, 0);
    EK_CHECK(ek_try_to_uint(ek_val_float(1.99), &out));
    EK_CHECK_U64(out, 1);
    EK_CHECK(!ek_try_to_uint(ek_val_float(18446744073709551616.0), &out));
    EK_CHECK(ek_try_to_uint(ek_val_float(18446744073709549568.0), &out));
    EK_CHECK_U64(out, 18446744073709549568ULL);
}

static void machine_width_rows_check_their_range(void)
{
    int64_t out = 0;
    uint64_t uout = 0;

    EK_CHECK(ek_try_to_i8(ek_val_int(127), &out));
    EK_CHECK_U64(out, 127);
    EK_CHECK(ek_try_to_i8(ek_val_int(-128), &out));
    EK_CHECK_U64(out, -128);
    EK_CHECK(!ek_try_to_i8(ek_val_int(128), &out));
    EK_CHECK(!ek_try_to_i8(ek_val_int(-129), &out));
    EK_CHECK(ek_try_to_i8(ek_val_uint(127), &out));
    EK_CHECK(!ek_try_to_i8(ek_val_uint(128), &out));
    /* the fraction between -129 and -128 still truncates in */
    EK_CHECK(try_i8(-128.5, &out));
    EK_CHECK_U64(out, -128);
    EK_CHECK(try_i8(127.9, &out));
    EK_CHECK_U64(out, 127);
    EK_CHECK(!try_i8(-129.0, &out));
    EK_CHECK(!try_i8(128.0, &out));

    EK_CHECK(ek_try_to_u8(ek_val_int(255), &uout));
    EK_CHECK_U64(uout, 255);
    EK_CHECK(!ek_try_to_u8(ek_val_int(-1), &uout));
    EK_CHECK(!ek_try_to_u8(ek_val_int(256), &uout));
    EK_CHECK(ek_try_to_u8(ek_val_float(255.9), &uout));
    EK_CHECK_U64(uout, 255);
    EK_CHECK(!ek_try_to_u8(ek_val_float(-0.5), &uout));

    EK_CHECK(ek_try_to_i16(ek_val_int(32767), &out));
    EK_CHECK(!ek_try_to_i16(ek_val_int(32768), &out));
    EK_CHECK(ek_try_to_u16(ek_val_int(65535), &uout));
    EK_CHECK(!ek_try_to_u16(ek_val_int(65536), &uout));
    EK_CHECK(ek_try_to_i32(ek_val_int(2147483647), &out));
    EK_CHECK(!ek_try_to_i32(ek_val_int(2147483648LL), &out));
    EK_CHECK(ek_try_to_u32(ek_val_int(4294967295LL), &uout));
    EK_CHECK(!ek_try_to_u32(ek_val_int(4294967296LL), &uout));

    EK_CHECK(ek_try_to_i64(ek_val_int(INT64_MIN), &out));
    EK_CHECK_U64(out, (unsigned long long)INT64_MIN);
    EK_CHECK(ek_try_to_i64(ek_val_uint((uint64_t)INT64_MAX), &out));
    EK_CHECK(!ek_try_to_i64(ek_val_uint((uint64_t)INT64_MAX + 1), &out));
    EK_CHECK(ek_try_to_u64(ek_val_uint(UINT64_MAX), &uout));
    EK_CHECK_U64(uout, UINT64_MAX);
    EK_CHECK(!ek_try_to_u64(ek_val_int(-1), &uout));
    EK_CHECK(ek_try_to_u64(ek_val_float(1.5e19), &uout));
}

static void float_sources_reject_the_nonfinite_for_words(void)
{
    int64_t out = 0;
    uint64_t uout = 0;

    EK_CHECK(!ek_try_to_i8(ek_val_float(NAN), &out));
    EK_CHECK(!ek_try_to_i8(ek_val_float(INFINITY), &out));
    EK_CHECK(!ek_try_to_u8(ek_val_float(NAN), &uout));
    EK_CHECK(!ek_try_to_u8(ek_val_float(-INFINITY), &uout));
}

static void total_rows_widen(void)
{
    ek_val f;

    f = ek_to_float(ek_val_int(-5));
    EK_CHECK(ek_val_is_float(f) && f.as.f == -5.0);
    f = ek_to_float(ek_val_uint(UINT64_MAX));
    EK_CHECK(ek_val_is_float(f) && f.as.f == 18446744073709551616.0);
    f = ek_to_float(ek_val_float(2.5));
    EK_CHECK(ek_val_is_float(f) && f.as.f == 2.5);

    f = ek_to_f32(ek_val_float(0.1));
    EK_CHECK(ek_val_is_float(f) && f.as.f == (double)(float)0.1);
    EK_CHECK(f.as.f != 0.1); /* the rounding through binary32 shows */
    f = ek_to_f32(ek_val_int(1));
    EK_CHECK(f.as.f == 1.0);
    f = ek_to_f64(ek_val_float(0.1));
    EK_CHECK(f.as.f == 0.1);
    f = ek_to_f64(ek_val_uint(UINT64_MAX));
    EK_CHECK(f.as.f == 18446744073709551616.0);

    EK_CHECK(ek_val_is_unit(ek_to_float(ek_val_unit())));
    EK_CHECK(ek_val_is_unit(ek_to_f32(ek_val_char('a'))));
}

int main(void)
{
    int_to_uint_checks_the_sign();
    uint_to_int_checks_the_top();
    float_to_int_truncates_and_checks();
    float_to_uint_checks_the_sign_first();
    machine_width_rows_check_their_range();
    float_sources_reject_the_nonfinite_for_words();
    total_rows_widen();
    return ek_test_report("rt/convert");
}
