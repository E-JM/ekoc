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

void ek_buf_append_bytes(ek_buf *buf, const void *data, size_t len)
{
    if (len == 0 || len > SIZE_MAX - buf->len - 1) {
        return;
    }
    if (buf_reserve(buf, buf->len + len + 1) != 0) {
        return;
    }
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
}

void ek_buf_append_str(ek_buf *buf, const char *str)
{
    ek_buf_append_bytes(buf, str, strlen(str));
}

void ek_buf_append_char(ek_buf *buf, char c)
{
    if (buf_reserve(buf, buf->len + 2) != 0) {
        return;
    }
    buf->data[buf->len++] = c;
    buf->data[buf->len] = '\0';
}

void ek_buf_vprintf(ek_buf *buf, const char *fmt, va_list args)
{
    va_list measure;
    int need;

    va_copy(measure, args);
    need = vsnprintf(NULL, 0, fmt, measure);
    va_end(measure);
    if (need <= 0 || (size_t)need > SIZE_MAX - buf->len - 1) {
        return;
    }
    if (buf_reserve(buf, buf->len + (size_t)need + 1) != 0) {
        return;
    }
    vsnprintf(buf->data + buf->len, (size_t)need + 1, fmt, args);
    buf->len += (size_t)need;
}

void ek_buf_printf(ek_buf *buf, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    ek_buf_vprintf(buf, fmt, args);
    va_end(args);
}

void ek_buf_destroy(ek_buf *buf)
{
    free(buf->data);
    ek_buf_init(buf);
}
