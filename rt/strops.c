#include "internal.h"

#include <stddef.h>
#include <string.h>

/* The string's scalar operations. Every entry walks the stored UTF-8
   bytes, which are always well formed: a scalar's lead byte tells its
   width, so the walk needs no validation. The scalar count and the
   byte count differ; only the byte count is O(1). */

/* The byte width of the scalar encoded at p, from its lead byte. */
static size_t scalar_step(const unsigned char *p)
{
    unsigned char b = p[0];

    if (b < 0xC0)
        return 1;
    if (b < 0xE0)
        return 2;
    if (b < 0xF0)
        return 3;
    return 4;
}

/* The code point encoded at p, gathering the continuation bytes. */
static uint32_t decode_scalar(const unsigned char *p)
{
    unsigned char b = p[0];

    if (b < 0x80)
        return b;
    if (b < 0xE0)
        return ((uint32_t)(b & 0x1F) << 6) | (p[1] & 0x3F);
    if (b < 0xF0)
        return ((uint32_t)(b & 0x0F) << 12) | ((p[1] & 0x3F) << 6)
            | (p[2] & 0x3F);
    return ((uint32_t)(b & 0x07) << 18) | ((p[1] & 0x3F) << 12)
        | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
}

size_t ek_utf8_encode(uint32_t code, char *out)
{
    if (code < 0x80) {
        out[0] = (char)code;
        return 1;
    }
    if (code < 0x800) {
        out[0] = (char)(0xC0 | (code >> 6));
        out[1] = (char)(0x80 | (code & 0x3F));
        return 2;
    }
    if (code < 0x10000) {
        out[0] = (char)(0xE0 | (code >> 12));
        out[1] = (char)(0x80 | ((code >> 6) & 0x3F));
        out[2] = (char)(0x80 | (code & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (code >> 18));
    out[1] = (char)(0x80 | ((code >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((code >> 6) & 0x3F));
    out[3] = (char)(0x80 | (code & 0x3F));
    return 4;
}

/* The byte width an encoded scalar will take. */
static size_t encode_width(uint32_t code)
{
    if (code < 0x80)
        return 1;
    if (code < 0x800)
        return 2;
    if (code < 0x10000)
        return 3;
    return 4;
}

size_t ek_str_length(ek_obj *obj)
{
    const unsigned char *p = (const unsigned char *)ek_str_cstr(obj);
    size_t len = ek_str_len(obj);
    size_t count = 0;
    size_t i = 0;

    while (i < len) {
        i += scalar_step(p + i);
        count++;
    }
    return count;
}

int ek_str_at(ek_obj *obj, int64_t index, uint32_t *code)
{
    const unsigned char *p = (const unsigned char *)ek_str_cstr(obj);
    size_t len = ek_str_len(obj);
    size_t i = 0;
    int64_t seen = 0;

    if (index < 0)
        return 0;
    while (i < len) {
        if (seen == index) {
            *code = decode_scalar(p + i);
            return 1;
        }
        i += scalar_step(p + i);
        seen++;
    }
    return 0;
}

ek_val ek_str_chars(ek_obj *obj)
{
    const unsigned char *p = (const unsigned char *)ek_str_cstr(obj);
    ek_val acc = ek_nil_val();
    size_t pos = ek_str_len(obj);

    /* The cells build back to front, so the walk runs from the last
       scalar to the first and each cons puts its head in front. The
       walk hands its own reference to the tail over to each new
       cell. */
    while (pos > 0) {
        size_t start = pos - 1;
        ek_val next;

        while (start > 0 && (p[start] & 0xC0) == 0x80)
            start--;
        next = ek_cons(ek_val_char(decode_scalar(p + start)), acc);
        ek_release(acc);
        acc = next;
        pos = start;
    }
    return acc;
}

ek_obj *ek_str_from_char(ek_val c)
{
    char buf[8];
    size_t len = 0;

    if (c.tag == EK_VAL_CHAR)
        len = ek_utf8_encode(c.as.c, buf);
    return ek_str_new(buf, len);
}

/* One pass over the cells sizes the bytes, the second writes them
   into the fresh string's own shape. */
ek_obj *ek_str_from_chars(ek_val chars)
{
    ek_val cur;
    size_t total = 0;
    char *buf;
    size_t fill = 0;
    ek_obj *out;

    for (cur = chars; ek_val_is_cons(cur); cur = ek_cons_tail(cur.as.ref)) {
        ek_val h = ek_cons_head(cur.as.ref);

        if (h.tag == EK_VAL_CHAR)
            total += encode_width(h.as.c);
    }
    buf = ek_rt_alloc_zeroed(total);
    for (cur = chars; ek_val_is_cons(cur); cur = ek_cons_tail(cur.as.ref)) {
        ek_val h = ek_cons_head(cur.as.ref);

        if (h.tag == EK_VAL_CHAR)
            fill += ek_utf8_encode(h.as.c, buf + fill);
    }
    out = ek_str_new(buf, total);
    free(buf);
    return out;
}

int ek_str_cmp(ek_obj *a, ek_obj *b)
{
    size_t na = ek_str_len(a);
    size_t nb = ek_str_len(b);
    size_t head = na < nb ? na : nb;
    int order = head == 0 ? 0 : memcmp(ek_str_cstr(a), ek_str_cstr(b), head);

    if (order != 0)
        return order < 0 ? -1 : 1;
    return na < nb ? -1 : na > nb ? 1 : 0;
}

int ek_str_lt(ek_obj *a, ek_obj *b)
{
    return ek_str_cmp(a, b) < 0;
}

int ek_str_le(ek_obj *a, ek_obj *b)
{
    return ek_str_cmp(a, b) <= 0;
}

int ek_str_gt(ek_obj *a, ek_obj *b)
{
    return ek_str_cmp(a, b) > 0;
}

int ek_str_ge(ek_obj *a, ek_obj *b)
{
    return ek_str_cmp(a, b) >= 0;
}
