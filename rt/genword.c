#include <stddef.h>

#include "internal.h"

/* The shared generic value builders over the layout contract. This
   object names no family's typeinfo: the callers pass their own, so a
   program pulling any one builder pulls none of the other families.
   A builder retains every reference it stores; the caller releases
   its own. */

/* The fieldless shape of a zero-payload constructor family. */
typedef struct {
    ek_obj obj;
} ek_gen_bare_shape;

/* A constructor payload sits in the first slot behind the header, the
   layout every emitted constructor struct follows. */
typedef char ek_gen_p0_slot
    [(offsetof(ek_gen_p0_shape, p0) == sizeof(ek_obj)) ? 1 : -1];

ek_val ek_gen_one(const ek_typeinfo *info, ek_val p0)
{
    ek_gen_p0_shape *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = info;
    o->obj.rc = 1;
    o->p0 = p0;
    ek_retain(p0);
    return ek_val_ref(&o->obj);
}

ek_val ek_gen_zero(const ek_typeinfo *info)
{
    ek_gen_bare_shape *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = info;
    o->obj.rc = 1;
    return ek_val_ref(&o->obj);
}
