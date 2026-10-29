#include "harness.h"

#include <stddef.h>

#include "ekort.h"

/* The list family: cons cells retain their children, the empty list is
   the immortal singleton, append copies the spine and shares the tail,
   and the last release walks long spines in constant C stack. */

static void nil_is_the_empty_list(void)
{
    ek_val nil = ek_nil_val();

    EK_CHECK(ek_val_is_nil(nil));
    EK_CHECK(ek_val_is_ref(nil));
    EK_CHECK_U64(ek_list_len(nil), 0);
    EK_CHECK(!ek_val_is_nil(ek_val_int(0)));
    EK_CHECK(!ek_val_is_nil(ek_val_ref(NULL)));
}

static void cons_round_trips(void)
{
    ek_val cell = ek_cons(ek_val_int(1), ek_nil_val());

    EK_CHECK(!ek_val_is_nil(cell));
    EK_CHECK_U64(ek_list_len(cell), 1);
    EK_CHECK(ek_val_is_int(ek_cons_head(cell.as.ref)));
    EK_CHECK_U64(ek_cons_head(cell.as.ref).as.i, 1);
    EK_CHECK(ek_val_is_nil(ek_cons_tail(cell.as.ref)));
    ek_release(cell);
}

static void chain_walks_in_order(void)
{
    ek_val c3 = ek_cons(ek_val_int(3), ek_nil_val());
    ek_val c2 = ek_cons(ek_val_int(2), c3);
    ek_val c1 = ek_cons(ek_val_int(1), c2);
    ek_obj *cells[3];
    size_t i;

    ek_release(c3); /* the temporaries' birth references */
    ek_release(c2);
    EK_CHECK_U64(ek_list_len(c1), 3);
    cells[0] = c1.as.ref;
    for (i = 0; i < 3; i++) {
        EK_CHECK_U64(ek_cons_head(cells[i]).as.i, i + 1);
        if (i < 2)
            cells[i + 1] = ek_cons_tail(cells[i]).as.ref;
    }
    EK_CHECK(ek_val_is_nil(ek_cons_tail(cells[2])));
    ek_release(c1);
}

static void cell_retains_its_head(void)
{
    ek_obj *s = ek_str_from_cstr("head");
    ek_val cell = ek_cons(ek_val_ref(s), ek_nil_val());

    EK_CHECK_U64(s->rc, 2); /* the caller's and the cell's */
    ek_release(cell);
    EK_CHECK_U64(s->rc, 1); /* the cell's reference died with it */
    ek_release(ek_val_ref(s));
}

static void child_survives_the_container(void)
{
    ek_obj *s = ek_str_from_cstr("kept");
    ek_val inner = ek_cons(ek_val_ref(s), ek_nil_val());
    ek_val outer = ek_cons(ek_val_int(1), inner);

    ek_release(inner); /* the temporary's birth reference */
    EK_CHECK_U64(s->rc, 2);
    ek_release(outer); /* both cells go, the head stays */
    EK_CHECK_U64(s->rc, 1);
    ek_release(ek_val_ref(s));
}

static void child_dies_with_its_last_container(void)
{
    ek_obj *s = ek_str_from_cstr("sole");
    ek_val cell = ek_cons(ek_val_ref(s), ek_nil_val());

    ek_release(ek_val_ref(s)); /* the cell is the only owner now */
    EK_CHECK_U64(s->rc, 1);
    ek_release(cell); /* reclaims the cell and the string */
}

static void append_copies_the_spine_and_shares_the_tail(void)
{
    ek_val a_in = ek_cons(ek_val_int(2), ek_nil_val());
    ek_val a = ek_cons(ek_val_int(1), a_in);
    ek_val b = ek_cons(ek_val_int(3), ek_nil_val());
    ek_obj *a_last = a_in.as.ref;
    ek_val r;
    ek_obj *r_mid;

    ek_release(a_in); /* the temporary's birth reference: a owns it now */
    r = ek_list_append(a, b);
    r_mid = ek_cons_tail(r.as.ref).as.ref;

    EK_CHECK_U64(ek_list_len(r), 3);
    EK_CHECK_U64(ek_list_len(a), 2); /* a is untouched */
    EK_CHECK(r.as.ref != a.as.ref);  /* a fresh spine */
    EK_CHECK_U64(ek_cons_head(r.as.ref).as.i, 1);
    EK_CHECK_U64(ek_cons_head(r_mid).as.i, 2);
    EK_CHECK(ek_cons_tail(r_mid).as.ref == b.as.ref);
    EK_CHECK_U64(b.as.ref->rc, 2); /* the new spine owns b too */
    EK_CHECK_U64(a.as.ref->rc, 1); /* a's cells were copied, not shared */
    EK_CHECK_U64(a_last->rc, 1);   /* a's inner cell keeps its own count */
    ek_release(r);
    ek_release(a);
    ek_release(b);
}

static void append_shares_and_retains_the_heads(void)
{
    ek_obj *s = ek_str_from_cstr("x");
    ek_val a = ek_cons(ek_val_ref(s), ek_nil_val());
    ek_val r = ek_list_append(a, ek_nil_val());

    EK_CHECK_U64(s->rc, 3); /* caller, a's cell, the new spine's cell */
    ek_release(r);
    EK_CHECK_U64(s->rc, 2);
    ek_release(a);
    EK_CHECK_U64(s->rc, 1);
    ek_release(ek_val_ref(s));
}

static void append_onto_empty_yields_a_retained_tail(void)
{
    ek_val b = ek_cons(ek_val_int(7), ek_nil_val());
    ek_val r = ek_list_append(ek_nil_val(), b);

    EK_CHECK(r.as.ref == b.as.ref);
    EK_CHECK_U64(b.as.ref->rc, 2);
    EK_CHECK_U64(ek_list_len(r), 1);
    ek_release(r);
    ek_release(b);
}

static void spine_release_stops_at_a_surviving_tail(void)
{
    ek_val tail = ek_cons(ek_val_int(2), ek_nil_val());
    ek_val cell = ek_cons(ek_val_int(1), tail);

    EK_CHECK_U64(tail.as.ref->rc, 2); /* the caller's and the cell's */
    ek_release(cell); /* consumes its own cell, spares the tail */
    EK_CHECK_U64(tail.as.ref->rc, 1);
    EK_CHECK_U64(ek_list_len(tail), 1);
    ek_release(tail);
}

static void head_cascade_releases_a_nested_spine(void)
{
    ek_obj *s = ek_str_from_cstr("deep");
    ek_val inner = ek_cons(ek_val_ref(s), ek_nil_val());
    ek_val outer = ek_cons(inner, ek_nil_val());

    ek_release(inner); /* the temporary's birth reference */
    EK_CHECK_U64(s->rc, 2); /* the caller's and the inner cell's */
    ek_release(outer); /* the outer cell and the inner spine go */
    EK_CHECK_U64(s->rc, 1);
    ek_release(ek_val_ref(s));
}

/* Releasing a long spine must not recurse: the walk consumes the chain
   cell by cell, so the C stack stays flat. */
static void long_spine_release_is_iterative(void)
{
    ek_val list = ek_nil_val();
    int i;

    for (i = 0; i < 200000; i++) {
        ek_val next = ek_cons(ek_val_int(i), list);

        ek_release(list); /* the rebound value's reference */
        list = next;
    }
    EK_CHECK_U64(ek_list_len(list), 200000);
    ek_release(list); /* one walk consumes the whole spine */
}

int main(void)
{
    nil_is_the_empty_list();
    cons_round_trips();
    chain_walks_in_order();
    cell_retains_its_head();
    child_survives_the_container();
    child_dies_with_its_last_container();
    append_copies_the_spine_and_shares_the_tail();
    append_shares_and_retains_the_heads();
    append_onto_empty_yields_a_retained_tail();
    spine_release_stops_at_a_surviving_tail();
    head_cascade_releases_a_nested_spine();
    long_spine_release_is_iterative();
    return ek_test_report("rt/list");
}
