#include "harness.h"

#include <stddef.h>
#include <string.h>

#include "ekort.h"

/* The string family: construction copies the bytes, the C-string view
   is zero-copy and terminated, concatenation builds a fresh string,
   and equality compares lengths and bytes. The UTF-8 gate accepts the
   shapes the encoding allows and rejects the rest. */

static void construction_round_trips(void)
{
    ek_obj *s = ek_str_from_cstr("eko");

    EK_CHECK_U64(s->rc, 1);
    EK_CHECK_U64(s->info->kind, EK_KIND_STR);
    EK_CHECK_U64(ek_str_len(s), 3);
    EK_CHECK_STR(ek_str_cstr(s), "eko");
    ek_release(ek_val_ref(s));
}

static void construction_copies_the_bytes(void)
{
    char buf[4] = "abc";
    ek_obj *s = ek_str_new(buf, 3);

    buf[0] = 'X';
    EK_CHECK_STR(ek_str_cstr(s), "abc");
    ek_release(ek_val_ref(s));
}

static void embedded_nul_keeps_full_length(void)
{
    ek_obj *s = ek_str_new("a\0b", 3);

    EK_CHECK_U64(ek_str_len(s), 3);
    EK_CHECK(memcmp(ek_str_cstr(s), "a\0b", 3) == 0);
    EK_CHECK_U64(ek_str_cstr(s)[3], '\0');
    ek_release(ek_val_ref(s));
}

static void empty_string_round_trips(void)
{
    ek_obj *s = ek_str_new(NULL, 0);
    ek_obj *t = ek_str_from_cstr("");

    EK_CHECK_U64(ek_str_len(s), 0);
    EK_CHECK(ek_str_cstr(s)[0] == '\0');
    EK_CHECK_U64(ek_str_len(t), 0);
    ek_release(ek_val_ref(s));
    ek_release(ek_val_ref(t));
}

static void concat_builds_a_fresh_string(void)
{
    ek_obj *a = ek_str_from_cstr("foo");
    ek_obj *b = ek_str_from_cstr("bar");
    ek_obj *c = ek_str_concat(a, b);

    EK_CHECK(c != a && c != b);
    EK_CHECK_U64(ek_str_len(c), 6);
    EK_CHECK_STR(ek_str_cstr(c), "foobar");
    EK_CHECK_STR(ek_str_cstr(a), "foo"); /* the operands are untouched */
    EK_CHECK_STR(ek_str_cstr(b), "bar");
    ek_release(ek_val_ref(a));
    ek_release(ek_val_ref(b));
    ek_release(ek_val_ref(c));
}

static void concat_with_empty_keeps_bytes(void)
{
    ek_obj *a = ek_str_from_cstr("eko");
    ek_obj *e = ek_str_new(NULL, 0);
    ek_obj *l = ek_str_concat(e, a);
    ek_obj *r = ek_str_concat(a, e);

    EK_CHECK_STR(ek_str_cstr(l), "eko");
    EK_CHECK_STR(ek_str_cstr(r), "eko");
    ek_release(ek_val_ref(a));
    ek_release(ek_val_ref(e));
    ek_release(ek_val_ref(l));
    ek_release(ek_val_ref(r));
}

static void equality_compares_lengths_then_bytes(void)
{
    ek_obj *a = ek_str_from_cstr("eko");
    ek_obj *b = ek_str_from_cstr("eko");
    ek_obj *c = ek_str_from_cstr("ekoc");
    ek_obj *d = ek_str_from_cstr("ekox");
    ek_obj *nul = ek_str_new("a\0b", 3);
    ek_obj *ter = ek_str_from_cstr("a");
    ek_obj *e0 = ek_str_new(NULL, 0);
    ek_obj *e1 = ek_str_from_cstr("");

    EK_CHECK(ek_str_eq(a, a));
    EK_CHECK(ek_str_eq(a, b));
    EK_CHECK(!ek_str_eq(a, c)); /* length decides first */
    EK_CHECK(!ek_str_eq(c, d)); /* then the bytes */
    EK_CHECK(!ek_str_eq(nul, ter));
    EK_CHECK(ek_str_eq(e0, e1));
    ek_release(ek_val_ref(a));
    ek_release(ek_val_ref(b));
    ek_release(ek_val_ref(c));
    ek_release(ek_val_ref(d));
    ek_release(ek_val_ref(nul));
    ek_release(ek_val_ref(ter));
    ek_release(ek_val_ref(e0));
    ek_release(ek_val_ref(e1));
}

static void retain_release_accounting(void)
{
    ek_obj *s = ek_str_from_cstr("shared");

    ek_retain(ek_val_ref(s));
    EK_CHECK_U64(s->rc, 2);
    ek_release(ek_val_ref(s));
    EK_CHECK_U64(s->rc, 1);
    ek_release(ek_val_ref(s));
}

static const struct {
    const char *bytes;
    size_t len;
} good_utf8[] = {
    { "", 0 },
    { "abc", 3 },
    { "h\xc3\xa9llo", 6 },              /* two-byte form */
    { "\xe2\x82\xac", 3 },              /* three-byte form */
    { "\xed\x9f\xbf", 3 },              /* just below the surrogates */
    { "\xf0\x9f\x98\x80", 4 },          /* four-byte form */
    { "\xf4\x8f\xbf\xbf", 4 },          /* the largest code point */
    { "a\xc3\xa9\xf0\x9f\x98\x80", 7 }, /* mixed widths in one run */
};

static const struct {
    const char *bytes;
    size_t len;
} bad_utf8[] = {
    { "\x80", 1 },             /* stray continuation byte */
    { "\xc0\xaf", 2 },         /* overlong slash */
    { "\xc1\xbf", 2 },         /* overlong two-byte lead */
    { "\xe0\x80\x80", 3 },     /* overlong three-byte form */
    { "\xed\xa0\x80", 3 },     /* surrogate encoding */
    { "\xf4\x90\x80\x80", 4 }, /* above the maximum */
    { "\xf5\x80\x80\x80", 4 }, /* lead beyond the maximum */
    { "\xfe\xff", 2 },         /* impossible leads */
    { "\xc3", 1 },             /* truncated two-byte form */
    { "\xe2\x28\xa1", 3 },     /* bad byte inside a three-byte form */
    { "\xf0\x9f\x98", 3 },     /* truncated four-byte form */
    { "a\xc3", 2 },            /* truncated form after text */
};

static void utf8_gate_accepts_well_formed(void)
{
    size_t i;

    for (i = 0; i < sizeof good_utf8 / sizeof good_utf8[0]; i++)
        EK_CHECK(ek_utf8_valid(good_utf8[i].bytes, good_utf8[i].len));
}

static void utf8_gate_rejects_malformed(void)
{
    size_t i;

    for (i = 0; i < sizeof bad_utf8 / sizeof bad_utf8[0]; i++)
        EK_CHECK(!ek_utf8_valid(bad_utf8[i].bytes, bad_utf8[i].len));
}

int main(void)
{
    construction_round_trips();
    construction_copies_the_bytes();
    embedded_nul_keeps_full_length();
    empty_string_round_trips();
    concat_builds_a_fresh_string();
    concat_with_empty_keeps_bytes();
    equality_compares_lengths_then_bytes();
    retain_release_accounting();
    utf8_gate_accepts_well_formed();
    utf8_gate_rejects_malformed();
    return ek_test_report("rt/string");
}
