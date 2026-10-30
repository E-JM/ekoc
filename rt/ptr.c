#include <errno.h>

#include "internal.h"

/* The C namespace's machine vocabulary: the opaque pointer object and
   the errno reader. */

/* The wrapper object: header plus the carried pointer. The object
   holds no child references and owns no storage. */
typedef struct {
    ek_obj obj;
    void *p;
} ek_ptr;

static const ek_typeinfo ek_typeinfo_ptr = { EK_KIND_STRUCT, NULL };

ek_val ek_ptr_wrap(void *p)
{
    ek_ptr *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = &ek_typeinfo_ptr;
    o->obj.rc = 1;
    o->p = p;
    return ek_val_ref(&o->obj);
}

void *ek_ptr_unwrap(ek_val v)
{
    const ek_ptr *o = (const ek_ptr *)v.as.ref;

    return o->p;
}

/* The C thread-local errno: program-internal state, so the reader
   sets no world bit. */
int ek_get_errno(void)
{
    return errno;
}
