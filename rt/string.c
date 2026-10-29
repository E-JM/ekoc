#include "internal.h"

#include <stddef.h>
#include <string.h>

/* Strings are immutable UTF-8 byte sequences. The bytes live inside
   the object behind the length, with a NUL terminator stored after
   them, so the C-string view is zero-copy. Construction copies; no
   operation ever writes the stored bytes. */

typedef struct {
    ek_obj obj;
    size_t len;
    char bytes[];
} ek_str;

ek_obj *ek_str_new(const char *utf8, size_t len)
{
    ek_str *str = ek_rt_alloc_zeroed(sizeof *str + len + 1);

    str->obj.info = &ek_typeinfo_str;
    str->obj.rc = 1;
    str->len = len;
    if (len > 0)
        memcpy(str->bytes, utf8, len);
    str->bytes[len] = '\0';
    return &str->obj;
}

ek_obj *ek_str_from_cstr(const char *s)
{
    return ek_str_new(s, strlen(s));
}

size_t ek_str_len(ek_obj *obj)
{
    return ((const ek_str *)obj)->len;
}

const char *ek_str_cstr(ek_obj *obj)
{
    return ((const ek_str *)obj)->bytes;
}

ek_obj *ek_str_concat(ek_obj *a, ek_obj *b)
{
    const ek_str *sa = (const ek_str *)a;
    const ek_str *sb = (const ek_str *)b;
    ek_str *out = ek_rt_alloc_zeroed(sizeof *out + sa->len + sb->len + 1);

    out->obj.info = &ek_typeinfo_str;
    out->obj.rc = 1;
    out->len = sa->len + sb->len;
    if (sa->len > 0)
        memcpy(out->bytes, sa->bytes, sa->len);
    if (sb->len > 0)
        memcpy(out->bytes + sa->len, sb->bytes, sb->len);
    out->bytes[out->len] = '\0';
    return &out->obj;
}

int ek_str_eq(ek_obj *a, ek_obj *b)
{
    const ek_str *sa = (const ek_str *)a;
    const ek_str *sb = (const ek_str *)b;

    return sa->len == sb->len
        && (sa->len == 0 || memcmp(sa->bytes, sb->bytes, sa->len) == 0);
}

/* Nonzero when b is a continuation byte between lo and hi. */
static int in_range(unsigned char b, unsigned char lo, unsigned char hi)
{
    return b >= lo && b <= hi;
}

/* Checks the sequence starting at p against the encoding: returns its
   length when the bytes form one code point, or 0 when they do not.
   Overlong forms, surrogate encodings, code points above the maximum,
   and truncated tails are invalid. left is the bytes remaining. The
   contract with the other decoders is in internal.h. */
size_t ek_utf8_seq_len(const unsigned char *p, size_t left)
{
    unsigned char b = p[0];
    unsigned char lo;
    unsigned char hi;

    if (b < 0x80)
        return 1;
    if (left < 2 || b < 0xC2)
        return 0; /* truncated, stray, or overlong lead */
    if (b < 0xE0)
        return in_range(p[1], 0x80, 0xBF) ? 2 : 0;
    if (left < 3)
        return 0;
    if (b < 0xF0) {
        lo = 0x80;
        hi = 0xBF;
        if (b == 0xE0)
            lo = 0xA0; /* no overlong three-byte form */
        if (b == 0xED)
            hi = 0x9F; /* no surrogate encodings */
        return in_range(p[1], lo, hi) && in_range(p[2], 0x80, 0xBF) ? 3 : 0;
    }
    if (left < 4 || b > 0xF4)
        return 0;
    lo = 0x80;
    hi = 0xBF;
    if (b == 0xF0)
        lo = 0x90; /* no overlong four-byte form */
    if (b == 0xF4)
        hi = 0x8F; /* nothing above the maximum */
    return in_range(p[1], lo, hi) && in_range(p[2], 0x80, 0xBF)
        && in_range(p[3], 0x80, 0xBF) ? 4 : 0;
}

int ek_utf8_valid(const char *s, size_t len)
{
    const unsigned char *p = (const unsigned char *)s;
    size_t i = 0;

    while (i < len) {
        size_t n = ek_utf8_seq_len(p + i, len - i);

        if (n == 0)
            return 0;
        i += n;
    }
    return 1;
}
