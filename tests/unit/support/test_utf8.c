#include "harness.h"

#include <string.h>

#include "support/utf8.h"

static const char *bytes(const unsigned char *raw, size_t len)
{
    static char buf[8];
    memcpy(buf, raw, len);
    return buf;
}

static void expect_ok(const unsigned char *raw, size_t len, uint32_t cp)
{
    ek_utf8_decoded r = ek_utf8_decode(bytes(raw, len), len);
    if (r.status != EK_UTF8_OK || r.cp != cp || r.len != len) {
        EK_CHECK_U64(r.status, EK_UTF8_OK);
        EK_CHECK_U64(r.cp, cp);
        EK_CHECK_U64(r.len, len);
    } else {
        EK_CHECK(1);
    }
}

static void expect_raw(const unsigned char *raw, size_t len,
                       ek_utf8_result status, size_t consumed)
{
    ek_utf8_decoded r = ek_utf8_decode(bytes(raw, len), len);
    EK_CHECK_U64(r.status, status);
    EK_CHECK_U64(r.len, consumed);
    if (status != EK_UTF8_OK) {
        EK_CHECK_U64(r.cp, 0);
    }
}

static void test_decode_ascii(void)
{
    expect_ok((const unsigned char *)"a", 1, 'a');
    expect_ok((const unsigned char *)"\0", 1, 0);
    expect_ok((const unsigned char *)"\x7f", 1, 0x7F);
}

static void test_decode_multibyte(void)
{
    expect_ok((const unsigned char *)"\xce\xbb", 2, 0x03BB);        /* lambda */
    expect_ok((const unsigned char *)"\xc2\xa0", 2, 0x00A0);        /* nbsp */
    expect_ok((const unsigned char *)"\xe2\x82\xac", 3, 0x20AC);    /* euro */
    expect_ok((const unsigned char *)"\xe4\xb8\x80", 3, 0x4E00);    /* CJK */
    expect_ok((const unsigned char *)"\xf0\x9f\x98\x80", 4, 0x1F600);
    expect_ok((const unsigned char *)"\xf4\x8f\xbf\xbf", 4, 0x10FFFF);
}

static void test_decode_truncated(void)
{
    expect_raw((const unsigned char *)"", 0, EK_UTF8_TRUNCATED, 0);
    expect_raw((const unsigned char *)"\xc3", 1, EK_UTF8_TRUNCATED, 1);
    expect_raw((const unsigned char *)"\xe2\x82", 2, EK_UTF8_TRUNCATED, 2);
    expect_raw((const unsigned char *)"\xf0\x9f\x98", 3, EK_UTF8_TRUNCATED, 3);
    /* continuation breaks before the input ends: invalid, not truncated */
    expect_raw((const unsigned char *)"\xe2?", 2, EK_UTF8_INVALID, 1);
}

static void test_decode_invalid_lead(void)
{
    expect_raw((const unsigned char *)"\x80", 1, EK_UTF8_INVALID, 1);
    expect_raw((const unsigned char *)"\xbf", 1, EK_UTF8_INVALID, 1);
    expect_raw((const unsigned char *)"\xff", 1, EK_UTF8_INVALID, 1);
    expect_raw((const unsigned char *)"\xf8\x88\x80\x80\x80", 5,
               EK_UTF8_INVALID, 1);
    /* C0/C1 leads are reserved: they only ever encode overlong forms */
    expect_raw((const unsigned char *)"\xc0", 1, EK_UTF8_INVALID, 1);
    expect_raw((const unsigned char *)"\xc1\xff", 2, EK_UTF8_INVALID, 1);
}

static void test_decode_overlong(void)
{
    expect_raw((const unsigned char *)"\xe0\x80\x80", 3, EK_UTF8_INVALID, 3);
    expect_raw((const unsigned char *)"\xe0\x9f\xbf", 3, EK_UTF8_INVALID, 3);
    expect_raw((const unsigned char *)"\xf0\x80\x80\x80", 4,
               EK_UTF8_INVALID, 4);
    expect_raw((const unsigned char *)"\xf0\x8f\xbf\xbf", 4,
               EK_UTF8_INVALID, 4);
}

static void test_decode_surrogate(void)
{
    expect_raw((const unsigned char *)"\xed\xa0\x80", 3, EK_UTF8_SURROGATE, 3);
    expect_raw((const unsigned char *)"\xed\xbf\xbf", 3, EK_UTF8_SURROGATE, 3);
}

static void test_decode_beyond_range(void)
{
    expect_raw((const unsigned char *)"\xf4\x90\x80\x80", 4,
               EK_UTF8_INVALID, 4);
    /* F5..FF leads can only encode beyond U+10FFFF: rejected at the lead */
    expect_raw((const unsigned char *)"\xf7\xbf\xbf\xbf", 4,
               EK_UTF8_INVALID, 1);
}

static void test_decode_maximal_subpart(void)
{
    /* regression: the ill-formed unit ends before the first byte that
       cannot continue the sequence, not at the lead alone */
    expect_raw((const unsigned char *)"\xe2\x82?", 3, EK_UTF8_INVALID, 2);
    expect_raw((const unsigned char *)"\xf0\x9f\x98?", 4,
               EK_UTF8_INVALID, 3);
    expect_raw((const unsigned char *)"\xf0\x9f?", 3, EK_UTF8_INVALID, 2);
    expect_raw((const unsigned char *)"\xf0\x9f\x4a\x80", 4,
               EK_UTF8_INVALID, 2);
    /* a valid prefix cut off by end of input stays truncated */
    expect_raw((const unsigned char *)"\xf0\x9f", 2, EK_UTF8_TRUNCATED, 2);
    expect_raw((const unsigned char *)"\xf0\x9f\x98", 3,
               EK_UTF8_TRUNCATED, 3);
    EK_CHECK_U64(ek_utf8_advance("\xe2\x82?.", 4), 2);
    EK_CHECK_U64(ek_utf8_advance("\xf0\x9f?.", 4), 2);
}

static void test_encode_roundtrip(void)
{
    uint32_t cps[] = { 0x00, 0x24, 0x7F, 0x80, 0x03BB, 0x07FF, 0x0800,
                       0x20AC, 0x4E00, 0xFFFD, 0xFFFF, 0x10000, 0x1F600,
                       0x10FFFF };
    size_t i;

    for (i = 0; i < sizeof cps / sizeof cps[0]; i++) {
        char out[4] = { 0 };
        size_t n = ek_utf8_encode(cps[i], out);
        ek_utf8_decoded r = ek_utf8_decode(out, n);
        EK_CHECK_U64(r.status, EK_UTF8_OK);
        EK_CHECK_U64(r.cp, cps[i]);
        EK_CHECK_U64(r.len, n);
    }
}

static void test_encode_rejects_nonscalars(void)
{
    char out[4];

    EK_CHECK_U64(ek_utf8_encode(0xD800, out), 0);
    EK_CHECK_U64(ek_utf8_encode(0xDFFF, out), 0);
    EK_CHECK_U64(ek_utf8_encode(0x110000, out), 0);
    EK_CHECK_U64(ek_utf8_encode(0xFFFFFFFF, out), 0);
}

static void test_lead_len(void)
{
    EK_CHECK_U64(ek_utf8_lead_len(0x00), 1);
    EK_CHECK_U64(ek_utf8_lead_len(0x41), 1);
    EK_CHECK_U64(ek_utf8_lead_len(0x7F), 1);
    EK_CHECK_U64(ek_utf8_lead_len(0x80), 0);
    EK_CHECK_U64(ek_utf8_lead_len(0xC2), 2);
    EK_CHECK_U64(ek_utf8_lead_len(0xDF), 2);
    EK_CHECK_U64(ek_utf8_lead_len(0xC1), 0);
    EK_CHECK_U64(ek_utf8_lead_len(0xE0), 3);
    EK_CHECK_U64(ek_utf8_lead_len(0xEF), 3);
    EK_CHECK_U64(ek_utf8_lead_len(0xF0), 4);
    EK_CHECK_U64(ek_utf8_lead_len(0xF4), 4);
    EK_CHECK_U64(ek_utf8_lead_len(0xF5), 0);
    EK_CHECK_U64(ek_utf8_lead_len(0xFF), 0);
}

static void test_advance(void)
{
    EK_CHECK_U64(ek_utf8_advance("", 0), 0);
    EK_CHECK_U64(ek_utf8_advance("ab", 2), 1);
    EK_CHECK_U64(ek_utf8_advance("\xce\xbb.", 3), 2);
    EK_CHECK_U64(ek_utf8_advance("\xe2\x82\xac.", 4), 3);
    EK_CHECK_U64(ek_utf8_advance("\xf0\x9f\x98\x80.", 5), 4);
    /* errors still make progress over the maximal subpart */
    EK_CHECK_U64(ek_utf8_advance("\x80", 1), 1);
    EK_CHECK_U64(ek_utf8_advance("\xed\xa0\x80x", 4), 3);
    EK_CHECK_U64(ek_utf8_advance("\xf4\x90\x80\x80x", 5), 4);
    EK_CHECK_U64(ek_utf8_advance("\xe2\x82", 2), 2);
}

int main(void)
{
    test_decode_ascii();
    test_decode_multibyte();
    test_decode_truncated();
    test_decode_invalid_lead();
    test_decode_overlong();
    test_decode_surrogate();
    test_decode_beyond_range();
    test_decode_maximal_subpart();
    test_encode_roundtrip();
    test_encode_rejects_nonscalars();
    test_lead_len();
    test_advance();
    return ek_test_report("support/utf8");
}
