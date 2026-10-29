#include "harness.h"

#include <stddef.h>

#include "ekort.h"

/* The dictionary family: insert builds a fresh table and retains what
   it stores, keys of different tags stay distinct, the table grows
   under insertion, and the last release lets go of every entry. */

static void empty_dictionary(void)
{
    ek_val d = ek_dict_new();
    ek_val out;

    EK_CHECK(ek_val_is_ref(d));
    EK_CHECK_U64(ek_dict_len(d), 0);
    EK_CHECK(!ek_dict_get(d, ek_val_int(1), &out));
    ek_release(d);
}

static void set_then_get(void)
{
    ek_obj *v = ek_str_from_cstr("val");
    ek_val d0 = ek_dict_new();
    ek_val d1 = ek_dict_set(d0, ek_val_int(1), ek_val_ref(v));
    ek_val out;

    EK_CHECK_U64(ek_dict_len(d1), 1);
    EK_CHECK_U64(ek_dict_len(d0), 0); /* the old dictionary is untouched */
    EK_CHECK(ek_dict_get(d1, ek_val_int(1), &out));
    EK_CHECK(out.as.ref == v); /* the same object, shared */
    EK_CHECK_U64(v->rc, 2);    /* the caller's and the table's */
    EK_CHECK(!ek_dict_get(d1, ek_val_int(2), &out));
    ek_release(d1);
    EK_CHECK_U64(v->rc, 1); /* the entry went with the table */
    ek_release(d0);
    ek_release(ek_val_ref(v));
}

static void overwrite_replaces_only_the_value(void)
{
    ek_obj *v1 = ek_str_from_cstr("old");
    ek_obj *v2 = ek_str_from_cstr("new");
    ek_val d0 = ek_dict_new();
    ek_val d1 = ek_dict_set(d0, ek_val_int(1), ek_val_ref(v1));
    ek_val d2 = ek_dict_set(d1, ek_val_int(1), ek_val_ref(v2));
    ek_val out;

    EK_CHECK_U64(ek_dict_len(d2), 1);
    EK_CHECK(ek_dict_get(d2, ek_val_int(1), &out));
    EK_CHECK(out.as.ref == v2);
    EK_CHECK(ek_dict_get(d1, ek_val_int(1), &out));
    EK_CHECK(out.as.ref == v1); /* the old mapping survives */
    EK_CHECK_U64(v1->rc, 2);
    EK_CHECK_U64(v2->rc, 2);
    ek_release(d1);
    EK_CHECK_U64(v1->rc, 1);
    EK_CHECK_U64(v2->rc, 2); /* the fresh table holds the new value */
    ek_release(d2);
    EK_CHECK_U64(v2->rc, 1);
    ek_release(d0);
    ek_release(ek_val_ref(v1));
    ek_release(ek_val_ref(v2));
}

/* The keys carry the same payload under different tags, so only the
   tag keeps them apart. */
static void keys_differ_across_tags(void)
{
    ek_val keys[6] = {
        ek_val_int(1), ek_val_uint(1), ek_val_float(1.0),
        ek_val_char('1'), ek_val_int(2), ek_val_int(-1),
    };
    ek_val d = ek_dict_new();
    ek_val out;
    size_t i;

    for (i = 0; i < 6; i++) {
        ek_val next = ek_dict_set(d, keys[i], ek_val_int((int64_t)(i + 1) * 10));

        ek_release(d);
        d = next;
    }
    EK_CHECK_U64(ek_dict_len(d), 6);
    for (i = 0; i < 6; i++) {
        EK_CHECK(ek_dict_get(d, keys[i], &out));
        EK_CHECK_U64(out.as.i, (i + 1) * 10);
    }
    EK_CHECK(!ek_dict_get(d, ek_val_uint(2), &out));
    EK_CHECK(!ek_dict_get(d, ek_val_int(3), &out));
    ek_release(d);
}

static void growth_keeps_every_entry(void)
{
    ek_val d = ek_dict_new();
    ek_val out;
    int i;

    for (i = 0; i < 100; i++) {
        ek_val next = ek_dict_set(d, ek_val_int(i), ek_val_int(i * 10));

        ek_release(d);
        d = next;
    }
    EK_CHECK_U64(ek_dict_len(d), 100);
    for (i = 0; i < 100; i++) {
        EK_CHECK(ek_dict_get(d, ek_val_int(i), &out));
        EK_CHECK_U64(out.as.i, (unsigned long long)i * 10);
    }
    EK_CHECK(!ek_dict_get(d, ek_val_int(100), &out));
    ek_release(d);
}

int main(void)
{
    empty_dictionary();
    set_then_get();
    overwrite_replaces_only_the_value();
    keys_differ_across_tags();
    growth_keeps_every_entry();
    return ek_test_report("rt/dict");
}
