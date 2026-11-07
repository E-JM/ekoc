#ifndef EK_TESTS_RT_KEYFAM_H
#define EK_TESTS_RT_KEYFAM_H

#include "ekort.h"

#include <stdlib.h>

/* The fake key-domain families the hook suites share: two families
   over the same two-slot layout whose hooks compare and mix the slots,
   and a family without hooks whose keys fall to the object's address.
   A suite includes this once, and one suite is one unit. */

typedef struct {
    ek_obj obj;
    ek_val x;
    ek_val y;
} ek_pair;

int pair_eq(ek_val a, ek_val b)
{
    const ek_pair *pa = (const ek_pair *)a.as.ref;
    const ek_pair *pb = (const ek_pair *)b.as.ref;

    return a.as.ref->info == b.as.ref->info
        && pa->x.as.i == pb->x.as.i
        && pa->y.as.i == pb->y.as.i;
}

unsigned long pair_hash(ek_val v)
{
    const ek_pair *p = (const ek_pair *)v.as.ref;
    unsigned long h = 14695981039346656037ULL;

    h = (h ^ (unsigned long)v.as.ref->info) * 1099511628211ULL;
    h = (h ^ (unsigned long)p->x.as.i) * 1099511628211ULL;
    h = (h ^ (unsigned long)p->y.as.i) * 1099511628211ULL;
    return h;
}

const ek_typeinfo pair_a_info = { EK_KIND_STRUCT, NULL, pair_eq, pair_hash };
const ek_typeinfo pair_b_info = { EK_KIND_STRUCT, NULL, pair_eq, pair_hash };
const ek_typeinfo plain_info = { EK_KIND_STRUCT, NULL, NULL, NULL };

/* A pair object the caller owns and releases with one reference. */
ek_val pair_of(const ek_typeinfo *info, int64_t x, int64_t y)
{
    ek_pair *p = calloc(1, sizeof *p);

    p->obj.info = info;
    p->obj.rc = 1;
    p->x = ek_val_int(x);
    p->y = ek_val_int(y);
    return ek_val_ref(&p->obj);
}

#endif
