#include "internal.h"

#include <stddef.h>
#include <string.h>

/* Bytes values are immutable byte sequences. The bytes live inside the
   object behind the length, with no terminator: the length carries the
   extent. Construction copies; there is no mutation operation. */

typedef struct {
    ek_obj obj;
    size_t len;
    unsigned char bytes[];
} ek_bytes;

ek_obj *ek_bytes_new(const void *data, size_t len)
{
    ek_bytes *bytes = ek_rt_alloc_zeroed(sizeof *bytes + len);

    bytes->obj.info = &ek_typeinfo_bytes;
    bytes->obj.rc = 1;
    bytes->len = len;
    if (len > 0)
        memcpy(bytes->bytes, data, len);
    return &bytes->obj;
}

size_t ek_bytes_len(ek_obj *obj)
{
    return ((const ek_bytes *)obj)->len;
}

const void *ek_bytes_data(ek_obj *obj)
{
    return ((const ek_bytes *)obj)->bytes;
}
