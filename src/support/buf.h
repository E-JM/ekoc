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

/* Each append stores its input and returns 1, or returns 0 when the
   input cannot be stored: the host is out of memory, the request
   overflows size_t, or formatting fails. A failed append leaves the
   buffer holding exactly its previous content, valid for further
   appends and for destroy; a caller that ignores the return ships
   whatever prefix fit, so results feeding control flow must check. */
int ek_buf_append_bytes(ek_buf *buf, const void *data, size_t len);
int ek_buf_append_str(ek_buf *buf, const char *str);
int ek_buf_append_char(ek_buf *buf, char c);

int ek_buf_vprintf(ek_buf *buf, const char *fmt, va_list args);
int ek_buf_printf(ek_buf *buf, const char *fmt, ...);

void ek_buf_destroy(ek_buf *buf);

#endif
