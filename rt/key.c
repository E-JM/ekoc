#include "internal.h"

#include <string.h>

/* The default key discipline of the dictionary and set tables: the
   pair a container carries when its keys need no derived service. A
   String or a Bytes value keys its bytes, a scalar keys the tag and
   the payload, a Float keys IEEE equality with the two zeros reading
   as one and a NaN matching nothing, and a reference keys through the
   hooks its family carries, falling back to the object's address when
   the family has none. FNV-1a over the tag byte and the identifying
   bytes is the mixing step, so keys of different tags stay distinct
   even when their payloads coincide. */

/* The payload bytes that identify a scalar: a Char's code is four
   bytes wide, and every other payload fills the whole slot. */
static size_t payload_width(const ek_val *v)
{
    return v->tag == EK_VAL_CHAR ? sizeof v->as.c : sizeof v->as;
}

/* A String or Bytes value keys its bytes, not its address. */
static int content_key(const ek_val *v)
{
    if (v->tag != EK_VAL_REF || v->as.ref == NULL)
        return 0;
    return v->as.ref->info->kind == EK_KIND_STR
        || v->as.ref->info->kind == EK_KIND_BYTES;
}

/* The identifying bytes of a String or Bytes value. */
static void content_view(const ek_val *v, const unsigned char **p,
                         size_t *width)
{
    if (v->as.ref->info->kind == EK_KIND_STR) {
        *p = (const unsigned char *)ek_str_cstr(v->as.ref);
        *width = ek_str_len(v->as.ref);
    } else {
        *p = ek_bytes_data(v->as.ref);
        *width = ek_bytes_len(v->as.ref);
    }
}

/* Nonzero when the two content values key the same byte sequence of
   one family. */
static int content_eq(ek_val a, ek_val b)
{
    const unsigned char *pa, *pb;
    size_t wa, wb;

    if (a.as.ref->info->kind != b.as.ref->info->kind)
        return 0;
    content_view(&a, &pa, &wa);
    content_view(&b, &pb, &wb);
    return wa == wb && memcmp(pa, pb, wa) == 0;
}

unsigned long ek_key_hash(ek_val v)
{
    uint64_t h = 14695981039346656037ULL;
    const unsigned char *p = (const unsigned char *)&v.as;
    uint64_t bits = 0;
    size_t width;

    if (content_key(&v)) {
        content_view(&v, &p, &width);
    } else if (v.tag == EK_VAL_FLOAT) {
        /* the two zeros hash as one key, the way they compare */
        double zf = v.as.f == 0.0 ? 0.0 : v.as.f;

        memcpy(&bits, &zf, sizeof bits);
        p = (const unsigned char *)&bits;
        width = sizeof bits;
    } else if (v.tag == EK_VAL_REF && v.as.ref != NULL
               && v.as.ref->info->hash != NULL) {
        bits = (uint64_t)v.as.ref->info->hash(v);
        p = (const unsigned char *)&bits;
        width = sizeof bits;
    } else {
        /* a family without a hook keys on the object's address */
        width = payload_width(&v);
    }
    h = (h ^ (uint64_t)v.tag) * 1099511628211ULL;
    for (size_t i = 0; i < width; i++)
        h = (h ^ p[i]) * 1099511628211ULL;
    return (unsigned long)h;
}

int ek_key_eq(ek_val a, ek_val b)
{
    if (content_key(&a) || content_key(&b))
        return content_key(&a) && content_key(&b) && content_eq(a, b);
    if (a.tag != b.tag)
        return 0;
    if (a.tag == EK_VAL_FLOAT)
        return a.as.f == b.as.f;
    if (a.tag == EK_VAL_REF) {
        if (a.as.ref == NULL || b.as.ref == NULL)
            return a.as.ref == b.as.ref;
        if (a.as.ref->info == b.as.ref->info && a.as.ref->info->eq)
            return a.as.ref->info->eq(a, b);
        return a.as.ref == b.as.ref;
    }
    return memcmp(&a.as, &b.as, payload_width(&a)) == 0;
}
