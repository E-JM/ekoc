#include "internal.h"

#include "json.h"

#include <stdlib.h>
#include <string.h>

/* The text forms of JSON: the string decoder and the number scanner.
   The reader's scan state and dispatch live in json.c; these pieces
   turn the two quoted and numeric shapes into String values and Json
   scalars. */

/* The growable byte buffer of a decoded string. */
typedef struct {
    char *data;
    size_t len, cap;
} json_buf;

static void buf_add(json_buf *b, char c)
{
    if (b->len == b->cap) {
        size_t ncap = b->cap ? b->cap * 2 : 16;
        char *fresh = realloc(b->data, ncap);

        if (!fresh)
            ek_abort("out of memory");
        b->data = fresh;
        b->cap = ncap;
    }
    b->data[b->len++] = c;
}

/* The byte at the position, or -1 past the end. */
int json_peek(const json_scan *s)
{
    return s->pos < s->len ? (unsigned char)s->p[s->pos] : -1;
}

/* The four JSON whitespace characters. */
void json_skip_ws(json_scan *s)
{
    while (s->pos < s->len) {
        char c = s->p[s->pos];

        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            break;
        s->pos++;
    }
}

/* Writes the failure detail naming the offset and returns INVALID. */
json_status json_fail_at(const json_scan *s, ek_obj **detail,
                         const char *why)
{
    char msg[96];

    snprintf(msg, sizeof msg, "offset %lu: %s", (unsigned long)s->pos, why);
    *detail = ek_str_from_cstr(msg);
    return JSON_INVALID;
}

/* Consumes the expected byte, or writes the failure and returns zero. */
int json_expect(json_scan *s, char c, ek_obj **detail, const char *why)
{
    if (json_peek(s) != (unsigned char)c) {
        json_fail_at(s, detail, why);
        return 0;
    }
    s->pos++;
    return 1;
}

/* Matches the exact spelling at the position and moves past it. */
int json_match_lit(json_scan *s, const char *word)
{
    size_t n = strlen(word);

    if (s->len - s->pos < n || memcmp(s->p + s->pos, word, n) != 0)
        return 0;
    s->pos += n;
    return 1;
}

/* Enters one container level, panicking past the cap so a hostile
   document cannot exhaust the stack. */
void json_nest_in(json_scan *s)
{
    s->depth++;
    if (s->depth > JSON_MAX_DEPTH)
        ek_panic("P-panic", "json nesting too deep");
}

/* The four-hex-digit escape: its code point, or -1 when the digits do
   not form one. */
static long escape_hex(json_scan *s)
{
    long code = 0;
    int i;

    for (i = 0; i < 4; i++) {
        int c = json_peek(s);
        long d;

        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            return -1;
        code = code * 16 + d;
        s->pos++;
    }
    return code;
}

/* The \u escape, pairing surrogates: a high half must be followed by
   its low half, and a lone half is malformed. */
static json_status parse_unicode(json_scan *s, json_buf *b, ek_obj **detail)
{
    long code = escape_hex(s);
    char utf8[4];
    size_t w;
    size_t i;

    if (code < 0)
        return json_fail_at(s, detail, "incomplete unicode escape");
    if (code >= 0xD800 && code <= 0xDBFF) {
        long low;

        if (json_peek(s) != '\\' || s->pos + 1 >= s->len
            || s->p[s->pos + 1] != 'u')
            return json_fail_at(s, detail, "lone surrogate");
        s->pos += 2;
        low = escape_hex(s);
        if (low < 0xDC00 || low > 0xDFFF)
            return json_fail_at(s, detail, "lone surrogate");
        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
    } else if (code >= 0xDC00 && code <= 0xDFFF) {
        return json_fail_at(s, detail, "lone surrogate");
    }
    w = ek_utf8_encode((uint32_t)code, utf8);
    for (i = 0; i < w; i++)
        buf_add(b, utf8[i]);
    return JSON_OK;
}

/* The escape after the backslash, dispatched on its letter. */
static json_status parse_escape(json_scan *s, json_buf *b, ek_obj **detail)
{
    switch (json_peek(s)) {
    case '"': buf_add(b, '"'); s->pos++; return JSON_OK;
    case '\\': buf_add(b, '\\'); s->pos++; return JSON_OK;
    case '/': buf_add(b, '/'); s->pos++; return JSON_OK;
    case 'b': buf_add(b, '\b'); s->pos++; return JSON_OK;
    case 'f': buf_add(b, '\f'); s->pos++; return JSON_OK;
    case 'n': buf_add(b, '\n'); s->pos++; return JSON_OK;
    case 'r': buf_add(b, '\r'); s->pos++; return JSON_OK;
    case 't': buf_add(b, '\t'); s->pos++; return JSON_OK;
    case 'u':
        s->pos++;
        return parse_unicode(s, b, detail);
    default:
        return json_fail_at(s, detail, "bad escape");
    }
}

/* One string: the opening quote stands at the position. Raw multibyte
   scalars pass through, since the input String is always valid UTF-8;
   a raw control character and every malformed escape are rejected. */
json_status json_parse_string(json_scan *s, ek_val *out, ek_obj **detail)
{
    json_buf b;
    json_status st = JSON_OK;

    b.data = NULL;
    b.len = 0;
    b.cap = 0;
    s->pos++;
    while (json_peek(s) != '"') {
        int c = json_peek(s);
        size_t w;

        if (c < 0) {
            st = json_fail_at(s, detail, "unterminated string");
            break;
        }
        if (c >= 0x80) {
            w = ek_utf8_seq_len((const unsigned char *)s->p + s->pos,
                                s->len - s->pos);
            if (w == 0) {
                st = json_fail_at(s, detail, "invalid encoding");
                break;
            }
            for (size_t k = 0; k < w; k++)
                buf_add(&b, s->p[s->pos + k]);
            s->pos += w;
            continue;
        }
        if (c < 0x20) {
            st = json_fail_at(s, detail, "control character in string");
            break;
        }
        if (c != '\\') {
            buf_add(&b, (char)c);
            s->pos++;
            continue;
        }
        s->pos++;
        st = parse_escape(s, &b, detail);
        if (st != JSON_OK)
            break;
    }
    if (st == JSON_OK) {
        s->pos++;
        *out = ek_val_ref(ek_str_new(b.data, b.len));
    }
    free(b.data);
    return st;
}

/* The decimal digit run starting at i. */
static size_t digit_run(const char *p, size_t len, size_t i)
{
    while (i < len && p[i] >= '0' && p[i] <= '9')
        i++;
    return i;
}

/* The integer-syntax number as a Json Int, or RANGE when the digits
   leave the 64-bit signed domain. */
static json_status number_int(const char *p, size_t start, size_t end,
                              int negative, ek_val *out)
{
    uint64_t mag = 0;
    uint64_t limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    size_t i;

    for (i = start; i < end; i++) {
        if (p[i] == '-')
            continue;
        if (__builtin_mul_overflow(mag, (uint64_t)10, &mag)
            || __builtin_add_overflow(mag, (uint64_t)(p[i] - '0'), &mag)
            || mag > limit)
            return JSON_RANGE;
    }
    if (negative)
        *out = json_int(mag == limit ? INT64_MIN : -(int64_t)mag);
    else
        *out = json_int((int64_t)mag);
    return JSON_OK;
}

/* The fractional or exponent syntax as a Json Float, reading through
   strtod so overflow and underflow take the IEEE result. */
static json_status number_float(const char *p, size_t start, size_t end,
                                ek_val *out)
{
    char *buf = ek_rt_alloc_zeroed(end - start);
    ek_val v;

    memcpy(buf, p + start, end - start);
    v = json_float(strtod(buf, NULL));
    free(buf);
    *out = v;
    return JSON_OK;
}

/* One number: optional sign, then the integer part with no leading
   zero, then an optional fraction and exponent. */
json_status json_parse_number(json_scan *s, ek_val *out, ek_obj **detail)
{
    size_t start = s->pos;
    size_t i = s->pos;
    int negative = 0;
    int fractional = 0;

    if (s->p[i] == '-') {
        negative = 1;
        i++;
    }
    if (i >= s->len || s->p[i] < '0' || s->p[i] > '9') {
        s->pos = i;
        return json_fail_at(s, detail, "bad number");
    }
    if (s->p[i] == '0') {
        i++;
        if (i < s->len && s->p[i] >= '0' && s->p[i] <= '9')
            return json_fail_at(s, detail, "leading zero in number");
    } else {
        i = digit_run(s->p, s->len, i);
    }
    if (i < s->len && s->p[i] == '.') {
        fractional = 1;
        i++;
        if (i >= s->len || s->p[i] < '0' || s->p[i] > '9') {
            s->pos = i;
            return json_fail_at(s, detail, "bad number");
        }
        i = digit_run(s->p, s->len, i);
    }
    if (i < s->len && (s->p[i] == 'e' || s->p[i] == 'E')) {
        fractional = 1;
        i++;
        if (i < s->len && (s->p[i] == '+' || s->p[i] == '-'))
            i++;
        if (i >= s->len || s->p[i] < '0' || s->p[i] > '9') {
            s->pos = i;
            return json_fail_at(s, detail, "bad number");
        }
        i = digit_run(s->p, s->len, i);
    }
    s->pos = i;
    if (fractional)
        return number_float(s->p, start, i, out);
    return number_int(s->p, start, i, negative, out);
}
