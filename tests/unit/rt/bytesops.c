#include "harness.h"

#include <stddef.h>
#include <string.h>

#include "ekort.h"

/* The bytes operations: the element domain is Int in 0..255, the
   bridges move between the block and lists, the hex form is lowercase
   without separators, and the decode back to text names the first
   invalid offset. */

static ek_obj *bytes_of(const unsigned char *p, size_t len)
{
    return ek_bytes_new(p, len);
}

static ek_val int_list(const int64_t *xs, size_t n)
{
    ek_val list = ek_nil_val();
    size_t i = n;

    while (i > 0) {
        ek_val next;

        i--;
        next = ek_cons(ek_val_int(xs[i]), list);
        ek_release(list);
        list = next;
    }
    return list;
}

static void get_reads_the_indexed_byte(void)
{
    static const unsigned char raw[3] = {0x00, 0x7F, 0xFF};
    ek_obj *b = bytes_of(raw, 3);
    int64_t out = -1;

    EK_CHECK(ek_bytes_get(b, ek_val_int(0), &out));
    EK_CHECK_U64(out, 0);
    EK_CHECK(ek_bytes_get(b, ek_val_int(1), &out));
    EK_CHECK_U64(out, 0x7F);
    EK_CHECK(ek_bytes_get(b, ek_val_int(2), &out));
    EK_CHECK_U64(out, 0xFF);
    EK_CHECK(!ek_bytes_get(b, ek_val_int(3), &out));
    EK_CHECK_U64(out, 0xFF); /* a miss leaves the out-param alone */
    EK_CHECK(!ek_bytes_get(b, ek_val_int(-1), &out));
    EK_CHECK_U64(out, 0xFF);
    ek_release(ek_val_ref(b));
}

static void concat_builds_a_fresh_block(void)
{
    static const unsigned char pa[2] = {1, 2};
    static const unsigned char pb[2] = {3, 4};
    ek_obj *a = bytes_of(pa, 2);
    ek_obj *b = bytes_of(pb, 2);
    ek_obj *c = ek_bytes_concat(a, b);
    static const unsigned char both[4] = {1, 2, 3, 4};

    EK_CHECK(c != a && c != b);
    EK_CHECK_U64(ek_bytes_len(c), 4);
    EK_CHECK(memcmp(ek_bytes_data(c), both, 4) == 0);
    EK_CHECK_U64(ek_bytes_len(a), 2); /* the operands are untouched */
    ek_release(ek_val_ref(a));
    ek_release(ek_val_ref(b));
    ek_release(ek_val_ref(c));
}

static void to_ints_lists_in_order(void)
{
    static const unsigned char raw[3] = {0, 255, 7};
    ek_obj *b = bytes_of(raw, 3);
    ek_val list = ek_bytes_to_ints(b);
    ek_val cur = list;

    EK_CHECK_U64(ek_list_len(list), 3);
    EK_CHECK_U64(ek_cons_head(cur.as.ref).as.i, 0);
    cur = ek_cons_tail(cur.as.ref);
    EK_CHECK_U64(ek_cons_head(cur.as.ref).as.i, 255);
    cur = ek_cons_tail(cur.as.ref);
    EK_CHECK_U64(ek_cons_head(cur.as.ref).as.i, 7);
    EK_CHECK(ek_val_is_nil(ek_cons_tail(cur.as.ref)));
    ek_release(list);
    ek_release(ek_val_ref(b));
}

static void from_ints_checks_the_element_domain(void)
{
    static const int64_t good[3] = {0, 255, 7};
    static const int64_t high[2] = {1, 256};
    static const int64_t negative[1] = {-1};
    ek_val list = int_list(good, 3);
    ek_obj *b = ek_bytes_from_ints(list);
    static const unsigned char raw[3] = {0, 255, 7};

    EK_CHECK(b != NULL);
    EK_CHECK_U64(ek_bytes_len(b), 3);
    EK_CHECK(memcmp(ek_bytes_data(b), raw, 3) == 0);
    ek_release(ek_val_ref(b));
    ek_release(list);

    /* an element outside the domain refuses the whole block */
    {
        ek_val hl = int_list(high, 2);
        ek_val nl = int_list(negative, 1);

        EK_CHECK(ek_bytes_from_ints(hl) == NULL);
        EK_CHECK(ek_bytes_from_ints(nl) == NULL);
        {
            ek_obj *eb = ek_bytes_from_ints(ek_nil_val());

            EK_CHECK(eb != NULL);
            EK_CHECK_U64(ek_bytes_len(eb), 0);
            ek_release(ek_val_ref(eb));
        }
        ek_release(hl);
        ek_release(nl);
    }
}

static void hex_is_lowercase_and_round_trips(void)
{
    static const unsigned char raw[2] = {0x41, 0x0F};
    ek_obj *b = bytes_of(raw, 2);
    ek_obj *hex = ek_bytes_to_hex(b);
    ek_obj *back = NULL;
    ek_obj *detail = NULL;

    EK_CHECK_STR(ek_str_cstr(hex), "410f");
    EK_CHECK(ek_bytes_from_hex(hex, &back, &detail));
    EK_CHECK_U64(ek_bytes_len(back), 2);
    EK_CHECK(memcmp(ek_bytes_data(back), raw, 2) == 0);
    ek_release(ek_val_ref(back));
    ek_release(ek_val_ref(detail));
    ek_release(ek_val_ref(hex));
    ek_release(ek_val_ref(b));
}

static void from_hex_validates_the_text(void)
{
    ek_obj *out = NULL;
    ek_obj *detail = NULL;
    ek_obj *odd = ek_str_from_cstr("414");
    ek_obj *bad = ek_str_from_cstr("4g");
    ek_obj *empty = ek_str_from_cstr("");
    ek_obj *upper = ek_str_from_cstr("4A");

    EK_CHECK(!ek_bytes_from_hex(odd, &out, &detail));
    EK_CHECK_STR(ek_str_cstr(detail), "odd length");
    ek_release(ek_val_ref(detail));
    EK_CHECK(!ek_bytes_from_hex(bad, &out, &detail));
    EK_CHECK_STR(ek_str_cstr(detail), "offset 1 is not hexadecimal");
    ek_release(ek_val_ref(detail));
    EK_CHECK(ek_bytes_from_hex(empty, &out, &detail));
    EK_CHECK_U64(ek_bytes_len(out), 0);
    ek_release(ek_val_ref(out));
    EK_CHECK(ek_bytes_from_hex(upper, &out, &detail));
    EK_CHECK_U64(ek_bytes_len(out), 1);
    EK_CHECK(((const unsigned char *)ek_bytes_data(out))[0] == 0x4A);
    ek_release(ek_val_ref(out));
    ek_release(ek_val_ref(odd));
    ek_release(ek_val_ref(bad));
    ek_release(ek_val_ref(empty));
    ek_release(ek_val_ref(upper));
}

static void to_string_decodes_well_formed_utf8(void)
{
    ek_obj *s = ek_str_from_cstr("h\xc3\xa9\xf0\x9f\x98\x80");
    ek_obj *empty = ek_str_from_cstr("");
    ek_obj *b = ek_str_to_bytes(s);
    ek_obj *back = NULL;
    ek_obj *detail = NULL;

    EK_CHECK_U64(ek_bytes_len(b), 7);
    EK_CHECK(ek_bytes_to_string(b, &back, &detail));
    EK_CHECK(ek_str_eq(s, back));
    ek_release(ek_val_ref(back));
    {
        ek_obj *eb = ek_str_to_bytes(empty);

        EK_CHECK(ek_bytes_to_string(eb, &back, &detail));
        EK_CHECK_U64(ek_str_len(back), 0);
        ek_release(ek_val_ref(eb));
    }
    ek_release(ek_val_ref(back));
    ek_release(ek_val_ref(b));
    ek_release(ek_val_ref(s));
    ek_release(ek_val_ref(empty));
}

/* The decode failure names the first invalid offset and the bytes
   that start there. */
static void to_string_names_the_first_bad_offset(void)
{
    static const unsigned char surrogate[3] = {0xED, 0xA0, 0x80};
    static const unsigned char after_text[5] = {'a', 'b', 0xED, 0xA0, 0x80};
    static const unsigned char truncated[2] = {0xE2, 0x28};
    ek_obj *detail = NULL;
    ek_obj *out = NULL;

    {
        ek_obj *b = bytes_of(surrogate, 3);

        EK_CHECK(!ek_bytes_to_string(b, &out, &detail));
        ek_release(ek_val_ref(b));
    }
    EK_CHECK_STR(ek_str_cstr(detail), "offset 0 near ed a0 80");
    ek_release(ek_val_ref(detail));
    {
        ek_obj *b = bytes_of(after_text, 5);

        EK_CHECK(!ek_bytes_to_string(b, &out, &detail));
        ek_release(ek_val_ref(b));
    }
    EK_CHECK_STR(ek_str_cstr(detail), "offset 2 near ed a0 80");
    ek_release(ek_val_ref(detail));
    {
        ek_obj *b = bytes_of(truncated, 2);

        EK_CHECK(!ek_bytes_to_string(b, &out, &detail));
        ek_release(ek_val_ref(b));
    }
    EK_CHECK_STR(ek_str_cstr(detail), "offset 0 near e2 28");
    ek_release(ek_val_ref(detail));
}

/* The machine word's bridge: the U8 list of a block and the block of a
   U8 list, with the element domain enforced through the uint slot. */
static ek_val u8_list(const uint64_t *xs, size_t n)
{
    ek_val list = ek_nil_val();
    size_t i = n;

    while (i > 0) {
        ek_val next;

        i--;
        next = ek_cons(ek_val_uint(xs[i]), list);
        ek_release(list);
        list = next;
    }
    return list;
}

static void u8_bridge_moves_both_ways(void)
{
    static const unsigned char raw[3] = {0, 127, 255};
    ek_obj *b = bytes_of(raw, 3);
    ek_val list = ek_bytes_to_u8s(b);
    ek_val cur;
    size_t i = 0;

    for (cur = list; ek_val_is_cons(cur); cur = ek_cons_tail(cur.as.ref)) {
        ek_val h = ek_cons_head(cur.as.ref);

        EK_CHECK_U64(h.tag, EK_VAL_UINT);
        EK_CHECK_U64(h.as.u, raw[i++]);
    }
    EK_CHECK_U64(i, 3);
    {
        ek_obj *back = ek_bytes_from_u8s(list);

        EK_CHECK(back != NULL);
        EK_CHECK(memcmp(ek_bytes_data(back), raw, 3) == 0);
        ek_release(ek_val_ref(back));
    }
    ek_release(list);
    ek_release(ek_val_ref(b));
}

static void from_u8s_refuses_words_outside_the_domain(void)
{
    static const uint64_t high[2] = {1, 256};
    static const int64_t as_int[2] = {1, 2};
    ek_val hl = u8_list(high, 2);
    ek_val il = int_list(as_int, 2);

    EK_CHECK(ek_bytes_from_u8s(hl) == NULL);
    /* an Int element is a value of another slot: the bridge refuses it */
    EK_CHECK(ek_bytes_from_u8s(il) == NULL);
    ek_release(hl);
    ek_release(il);
}

int main(void)
{
    get_reads_the_indexed_byte();
    concat_builds_a_fresh_block();
    to_ints_lists_in_order();
    from_ints_checks_the_element_domain();
    u8_bridge_moves_both_ways();
    from_u8s_refuses_words_outside_the_domain();
    hex_is_lowercase_and_round_trips();
    from_hex_validates_the_text();
    to_string_decodes_well_formed_utf8();
    to_string_names_the_first_bad_offset();
    return ek_test_report("rt/bytesops");
}
