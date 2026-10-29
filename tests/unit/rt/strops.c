#include "harness.h"

#include <stddef.h>
#include <stdint.h>

#include "ekort.h"

/* The string's scalar surface: the length counts Unicode scalar
   values rather than bytes, the access walks to the scalar index, the
   bridge members move between the string and a list of Char values,
   and the ordering family is lexicographic over the scalars. */

static void length_counts_scalars_not_bytes(void)
{
    static const char *samples[] = {
        "", "abc", "h\xc3\xa9llo", "\xf0\x9f\x98\x80",
        "a\xc3\xa9\xf0\x9f\x98\x80",
    };
    static const size_t counts[] = {0, 3, 5, 1, 3};
    size_t i;

    for (i = 0; i < sizeof samples / sizeof samples[0]; i++) {
        ek_obj *s = ek_str_from_cstr(samples[i]);

        EK_CHECK_U64(ek_str_length(s), counts[i]);
        ek_release(ek_val_ref(s));
    }
}

static void at_walks_to_the_scalar_index(void)
{
    ek_obj *s = ek_str_from_cstr("a\xc3\xa9\xf0\x9f\x98\x80" "b");
    uint32_t code = 0;

    EK_CHECK(ek_str_at(s, 0, &code));
    EK_CHECK_U64(code, 'a');
    EK_CHECK(ek_str_at(s, 1, &code));
    EK_CHECK_U64(code, 0xE9);
    EK_CHECK(ek_str_at(s, 2, &code));
    EK_CHECK_U64(code, 0x1F600);
    EK_CHECK(ek_str_at(s, 3, &code));
    EK_CHECK_U64(code, 'b');
    EK_CHECK(!ek_str_at(s, 4, &code));
    EK_CHECK(!ek_str_at(s, -1, &code));
    ek_release(ek_val_ref(s));
}

/* The chars list holds one Char per scalar, in string order. */
static void chars_lists_the_scalars_in_order(void)
{
    ek_obj *s = ek_str_from_cstr("a\xc3\xa9\xf0\x9f\x98\x80");
    ek_val list = ek_str_chars(s);
    ek_val cur = list;

    EK_CHECK_U64(ek_list_len(list), 3);
    EK_CHECK(ek_val_is_char(ek_cons_head(cur.as.ref)));
    EK_CHECK_U64(ek_cons_head(cur.as.ref).as.c, 'a');
    cur = ek_cons_tail(cur.as.ref);
    EK_CHECK_U64(ek_cons_head(cur.as.ref).as.c, 0xE9);
    cur = ek_cons_tail(cur.as.ref);
    EK_CHECK_U64(ek_cons_head(cur.as.ref).as.c, 0x1F600);
    EK_CHECK(ek_val_is_nil(ek_cons_tail(cur.as.ref)));
    ek_release(list);
    ek_release(ek_val_ref(s));
}

static void from_char_encodes_one_scalar(void)
{
    ek_obj *a = ek_str_from_char(ek_val_char('a'));
    ek_obj *e = ek_str_from_char(ek_val_char(0x1F600));

    EK_CHECK_U64(ek_str_len(a), 1);
    EK_CHECK_STR(ek_str_cstr(a), "a");
    EK_CHECK_U64(ek_str_len(e), 4);
    EK_CHECK_STR(ek_str_cstr(e), "\xf0\x9f\x98\x80");
    ek_release(ek_val_ref(a));
    ek_release(ek_val_ref(e));
}

/* The batch bridge rebuilds the exact bytes, so chars then from_chars
   is the identity on any string. */
static void from_chars_rebuilds_the_string(void)
{
    ek_obj *s = ek_str_from_cstr("h\xc3\xa9llo \xf0\x9f\x98\x80");
    ek_val chars = ek_str_chars(s);
    ek_obj *back = ek_str_from_chars(chars);
    ek_obj *empty = ek_str_from_chars(ek_nil_val());

    EK_CHECK(ek_str_eq(s, back));
    EK_CHECK_U64(ek_str_len(empty), 0);
    ek_release(chars);
    ek_release(ek_val_ref(back));
    ek_release(ek_val_ref(empty));
    ek_release(ek_val_ref(s));
}

static void ordering_is_lexicographic(void)
{
    ek_obj *empty = ek_str_from_cstr("");
    ek_obj *abc = ek_str_from_cstr("abc");
    ek_obj *abcd = ek_str_from_cstr("abcd");
    ek_obj *b = ek_str_from_cstr("b");

    EK_CHECK(ek_str_cmp(empty, abc) < 0);
    EK_CHECK(ek_str_cmp(abc, abcd) < 0); /* a prefix orders first */
    EK_CHECK(ek_str_cmp(abcd, abc) > 0);
    EK_CHECK(ek_str_cmp(abc, b) < 0);
    EK_CHECK(ek_str_cmp(abc, abc) == 0);
    ek_release(ek_val_ref(empty));
    ek_release(ek_val_ref(abc));
    ek_release(ek_val_ref(abcd));
    ek_release(ek_val_ref(b));
}

static void ordering_family_agrees_with_cmp(void)
{
    ek_obj *a = ek_str_from_cstr("eko");
    ek_obj *b = ek_str_from_cstr("ekoc");
    ek_obj *same = ek_str_from_cstr("eko");

    EK_CHECK(ek_str_lt(a, b));
    EK_CHECK(!ek_str_lt(b, a));
    EK_CHECK(!ek_str_lt(a, same));
    EK_CHECK(ek_str_le(a, b));
    EK_CHECK(ek_str_le(a, same));
    EK_CHECK(!ek_str_le(b, a));
    EK_CHECK(ek_str_gt(b, a));
    EK_CHECK(!ek_str_gt(a, b));
    EK_CHECK(ek_str_ge(a, same));
    EK_CHECK(!ek_str_ge(a, b));
    ek_release(ek_val_ref(a));
    ek_release(ek_val_ref(b));
    ek_release(ek_val_ref(same));
}

/* UTF-8 byte order is scalar order: the two-byte form of a low code
   point orders above any ASCII byte, and the four-byte form orders
   above the three-byte forms. */
static void byte_order_carries_scalar_order(void)
{
    ek_obj *ascii = ek_str_from_cstr("~");
    ek_obj *eacute = ek_str_from_cstr("\xc3\xa9");
    ek_obj *euro = ek_str_from_cstr("\xe2\x82\xac");
    ek_obj *emoji = ek_str_from_cstr("\xf0\x9f\x98\x80");

    EK_CHECK(ek_str_lt(ascii, eacute));
    EK_CHECK(ek_str_lt(eacute, euro));
    EK_CHECK(ek_str_lt(euro, emoji));
    ek_release(ek_val_ref(ascii));
    ek_release(ek_val_ref(eacute));
    ek_release(ek_val_ref(euro));
    ek_release(ek_val_ref(emoji));
}

int main(void)
{
    length_counts_scalars_not_bytes();
    at_walks_to_the_scalar_index();
    chars_lists_the_scalars_in_order();
    from_char_encodes_one_scalar();
    from_chars_rebuilds_the_string();
    ordering_is_lexicographic();
    ordering_family_agrees_with_cmp();
    byte_order_carries_scalar_order();
    return ek_test_report("rt/strops");
}
