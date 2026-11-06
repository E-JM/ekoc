#include "harness.h"

#include <stddef.h>

#include "ekort.h"

/* The dictionary's build-order surface: the walk visits insertion
   order even after the keys rehash into a grown table, a replacement
   keeps its place, removing an absent key yields the original, and a
   merge lets the right side win on duplicate keys. */

static ek_val int_dict(const int64_t *keys, size_t n)
{
    ek_val d = ek_dict_new(ek_key_eq, ek_key_hash);
    size_t i;

    for (i = 0; i < n; i++) {
        ek_val next = ek_dict_set(d, ek_val_int(keys[i]),
                                  ek_val_int(keys[i] * 10));

        ek_release(d);
        d = next;
    }
    return d;
}

/* Walks the dictionary and writes the visited keys into out; returns
   how many were seen. */
static size_t walk_keys(ek_val d, int64_t *out, size_t cap)
{
    size_t cursor = 0;
    size_t n = 0;
    ek_val key;
    ek_val val;

    while (ek_dict_next(d, &cursor, &key, &val)) {
        if (n < cap)
            out[n] = key.as.i;
        n++;
    }
    return n;
}

static void walk_visits_insertion_order(void)
{
    static const int64_t keys[5] = {5, -3, 9, 0, 7};
    ek_val d = int_dict(keys, 5);
    int64_t seen[5];

    EK_CHECK_U64(walk_keys(d, seen, 5), 5);
    EK_CHECK_U64(seen[0], 5);
    EK_CHECK_U64(seen[1], -3);
    EK_CHECK_U64(seen[2], 9);
    EK_CHECK_U64(seen[3], 0);
    EK_CHECK_U64(seen[4], 7);
    ek_release(d);
}

/* One hundred keys hash into several table sizes, so the chain has to
   survive every rebuild. */
static void walk_order_survives_growth(void)
{
    ek_val d = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val out;
    int64_t seen[100];
    int64_t want[100];
    size_t cursor = 0;
    size_t n = 0;
    int i;

    for (i = 0; i < 100; i++) {
        ek_val next = ek_dict_set(d, ek_val_int(i), ek_val_int(i));

        ek_release(d);
        d = next;
        want[i] = i;
    }
    EK_CHECK_U64(ek_dict_len(d), 100);
    while (ek_dict_next(d, &cursor, &out, &out))
        seen[n++] = out.as.i;
    EK_CHECK_U64(n, 100);
    for (i = 0; i < 100; i++)
        EK_CHECK_U64(seen[i], want[i]);
    ek_release(d);
}

static void replacement_keeps_its_place(void)
{
    ek_val d0 = int_dict((const int64_t[]){1, 2, 3}, 3);
    ek_val d1 = ek_dict_set(d0, ek_val_int(2), ek_val_int(99));
    int64_t seen[3];

    EK_CHECK_U64(walk_keys(d1, seen, 3), 3);
    EK_CHECK_U64(seen[0], 1);
    EK_CHECK_U64(seen[1], 2); /* the old position, with the new value */
    EK_CHECK_U64(seen[2], 3);
    ek_release(d1);
    ek_release(d0);
}

static void walk_borrows_its_entries(void)
{
    ek_obj *v = ek_str_from_cstr("val");
    ek_val d0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val d1 = ek_dict_set(d0, ek_val_int(1), ek_val_ref(v));
    size_t cursor = 0;
    ek_val key;
    ek_val out;

    EK_CHECK(ek_dict_next(d1, &cursor, &key, &out));
    EK_CHECK(out.as.ref == v);
    EK_CHECK_U64(v->rc, 2); /* the walk adds no reference */
    EK_CHECK(!ek_dict_next(d1, &cursor, &key, &out));
    ek_release(d1);
    ek_release(d0);
    ek_release(ek_val_ref(v));
}

static void removing_an_absent_key_yields_the_original(void)
{
    ek_val d = int_dict((const int64_t[]){1, 2}, 2);
    ek_val r = ek_dict_remove(d, ek_val_int(9));
    ek_val r2 = ek_dict_remove(d, ek_val_uint(1));

    EK_CHECK(r.as.ref == d.as.ref);
    EK_CHECK(r2.as.ref == d.as.ref);
    EK_CHECK_U64(d.as.ref->rc, 3); /* the caller's and the two aliases' */
    ek_release(r2);
    ek_release(r);
    EK_CHECK_U64(d.as.ref->rc, 1);
    ek_release(d);
}

static void remove_keeps_the_remaining_order(void)
{
    ek_obj *v = ek_str_from_cstr("gone");
    ek_val d0 = int_dict((const int64_t[]){1, 2, 3}, 3);
    ek_val d1 = ek_dict_set(d0, ek_val_int(2), ek_val_ref(v));
    ek_val d2 = ek_dict_remove(d1, ek_val_int(2));
    int64_t seen[2];

    EK_CHECK_U64(ek_dict_len(d2), 2);
    EK_CHECK_U64(walk_keys(d2, seen, 2), 2);
    EK_CHECK_U64(seen[0], 1);
    EK_CHECK_U64(seen[1], 3);
    EK_CHECK(!ek_dict_get(d2, ek_val_int(2), &(ek_val){0}));
    ek_release(d2);
    EK_CHECK_U64(v->rc, 2); /* the middle dictionary still holds it */
    ek_release(d1);
    EK_CHECK_U64(v->rc, 1); /* the entry went with the table */
    ek_release(d0);
    ek_release(ek_val_ref(v));
}

static void merge_lets_the_right_side_win(void)
{
    static const int64_t a_keys[2] = {1, 2};
    ek_obj *bv = ek_str_from_cstr("right");
    ek_val a = int_dict(a_keys, 2);
    ek_val b0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val b1 = ek_dict_set(b0, ek_val_int(2), ek_val_ref(bv));
    ek_val b = ek_dict_set(b1, ek_val_int(3), ek_val_int(30));
    ek_val m = ek_dict_merge(a, b);
    int64_t seen[3];
    ek_val out;

    EK_CHECK_U64(ek_dict_len(m), 3);
    EK_CHECK_U64(walk_keys(m, seen, 3), 3);
    EK_CHECK_U64(seen[0], 1); /* a's order first */
    EK_CHECK_U64(seen[1], 2); /* the duplicate keeps a's place */
    EK_CHECK_U64(seen[2], 3); /* b's new key appends */
    EK_CHECK(ek_dict_get(m, ek_val_int(2), &out));
    EK_CHECK(out.as.ref == bv); /* b won the duplicate */
    ek_release(m);
    ek_release(b);
    ek_release(b1);
    ek_release(b0);
    ek_release(a);
    ek_release(ek_val_ref(bv));
}

static void merge_with_the_empty_side_yields_the_other(void)
{
    static const int64_t keys[2] = {1, 2};
    ek_val d = int_dict(keys, 2);
    ek_val empty = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val lr = ek_dict_merge(d, empty);
    ek_val rl = ek_dict_merge(empty, d);
    int64_t seen[2];

    EK_CHECK_U64(walk_keys(lr, seen, 2), 2);
    EK_CHECK_U64(seen[0], 1);
    EK_CHECK_U64(seen[1], 2);
    EK_CHECK_U64(walk_keys(rl, seen, 2), 2);
    EK_CHECK_U64(seen[0], 1);
    ek_release(lr);
    ek_release(rl);
    ek_release(empty);
    ek_release(d);
}

int main(void)
{
    walk_visits_insertion_order();
    walk_order_survives_growth();
    replacement_keeps_its_place();
    walk_borrows_its_entries();
    removing_an_absent_key_yields_the_original();
    remove_keeps_the_remaining_order();
    merge_lets_the_right_side_win();
    merge_with_the_empty_side_yields_the_other();
    return ek_test_report("rt/dictops");
}
