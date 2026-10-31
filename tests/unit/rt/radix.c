#include "harness.h"

#include <stdint.h>

#include "ekort.h"

/* The radix channel: the digits carry the unsigned magnitude of the
   64-bit slot word, so a negative Int renders in two's-complement form
   and a UInt renders its own word. The form is lowercase and shortest,
   and the fixed-width hex pads with zeros and never truncates. */

static void check_radix(const char *want, ek_val v, int radix)
{
    ek_obj *s = ek_show_radix(v, radix);

    EK_CHECK_STR(ek_str_cstr(s), want);
    ek_release(ek_val_ref(s));
}

static void check_pad(const char *want, ek_val v, uint64_t width)
{
    ek_obj *s = ek_show_hex_pad(v, ek_val_uint(width));

    EK_CHECK_STR(ek_str_cstr(s), want);
    ek_release(ek_val_ref(s));
}

static void hex_prints_the_shortest_form(void)
{
    check_radix("0", ek_val_int(0), 16);
    check_radix("f", ek_val_int(15), 16);
    check_radix("ff", ek_val_int(255), 16);
    check_radix("7fffffff", ek_val_int(INT32_MAX), 16);
    check_radix("7fffffffffffffff", ek_val_int(INT64_MAX), 16);
    check_radix("deadbeef", ek_val_int(0xDEADBEEF), 16);
    check_radix("deadbeef", ek_val_int(0xdeadbeef), 16);
}

/* The two's-complement rule: a negative Int is the full 64-bit word. */
static void hex_prints_negatives_in_twos_complement(void)
{
    check_radix("ffffffffffffffff", ek_val_int(-1), 16);
    check_radix("fffffffffffffffe", ek_val_int(-2), 16);
    check_radix("8000000000000000", ek_val_int(INT64_MIN), 16);
}

static void oct_prints_the_shortest_form(void)
{
    check_radix("0", ek_val_int(0), 8);
    check_radix("7", ek_val_int(7), 8);
    check_radix("10", ek_val_int(8), 8);
    check_radix("777", ek_val_int(511), 8);
    check_radix("1777777777777777777777", ek_val_int(-1), 8);
}

static void bin_prints_the_shortest_form(void)
{
    check_radix("0", ek_val_int(0), 2);
    check_radix("1", ek_val_int(1), 2);
    check_radix("101", ek_val_int(5), 2);
    check_radix("1000000000000000000000000000000000000000000000000000000000000000",
                ek_val_int(INT64_MIN), 2);
}

static void uint_prints_its_own_word(void)
{
    check_radix("ff", ek_val_uint(255), 16);
    check_radix("ffffffffffffffff", ek_val_uint(UINT64_MAX), 16);
    check_radix("8000000000000000", ek_val_uint(0x8000000000000000ULL), 16);
    check_radix("1777777777777777777777", ek_val_uint(UINT64_MAX), 8);
    check_radix(
        "1111111111111111111111111111111111111111111111111111111111111111",
        ek_val_uint(UINT64_MAX), 2);
}

/* The pad adds zeros on the left and never truncates: a width below
   the digit count changes nothing, and a negative Int keeps its full
   64-digit form. */
static void hex_pad_pads_and_never_truncates(void)
{
    check_pad("ff", ek_val_int(255), 0);
    check_pad("ff", ek_val_int(255), 1);
    check_pad("ff", ek_val_int(255), 2);
    check_pad("0ff", ek_val_int(255), 3);
    check_pad("00ff", ek_val_int(255), 4);
    check_pad("000000ff", ek_val_int(255), 8);
    check_pad("001234", ek_val_uint(0x1234), 6);
    check_pad("ffffffffffffffff", ek_val_int(-1), 4);
    check_pad("0000000000000001", ek_val_int(1), 16);
    check_pad("0", ek_val_int(0), 1);
    check_pad("00", ek_val_int(0), 2);
}

int main(void)
{
    hex_prints_the_shortest_form();
    hex_prints_negatives_in_twos_complement();
    oct_prints_the_shortest_form();
    bin_prints_the_shortest_form();
    uint_prints_its_own_word();
    hex_pad_pads_and_never_truncates();
    return ek_test_report("rt/radix");
}
