#ifndef EK_RT_INTERNAL_H
#define EK_RT_INTERNAL_H

#include "ekort.h"

/* Shared declarations inside the runtime. Not part of the public
   surface and not visible to compiled programs. */

/* Static typeinfos of the runtime-owned families. A family's drop
   lands with its implementation; NULL means the family holds no child
   references yet. */
extern const ek_typeinfo ek_typeinfo_str;
extern const ek_typeinfo ek_typeinfo_bytes;
extern const ek_typeinfo ek_typeinfo_cons;
extern const ek_typeinfo ek_typeinfo_dict;
extern const ek_typeinfo ek_typeinfo_closure;
extern const ek_typeinfo ek_typeinfo_nil;

/* The immortal empty-list object: static storage, born with one count,
   never dropped and never freed. */
ek_obj *ek_nil_obj(void);

#endif
