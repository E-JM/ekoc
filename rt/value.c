#include "internal.h"

#include <stddef.h>

/* The registry of the runtime's static typeinfos and the immortal nil
   singleton. Families that hold child references name the drop that
   lands with their implementation; the others carry none. */

const ek_typeinfo ek_typeinfo_str     = { EK_KIND_STR, NULL };
const ek_typeinfo ek_typeinfo_bytes   = { EK_KIND_BYTES, NULL };
const ek_typeinfo ek_typeinfo_cons    = { EK_KIND_CONS, ek_cons_drop };
const ek_typeinfo ek_typeinfo_dict    = { EK_KIND_DICT, ek_dict_drop };
const ek_typeinfo ek_typeinfo_set     = { EK_KIND_SET, ek_set_drop };
const ek_typeinfo ek_typeinfo_closure = { EK_KIND_CLOSURE, ek_closure_drop };
const ek_typeinfo ek_typeinfo_nil     = { EK_KIND_NIL, NULL };

/* The empty-list singleton: static storage, so the release path never
   reaches a free for it. */
static ek_obj nil_obj = { &ek_typeinfo_nil, 1 };

/* A value is exactly 16 bytes: the tag word and its payload. The
   generated code and the veneer compile against this layout. */
typedef char ek_val_is_16_bytes[sizeof(ek_val) == 16 ? 1 : -1];

ek_obj *ek_nil_obj(void)
{
    return &nil_obj;
}
