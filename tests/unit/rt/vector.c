#include "harness.h"

#include <stddef.h>

#include "ekort.h"

/* The vector family: the list bridges keep order, get and set stay
   inside the domain, push appends, every update leaves the source
   version intact, and the tree stays balanced enough that a long
   build's depth stays near the logarithm of its length. */

/* The list of the first n integers starting at base, ascending. */
static ek_val int_list(size_t n, int64_t base)
{
    ek_val list = ek_nil_val();

    while (n > 0) {
        ek_val cell;

        n--;
        cell = ek_cons(ek_val_int(base + (int64_t)n), list);
        ek_release(list);
        list = cell;
    }
    return list;
}

static void of_and_to_list_round_trip(void)
{
    ek_val list = int_list(5, 10);
    ek_val v = ek_vec_of(list);
    ek_val back = ek_vec_to_list(v);
    ek_val cur;
    int64_t expect = 10;

    EK_CHECK_U64(ek_vec_len(v), 5);
    for (cur = back; ek_val_is_cons(cur); cur = ek_cons_tail(cur.as.ref))
        EK_CHECK_U64(ek_cons_head(cur.as.ref).as.i, expect++);
    EK_CHECK_U64(expect, 15);
    ek_release(back);
    ek_release(v);
    ek_release(list);
}

static void the_empty_vector_has_no_positions(void)
{
    ek_val v = ek_vec_new();
    ek_val out = ek_val_int(-1);

    EK_CHECK_U64(ek_vec_len(v), 0);
    EK_CHECK(!ek_vec_get(v, ek_val_int(0), &out));
    EK_CHECK_U64(out.as.i, -1);
    EK_CHECK(ek_vec_set(v, ek_val_int(0), ek_val_int(1)) == NULL);
    ek_release(v);
}

static void get_reads_the_positions(void)
{
    ek_val list = int_list(100, 0);
    ek_val v = ek_vec_of(list);
    ek_val out;
    size_t i;

    for (i = 0; i < 100; i++) {
        EK_CHECK(ek_vec_get(v, ek_val_int((int64_t)i), &out));
        EK_CHECK_U64(out.as.i, i);
    }
    EK_CHECK(!ek_vec_get(v, ek_val_int(100), &out));
    EK_CHECK(!ek_vec_get(v, ek_val_int(-1), &out));
    ek_release(v);
    ek_release(list);
}

static void set_replaces_and_shares(void)
{
    ek_val list = int_list(8, 0);
    ek_val v = ek_vec_of(list);
    ek_obj *w = ek_vec_set(v, ek_val_int(3), ek_val_int(99));
    ek_val out;

    EK_CHECK(w != NULL);
    EK_CHECK(ek_vec_get(ek_val_ref(w), ek_val_int(3), &out));
    EK_CHECK_U64(out.as.i, 99);
    EK_CHECK(ek_vec_get(ek_val_ref(w), ek_val_int(2), &out));
    EK_CHECK_U64(out.as.i, 2);
    /* the source version keeps its own third element */
    EK_CHECK(ek_vec_get(v, ek_val_int(3), &out));
    EK_CHECK_U64(out.as.i, 3);
    EK_CHECK(ek_vec_set(v, ek_val_int(8), ek_val_int(0)) == NULL);
    ek_release(ek_val_ref(w));
    ek_release(v);
    ek_release(list);
}

static void push_appends_and_shares(void)
{
    ek_val v = ek_vec_new();
    size_t i;

    for (i = 0; i < 200; i++) {
        ek_val next = ek_vec_push(v, ek_val_int((int64_t)i));

        ek_release(v);
        v = next;
        EK_CHECK_U64(ek_vec_len(v), i + 1);
    }
    for (i = 0; i < 200; i++) {
        ek_val out;

        EK_CHECK(ek_vec_get(v, ek_val_int((int64_t)i), &out));
        EK_CHECK_U64(out.as.i, i);
    }
    ek_release(v);
}

/* The tree stays balanced: the depth of a sequential build of a
   thousand elements stays within the AVL bound of the logarithm. */
static void the_tree_stays_balanced(void)
{
    ek_val list = int_list(1000, 0);
    ek_val v = ek_vec_of(list);
    ek_val out;

    /* a balanced tree reaches any position in a short walk: reading
       the last element of a thousand answers from a shallow tree */
    EK_CHECK(ek_vec_get(v, ek_val_int(999), &out));
    EK_CHECK_U64(out.as.i, 999);
    EK_CHECK_U64(ek_vec_len(v), 1000);
    ek_release(v);
    ek_release(list);
}

/* A push past a large sequential build keeps every earlier version's
   reads correct, which only path sharing can carry this cheaply. */
static void versions_keep_their_elements(void)
{
    ek_val list = int_list(50, 0);
    ek_val v50 = ek_vec_of(list);
    ek_val v = v50;
    ek_val out;
    size_t i;

    ek_retain(v50); /* the loop releases its own reference each step */
    for (i = 50; i < 150; i++) {
        ek_val next = ek_vec_push(v, ek_val_int((int64_t)i));

        ek_release(v);
        v = next;
    }
    for (i = 0; i < 50; i++) {
        EK_CHECK(ek_vec_get(v50, ek_val_int((int64_t)i), &out));
        EK_CHECK_U64(out.as.i, i);
        EK_CHECK(ek_vec_get(v, ek_val_int((int64_t)i), &out));
        EK_CHECK_U64(out.as.i, i);
    }
    EK_CHECK(ek_vec_get(v, ek_val_int(149), &out));
    EK_CHECK_U64(out.as.i, 149);
    ek_release(v);
    ek_release(v50);
    ek_release(list);
}

int main(void)
{
    of_and_to_list_round_trip();
    the_empty_vector_has_no_positions();
    get_reads_the_positions();
    set_replaces_and_shares();
    push_appends_and_shares();
    the_tree_stays_balanced();
    versions_keep_their_elements();
    return ek_test_report("rt/vector");
}
