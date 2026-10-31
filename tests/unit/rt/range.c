#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <stdint.h>
#include <stdlib.h>

#include "ekort.h"

/* The range traversals over the compiler's struct layout: the two
   endpoints and the closed flag ride as one mirror object, the loops
   visit in ascending order, and an empty span visits nothing. The
   panicking overflows and callback runs live in the forked-child
   suite. */

/* The mirror of the generated range struct, per the layout contract
   in rt/ekort.h. */
typedef struct {
    ek_obj obj;
    ek_val start;
    ek_val end;
    ek_val closed;
} test_range;

/* The mirror's drop: the three fields release like the generated
   families' do. */
static void test_range_drop(ek_obj *obj)
{
    test_range *r = (test_range *)obj;

    ek_release(r->start);
    ek_release(r->end);
    ek_release(r->closed);
}

static const ek_typeinfo test_range_info = { EK_KIND_STRUCT,
                                             test_range_drop, NULL, NULL };

/* Builds one range value over the endpoint values. */
static ek_val make_range(ek_val start, ek_val end, int closed)
{
    test_range *r = calloc(1, sizeof *r);

    r->obj.info = &test_range_info;
    r->obj.rc = 1;
    r->start = start;
    r->end = end;
    r->closed = ek_bool(closed);
    return ek_val_ref(&r->obj);
}

static ek_val int_range(int64_t a, int64_t b, int closed)
{
    return make_range(ek_val_int(a), ek_val_int(b), closed);
}

static ek_val uint_range(uint64_t a, uint64_t b, int closed)
{
    return make_range(ek_val_uint(a), ek_val_uint(b), closed);
}

static ek_val char_range(uint32_t a, uint32_t b, int closed)
{
    return make_range(ek_val_char(a), ek_val_char(b), closed);
}

/* The callback bodies of the traversals. */

static ek_val sum_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)argc;
    return ek_val_int(env.as.i + args[0].as.i + args[1].as.i);
}

static ek_val dbl_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)argc;
    return ek_val_int(args[0].as.i * 2);
}

static ek_val keep_uint_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)argc;
    return ek_val_uint(args[0].as.u);
}

static ek_val keep_char_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)argc;
    return ek_val_char(args[0].as.c);
}

/* The visit record the index callbacks write, reset per test. */
static int visits;
static int64_t last_index;

static ek_val index_probe_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)argc;
    visits++;
    last_index = args[1].as.i;
    return ek_val_unit();
}

static void reset_visits(void)
{
    visits = 0;
    last_index = -1;
}

/* The list's Int elements as a comma-spaced text, for the pinned
   forms. */
static void list_text(ek_val list, char *out, size_t cap)
{
    ek_val cell = list;
    size_t pos = 0;
    int first = 1;

    out[0] = '\0';
    while (ek_val_is_cons(cell) && pos + 24 < cap) {
        ek_val h = ek_cons_head(cell.as.ref);
        int n;

        n = snprintf(out + pos, cap - pos, first ? "%lld" : ", %lld",
                     (long long)h.as.i);
        if (n < 0) {
            return;
        }
        pos += (size_t)n;
        first = 0;
        cell = ek_cons_tail(cell.as.ref);
    }
}

static void length_counts_the_span(void)
{
    ek_val r = int_range(1, 9, 0);
    ek_val n = ek_range_length(r);
    ek_val m = ek_range_length(int_range(1, 9, 1));

    EK_CHECK(ek_val_is_int(n));
    EK_CHECK_U64(n.as.i, 8);
    EK_CHECK_U64(m.as.i, 9);
    ek_release(n);
    ek_release(m);
    ek_release(r);
}

static void length_of_uint_and_char(void)
{
    ek_val u = ek_range_length(uint_range(10, 13, 0));
    ek_val uc = ek_range_length(uint_range(10, 13, 1));
    ek_val c = ek_range_length(char_range('a', 'z', 0));

    EK_CHECK_U64(u.as.i, 3);
    EK_CHECK_U64(uc.as.i, 4);
    EK_CHECK_U64(c.as.i, 25);
    ek_release(u);
    ek_release(uc);
    ek_release(c);
}

static void empty_spans_count_zero(void)
{
    ek_val open = ek_range_length(int_range(5, 5, 0));
    ek_val reversed = ek_range_length(int_range(3, 1, 1));
    ek_val chars = ek_range_length(char_range('c', 'a', 0));

    EK_CHECK_U64(open.as.i, 0);
    EK_CHECK_U64(reversed.as.i, 0);
    EK_CHECK_U64(chars.as.i, 0);
    ek_release(open);
    ek_release(reversed);
    ek_release(chars);
}

static void to_list_visits_ascending(void)
{
    ek_val r = int_range(1, 4, 1);
    ek_val xs = ek_range_to_list(r);
    char text[128];

    list_text(xs, text, sizeof text);
    EK_CHECK_STR(text, "1, 2, 3, 4");
    EK_CHECK_U64(ek_list_len(xs), 4);
    ek_release(xs);
    ek_release(r);
}

static void to_list_of_the_empty_and_single_spans(void)
{
    ek_val open = ek_range_to_list(int_range(5, 5, 0));
    ek_val single = ek_range_to_list(int_range(5, 5, 1));
    ek_val reversed = ek_range_to_list(int_range(9, 1, 0));

    EK_CHECK(ek_val_is_nil(open));
    EK_CHECK(ek_val_is_cons(single));
    EK_CHECK_U64(ek_list_len(single), 1);
    EK_CHECK(ek_val_is_nil(reversed));
    ek_release(open);
    ek_release(single);
    ek_release(reversed);
}

static void to_list_of_char_and_uint(void)
{
    ek_val cs = ek_range_to_list(char_range('a', 'c', 1));
    ek_val us = ek_range_to_list(uint_range(2, 5, 1));
    ek_val cell = cs;

    EK_CHECK(ek_val_is_char(ek_cons_head(cell.as.ref)));
    EK_CHECK_U64(ek_cons_head(cell.as.ref).as.c, 'a');
    cell = ek_cons_tail(cell.as.ref);
    EK_CHECK_U64(ek_cons_head(cell.as.ref).as.c, 'b');
    cell = ek_cons_tail(cell.as.ref);
    EK_CHECK_U64(ek_cons_head(cell.as.ref).as.c, 'c');
    EK_CHECK(ek_val_is_nil(ek_cons_tail(cell.as.ref)));
    EK_CHECK(ek_val_is_uint(ek_cons_head(us.as.ref)));
    EK_CHECK_U64(ek_list_len(us), 4);
    ek_release(cs);
    ek_release(us);
}

static void map_doubles_every_element(void)
{
    ek_val f = ek_closure_new(dbl_fn, ek_val_int(0));
    ek_val r = int_range(1, 4, 1);
    ek_val ys = ek_range_map(r, f);
    char text[128];

    list_text(ys, text, sizeof text);
    EK_CHECK_STR(text, "2, 4, 6, 8");
    ek_release(ys);
    ek_release(r);
    ek_release(f);
}

static void map_over_char_elements(void)
{
    ek_val f = ek_closure_new(keep_char_fn, ek_val_unit());
    ek_val r = char_range('a', 'c', 1);
    ek_val xs = ek_range_map(r, f);
    ek_val cell = xs;

    EK_CHECK(ek_val_is_char(ek_cons_head(cell.as.ref)));
    EK_CHECK_U64(ek_cons_head(cell.as.ref).as.c, 'a');
    cell = ek_cons_tail(cell.as.ref);
    EK_CHECK_U64(ek_cons_head(cell.as.ref).as.c, 'b');
    cell = ek_cons_tail(cell.as.ref);
    EK_CHECK_U64(ek_cons_head(cell.as.ref).as.c, 'c');
    EK_CHECK(ek_val_is_nil(ek_cons_tail(cell.as.ref)));
    ek_release(xs);
    ek_release(r);
    ek_release(f);
}

static void map_over_the_unsigned_top(void)
{
    ek_val f = ek_closure_new(keep_uint_fn, ek_val_unit());
    ek_val r = uint_range(UINT64_MAX - 1, UINT64_MAX, 1);
    ek_val xs = ek_range_map(r, f);

    /* the successor discipline steps past the second-highest element
       and stops at the top without wrapping */
    EK_CHECK_U64(ek_list_len(xs), 2);
    EK_CHECK_U64(ek_cons_head(xs.as.ref).as.u, UINT64_MAX - 1);
    ek_release(xs);
    ek_release(r);
    ek_release(f);
}

static void fold_accumulates_left_to_right(void)
{
    ek_val f = ek_closure_new(sum_fn, ek_val_int(0));
    ek_val r = int_range(1, 5, 0);
    ek_val z = ek_range_fold(r, ek_val_int(100), f);

    EK_CHECK_U64(z.as.i, 110); /* 100 + 1 + 2 + 3 + 4 */
    ek_release(z);
    ek_release(r);
    ek_release(f);
}

static void fold_over_empty_keeps_the_seed(void)
{
    ek_val f = ek_closure_new(sum_fn, ek_val_int(0));
    ek_val r = int_range(7, 7, 0);
    ek_val z = ek_range_fold(r, ek_val_int(9), f);

    EK_CHECK_U64(z.as.i, 9);
    ek_release(z);
    ek_release(r);
    ek_release(f);
}

static void each_index_carries_the_position(void)
{
    ek_val f = ek_closure_new(index_probe_fn, ek_val_unit());
    ek_val r = int_range(10, 13, 1);

    reset_visits();
    ek_release(ek_range_each_index(r, f));
    EK_CHECK_U64(visits, 4);
    EK_CHECK_U64(last_index, 3);
    ek_release(r);

    r = int_range(10, 13, 0);
    reset_visits();
    ek_release(ek_range_each_index(r, f));
    EK_CHECK_U64(visits, 3);
    EK_CHECK_U64(last_index, 2);
    ek_release(r);
    ek_release(f);
}

int main(void)
{
    length_counts_the_span();
    length_of_uint_and_char();
    empty_spans_count_zero();
    to_list_visits_ascending();
    to_list_of_the_empty_and_single_spans();
    to_list_of_char_and_uint();
    map_doubles_every_element();
    map_over_char_elements();
    map_over_the_unsigned_top();
    fold_accumulates_left_to_right();
    fold_over_empty_keeps_the_seed();
    each_index_carries_the_position();
    return ek_test_report("rt/range");
}
