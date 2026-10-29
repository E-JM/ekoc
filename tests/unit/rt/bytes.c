#include "harness.h"

#include <stddef.h>
#include <string.h>

#include "ekort.h"

/* The bytes family: construction copies, the length carries the extent
   with no terminator, and there is no mutation. */

static const unsigned char sample[5] = { 0x00, 0x01, 0xFF, 0x00, 0x7F };
static const unsigned char copied[3] = { 1, 2, 3 };

static void construction_round_trips(void)
{
    ek_obj *b = ek_bytes_new(sample, sizeof sample);

    EK_CHECK_U64(b->rc, 1);
    EK_CHECK_U64(b->info->kind, EK_KIND_BYTES);
    EK_CHECK_U64(ek_bytes_len(b), 5);
    EK_CHECK(memcmp(ek_bytes_data(b), sample, sizeof sample) == 0);
    ek_release(ek_val_ref(b));
}

static void construction_copies_the_bytes(void)
{
    unsigned char buf[3] = { 1, 2, 3 };
    ek_obj *b = ek_bytes_new(buf, 3);

    buf[0] = 9;
    EK_CHECK(memcmp(ek_bytes_data(b), copied, 3) == 0);
    ek_release(ek_val_ref(b));
}

static void interior_nuls_are_ordinary_bytes(void)
{
    ek_obj *b = ek_bytes_new(sample, sizeof sample);

    EK_CHECK_U64(ek_bytes_len(b), 5);
    EK_CHECK(((const unsigned char *)ek_bytes_data(b))[2] == 0xFF);
    ek_release(ek_val_ref(b));
}

static void empty_bytes_has_zero_length(void)
{
    ek_obj *b = ek_bytes_new(NULL, 0);

    EK_CHECK_U64(ek_bytes_len(b), 0);
    ek_release(ek_val_ref(b));
}

static void retain_release_accounting(void)
{
    ek_obj *b = ek_bytes_new(sample, sizeof sample);

    ek_retain(ek_val_ref(b));
    EK_CHECK_U64(b->rc, 2);
    ek_release(ek_val_ref(b));
    EK_CHECK_U64(b->rc, 1);
    ek_release(ek_val_ref(b));
}

int main(void)
{
    construction_round_trips();
    construction_copies_the_bytes();
    interior_nuls_are_ordinary_bytes();
    empty_bytes_has_zero_length();
    retain_release_accounting();
    return ek_test_report("rt/bytes");
}
