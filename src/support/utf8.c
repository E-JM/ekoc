#include "support/utf8.h"

static int is_continuation(unsigned char byte)
{
    return (byte & 0xC0) == 0x80;
}

size_t ek_utf8_lead_len(unsigned char lead)
{
    if (lead < 0x80) {
        return 1;
    }
    if (lead < 0xC2) {
        return 0; /* continuation bytes; C0/C1 only encode overlong */
    }
    if (lead < 0xE0) {
        return 2;
    }
    if (lead < 0xF0) {
        return 3;
    }
    if (lead < 0xF5) {
        return 4;
    }
    return 0; /* F5..FF only encode beyond U+10FFFF */
}

static uint32_t assemble(const char *bytes, size_t seq)
{
    unsigned char b0 = (unsigned char)bytes[0];
    unsigned char b1 = seq > 1 ? (unsigned char)bytes[1] : 0;
    unsigned char b2 = seq > 2 ? (unsigned char)bytes[2] : 0;
    unsigned char b3 = seq > 3 ? (unsigned char)bytes[3] : 0;

    if (seq == 1) {
        return b0;
    }
    if (seq == 2) {
        return ((uint32_t)(b0 & 0x1F) << 6) | (uint32_t)(b1 & 0x3F);
    }
    if (seq == 3) {
        return ((uint32_t)(b0 & 0x0F) << 12)
             | ((uint32_t)(b1 & 0x3F) << 6)
             | (uint32_t)(b2 & 0x3F);
    }
    return ((uint32_t)(b0 & 0x07) << 18)
         | ((uint32_t)(b1 & 0x3F) << 12)
         | ((uint32_t)(b2 & 0x3F) << 6)
         | (uint32_t)(b3 & 0x3F);
}

ek_utf8_decoded ek_utf8_decode(const char *bytes, size_t len)
{
    ek_utf8_decoded result = { EK_UTF8_INVALID, 0, 1 };
    size_t seq;
    size_t i;
    uint32_t cp;

    if (len == 0) {
        result.status = EK_UTF8_TRUNCATED;
        result.len = 0;
        return result;
    }
    seq = ek_utf8_lead_len((unsigned char)bytes[0]);
    if (seq == 0) {
        return result;
    }
    for (i = 1; i < seq && i < len; i++) {
        if (!is_continuation((unsigned char)bytes[i])) {
            result.len = i; /* the subpart ends before the first byte
                               that cannot continue it */
            return result;
        }
    }
    if (seq > len) {
        result.status = EK_UTF8_TRUNCATED;
        result.len = len;
        return result;
    }
    cp = assemble(bytes, seq);
    if (cp > 0x10FFFF
        || (seq == 3 && cp < 0x800)
        || (seq == 4 && cp < 0x10000)) {
        result.len = seq; /* the whole sequence is the ill-formed unit */
        return result;
    }
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        result.status = EK_UTF8_SURROGATE;
        result.len = seq;
        return result;
    }
    result.status = EK_UTF8_OK;
    result.cp = cp;
    result.len = seq;
    return result;
}

size_t ek_utf8_encode(uint32_t cp, char out[4])
{
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        return 0;
    }
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

size_t ek_utf8_advance(const char *bytes, size_t len)
{
    return ek_utf8_decode(bytes, len).len;
}
