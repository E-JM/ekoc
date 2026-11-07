#include <string.h>

#include "lower/internal.h"

/* The writer of one derived-service body: the buffer's life cycle,
   the line fragments, the body's temps, and the string joins the
   display bodies build from. */

/* Opens one service body's buffer. */
void lower_d_begin(lower_ctx *c, lower_demit *d, ek_buf *b)
{
    ek_buf_init(b);
    d->c = c;
    d->tmp = 0;
    d->buf = b;
}

/* Closes the body: its text joins the service buffer whole. */
void lower_d_end(lower_ctx *c, ek_buf *b)
{
    if (b->data && !ek_buf_append_bytes(&c->dsvc, b->data, b->len)) {
        c->oom = 1;
    }
    ek_buf_destroy(b);
}

/* Writes one line fragment into the body's buffer. */
void lower_d_printf(lower_demit *d, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (!ek_buf_vprintf(d->buf, fmt, ap)) {
        d->c->oom = 1;
    }
    va_end(ap);
}

/* A fresh temp of the service body, unique inside it. */
const char *lower_d_temp(lower_demit *d)
{
    d->tmp++;
    return lower_str(d->c, "s%u", d->tmp - 1);
}

/* The C string literal of a written text. */
const char *lower_d_cstr(lower_ctx *c, const char *s)
{
    ek_buf b;

    ek_buf_init(&b);
    lower_string_literal(c, &b, s, strlen(s));
    return lower_take(c, &b);
}

/* acc = acc ++ lit; each emitted line carries the block's indent. The
   literal reads its cache variable, kept for the join, and the old
   accumulator dies once the join exists. */
void lower_d_concat_lit(lower_demit *d, const char *indent, const char *acc,
                        const char *lit)
{
    const char *s = lower_d_temp(d);
    const char *n = lower_d_temp(d);
    const char *v = lower_str_cache(d->c, lit, strlen(lit));

    lower_d_printf(d, "%s{\n", indent);
    lower_d_printf(d, "%s    ek_obj *%s = ek_keep(%s).as.ref;\n", indent,
                   s, v);
    lower_d_printf(d, "%s    ek_obj *%s = ek_str_concat(%s, %s);\n", indent,
                   n, acc, s);
    lower_d_printf(d, "%s    ek_release_obj(%s);\n", indent, s);
    lower_d_printf(d, "%s    ek_release_obj(%s);\n", indent, acc);
    lower_d_printf(d, "%s    %s = %s;\n", indent, acc, n);
    lower_d_printf(d, "%s}\n", indent);
}

/* acc = acc ++ part; the part object and the old accumulator die. */
void lower_d_concat_obj(lower_demit *d, const char *indent, const char *acc,
                        const char *part)
{
    const char *n = lower_d_temp(d);

    lower_d_printf(d, "%s{\n", indent);
    lower_d_printf(d, "%s    ek_obj *%s = ek_str_concat(%s, %s);\n", indent,
                   n, acc, part);
    lower_d_printf(d, "%s    ek_release_obj(%s);\n", indent, part);
    lower_d_printf(d, "%s    ek_release_obj(%s);\n", indent, acc);
    lower_d_printf(d, "%s    %s = %s;\n", indent, acc, n);
    lower_d_printf(d, "%s}\n", indent);
}
