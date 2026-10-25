#ifndef EK_SUPPORT_BUF_H
#define EK_SUPPORT_BUF_H

#include <stdarg.h>
#include <stddef.h>

typedef struct {
    char *data; /* NUL-terminated at data[len]; NULL while nothing stored */
    size_t len; /* stored bytes, terminator excluded */
    size_t cap; /* allocated bytes, terminator included */
} ek_buf;

void ek_buf_init(ek_buf *buf);

/* Appends are best effort: on host allocation failure the buffer keeps
   its previous content. */
void ek_buf_append_bytes(ek_buf *buf, const void *data, size_t len);
void ek_buf_append_str(ek_buf *buf, const char *str);
void ek_buf_append_char(ek_buf *buf, char c);

void ek_buf_vprintf(ek_buf *buf, const char *fmt, va_list args);
void ek_buf_printf(ek_buf *buf, const char *fmt, ...);

void ek_buf_destroy(ek_buf *buf);

#endif
