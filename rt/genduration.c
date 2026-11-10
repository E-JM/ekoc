#include "internal.h"

/* The time companion's span, built from its nanosecond payload. One
   family per object: the emitted unit names the typeinfo of the
   families it constructs, so this object names Duration's alone. */

ek_val ek_gen_duration(int64_t ns)
{
    ek_gen_p0_shape *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = &ekT_time$Duration$Duration;
    o->obj.rc = 1;
    o->p0 = ek_val_int(ns);
    return ek_val_ref(&o->obj);
}
