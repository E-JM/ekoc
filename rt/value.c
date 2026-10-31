#include "internal.h"

#include <stddef.h>
#include <string.h>

/* The registry of the runtime's static typeinfos and the immortal nil
   singleton. Families that hold child references name the drop that
   lands with their implementation; the others carry none. The text
   families carry the structural keying hooks over their bytes, so a
   dictionary or set keyed on them reads one spelling of a text as one
   key; the remaining runtime families carry no hooks and key on
   identity. */

/* FNV-1a over the identifying bytes. */
static unsigned long text_hash(const unsigned char *p, size_t n)
{
    unsigned long h = 14695981039346656037ULL;
    size_t i;

    for (i = 0; i < n; i++) {
        h = (h ^ p[i]) * 1099511628211ULL;
    }
    return h;
}

static int str_eq(ek_val a, ek_val b)
{
    return ek_str_eq(a.as.ref, b.as.ref);
}

static unsigned long str_hash(ek_val v)
{
    return text_hash((const unsigned char *)ek_str_cstr(v.as.ref),
                     ek_str_len(v.as.ref));
}

static int bytes_eq(ek_val a, ek_val b)
{
    size_t la = ek_bytes_len(a.as.ref);

    return la == ek_bytes_len(b.as.ref)
        && memcmp(ek_bytes_data(a.as.ref), ek_bytes_data(b.as.ref),
                  la) == 0;
}

static unsigned long bytes_hash(ek_val v)
{
    return text_hash(ek_bytes_data(v.as.ref), ek_bytes_len(v.as.ref));
}

const ek_typeinfo ek_typeinfo_str     = { EK_KIND_STR, NULL, str_eq,
                                          str_hash };
const ek_typeinfo ek_typeinfo_bytes   = { EK_KIND_BYTES, NULL, bytes_eq,
                                          bytes_hash };
const ek_typeinfo ek_typeinfo_cons    = { EK_KIND_CONS, ek_cons_drop, NULL,
                                          NULL };
const ek_typeinfo ek_typeinfo_dict    = { EK_KIND_DICT, ek_dict_drop, NULL,
                                          NULL };
const ek_typeinfo ek_typeinfo_set     = { EK_KIND_SET, ek_set_drop, NULL,
                                          NULL };
const ek_typeinfo ek_typeinfo_closure = { EK_KIND_CLOSURE, ek_closure_drop,
                                          NULL, NULL };
const ek_typeinfo ek_typeinfo_nil     = { EK_KIND_NIL, NULL, NULL, NULL };

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
