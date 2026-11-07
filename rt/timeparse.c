#include "internal.h"
#include "json.h"

#include <stddef.h>

/* The time parsers. Time.parse reads exactly the ISO 8601 UTC forms
   the wall-clock display produces -- four digit year, two digit month,
   day, hour, minute, and second, an optional fraction of one to nine
   digits, the zone marker -- and Duration.parse reads the decimal
   seconds form, an optional sign, whole digits, an optional fraction
   of one to nine digits, and the unit letter. Both build the Result
   the parse family returns over the emitted unit's own identities:
   Err Empty for empty text, Err (Invalid detail) naming the first
   expectation the text breaks, Err OutOfRange when the nanosecond
   payload leaves the signed 64-bit domain. */

/* The days of one month of one year. */
static int days_in_month(int64_t y, int m)
{
    static const int plain[12] = {31, 28, 31, 30, 31, 30,
                                  31, 31, 30, 31, 30, 31};
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;

    return m == 2 && leap ? 29 : plain[m - 1];
}

/* Days since 1970-01-01 of a calendar date, Hinnant's days_from_civil
   with the era shift. */
static int64_t days_from_civil(int64_t y, int m, int d)
{
    int64_t yy = m <= 2 ? y - 1 : y;
    int64_t era = yy / 400;
    int64_t yoe = yy - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return era * 146097 + doe - 719468;
}

/* The scan: the text bytes and the reading position. */
typedef struct {
    const char *p;
    size_t len;
    size_t pos;
} time_scan;

/* The byte at the position, or -1 past the end. */
static int peek(const time_scan *s)
{
    return s->pos < s->len ? (unsigned char)s->p[s->pos] : -1;
}

/* Consumes the expected byte; nonzero when it stood there. */
static int expect(time_scan *s, char c)
{
    if (peek(s) == c) {
        s->pos++;
        return 1;
    }
    return 0;
}

/* Reads exactly `n` digits into *out; nonzero when all `n` stand
   there. */
static int digits(time_scan *s, int n, int64_t *out)
{
    int64_t v = 0;
    int i;

    for (i = 0; i < n; i++) {
        int c = peek(s);

        if (c < '0' || c > '9')
            return 0;
        s->pos++;
        v = v * 10 + (c - '0');
    }
    *out = v;
    return 1;
}

/* The error results. A detail argument is owned by the caller and
   owned by the built value afterwards. */
static ek_val invalid(const char *why)
{
    return parse_error_invalid(ek_val_ref(ek_str_from_cstr(why)));
}

static ek_val failed(ek_val reason)
{
    return ek_gen_result_err(reason);
}

/* The whole-second domain: a payload beyond this bound cannot carry
   its nanoseconds in a signed 64-bit word, fraction included. */
#define SECS_DOMAIN ((INT64_MAX - 999999999) / 1000000000)

/* The fraction past the whole part: a dot and one to nine digits, the
   digits' value into *frac and their count into *nfrac. The result is
   the failure of a malformed fraction -- an empty or oversized digit
   run -- and unit when the text carries none or a well formed one. */
static ek_val fraction(time_scan *s, int64_t *frac, int *nfrac)
{
    int64_t f = 0;
    int n = 0;

    if (peek(s) != '.')
        return ek_val_unit();
    s->pos++;
    for (;;) {
        int c = peek(s);

        if (c < '0' || c > '9')
            break;
        s->pos++;
        if (n < 9)
            f = f * 10 + (c - '0');
        n++;
    }
    if (n == 0)
        return invalid("expected fraction digits");
    if (n > 9)
        return invalid("the fraction exceeds nine digits");
    *frac = f;
    *nfrac = n;
    return ek_val_unit();
}

ek_val ek_parse_time(ek_obj *text)
{
    time_scan s = {ek_str_cstr(text), ek_str_len(text), 0};
    int64_t y, mo, d, h, mi, sec, frac = 0, days, secs;
    int nfrac = 0;
    ek_val v, bad;

    if (s.len == 0)
        return failed(parse_error_empty());
    if (!digits(&s, 4, &y) || !expect(&s, '-')
        || !digits(&s, 2, &mo) || !expect(&s, '-')
        || !digits(&s, 2, &d) || !expect(&s, 'T'))
        return failed(invalid("expected the calendar date"));
    if (mo < 1 || mo > 12 || d < 1 || d > days_in_month(y, mo))
        return failed(invalid("the date is not a calendar day"));
    if (!digits(&s, 2, &h) || !expect(&s, ':')
        || !digits(&s, 2, &mi) || !expect(&s, ':')
        || !digits(&s, 2, &sec))
        return failed(invalid("expected the clock"));
    if (h > 23 || mi > 59 || sec > 59)
        return failed(invalid("the clock leaves the day"));
    bad = fraction(&s, &frac, &nfrac);
    if (!ek_val_is_unit(bad))
        return failed(bad);
    if (!expect(&s, 'Z') || peek(&s) >= 0)
        return failed(invalid("expected the zone marker and the end"));
    days = days_from_civil(y, (int)mo, (int)d);
    secs = days * 86400 + h * 3600 + mi * 60 + sec;
    if (secs > SECS_DOMAIN || secs < -SECS_DOMAIN)
        return failed(parse_error_range());
    while (nfrac < 9) {
        frac *= 10;
        nfrac++;
    }
    v = ek_gen_time(secs * 1000000000 + frac);
    return ek_gen_result_ok(v);
}

ek_val ek_parse_duration(ek_obj *text)
{
    time_scan s = {ek_str_cstr(text), ek_str_len(text), 0};
    int64_t whole = 0, frac = 0;
    int neg = 0, nfrac = 0, any = 0;
    ek_val v, bad;

    if (s.len == 0)
        return failed(parse_error_empty());
    neg = expect(&s, '-');
    for (;;) {
        int c = peek(&s);

        if (c < '0' || c > '9')
            break;
        s.pos++;
        any = 1;
        if (whole <= SECS_DOMAIN)
            whole = whole * 10 + (c - '0');
    }
    if (!any)
        return failed(invalid("expected whole seconds"));
    bad = fraction(&s, &frac, &nfrac);
    if (!ek_val_is_unit(bad))
        return failed(bad);
    if (!expect(&s, 's') || peek(&s) >= 0)
        return failed(invalid("expected the unit and the end"));
    if (whole > SECS_DOMAIN)
        return failed(parse_error_range());
    while (nfrac < 9) {
        frac *= 10;
        nfrac++;
    }
    if (neg)
        whole = -whole;
    v = ek_gen_duration(whole * 1000000000 + (neg ? -frac : frac));
    return ek_gen_result_ok(v);
}
