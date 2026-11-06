#include <stddef.h>

#include "internal.h"

/* The shared value builders: the zero-payload shape over the layout
   contract (the one-payload shape is in internal.h), and the std Result
   and Option constructors the capability closures and the document
   reader hand back. Every
   builder stamps the emitted unit's typeinfo, so generated pattern
   matches and drops treat the value as one of their own. A builder
   retains every reference it stores; the caller releases its own. */

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

ek_val ek_gen_result_ok(ek_val payload)
{
    return ek_gen_one(&ekT_std$Result$Ok, payload);
}

ek_val ek_gen_result_err(ek_val payload)
{
    return ek_gen_one(&ekT_std$Result$Err, payload);
}

ek_val ek_gen_option_some(ek_val payload)
{
    return ek_gen_one(&ekT_std$Option$Some, payload);
}

ek_val ek_gen_option_none(void)
{
    return ek_gen_zero(&ekT_std$Option$None);
}
