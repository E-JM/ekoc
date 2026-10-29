#include "harness.h"

#include <stdint.h>

#include "ekort.h"

/* The code-point bridge and the Char ordering: the code is the scalar
   value as an Int, the constructor is a domain check that refuses the
   surrogates and everything above the encoding maximum, and the
   comparisons order by code point. */

static void to_code_is_the_scalar_value(void)
{
    ek_val c = ek_char_to_code(ek_val_char('a'));

    EK_CHECK(ek_val_is_int(c));
    EK_CHECK_U64(c.as.i, 97);
    EK_CHECK_U64(ek_char_to_code(ek_val_char(0x1F600)).as.i, 128512);
    EK_CHECK_U64(ek_char_to_code(ek_val_char(0)).as.i, 0);
}

static void from_code_checks_the_scalar_domain(void)
{
    uint32_t out = 7;

    EK_CHECK(ek_try_char_from_code(ek_val_int(0), &out));
    EK_CHECK_U64(out, 0);
    EK_CHECK(ek_try_char_from_code(ek_val_int('a'), &out));
    EK_CHECK_U64(out, 'a');
    EK_CHECK(ek_try_char_from_code(ek_val_int(0xD7FF), &out));
    EK_CHECK_U64(out, 0xD7FF); /* just below the surrogates */
    EK_CHECK(ek_try_char_from_code(ek_val_int(0xE000), &out));
    EK_CHECK_U64(out, 0xE000); /* just above them */
    EK_CHECK(ek_try_char_from_code(ek_val_int(0x10FFFF), &out));
    EK_CHECK_U64(out, 0x10FFFF);
    EK_CHECK(!ek_try_char_from_code(ek_val_int(-1), &out));
    EK_CHECK(!ek_try_char_from_code(ek_val_int(0xD800), &out));
    EK_CHECK(!ek_try_char_from_code(ek_val_int(0xDFFF), &out));
    EK_CHECK(!ek_try_char_from_code(ek_val_int(0x110000), &out));
    EK_CHECK_U64(out, 0x10FFFF); /* a miss leaves the out-param alone */
}

/* The bridge round-trips: the code back to the Char and out again. */
static void code_and_char_round_trip(void)
{
    uint32_t out = 0;

    EK_CHECK(ek_try_char_from_code(ek_char_to_code(ek_val_char(0x10FFFF)),
                                   &out));
    EK_CHECK_U64(out, 0x10FFFF);
}

/* The comparisons run in the numeric dispatch and order by code
   point, returning Bool. */
static void comparisons_order_by_code_point(void)
{
    ek_val a = ek_val_char('a');
    ek_val b = ek_val_char('b');
    ek_val emoji = ek_val_char(0x1F600);

    EK_CHECK(ek_num_eq(a, a).as.i == 1);
    EK_CHECK(ek_num_eq(a, b).as.i == 0);
    EK_CHECK(ek_num_ne(a, b).as.i == 1);
    EK_CHECK(ek_num_lt(a, b).as.i == 1);
    EK_CHECK(ek_num_le(a, b).as.i == 1);
    EK_CHECK(ek_num_le(a, a).as.i == 1);
    EK_CHECK(ek_num_gt(b, a).as.i == 1);
    EK_CHECK(ek_num_ge(a, a).as.i == 1);
    EK_CHECK(ek_num_lt(a, emoji).as.i == 1);
    EK_CHECK(ek_num_lt(emoji, a).as.i == 0);
}

int main(void)
{
    to_code_is_the_scalar_value();
    from_code_checks_the_scalar_domain();
    code_and_char_round_trip();
    comparisons_order_by_code_point();
    return ek_test_report("rt/charops");
}
