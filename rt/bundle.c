#include "internal.h"

/* The shared bundle families: the header followed by the member
   closures, in the slot order ekort.h publishes per capability. The
   drops release every member with the record. Each struct carries its
   own compile-time tie to the published slot count. */

typedef struct {
    ek_obj obj;
    ek_val a;
} ek_bundle1;

typedef struct {
    ek_obj obj;
    ek_val f[EK_CLOCK_SLOTS];
} ek_bundle3;

typedef struct {
    ek_obj obj;
    ek_val f[EK_FS_SLOTS];
} ek_bundle16;

typedef char bundle1_slots
    [(sizeof(ek_bundle1) == sizeof(ek_obj) + sizeof(ek_val)) ? 1 : -1];
typedef char bundle3_slots
    [(sizeof(ek_bundle3) == sizeof(ek_obj) + EK_CLOCK_SLOTS * sizeof(ek_val)) ? 1 : -1];
typedef char bundle16_slots
    [(sizeof(ek_bundle16) == sizeof(ek_obj) + EK_FS_SLOTS * sizeof(ek_val)) ? 1 : -1];

static void bundle1_drop(ek_obj *obj)
{
    ek_release(((ek_bundle1 *)obj)->a);
}

static void bundle3_drop(ek_obj *obj)
{
    ek_bundle3 *b = (ek_bundle3 *)obj;
    size_t i;

    for (i = 0; i < EK_CLOCK_SLOTS; i++) {
        ek_release(b->f[i]);
    }
}

static void bundle16_drop(ek_obj *obj)
{
    ek_bundle16 *b = (ek_bundle16 *)obj;
    size_t i;

    for (i = 0; i < EK_FS_SLOTS; i++) {
        ek_release(b->f[i]);
    }
}

static const ek_typeinfo bundle1_info = { EK_KIND_STRUCT, bundle1_drop, NULL, NULL };
static const ek_typeinfo bundle3_info = { EK_KIND_STRUCT, bundle3_drop, NULL, NULL };
static const ek_typeinfo bundle16_info = { EK_KIND_STRUCT, bundle16_drop, NULL, NULL };

ek_val ek_bundle1_new(ek_val a)
{
    ek_bundle1 *b = ek_rt_alloc_zeroed(sizeof *b);

    b->obj.info = &bundle1_info;
    b->obj.rc = 1;
    b->a = a;
    return ek_val_ref(&b->obj);
}

/* Both multi-slot constructors take the slots as an array indexed by
   the published order, so the filling site cannot place a closure in
   the wrong slot without the spelling saying so. */

ek_val ek_bundle3_new(const ek_val *slots)
{
    ek_bundle3 *r = ek_rt_alloc_zeroed(sizeof *r);
    size_t i;

    r->obj.info = &bundle3_info;
    r->obj.rc = 1;
    for (i = 0; i < EK_CLOCK_SLOTS; i++) {
        r->f[i] = slots[i];
    }
    return ek_val_ref(&r->obj);
}

ek_val ek_bundle16_new(const ek_val *slots)
{
    ek_bundle16 *b = ek_rt_alloc_zeroed(sizeof *b);
    size_t i;

    b->obj.info = &bundle16_info;
    b->obj.rc = 1;
    for (i = 0; i < EK_FS_SLOTS; i++) {
        b->f[i] = slots[i];
    }
    return ek_val_ref(&b->obj);
}
