#include "harness.h"

#include <stddef.h>

#include "ekort.h"

/* The dictionary traversals: native loops over the build-order chain,
   so they visit insertion order with constant C stack. The fold's
   accumulator is borrowed -- the caller's reference survives -- and
   the returned accumulator is the caller's to release. The callbacks
   receive the key first and the value second, the fold's accumulator
   ahead of both. */

/* The callback bodies. The sum adds the accumulator, the key, and the
   value; the probes record what arrived and in which order. */

static ek_val sum_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)argc;
    return ek_val_int(env.as.i + args[0].as.i + args[1].as.i
                      + args[2].as.i);
}

static ek_val keep_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)argc;
    ek_retain(args[0]);
    return args[0];
}

/* The two-argument probe: the pair as it arrived, and how many times. */
static int64_t pair_keys[8];
static int64_t pair_vals[8];
static size_t pair_count;

static ek_val pair_probe_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)argc;

    if (pair_count < 8) {
        pair_keys[pair_count] = args[0].as.i;
        pair_vals[pair_count] = args[1].as.i;
    }
    pair_count++;
    return ek_val_unit();
}

static void reset_probe(void)
{
    pair_count = 0;
}

static ek_val int_dict(const int64_t *keys, const int64_t *vals, size_t n)
{
    ek_val d = ek_dict_new(ek_key_eq, ek_key_hash);
    size_t i;

    for (i = 0; i < n; i++) {
        ek_val next = ek_dict_set(d, ek_val_int(keys[i]),
                                  ek_val_int(vals[i]));

        ek_release(d);
        d = next;
    }
    return d;
}

static void fold_accumulates_in_insertion_order(void)
{
    static const int64_t keys[3] = {1, 2, 3};
    static const int64_t vals[3] = {10, 20, 30};
    ek_val d = int_dict(keys, vals, 3);
    ek_val f = ek_closure_new(sum_fn, ek_val_int(0));
    ek_val z = ek_dict_fold(d, ek_val_int(100), f);

    EK_CHECK_U64(z.as.i, 100 + (1 + 10) + (2 + 20) + (3 + 30));
    ek_release(z);
    ek_release(f);
    ek_release(d);
}

/* The borrow rule: the seed reference stays the caller's, and the
   result comes back separately owned, even over an empty dictionary
   where no step runs. */
static void fold_borrows_the_accumulator(void)
{
    ek_obj *seed = ek_str_from_cstr("acc");
    ek_val d = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val f = ek_closure_new(keep_fn, ek_val_unit());
    ek_val z = ek_dict_fold(d, ek_val_ref(seed), f);

    EK_CHECK(z.as.ref == seed);
    EK_CHECK_U64(seed->rc, 2); /* the caller's and the fold's */
    ek_release(z);
    EK_CHECK_U64(seed->rc, 1);
    ek_release(f);
    ek_release(d);
    ek_release(ek_val_ref(seed));
}

/* The fold's order pin over string keys: the callback appends each key
   to the accumulator, so the result spells the insertion order. */
static ek_val append_key_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)argc;
    return ek_val_ref(ek_str_concat(args[0].as.ref, args[1].as.ref));
}

static void fold_gathers_string_keys_in_order(void)
{
    ek_obj *kb = ek_str_from_cstr("b");
    ek_obj *ka = ek_str_from_cstr("a");
    ek_obj *empty = ek_str_from_cstr("");
    ek_val d0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val d1 = ek_dict_set(d0, ek_val_ref(kb), ek_val_int(1));
    ek_val d = ek_dict_set(d1, ek_val_ref(ka), ek_val_int(2));
    ek_val f = ek_closure_new(append_key_fn, ek_val_unit());
    ek_val z = ek_dict_fold(d, ek_val_ref(empty), f);

    EK_CHECK_STR(ek_str_cstr(z.as.ref), "ba");
    ek_release(z);
    ek_release(f);
    ek_release(d);
    ek_release(d1);
    ek_release(d0);
    ek_release(ek_val_ref(empty));
    ek_release(ek_val_ref(ka));
    ek_release(ek_val_ref(kb));
}

static void each_visits_key_then_value_in_order(void)
{
    static const int64_t keys[3] = {5, -3, 9};
    static const int64_t vals[3] = {50, -30, 90};
    ek_val d = int_dict(keys, vals, 3);
    ek_val f = ek_closure_new(pair_probe_fn, ek_val_unit());
    ek_val u;
    size_t i;

    reset_probe();
    u = ek_dict_each(d, f);
    EK_CHECK(ek_val_is_unit(u));
    EK_CHECK_U64(pair_count, 3);
    for (i = 0; i < 3; i++) {
        EK_CHECK_U64(pair_keys[i], keys[i]);
        EK_CHECK_U64(pair_vals[i], vals[i]);
    }
    ek_release(u);
    ek_release(f);
    ek_release(d);
}

static void each_over_the_empty_dictionary_visits_nothing(void)
{
    ek_val d = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val f = ek_closure_new(pair_probe_fn, ek_val_unit());
    ek_val u;

    reset_probe();
    u = ek_dict_each(d, f);
    EK_CHECK(ek_val_is_unit(u));
    EK_CHECK_U64(pair_count, 0);
    ek_release(u);
    ek_release(f);
    ek_release(d);
}

/* The map's callback doubles every value; the keys, their order, and
   the receiver stay as they were. */
static ek_val double_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)argc;
    return ek_val_int(args[1].as.i * 2);
}

static ek_val even_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)argc;
    return ek_bool(args[1].as.i % 2 == 0);
}

static ek_val true_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)args;
    (void)argc;
    return ek_bool(1);
}

static ek_val mapped_get(ek_val d, int64_t key)
{
    ek_val out;

    return ek_dict_get(d, ek_val_int(key), &out) ? out : ek_val_unit();
}

static void map_builds_a_fresh_dictionary_in_order(void)
{
    static const int64_t keys[3] = {7, 1, 9};
    ek_val d = int_dict(keys, (const int64_t[]){1, 2, 3}, 3);
    ek_val f = ek_closure_new(double_fn, ek_val_unit());
    ek_val m = ek_dict_map(d, f);

    EK_CHECK_U64(ek_dict_len(m), 3);
    EK_CHECK_U64(mapped_get(m, 7).as.i, 2);
    EK_CHECK_U64(mapped_get(m, 1).as.i, 4);
    EK_CHECK_U64(mapped_get(m, 9).as.i, 6);
    EK_CHECK_U64(mapped_get(d, 7).as.i, 1); /* the receiver is untouched */
    ek_release(m);
    ek_release(f);
    ek_release(d);
}

static void map_keeps_the_insertion_order(void)
{
    static const int64_t keys[3] = {5, -3, 9};
    ek_val d = int_dict(keys, keys, 3);
    ek_val f = ek_closure_new(keep_fn, ek_val_unit());
    ek_val m = ek_dict_map(d, f);
    size_t cursor = 0;
    ek_val key;
    ek_val val;

    EK_CHECK(ek_dict_next(m, &cursor, &key, &val));
    EK_CHECK_U64(key.as.i, 5);
    EK_CHECK(ek_dict_next(m, &cursor, &key, &val));
    EK_CHECK_U64(key.as.i, -3);
    EK_CHECK(ek_dict_next(m, &cursor, &key, &val));
    EK_CHECK_U64(key.as.i, 9);
    EK_CHECK(!ek_dict_next(m, &cursor, &key, &val));
    ek_release(m);
    ek_release(f);
    ek_release(d);
}

static void filter_keeps_the_true_mappings_in_order(void)
{
    static const int64_t keys[4] = {1, 2, 3, 4};
    ek_val d = int_dict(keys, (const int64_t[]){1, 2, 3, 4}, 4);
    ek_val f = ek_closure_new(even_fn, ek_val_unit());
    ek_val r = ek_dict_filter(d, f);
    size_t cursor = 0;
    ek_val key;
    ek_val val;

    EK_CHECK_U64(ek_dict_len(r), 2);
    EK_CHECK(ek_dict_next(r, &cursor, &key, &val));
    EK_CHECK_U64(key.as.i, 2);
    EK_CHECK(ek_dict_next(r, &cursor, &key, &val));
    EK_CHECK_U64(key.as.i, 4);
    EK_CHECK(!ek_dict_next(r, &cursor, &key, &val));
    ek_release(r);
    ek_release(f);
    ek_release(d);
}

static void filter_keeping_everything_preserves_the_pairs(void)
{
    static const int64_t keys[2] = {1, 2};
    ek_val d = int_dict(keys, keys, 2);
    ek_val f = ek_closure_new(true_fn, ek_val_unit());
    ek_val r = ek_dict_filter(d, f);
    ek_val out;

    EK_CHECK_U64(ek_dict_len(r), 2);
    EK_CHECK(ek_dict_get(r, ek_val_int(1), &out));
    EK_CHECK_U64(out.as.i, 1);
    EK_CHECK(ek_dict_get(r, ek_val_int(2), &out));
    EK_CHECK_U64(out.as.i, 2);
    ek_release(r);
    ek_release(f);
    ek_release(d);
}

int main(void)
{
    fold_accumulates_in_insertion_order();
    fold_borrows_the_accumulator();
    fold_gathers_string_keys_in_order();
    each_visits_key_then_value_in_order();
    each_over_the_empty_dictionary_visits_nothing();
    map_builds_a_fresh_dictionary_in_order();
    map_keeps_the_insertion_order();
    filter_keeps_the_true_mappings_in_order();
    filter_keeping_everything_preserves_the_pairs();
    return ek_test_report("rt/dictfold");
}
