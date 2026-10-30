#include "internal.h"

#include <stdint.h>
#include <stdlib.h>

/* The Range members: the traversals run as native C loops over the
   two endpoints, so the stack usage of each/map/fold is independent
   of the element count. A range value is a compiler-emitted struct
   whose object header is followed by three ek_val fields in this
   order -- start, end, closed -- and this mirror is the shared
   layout contract with the generated struct families (the ekort.h
   entry block states it too). The closed field carries Bool, an Int
   of 0 or 1. */

typedef struct {
    ek_obj obj;
    ek_val start;
    ek_val end;
    ek_val closed;
} ek_range_obj;

/* The element domain of one range, decided by the endpoint tag. */
typedef enum {
    RG_INT,
    RG_UINT,
    RG_CHAR
} range_kind;

static range_kind kind_of(ek_val start)
{
    if (start.tag == EK_VAL_UINT)
        return RG_UINT;
    if (start.tag == EK_VAL_CHAR)
        return RG_CHAR;
    return RG_INT;
}

/* The endpoints and flag of the range value, borrowed. */
static void range_fields(ek_val range, ek_val *start, ek_val *end,
                         ek_val *closed)
{
    const ek_range_obj *r = (const ek_range_obj *)range.as.ref;

    *start = r->start;
    *end = r->end;
    *closed = r->closed;
}

/* The per-element body of a traversal: the element, its position,
   and the traversal's own state. */
typedef void (*range_visit)(void *u, ek_val v, int64_t i);

typedef void (*range_walk)(ek_val range, int closed, range_visit visit,
                           void *u);

/* The traversal of one Int range: the successor is only computed
   while the current element is below the end, so the increment can
   never overflow, and the inclusive form breaks at the end instead
   of stepping past it. */
static void walk_int(ek_val range, int closed, range_visit visit, void *u)
{
    ek_val s, e, c;
    int64_t a, b, i;

    range_fields(range, &s, &e, &c);
    a = s.as.i;
    b = e.as.i;
    if (closed) {
        if (a > b)
            return;
        for (i = a;; i++) {
            visit(u, ek_val_int(i), i - a);
            if (i == b)
                break;
        }
        return;
    }
    for (i = a; i < b; i++) {
        visit(u, ek_val_int(i), i - a);
    }
}

static void walk_uint(ek_val range, int closed, range_visit visit, void *u)
{
    ek_val s, e, c;
    uint64_t a, b, i;

    range_fields(range, &s, &e, &c);
    a = s.as.u;
    b = e.as.u;
    if (closed) {
        if (a > b)
            return;
        for (i = a;; i++) {
            visit(u, ek_val_uint(i), (int64_t)(i - a));
            if (i == b)
                break;
        }
        return;
    }
    for (i = a; i < b; i++) {
        visit(u, ek_val_uint(i), (int64_t)(i - a));
    }
}

static void walk_char(ek_val range, int closed, range_visit visit, void *u)
{
    ek_val s, e, c;
    uint32_t a, b, i;
    int64_t k = 0;

    range_fields(range, &s, &e, &c);
    a = s.as.c;
    b = e.as.c;
    if (closed) {
        if (a > b)
            return;
        for (i = a;; i++) {
            visit(u, ek_val_char(i), k);
            if (i == b)
                break;
            k++;
        }
        return;
    }
    for (i = a; i < b; i++) {
        visit(u, ek_val_char(i), k);
        k++;
    }
}

/* The walker of one range value, dispatched on the endpoint tag. */
static range_walk walker_of(ek_val range)
{
    ek_val s, e, c;

    range_fields(range, &s, &e, &c);
    switch (kind_of(s)) {
    case RG_UINT:
        return walk_uint;
    case RG_CHAR:
        return walk_char;
    default:
        return walk_int;
    }
}

/* The growable element buffer of a collecting traversal. */
typedef struct {
    ek_val *data;
    size_t len, cap;
} collector;

static void collect_add(collector *cl, ek_val v)
{
    if (cl->len == cl->cap) {
        size_t ncap = cl->cap ? cl->cap * 2 : 16;
        ek_val *fresh = realloc(cl->data, ncap * sizeof *fresh);

        if (!fresh)
            ek_abort("out of memory");
        cl->data = fresh;
        cl->cap = ncap;
    }
    cl->data[cl->len++] = v;
}

/* The list builder: the collected elements join a fresh spine from
   the right, so the walk stays constant-stack and each element's
   reference moves into its cell. */
static ek_val list_of(collector *cl)
{
    ek_val acc = ek_nil_val();
    size_t i = cl->len;

    while (i > 0) {
        ek_val cell;

        i--;
        cell = ek_cons(cl->data[i], acc);
        ek_release(acc);
        ek_release(cl->data[i]);
        acc = cell;
    }
    free(cl->data);
    cl->data = NULL;
    return acc;
}

/* The collecting bodies: the elements themselves, and the callback
   results. */

static void collect_plain_visit(void *u, ek_val v, int64_t i)
{
    (void)i;
    ek_retain(v);
    collect_add(u, v);
}

typedef struct {
    ek_val f;
    collector cl;
} mapper;

static void collect_map_visit(void *u, ek_val v, int64_t i)
{
    mapper *m = u;
    ek_val out;

    (void)i;
    out = ek_closure_call(m->f, &v, 1);
    collect_add(&m->cl, out);
}

/* The calling bodies: the result drops, and the fold's accumulator
   flows through the callback. */

static void each_visit(void *u, ek_val v, int64_t i)
{
    ek_val *f = u;
    ek_val out;

    (void)i;
    out = ek_closure_call(*f, &v, 1);
    ek_release(out);
}

static void each_index_visit(void *u, ek_val v, int64_t i)
{
    ek_val *f = u;
    ek_val args[2];
    ek_val out;

    args[0] = v;
    args[1] = ek_val_int(i);
    out = ek_closure_call(*f, args, 2);
    ek_release(out);
}

typedef struct {
    ek_val f;
    ek_val acc;
} folder;

static void fold_visit(void *u, ek_val v, int64_t i)
{
    folder *fd = u;
    ek_val args[2];
    ek_val out;

    (void)i;
    args[0] = fd->acc;
    args[1] = v;
    out = ek_closure_call(fd->f, args, 2);
    ek_release(fd->acc);
    fd->acc = out;
}

/* The count of the range's elements: empty when the start exceeds
   the end, otherwise the span widened by the closed flag. The span
   is computed through the language's checked arithmetic, so a count
   outside the Int domain panics like any other overflow. */
ek_val ek_range_length(ek_val range)
{
    ek_val s, e, c;
    int closed;

    if (!ek_val_is_ref(range)) {
        return ek_val_int(0);
    }
    range_fields(range, &s, &e, &c);
    closed = c.as.i != 0;
    if (kind_of(s) == RG_CHAR) {
        if (s.as.c > e.as.c) {
            return ek_val_int(0);
        }
        return ek_val_int((int64_t)(e.as.c - s.as.c) + (closed ? 1 : 0));
    }
    {
        int is_uint = kind_of(s) == RG_UINT;
        int empty = is_uint ? s.as.u > e.as.u : s.as.i > e.as.i;
        ek_val span;

        if (empty) {
            return ek_val_int(0);
        }
        span = ek_num_sub(e, s);
        if (closed) {
            span = ek_num_add(span, is_uint ? ek_val_uint(1)
                                            : ek_val_int(1));
        }
        return span;
    }
}

/* The elements in ascending order as a fresh list. */
ek_val ek_range_to_list(ek_val range)
{
    ek_val s, e, c;
    collector cl;

    if (!ek_val_is_ref(range)) {
        return ek_nil_val();
    }
    range_fields(range, &s, &e, &c);
    cl.data = NULL;
    cl.len = 0;
    cl.cap = 0;
    walker_of(range)(range, c.as.i != 0, collect_plain_visit, &cl);
    return list_of(&cl);
}

/* The mapped results in ascending order as a fresh list. */
ek_val ek_range_map(ek_val range, ek_val f)
{
    ek_val s, e, c;
    mapper m;

    if (!ek_val_is_ref(range)) {
        return ek_nil_val();
    }
    range_fields(range, &s, &e, &c);
    m.f = f;
    m.cl.data = NULL;
    m.cl.len = 0;
    m.cl.cap = 0;
    walker_of(range)(range, c.as.i != 0, collect_map_visit, &m);
    return list_of(&m.cl);
}

/* The callback over every element in ascending order; the results
   are dropped. */
ek_val ek_range_each(ek_val range, ek_val f)
{
    ek_val s, e, c;

    if (!ek_val_is_ref(range)) {
        return ek_val_unit();
    }
    range_fields(range, &s, &e, &c);
    walker_of(range)(range, c.as.i != 0, each_visit, &f);
    return ek_val_unit();
}

/* The callback over every element with its position from zero. */
ek_val ek_range_each_index(ek_val range, ek_val f)
{
    ek_val s, e, c;

    if (!ek_val_is_ref(range)) {
        return ek_val_unit();
    }
    range_fields(range, &s, &e, &c);
    walker_of(range)(range, c.as.i != 0, each_index_visit, &f);
    return ek_val_unit();
}

/* The left-to-right fold: the accumulator flows through the callback,
   the previous one released once its successor exists. */
ek_val ek_range_fold(ek_val range, ek_val acc, ek_val f)
{
    ek_val s, e, c;

    if (!ek_val_is_ref(range)) {
        ek_release(acc);
        return ek_val_unit();
    }
    range_fields(range, &s, &e, &c);
    {
        folder fd;

        fd.f = f;
        fd.acc = acc;
        walker_of(range)(range, c.as.i != 0, fold_visit, &fd);
        return fd.acc;
    }
}
