#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The bytes operations: indexed access over the element domain Int in
   0..255, concatenation, the list bridges, and the text forms. The
   block shape lives in bytes.c; these entries go through its
   accessors. */

int ek_bytes_get(ek_obj *bytes, ek_val index, int64_t *out)
{
    const unsigned char *p = ek_bytes_data(bytes);
    int64_t i = index.tag == EK_VAL_INT ? index.as.i : -1;

    if (i < 0 || (size_t)i >= ek_bytes_len(bytes))
        return 0;
    *out = p[i];
    return 1;
}

ek_obj *ek_bytes_concat(ek_obj *a, ek_obj *b)
{
    size_t na = ek_bytes_len(a);
    size_t nb = ek_bytes_len(b);
    unsigned char *buf = ek_rt_alloc_zeroed(na + nb);
    ek_obj *out;

    memcpy(buf, ek_bytes_data(a), na);
    memcpy(buf + na, ek_bytes_data(b), nb);
    out = ek_bytes_new(buf, na + nb);
    free(buf);
    return out;
}

/* The cells build back to front, so the walk runs from the last byte
   to the first and each cons puts its head in front. The walk hands
   its own reference to the tail over to each new cell. */
ek_val ek_bytes_to_ints(ek_obj *bytes)
{
    const unsigned char *p = ek_bytes_data(bytes);
    ek_val acc = ek_nil_val();
    size_t i = ek_bytes_len(bytes);

    while (i > 0) {
        ek_val next;

        i--;
        next = ek_cons(ek_val_int(p[i]), acc);
        ek_release(acc);
        acc = next;
    }
    return acc;
}

/* The first pass validates every element so the second can only
   store; a value outside the element domain refuses the whole
   block. */
ek_obj *ek_bytes_from_ints(ek_val ints)
{
    ek_val cur;
    size_t n = 0;
    unsigned char *buf;
    size_t i = 0;
    ek_obj *out;

    for (cur = ints; ek_val_is_cons(cur); cur = ek_cons_tail(cur.as.ref)) {
        ek_val h = ek_cons_head(cur.as.ref);

        if (h.tag != EK_VAL_INT || h.as.i < 0 || h.as.i > 255)
            return NULL;
        n++;
    }
    buf = ek_rt_alloc_zeroed(n);
    for (cur = ints; ek_val_is_cons(cur); cur = ek_cons_tail(cur.as.ref))
        buf[i++] = (unsigned char)ek_cons_head(cur.as.ref).as.i;
    out = ek_bytes_new(buf, n);
    free(buf);
    return out;
}

ek_obj *ek_bytes_to_hex(ek_obj *bytes)
{
    static const char digits[] = "0123456789abcdef";
    const unsigned char *p = ek_bytes_data(bytes);
    size_t n = ek_bytes_len(bytes);
    char *buf = ek_rt_alloc_zeroed(n * 2);
    size_t i;
    ek_obj *out;

    for (i = 0; i < n; i++) {
        buf[i * 2] = digits[p[i] >> 4];
        buf[i * 2 + 1] = digits[p[i] & 0x0F];
    }
    out = ek_str_new(buf, n * 2);
    free(buf);
    return out;
}

/* The value of a hexadecimal digit, or -1 when c is not one. */
static int nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

int ek_bytes_from_hex(ek_obj *hex, ek_obj **out, ek_obj **detail)
{
    const char *p = ek_str_cstr(hex);
    size_t len = ek_str_len(hex);
    size_t i;
    unsigned char *buf;

    if (len % 2 != 0) {
        *detail = ek_str_from_cstr("odd length");
        return 0;
    }
    buf = ek_rt_alloc_zeroed(len / 2);
    for (i = 0; i < len; i += 2) {
        int hi = nibble(p[i]);
        int lo = nibble(p[i + 1]);

        if (hi < 0 || lo < 0) {
            char msg[64];

            snprintf(msg, sizeof msg, "offset %lu is not hexadecimal",
                     (unsigned long)(hi < 0 ? i : i + 1));
            *detail = ek_str_from_cstr(msg);
            free(buf);
            return 0;
        }
        buf[i / 2] = (unsigned char)(hi << 4 | lo);
    }
    *out = ek_bytes_new(buf, len / 2);
    free(buf);
    return 1;
}

int ek_bytes_to_string(ek_obj *bytes, ek_obj **out, ek_obj **detail)
{
    const unsigned char *p = ek_bytes_data(bytes);
    size_t len = ek_bytes_len(bytes);
    size_t i = 0;

    while (i < len) {
        size_t n = ek_utf8_seq_len(p + i, len - i);

        if (n == 0) {
            char msg[96];
            size_t j = i;
            int pos = snprintf(msg, sizeof msg, "offset %lu near",
                               (unsigned long)i);

            while (j < len && j < i + 4 && pos >= 0
                   && (size_t)pos < sizeof msg) {
                int step = snprintf(msg + pos, sizeof msg - (size_t)pos,
                                    " %02x", p[j]);

                if (step < 0)
                    break;
                pos += step;
                j++;
            }
            *detail = ek_str_from_cstr(msg);
            return 0;
        }
        i += n;
    }
    *out = ek_str_new((const char *)p, len);
    return 1;
}

ek_obj *ek_str_to_bytes(ek_obj *str)
{
    return ek_bytes_new(ek_str_cstr(str), ek_str_len(str));
}
