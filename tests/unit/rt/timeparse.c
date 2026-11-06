#include "harness.h"

#include "capident.h"

#include <stddef.h>

/* The time parsers: the wall-clock reading of the ISO 8601 UTC form
   and the span of the decimal seconds form, their round trips against
   the displays' own shapes, the calendar and clock validation, and
   the three error reasons. */

/* The payload slot of a one-payload value, per the layout contract. */
static ek_val p0(ek_val v)
{
    return ((ek_val *)v.as.ref)[1];
}

/* True when the Result holds the Ok of a value of the given identity
   with exactly this nanosecond payload. */
static int ok_of(ek_val r, const ek_typeinfo *identity, int64_t ns)
{
    if (!ek_val_is_ref(r) || r.as.ref->info != &ekT_std$Result$Ok)
        return 0;
    if (!ek_val_is_ref(p0(r)) || p0(r).as.ref->info != identity)
        return 0;
    return p0(p0(r)).as.i == ns;
}

static int ok_time(ek_val r, int64_t ns)
{
    return ok_of(r, &ekT_time$Time$Time, ns);
}

static int ok_span(ek_val r, int64_t ns)
{
    return ok_of(r, &ekT_time$Duration$Duration, ns);
}

/* True when the Result holds the named ParseError variant. */
static int err_kind(ek_val r, const ek_typeinfo *kind)
{
    if (!ek_val_is_ref(r) || r.as.ref->info != &ekT_std$Result$Err)
        return 0;
    return ek_val_is_ref(p0(r)) && p0(r).as.ref->info == kind;
}

static void epoch_and_fractions_read(void)
{
    EK_CHECK(ok_time(ek_parse_time(ek_str_from_cstr("1970-01-01T00:00:00Z")),
                     0));
    EK_CHECK(ok_time(
        ek_parse_time(ek_str_from_cstr("1970-01-01T00:00:00.123456789Z")),
        123456789));
    EK_CHECK(ok_time(ek_parse_time(ek_str_from_cstr("1970-01-01T00:00:00.5Z")),
                     500000000));
    EK_CHECK(ok_time(
        ek_parse_time(ek_str_from_cstr("2026-11-07T12:34:56Z")),
        1794054896LL * 1000000000));
}

static void before_the_epoch_reads(void)
{
    EK_CHECK(ok_time(ek_parse_time(ek_str_from_cstr("1969-12-31T23:59:59Z")),
                     -1000000000));
}

static void the_calendar_and_clock_validate(void)
{
    EK_CHECK(ok_time(ek_parse_time(ek_str_from_cstr("2024-02-29T00:00:00Z")),
                     1709164800LL * 1000000000));
    EK_CHECK(err_kind(ek_parse_time(ek_str_from_cstr("2023-02-29T00:00:00Z")),
                      &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(ek_parse_time(ek_str_from_cstr("2026-13-01T00:00:00Z")),
                      &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(ek_parse_time(ek_str_from_cstr("2026-11-07T24:00:00Z")),
                      &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(
        ek_parse_time(ek_str_from_cstr("2026-11-07T12:60:00Z")),
        &ekT_std$ParseError$Invalid));
}

static void malformed_time_text_reports_invalid(void)
{
    EK_CHECK(err_kind(ek_parse_time(ek_str_from_cstr("")),
                      &ekT_std$ParseError$Empty));
    EK_CHECK(err_kind(ek_parse_time(ek_str_from_cstr("2026-11-07")),
                      &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(
        ek_parse_time(ek_str_from_cstr("2026-11-07T12:34:56")),
        &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(
        ek_parse_time(ek_str_from_cstr("2026-11-07T12:34:56+01:00")),
        &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(
        ek_parse_time(ek_str_from_cstr("2026-11-07T12:34:56.Z")),
        &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(
        ek_parse_time(ek_str_from_cstr("2026-11-07T12:34:56.1234567890Z")),
        &ekT_std$ParseError$Invalid));
}

static void a_year_out_of_the_domain_reports_range(void)
{
    EK_CHECK(err_kind(ek_parse_time(ek_str_from_cstr("9999-12-31T23:59:59Z")),
                      &ekT_std$ParseError$OutOfRange));
}

static void spans_read_with_sign_and_fraction(void)
{
    EK_CHECK(ok_span(ek_parse_duration(ek_str_from_cstr("0s")), 0));
    EK_CHECK(ok_span(ek_parse_duration(ek_str_from_cstr("2s")),
                     2000000000));
    EK_CHECK(ok_span(ek_parse_duration(ek_str_from_cstr("1.5s")),
                     1500000000));
    EK_CHECK(ok_span(ek_parse_duration(ek_str_from_cstr("-0.001s")),
                     -1000000));
    EK_CHECK(ok_span(ek_parse_duration(ek_str_from_cstr("-1.000000001s")),
                     -1000000001));
}

static void malformed_span_text_reports_invalid(void)
{
    EK_CHECK(err_kind(ek_parse_duration(ek_str_from_cstr("")),
                      &ekT_std$ParseError$Empty));
    EK_CHECK(err_kind(ek_parse_duration(ek_str_from_cstr("1.2.3s")),
                      &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(ek_parse_duration(ek_str_from_cstr("1.5")),
                      &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(ek_parse_duration(ek_str_from_cstr("s")),
                      &ekT_std$ParseError$Invalid));
    EK_CHECK(err_kind(ek_parse_duration(ek_str_from_cstr("1.s")),
                      &ekT_std$ParseError$Invalid));
}

static void a_span_past_the_domain_reports_range(void)
{
    EK_CHECK(err_kind(
        ek_parse_duration(ek_str_from_cstr("922337203686s")),
        &ekT_std$ParseError$OutOfRange));
}

int main(void)
{
    epoch_and_fractions_read();
    before_the_epoch_reads();
    the_calendar_and_clock_validate();
    malformed_time_text_reports_invalid();
    a_year_out_of_the_domain_reports_range();
    spans_read_with_sign_and_fraction();
    malformed_span_text_reports_invalid();
    a_span_past_the_domain_reports_range();
    return ek_test_report("rt/timeparse");
}
