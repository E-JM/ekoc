#include "internal.h"

/* The time companion's wall-clock point, monotonic reading, and span,
   built from their nanosecond payloads, and the payload read back
   out. The values are built here over the published layout so a
   clock-only program links no status family. */

static ek_val time_value(const ek_typeinfo *info, int64_t ns)
{
    ek_gen_p0_shape *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = info;
    o->obj.rc = 1;
    o->p0 = ek_val_int(ns);
    return ek_val_ref(&o->obj);
}

ek_val ek_gen_time(int64_t ns)
{
    return time_value(&ekT_time$Time$Time, ns);
}

ek_val ek_gen_mono(int64_t ns)
{
    return time_value(&ekT_time$Mono$Mono, ns);
}

ek_val ek_gen_duration(int64_t ns)
{
    return time_value(&ekT_time$Duration$Duration, ns);
}

int64_t ek_gen_ns(ek_val v)
{
    if (!ek_val_is_ref(v)) {
        return 0;
    }
    return ((const ek_gen_p0_shape *)v.as.ref)->p0.as.i;
}
