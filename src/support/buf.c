#include "support/buf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_MIN_CAP ((size_t)64)

void ek_buf_init(ek_buf *buf)
{
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

/* total is the required capacity including the terminator */
static int buf_reserve(ek_buf *buf, size_t total)
{
    size_t cap;
    char *grown;

    if (total <= buf->cap) {
        return 0;
    }
    cap = buf->cap != 0 ? buf->cap : BUF_MIN_CAP;
    while (cap < total) {
        if (cap > SIZE_MAX / 2) {
            cap = total;
            break;
        }
        cap *= 2;
    }
    grown = realloc(buf->data, cap);
    if (grown == NULL) {
        return -1;
    }
    buf->data = grown;
    buf->cap = cap;
    return 0;
}

int ek_buf_append_bytes(ek_buf *buf, const void *data, size_t len)
{
    if (len == 0) {
        return 1; /* storing nothing is a completed append */
    }
    if (len > SIZE_MAX - buf->len - 1) {
        return 0;
    }
    if (buf_reserve(buf, buf->len + len + 1) != 0) {
        return 0;
    }
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return 1;
}

int ek_buf_append_str(ek_buf *buf, const char *str)
{
    return ek_buf_append_bytes(buf, str, strlen(str));
}

int ek_buf_append_char(ek_buf *buf, char c)
{
    if (buf->len > SIZE_MAX - 2) { /* keep buf->len + 2 from wrapping,
                                      as append_bytes guards its own sum */
        return 0;
    }
    if (buf_reserve(buf, buf->len + 2) != 0) {
        return 0;
    }
    buf->data[buf->len++] = c;
    buf->data[buf->len] = '\0';
    return 1;
}

int ek_buf_vprintf(ek_buf *buf, const char *fmt, va_list args)
{
    va_list measure;
    int need;

    va_copy(measure, args);
    need = vsnprintf(NULL, 0, fmt, measure);
    va_end(measure);
    if (need < 0) {
        return 0; /* the rendering itself failed, so nothing to store */
    }
    if (need == 0) {
        return 1; /* an empty rendering stores nothing */
    }
    if ((size_t)need > SIZE_MAX - buf->len - 1
        || buf_reserve(buf, buf->len + (size_t)need + 1) != 0) {
        return 0;
    }
    vsnprintf(buf->data + buf->len, (size_t)need + 1, fmt, args);
    buf->len += (size_t)need;
    return 1;
}

int ek_buf_printf(ek_buf *buf, const char *fmt, ...)
{
    va_list args;
    int ok;

    va_start(args, fmt);
    ok = ek_buf_vprintf(buf, fmt, args);
    va_end(args);
    return ok;
}

void ek_buf_destroy(ek_buf *buf)
{
    free(buf->data);
    ek_buf_init(buf);
}
