#include "harness.h"

#include <stdint.h>

#include "ekort.h"

/* The value core: scalars travel in the 16-byte tagged union and the
   composite families behind a reference. These checks pin the tag
   round-trips, the constructor and predicate surface, and the exact
   layout the generated code compiles against. */

static const ek_typeinfo probe_info = { EK_KIND_STRUCT, NULL, NULL, NULL };
static ek_obj probe_obj = { &probe_info, 1 };

static void unit_is_tag_only(void)
{
    ek_val v = ek_val_unit();

    EK_CHECK_U64(v.tag, EK_VAL_UNIT);
    EK_CHECK(ek_val_is_unit(v));
    EK_CHECK(!ek_val_is_int(v));
    EK_CHECK(!ek_val_is_ref(v));
}

static void int_round_trip(void)
{
    ek_val v = ek_val_int(-5);

    EK_CHECK_U64(v.tag, EK_VAL_INT);
    EK_CHECK(ek_val_is_int(v));
    EK_CHECK(v.as.i == -5);
    EK_CHECK(ek_val_int(INT64_MIN).as.i == INT64_MIN);
    EK_CHECK(ek_val_int(INT64_MAX).as.i == INT64_MAX);
}

static void uint_round_trip(void)
{
    ek_val v = ek_val_uint(UINT64_MAX);

    EK_CHECK_U64(v.tag, EK_VAL_UINT);
    EK_CHECK(ek_val_is_uint(v));
    EK_CHECK(v.as.u == UINT64_MAX);
    EK_CHECK(ek_val_uint(0).as.u == 0);
}

static void float_round_trip(void)
{
    ek_val v = ek_val_float(-1.25);

    EK_CHECK_U64(v.tag, EK_VAL_FLOAT);
    EK_CHECK(ek_val_is_float(v));
    EK_CHECK(v.as.f == -1.25);
    EK_CHECK(ek_val_float(0.0).as.f == 0.0);
}

static void char_round_trip(void)
{
    ek_val v = ek_val_char('a');

    EK_CHECK_U64(v.tag, EK_VAL_CHAR);
    EK_CHECK(ek_val_is_char(v));
    EK_CHECK_U64(v.as.c, 'a');
    EK_CHECK_U64(ek_val_char(0x1F600u).as.c, 0x1F600u);
}

static void ref_round_trip(void)
{
    ek_val v = ek_val_ref(&probe_obj);

    EK_CHECK_U64(v.tag, EK_VAL_REF);
    EK_CHECK(ek_val_is_ref(v));
    EK_CHECK(v.as.ref == &probe_obj);
}

/* Bool has no tag of its own: it travels as an Int of 0 or 1. */
static void bool_travels_as_int(void)
{
    EK_CHECK(ek_val_is_int(ek_val_int(0)));
    EK_CHECK(ek_val_is_int(ek_val_int(1)));
    EK_CHECK(ek_val_int(1).as.i == 1);
}

static void predicates_are_exclusive(void)
{
    ek_val values[6];
    int matches;
    size_t i;

    values[0] = ek_val_unit();
    values[1] = ek_val_int(7);
    values[2] = ek_val_uint(7);
    values[3] = ek_val_float(7.0);
    values[4] = ek_val_char('7');
    values[5] = ek_val_ref(&probe_obj);

    for (i = 0; i < 6; i++) {
        matches = 0;
        matches += ek_val_is_unit(values[i]);
        matches += ek_val_is_int(values[i]);
        matches += ek_val_is_uint(values[i]);
        matches += ek_val_is_float(values[i]);
        matches += ek_val_is_char(values[i]);
        matches += ek_val_is_ref(values[i]);
        EK_CHECK_U64(matches, 1);
    }
}

static void value_layout(void)
{
    EK_CHECK_U64(sizeof(ek_val), 16);
}

int main(void)
{
    unit_is_tag_only();
    int_round_trip();
    uint_round_trip();
    float_round_trip();
    char_round_trip();
    ref_round_trip();
    bool_travels_as_int();
    predicates_are_exclusive();
    value_layout();
    return ek_test_report("rt/value");
}
