#ifndef EK_SUPPORT_UTF8_H
#define EK_SUPPORT_UTF8_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    EK_UTF8_OK = 0,
    EK_UTF8_TRUNCATED, /* input ends inside a sequence */
    EK_UTF8_INVALID,   /* bad lead, bad continuation, overlong, > U+10FFFF */
    EK_UTF8_SURROGATE  /* decodes to U+D800..U+DFFF */
} ek_utf8_result;

typedef struct {
    ek_utf8_result status;
    uint32_t cp; /* the scalar when status is EK_UTF8_OK, else 0 */
    size_t len;  /* consumed bytes: sequence length when OK, bytes
                    available when truncated, maximal subpart otherwise */
} ek_utf8_decoded;

ek_utf8_decoded ek_utf8_decode(const char *bytes, size_t len);

/* Writes 1..4 bytes and returns how many; returns 0 when cp is a
   surrogate or beyond U+10FFFF. */
size_t ek_utf8_encode(uint32_t cp, char out[4]);

/* Sequence length (1..4) implied by a lead byte, 0 when it cannot lead. */
size_t ek_utf8_lead_len(unsigned char lead);

/* Bytes to skip for one scalar, never 0 on non-empty input. */
size_t ek_utf8_advance(const char *bytes, size_t len);

#endif
