#include "internal.h"

/* The time companion's wall-clock point, built from its nanosecond
   payload, and the payload read back out. One family per object: the
   emitted unit names the typeinfo of the families it constructs, so
   this object names Time's alone. */

ek_val ek_gen_time(int64_t ns)
{
    ek_gen_p0_shape *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = &ekT_time$Time$Time;
    o->obj.rc = 1;
    o->p0 = ek_val_int(ns);
    return ek_val_ref(&o->obj);
}

int64_t ek_gen_ns(ek_val v)
{
    if (!ek_val_is_ref(v)) {
        return 0;
    }
    return ((const ek_gen_p0_shape *)v.as.ref)->p0.as.i;
}
