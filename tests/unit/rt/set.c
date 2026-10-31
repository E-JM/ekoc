#include "harness.h"

#include <stddef.h>

#include "ekort.h"

/* The set family: insert builds a fresh table and retains what it
   stores, String and Bytes elements key on their bytes so two
   spellings of one text are one element, 0.0 and -0.0 are one element
   while a NaN element never matches, and to_list walks insertion
   order, which survives every rehash. */

/* The list's Int elements as a set, in the given order. */
static ek_val int_set(const int64_t *elems, size_t n)
{
    ek_val list = ek_nil_val();
    ek_val s;
    size_t i = n;

    while (i > 0) {
        ek_val cell;

        i--;
        cell = ek_cons(ek_val_int(elems[i]), list);
        ek_release(list);
        list = cell;
    }
    s = ek_set_of(list);
    ek_release(list);
    return s;
}

/* Walks the set and writes the visited elements into out; returns how
   many were seen. */
static size_t walk_elems(ek_val s, int64_t *out, size_t cap)
{
    ek_val list = ek_set_to_list(s);
    ek_val cur;
    size_t n = 0;

    for (cur = list; ek_val_is_cons(cur); cur = ek_cons_tail(cur.as.ref)) {
        if (n < cap)
            out[n] = ek_cons_head(cur.as.ref).as.i;
        n++;
    }
    ek_release(list);
    return n;
}

static void empty_set(void)
{
    ek_val s = ek_set_new();

    EK_CHECK(ek_val_is_ref(s));
    EK_CHECK(s.as.ref->info->kind == EK_KIND_SET);
    EK_CHECK_U64(ek_set_len(s), 0);
    EK_CHECK(!ek_set_contains(s, ek_val_int(1)));
    ek_release(s);
}

static void insert_then_contains(void)
{
    ek_obj *v = ek_str_from_cstr("a");
    ek_val s0 = ek_set_new();
    ek_val s1 = ek_set_insert(s0, ek_val_ref(v));

    EK_CHECK_U64(ek_set_len(s1), 1);
    EK_CHECK_U64(ek_set_len(s0), 0); /* the old set is untouched */
    EK_CHECK(ek_set_contains(s1, ek_val_ref(v)));
    EK_CHECK_U64(v->rc, 2); /* the caller's and the table's */
    EK_CHECK(!ek_set_contains(s1, ek_val_int(1)));
    ek_release(s1);
    EK_CHECK_U64(v->rc, 1); /* the element went with the table */
    ek_release(s0);
    ek_release(ek_val_ref(v));
}

/* A no-op insert or remove returns the original with one added
   reference: the returned set is the caller's to release either way. */
static void inserting_a_present_element_yields_the_original(void)
{
    ek_val s0 = int_set((const int64_t[]){1, 2}, 2);
    ek_val s1 = ek_set_insert(s0, ek_val_int(1));
    ek_val s2 = ek_set_insert(s0, ek_val_int(9));

    EK_CHECK(s1.as.ref == s0.as.ref);
    EK_CHECK_U64(s0.as.ref->rc, 2); /* the caller's and the alias's */
    EK_CHECK(s2.as.ref != s0.as.ref);
    EK_CHECK_U64(ek_set_len(s2), 3);
    ek_release(s1);
    EK_CHECK_U64(s0.as.ref->rc, 1);
    ek_release(s2);
    ek_release(s0);
}

static void removing_an_absent_element_yields_the_original(void)
{
    ek_val s = int_set((const int64_t[]){1, 2}, 2);
    ek_val r = ek_set_remove(s, ek_val_int(9));

    EK_CHECK(r.as.ref == s.as.ref);
    ek_release(r);
    ek_release(s);
}

static void of_dedups_keeping_the_first_occurrence(void)
{
    ek_val s = int_set((const int64_t[]){5, 3, 5, 9, 3, 5}, 6);
    int64_t seen[3];

    EK_CHECK_U64(ek_set_len(s), 3);
    EK_CHECK_U64(walk_elems(s, seen, 3), 3);
    EK_CHECK_U64(seen[0], 5);
    EK_CHECK_U64(seen[1], 3);
    EK_CHECK_U64(seen[2], 9);
    ek_release(s);
}

/* Two distinct String objects with the same bytes are one element, in
   both directions: contains finds the one spelling from the other, and
   remove drops it either way. */
static void string_elements_key_on_their_bytes(void)
{
    ek_obj *a = ek_str_from_cstr("key");
    ek_obj *b = ek_str_from_cstr("key");
    ek_val s0 = ek_set_new();
    ek_val s1 = ek_set_insert(s0, ek_val_ref(a));
    ek_val s2 = ek_set_remove(s1, ek_val_ref(b));

    EK_CHECK(a != b);
    EK_CHECK(ek_set_contains(s1, ek_val_ref(b)));
    EK_CHECK_U64(ek_set_len(s2), 0);
    ek_release(s2);
    ek_release(s1);
    ek_release(s0);
    ek_release(ek_val_ref(a));
    ek_release(ek_val_ref(b));
}

/* A String and a Bytes value over the same bytes stay distinct: the
   family separates them. */
static void string_and_bytes_stay_distinct(void)
{
    ek_obj *s = ek_str_from_cstr("ab");
    ek_obj *b = ek_bytes_new("ab", 2);
    ek_val set = ek_set_of(ek_nil_val());
    ek_val s1 = ek_set_insert(set, ek_val_ref(s));
    ek_val s2 = ek_set_insert(s1, ek_val_ref(b));

    EK_CHECK_U64(ek_set_len(s2), 2);
    EK_CHECK(ek_set_contains(s2, ek_val_ref(s)));
    EK_CHECK(ek_set_contains(s2, ek_val_ref(b)));
    ek_release(s2);
    ek_release(s1);
    ek_release(set);
    ek_release(ek_val_ref(s));
    ek_release(ek_val_ref(b));
}

/* Two distinct Bytes blocks over the same bytes are one element. */
static void bytes_elements_key_on_their_bytes(void)
{
    ek_obj *a = ek_bytes_new("\x01\x02", 2);
    ek_obj *b = ek_bytes_new("\x01\x02", 2);
    ek_val s0 = ek_set_new();
    ek_val s1 = ek_set_insert(s0, ek_val_ref(a));

    EK_CHECK(ek_set_contains(s1, ek_val_ref(b)));
    EK_CHECK_U64(ek_set_len(s1), 1);
    ek_release(s1);
    ek_release(s0);
    ek_release(ek_val_ref(a));
    ek_release(ek_val_ref(b));
}

static void elements_differ_across_tags(void)
{
    ek_val elems[5] = {
        ek_val_int(1), ek_val_uint(1), ek_val_float(1.0),
        ek_val_char('1'), ek_val_int(2),
    };
    ek_val s = ek_set_new();
    size_t i;

    for (i = 0; i < 5; i++) {
        ek_val next = ek_set_insert(s, elems[i]);

        ek_release(s);
        s = next;
    }
    EK_CHECK_U64(ek_set_len(s), 5);
    for (i = 0; i < 5; i++)
        EK_CHECK(ek_set_contains(s, elems[i]));
    ek_release(s);
}

/* The IEEE rule the element domain pins: 0.0 and -0.0 are one element,
   and a NaN element never matches, not even itself. */
static void float_elements_follow_ieee_equality(void)
{
    ek_val s0 = ek_set_new();
    ek_val s1 = ek_set_insert(s0, ek_val_float(0.0));
    ek_val s2 = ek_set_insert(s1, ek_val_float(-0.0));
    ek_val s3 = ek_set_insert(s2, ek_val_float(1.0));
    ek_val s4 = ek_set_insert(s3, ek_val_float(0.0 / 0.0));

    EK_CHECK_U64(ek_set_len(s2), 1); /* -0.0 found 0.0 */
    EK_CHECK(ek_set_contains(s2, ek_val_float(-0.0)));
    EK_CHECK_U64(ek_set_len(s3), 2);
    EK_CHECK_U64(ek_set_len(s4), 3); /* the NaN lands, unequal to itself */
    EK_CHECK(!ek_set_contains(s4, ek_val_float(0.0 / 0.0)));
    ek_release(s4);
    ek_release(s3);
    ek_release(s2);
    ek_release(s1);
    ek_release(s0);
}

static void remove_keeps_the_remaining_order(void)
{
    ek_val s0 = int_set((const int64_t[]){4, 8, 15, 16}, 4);
    ek_val s1 = ek_set_remove(s0, ek_val_int(8));
    int64_t seen[3];

    EK_CHECK_U64(ek_set_len(s1), 3);
    EK_CHECK_U64(walk_elems(s1, seen, 3), 3);
    EK_CHECK_U64(seen[0], 4);
    EK_CHECK_U64(seen[1], 15);
    EK_CHECK_U64(seen[2], 16);
    ek_release(s1);
    ek_release(s0);
}

static void order_survives_growth(void)
{
    ek_val s = ek_set_new();
    int64_t seen[100];
    int i;

    for (i = 0; i < 100; i++) {
        ek_val next = ek_set_insert(s, ek_val_int(i * 7 % 100));

        ek_release(s);
        s = next;
    }
    EK_CHECK_U64(ek_set_len(s), 100);
    EK_CHECK_U64(walk_elems(s, seen, 100), 100);
    for (i = 0; i < 100; i++)
        EK_CHECK_U64(seen[i], i * 7 % 100);
    ek_release(s);
}

static void to_list_borrows_nothing(void)
{
    ek_obj *v = ek_str_from_cstr("solo");
    ek_val s0 = ek_set_new();
    ek_val s1 = ek_set_insert(s0, ek_val_ref(v));
    ek_val list = ek_set_to_list(s1);

    EK_CHECK_U64(v->rc, 3); /* the caller's, the table's, the list's */
    EK_CHECK_U64(ek_list_len(list), 1);
    ek_release(list);
    EK_CHECK_U64(v->rc, 2);
    ek_release(s1);
    ek_release(s0);
    ek_release(ek_val_ref(v));
}

static void of_over_the_empty_list_is_empty(void)
{
    ek_val s = ek_set_of(ek_nil_val());

    EK_CHECK_U64(ek_set_len(s), 0);
    ek_release(s);
}

int main(void)
{
    empty_set();
    insert_then_contains();
    inserting_a_present_element_yields_the_original();
    of_dedups_keeping_the_first_occurrence();
    string_elements_key_on_their_bytes();
    string_and_bytes_stay_distinct();
    bytes_elements_key_on_their_bytes();
    elements_differ_across_tags();
    float_elements_follow_ieee_equality();
    remove_keeps_the_remaining_order();
    removing_an_absent_element_yields_the_original();
    order_survives_growth();
    to_list_borrows_nothing();
    of_over_the_empty_list_is_empty();
    return ek_test_report("rt/set");
}
