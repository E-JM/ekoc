#include "harness.h"

#include <stddef.h>
#include <stdlib.h>

#include "ekort.h"

/* The structural keying hooks: a family whose typeinfo carries eq and
   hash keys dictionaries and sets structurally, so equal-but-distinct
   objects are one key, unequal ones hash apart, and the identity holds
   across every lookup within the run. A family without hooks keeps the
   address rule, and a key of another family never matches. The text
   families carry the hooks over their bytes. */

/* Two fake families over the same two-slot layout: the hooks compare
   and mix the slots, the identity separating the families stays with
   the typeinfo pointer. */
typedef struct {
    ek_obj obj;
    ek_val x;
    ek_val y;
} ek_pair;

static const ek_typeinfo pair_a_info;
static const ek_typeinfo pair_b_info;

static int pair_eq(ek_val a, ek_val b)
{
    const ek_pair *pa = (const ek_pair *)a.as.ref;
    const ek_pair *pb = (const ek_pair *)b.as.ref;

    return a.as.ref->info == b.as.ref->info
        && pa->x.as.i == pb->x.as.i
        && pa->y.as.i == pb->y.as.i;
}

static unsigned long pair_hash(ek_val v)
{
    const ek_pair *p = (const ek_pair *)v.as.ref;
    unsigned long h = 14695981039346656037ULL;

    h = (h ^ (unsigned long)v.as.ref->info) * 1099511628211ULL;
    h = (h ^ (unsigned long)p->x.as.i) * 1099511628211ULL;
    h = (h ^ (unsigned long)p->y.as.i) * 1099511628211ULL;
    return h;
}

static const ek_typeinfo pair_a_info = { EK_KIND_STRUCT, NULL,
                                         pair_eq, pair_hash };
static const ek_typeinfo pair_b_info = { EK_KIND_STRUCT, NULL,
                                         pair_eq, pair_hash };
static const ek_typeinfo plain_info = { EK_KIND_STRUCT, NULL, NULL, NULL };

/* A pair object the caller owns and releases with one reference. */
static ek_val pair_of(const ek_typeinfo *info, int64_t x, int64_t y)
{
    ek_pair *p = calloc(1, sizeof *p);

    p->obj.info = info;
    p->obj.rc = 1;
    p->x = ek_val_int(x);
    p->y = ek_val_int(y);
    return ek_val_ref(&p->obj);
}

static void hooks_answer_over_the_layout(void)
{
    ek_val a = pair_of(&pair_a_info, 1, 2);
    ek_val b = pair_of(&pair_a_info, 1, 2);
    ek_val c = pair_of(&pair_a_info, 1, 3);
    unsigned long ha = a.as.ref->info->hash(a);
    unsigned long hb = b.as.ref->info->hash(b);

    EK_CHECK(a.as.ref != b.as.ref);
    EK_CHECK(pair_eq(a, b));
    EK_CHECK(!pair_eq(a, c));
    EK_CHECK_U64(ha, hb); /* equal values hash equal */
    EK_CHECK(pair_hash(a) == ha); /* the identity holds over the run */
    EK_CHECK_U64(pair_hash(a) == pair_hash(c), 0);
    EK_CHECK(pair_eq(a, a));
    ek_release(a);
    ek_release(b);
    ek_release(c);
}

static void dict_keys_structurally(void)
{
    ek_val d0 = ek_dict_new();
    ek_val a = pair_of(&pair_a_info, 4, 5);
    ek_val b = pair_of(&pair_a_info, 4, 5);
    ek_val c = pair_of(&pair_a_info, 4, 6);
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(7));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(9));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d1), 1);
    EK_CHECK_U64(ek_dict_len(d2), 1); /* b overwrote a */
    EK_CHECK(ek_dict_get(d2, b, &out));
    EK_CHECK_U64(out.as.i, 9);
    EK_CHECK(ek_dict_get(d2, a, &out)); /* the same key either spelling */
    EK_CHECK_U64(out.as.i, 9);
    EK_CHECK(!ek_dict_get(d2, c, &out));
    EK_CHECK_U64(a.as.ref->rc, 3); /* the caller's, d1's and d2's */
    ek_release(d2);
    EK_CHECK_U64(a.as.ref->rc, 2); /* d1 keeps its own reference */
    ek_release(d1);
    EK_CHECK_U64(a.as.ref->rc, 1);
    ek_release(d0);
    ek_release(a);
    ek_release(b);
    ek_release(c);
}

static void dict_distinct_keys_coexist(void)
{
    ek_val a = pair_of(&pair_a_info, 1, 1);
    ek_val b = pair_of(&pair_a_info, 2, 2);
    ek_val d0 = ek_dict_new();
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(2));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d2), 2);
    EK_CHECK(ek_dict_get(d2, a, &out) && out.as.i == 1);
    EK_CHECK(ek_dict_get(d2, b, &out) && out.as.i == 2);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(a);
    ek_release(b);
}

/* The typeinfo pointer is the precheck: two families over one layout
   stay apart even when the fields read equal. */
static void dict_separates_the_families(void)
{
    ek_val a = pair_of(&pair_a_info, 3, 4);
    ek_val b = pair_of(&pair_b_info, 3, 4);
    ek_val d0 = ek_dict_new();
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(2));
    ek_val out = ek_val_unit();

    EK_CHECK(!pair_eq(a, b));
    EK_CHECK_U64(ek_dict_len(d2), 2);
    EK_CHECK(ek_dict_get(d2, a, &out) && out.as.i == 1);
    EK_CHECK(ek_dict_get(d2, b, &out) && out.as.i == 2);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(a);
    ek_release(b);
}

/* A family without hooks keys on the object's address, the rule the
   hookless families always applied. */
static void dict_keeps_the_address_rule_without_hooks(void)
{
    ek_val a = pair_of(&plain_info, 8, 9);
    ek_val b = pair_of(&plain_info, 8, 9);
    ek_val d0 = ek_dict_new();
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(2));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d2), 2); /* distinct objects, two keys */
    EK_CHECK(ek_dict_get(d2, a, &out) && out.as.i == 1);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(a);
    ek_release(b);
}

static void set_keys_structurally(void)
{
    ek_val a = pair_of(&pair_a_info, 6, 7);
    ek_val b = pair_of(&pair_a_info, 6, 7);
    ek_val c = pair_of(&pair_a_info, 9, 9);
    ek_val s0 = ek_set_new();
    ek_val s1 = ek_set_insert(s0, a);
    ek_val s2 = ek_set_insert(s1, b);
    ek_val s3 = ek_set_insert(s2, c);
    ek_val other = pair_of(&pair_b_info, 6, 7);

    EK_CHECK_U64(ek_set_len(s1), 1);
    EK_CHECK_U64(ek_set_len(s2), 1); /* b is a */
    EK_CHECK(ek_set_contains(s2, a));
    EK_CHECK(ek_set_contains(s2, b));
    EK_CHECK_U64(ek_set_len(s3), 2);
    EK_CHECK(!ek_set_contains(s3, other)); /* another family never */
    EK_CHECK_U64(a.as.ref->rc, 3); /* the caller's, s1's and s3's */
    ek_release(s3);
    EK_CHECK_U64(a.as.ref->rc, 2);
    ek_release(s2); /* s2 is s1: the no-op insert returned the original */
    EK_CHECK_U64(a.as.ref->rc, 2);
    ek_release(s1);
    EK_CHECK_U64(a.as.ref->rc, 1);
    ek_release(s0);
    ek_release(other);
    ek_release(a);
    ek_release(b);
    ek_release(c);
}

static void set_dedups_over_the_hooks(void)
{
    ek_val a = pair_of(&pair_a_info, 1, 2);
    ek_val b = pair_of(&pair_a_info, 1, 2);
    ek_val c = pair_of(&pair_a_info, 1, 3);
    ek_val other = pair_of(&pair_b_info, 1, 2);
    ek_val list = ek_cons(a, ek_cons(b, ek_cons(c, ek_nil_val())));
    ek_val s = ek_set_of(list);
    ek_val less = ek_set_remove(s, b);

    EK_CHECK_U64(ek_set_len(s), 2); /* the first occurrence keeps */
    EK_CHECK(ek_set_contains(s, b));
    EK_CHECK(!ek_set_contains(s, other));
    EK_CHECK(less.as.ref != s.as.ref);
    EK_CHECK_U64(ek_set_len(less), 1);
    ek_release(less);
    ek_release(s);
    ek_release(list);
    ek_release(other);
    ek_release(a);
    ek_release(b);
    ek_release(c);
}

/* The text hooks: two spellings of one text are one key. */
static void text_keys_stay_byte_based(void)
{
    ek_obj *k1 = ek_str_from_cstr("key");
    ek_obj *k2 = ek_str_from_cstr("key");
    ek_val d0 = ek_dict_new();
    ek_val d1 = ek_dict_set(d0, ek_val_ref(k1), ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, ek_val_ref(k2), ek_val_int(2));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d2), 1);
    EK_CHECK(ek_dict_get(d2, ek_val_ref(k2), &out));
    EK_CHECK_U64(out.as.i, 2);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(ek_val_ref(k1));
    ek_release(ek_val_ref(k2));
}

/* The text hooks on a set: two spellings of one text are one element,
   and a String and a Bytes value over one byte sequence stay apart. */
static void set_text_hooks(void)
{
    ek_obj *t1 = ek_str_from_cstr("k");
    ek_obj *t2 = ek_str_from_cstr("k");
    ek_obj *b = ek_bytes_new("k", 1);
    ek_val s0 = ek_set_new();
    ek_val s1 = ek_set_insert(s0, ek_val_ref(t1));
    ek_val s2 = ek_set_insert(s1, ek_val_ref(t2));
    ek_val s3 = ek_set_insert(s2, ek_val_ref(b));

    EK_CHECK_U64(ek_set_len(s2), 1);
    EK_CHECK_U64(ek_set_len(s3), 2); /* Bytes stays a family apart */
    ek_release(s3);
    ek_release(s2);
    ek_release(s1);
    ek_release(s0);
    ek_release(ek_val_ref(t1));
    ek_release(ek_val_ref(t2));
    ek_release(ek_val_ref(b));
}

/* The Float key rules the element domain pins: 0.0 and -0.0 are one
   element and a NaN element never matches, not even itself. */
static void set_float_elements_follow_ieee_equality(void)
{
    ek_val s0 = ek_set_new();
    ek_val s1 = ek_set_insert(s0, ek_val_float(0.0));
    ek_val s2 = ek_set_insert(s1, ek_val_float(-0.0));
    ek_val s3 = ek_set_insert(s2, ek_val_float(1.5));
    ek_val s4 = ek_set_insert(s3, ek_val_float(0.0 / 0.0));

    EK_CHECK_U64(ek_set_len(s2), 1); /* -0.0 found 0.0 */
    EK_CHECK(ek_set_contains(s2, ek_val_float(-0.0)));
    EK_CHECK_U64(ek_set_len(s4), 3);
    EK_CHECK(!ek_set_contains(s4, ek_val_float(0.0 / 0.0)));
    ek_release(s4);
    ek_release(s3);
    ek_release(s2);
    ek_release(s1);
    ek_release(s0);
}

int main(void)
{
    hooks_answer_over_the_layout();
    dict_keys_structurally();
    dict_distinct_keys_coexist();
    dict_separates_the_families();
    dict_keeps_the_address_rule_without_hooks();
    set_keys_structurally();
    set_dedups_over_the_hooks();
    text_keys_stay_byte_based();
    set_text_hooks();
    set_float_elements_follow_ieee_equality();
    return ek_test_report("rt/hooks");
}
