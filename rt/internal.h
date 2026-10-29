#ifndef EK_RT_INTERNAL_H
#define EK_RT_INTERNAL_H

#include <stddef.h>
#include <stdlib.h>

#include "ekort.h"

/* Shared declarations inside the runtime. Not part of the public
   surface and not visible to compiled programs. */

/* Static typeinfos of the runtime-owned families. A family's drop
   lands with its implementation; NULL means the family holds no child
   references. */
extern const ek_typeinfo ek_typeinfo_str;
extern const ek_typeinfo ek_typeinfo_bytes;
extern const ek_typeinfo ek_typeinfo_cons;
extern const ek_typeinfo ek_typeinfo_dict;
extern const ek_typeinfo ek_typeinfo_closure;
extern const ek_typeinfo ek_typeinfo_nil;

/* Drops of the families that hold child references. The release path
   calls them at the last reference; each releases its children with
   constant C stack, so long chains and wide tables unwind without
   recursion. */
void ek_cons_drop(ek_obj *obj);
void ek_dict_drop(ek_obj *obj);
void ek_closure_drop(ek_obj *obj);

/* Steps one count down and returns the count after the step. A zero
   result hands the reclamation to the caller, which releases the
   children and frees the storage; the list spine release uses it to
   consume a chain of cells in constant stack. Ordinary releases go
   through ek_release. */
unsigned long ek_rc_dec(ek_obj *obj);

/* The length of the sequence starting at p, or 0 when the bytes do
   not form one code point within left bytes: overlong forms,
   surrogate encodings, code points above the maximum, and truncated
   tails are invalid. Shared by the string and bytes decoders. */
size_t ek_utf8_seq_len(const unsigned char *p, size_t left);

/* Writes the UTF-8 encoding of code into out and returns its length.
   The code must be a scalar value; the caller checks the domain. */
size_t ek_utf8_encode(uint32_t code, char *out);

/* Zeroed storage of the given size, or an abort: memory exhaustion
   ends the process, it is not a panic. */
static inline void *ek_rt_alloc_zeroed(size_t size)
{
    void *p = calloc(1, size);

    if (p == NULL)
        ek_abort("out of memory");
    return p;
}

/* The immortal empty-list object: static storage, born with one count,
   never dropped and never freed. */
ek_obj *ek_nil_obj(void);

#endif
