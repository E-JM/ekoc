#include "harness.h"
#include "capident.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "ekort.h"

/* The Json document reader: the seven variants build over the shared
   identities the way an emitted unit defines them, integer syntax
   reads as Int and fractional or exponent syntax as Float, the
   non-finite spellings round-trip, and every malformed piece reports
   the ParseError variant and the offset detail. The payloads read
   through the layout contract: the object header, then the payload
   slot. */

/* The payload slot of a one-payload constructor value. */
static ek_val payload0(ek_val v)
{
    return ((const ek_val *)v.as.ref)[1];
}

/* Nonzero when v is a reference to the named constructor family. */
static int is_ctor(ek_val v, const ek_typeinfo *id)
{
    return ek_val_is_ref(v) && v.as.ref->info == id;
}

/* Parses the text and checks the Ok shape; the returned Json value is
   the caller's to release. */
static ek_val parse_ok(const char *text, const ek_typeinfo *id)
{
    ek_obj *in = ek_str_from_cstr(text);
    ek_val r = ek_json_parse(in);
    ek_val out = payload0(r);

    EK_CHECK(is_ctor(r, &ekT_std$Result$Ok));
    EK_CHECK(is_ctor(out, id));
    ek_retain(out);
    ek_release(r);
    ek_release(ek_val_ref(in));
    return out;
}

/* Parses the text and checks the Err shape; the returned ParseError is
   the caller's to release. */
static ek_val parse_err(const char *text, const ek_typeinfo *id)
{
    ek_obj *in = ek_str_from_cstr(text);
    ek_val r = ek_json_parse(in);
    ek_val out = payload0(r);

    EK_CHECK(is_ctor(r, &ekT_std$Result$Err));
    EK_CHECK(is_ctor(out, id));
    ek_retain(out);
    ek_release(r);
    ek_release(ek_val_ref(in));
    return out;
}

static void null_and_bools_build_over_the_identities(void)
{
    ek_val j;

    j = parse_ok("null", &ekT_std$Json$Null);
    ek_release(j);
    j = parse_ok("true", &ekT_std$Json$Bool);
    EK_CHECK_U64(payload0(j).as.i, 1);
    ek_release(j);
    j = parse_ok("false", &ekT_std$Json$Bool);
    EK_CHECK_U64(payload0(j).as.i, 0);
    ek_release(j);
}

static void integers_read_as_int(void)
{
    ek_val j;

    j = parse_ok("0", &ekT_std$Json$Int);
    EK_CHECK_U64(payload0(j).as.i, 0);
    ek_release(j);
    j = parse_ok("42", &ekT_std$Json$Int);
    EK_CHECK_U64(payload0(j).as.i, 42);
    ek_release(j);
    j = parse_ok("-7", &ekT_std$Json$Int);
    EK_CHECK_U64(payload0(j).as.i, -7);
    ek_release(j);
    j = parse_ok("-0", &ekT_std$Json$Int);
    EK_CHECK_U64(payload0(j).as.i, 0);
    ek_release(j);
    j = parse_ok("9223372036854775807", &ekT_std$Json$Int);
    EK_CHECK_U64(payload0(j).as.i, INT64_MAX);
    ek_release(j);
    j = parse_ok("-9223372036854775808", &ekT_std$Json$Int);
    EK_CHECK_U64(payload0(j).as.i, INT64_MIN);
    ek_release(j);
}

static void fractions_and_exponents_read_as_float(void)
{
    ek_val j;

    j = parse_ok("3.5", &ekT_std$Json$Float);
    EK_CHECK(payload0(j).as.f == 3.5);
    ek_release(j);
    j = parse_ok("1.0", &ekT_std$Json$Float); /* not an Int */
    EK_CHECK(payload0(j).as.f == 1.0);
    ek_release(j);
    j = parse_ok("-0.25", &ekT_std$Json$Float);
    EK_CHECK(payload0(j).as.f == -0.25);
    ek_release(j);
    j = parse_ok("1e3", &ekT_std$Json$Float);
    EK_CHECK(payload0(j).as.f == 1000.0);
    ek_release(j);
    j = parse_ok("1.5e-2", &ekT_std$Json$Float);
    EK_CHECK(payload0(j).as.f == 0.015);
    ek_release(j);
}

/* The display vocabulary's three spellings come back as the floats
   they name. */
static void nonfinite_spellings_round_trip(void)
{
    ek_val j;

    j = parse_ok("NaN", &ekT_std$Json$Float);
    EK_CHECK(isnan(payload0(j).as.f));
    ek_release(j);
    j = parse_ok("Inf", &ekT_std$Json$Float);
    EK_CHECK(isinf(payload0(j).as.f) && payload0(j).as.f > 0);
    ek_release(j);
    j = parse_ok("-Inf", &ekT_std$Json$Float);
    EK_CHECK(isinf(payload0(j).as.f) && payload0(j).as.f < 0);
    ek_release(j);
}

static void strings_decode_the_escapes(void)
{
    ek_val j;
    ek_obj *s;

    j = parse_ok("\"hi\"", &ekT_std$Json$Str);
    s = payload0(j).as.ref;
    EK_CHECK_STR(ek_str_cstr(s), "hi");
    EK_CHECK_U64(s->rc, 1); /* the fresh decode, owned by the variant */
    ek_release(j);
    j = parse_ok("\"a\\nb\\t\\r\\b\\f\"", &ekT_std$Json$Str);
    EK_CHECK_STR(ek_str_cstr(payload0(j).as.ref), "a\nb\t\r\b\f");
    ek_release(j);
    j = parse_ok("\"\\/\\\"\\\\\"", &ekT_std$Json$Str);
    EK_CHECK_STR(ek_str_cstr(payload0(j).as.ref), "/\"\\");
    ek_release(j);
    j = parse_ok("\"\\u0041\\u00e9\"", &ekT_std$Json$Str);
    EK_CHECK_STR(ek_str_cstr(payload0(j).as.ref), "A\xc3\xa9");
    ek_release(j);
    j = parse_ok("\"\xc3\xa9\"", &ekT_std$Json$Str); /* raw scalars pass */
    EK_CHECK_STR(ek_str_cstr(payload0(j).as.ref), "\xc3\xa9");
    ek_release(j);
}

/* A surrogate pair combines; either half alone is malformed. */
static void surrogates_parse_as_pairs(void)
{
    ek_val j = parse_ok("\"\\ud83d\\ude00\"", &ekT_std$Json$Str);
    ek_obj *s = payload0(j).as.ref;

    EK_CHECK_U64(ek_str_len(s), 4);
    EK_CHECK(memcmp(ek_str_cstr(s), "\xf0\x9f\x98\x80", 4) == 0);
    ek_release(j);
    parse_err("\"\\ud83d\"", &ekT_std$ParseError$Invalid);
    parse_err("\"\\ude00\"", &ekT_std$ParseError$Invalid);
    parse_err("\"\\ud83dx\"", &ekT_std$ParseError$Invalid);
}

static void arrays_build_the_list_spine(void)
{
    ek_val j = parse_ok("[1,2,3]", &ekT_std$Json$Arr);
    ek_val list = payload0(j);
    ek_val cur;
    int64_t want = 1;

    EK_CHECK_U64(ek_list_len(list), 3);
    for (cur = list; ek_val_is_cons(cur); cur = ek_cons_tail(cur.as.ref)) {
        ek_val elem = ek_cons_head(cur.as.ref);

        EK_CHECK(is_ctor(elem, &ekT_std$Json$Int));
        EK_CHECK_U64(payload0(elem).as.i, want);
        want++;
    }
    ek_release(j);
    j = parse_ok("[]", &ekT_std$Json$Arr);
    EK_CHECK(ek_val_is_nil(payload0(j)));
    ek_release(j);
    j = parse_ok("[[1],[2,[3]]]", &ekT_std$Json$Arr);
    EK_CHECK_U64(ek_list_len(payload0(j)), 2);
    ek_release(j);
}

/* The keys walk in the text's order, and a duplicate replaces the
   value and keeps its place. */
static size_t walk_obj_keys(ek_val dict, char out[][8], size_t cap)
{
    ek_dict_iter it;
    size_t n = 0;
    ek_val key;
    ek_val val;

    ek_dict_iter_begin(&it, dict);
    while (ek_dict_iter_next(&it, &key, &val)) {
        if (n < cap)
            snprintf(out[n], 8, "%s", ek_str_cstr(key.as.ref));
        n++;
    }
    ek_dict_iter_end(&it);
    return n;
}

static void objects_build_the_dictionary(void)
{
    ek_val j = parse_ok("{\"a\": 1, \"b\": 2}", &ekT_std$Json$Obj);
    ek_val dict = payload0(j);
    ek_val out;
    ek_obj *key;
    char keys[2][8];

    EK_CHECK_U64(ek_dict_len(dict), 2);
    key = ek_str_from_cstr("a");
    EK_CHECK(ek_dict_get(dict, ek_val_ref(key), &out));
    EK_CHECK(is_ctor(out, &ekT_std$Json$Int));
    EK_CHECK_U64(payload0(out).as.i, 1);
    ek_release(ek_val_ref(key));
    ek_release(j);

    j = parse_ok("{\"b\": 1, \"a\": 2}", &ekT_std$Json$Obj);
    EK_CHECK_U64(walk_obj_keys(payload0(j), keys, 2), 2);
    EK_CHECK_STR(keys[0], "b");
    EK_CHECK_STR(keys[1], "a");
    ek_release(j);

    j = parse_ok("{\"a\": 1, \"a\": 2}", &ekT_std$Json$Obj);
    EK_CHECK_U64(ek_dict_len(payload0(j)), 1);
    key = ek_str_from_cstr("a");
    EK_CHECK(ek_dict_get(payload0(j), ek_val_ref(key), &out));
    EK_CHECK_U64(payload0(out).as.i, 2);
    ek_release(ek_val_ref(key));
    ek_release(j);

    j = parse_ok("{}", &ekT_std$Json$Obj);
    EK_CHECK_U64(ek_dict_len(payload0(j)), 0);
    ek_release(j);
}

static void whitespace_around_every_token(void)
{
    ek_val j = parse_ok(" \t[\n 1\r,\n 2 ]\n", &ekT_std$Json$Arr);

    EK_CHECK_U64(ek_list_len(payload0(j)), 2);
    ek_release(j);
    j = parse_ok(" { \"a\" : true } ", &ekT_std$Json$Obj);
    EK_CHECK_U64(ek_dict_len(payload0(j)), 1);
    ek_release(j);
}

/* The error taxonomy: a missing value reads Empty, integer syntax
   outside the Int domain reads OutOfRange, and every malformed piece
   reads Invalid over the offset detail. */
static void empty_input_reads_empty(void)
{
    ek_val e = parse_err("", &ekT_std$ParseError$Empty);
    ek_val e2 = parse_err("  \n\t ", &ekT_std$ParseError$Empty);

    ek_release(e);
    ek_release(e2);
}

static void out_of_domain_integers_read_out_of_range(void)
{
    ek_val e = parse_err("9223372036854775808",
                         &ekT_std$ParseError$OutOfRange);
    ek_val e2 = parse_err("-9223372036854775809",
                          &ekT_std$ParseError$OutOfRange);

    ek_release(e);
    ek_release(e2);
}

/* The Invalid detail names the byte offset and the context; these are
   the reader's own wordings, pinned here. */
static void check_detail(const char *text, const char *want)
{
    ek_obj *in = ek_str_from_cstr(text);
    ek_val r = ek_json_parse(in);
    ek_val perr;
    ek_val detail;

    EK_CHECK(is_ctor(r, &ekT_std$Result$Err));
    perr = payload0(r);
    EK_CHECK(is_ctor(perr, &ekT_std$ParseError$Invalid));
    detail = payload0(perr);
    EK_CHECK_STR(ek_str_cstr(detail.as.ref), want);
    ek_release(r);
    ek_release(ek_val_ref(in));
}

static void invalid_inputs_name_the_offset(void)
{
    check_detail("x", "offset 0: unexpected character");
    check_detail("  x", "offset 2: unexpected character");
    check_detail("tru", "offset 0: bad literal");
    check_detail("01", "offset 0: leading zero in number");
    check_detail("-", "offset 1: bad number");
    check_detail("1.", "offset 2: bad number");
    check_detail("\"abc", "offset 4: unterminated string");
    check_detail("\"\\x\"", "offset 2: bad escape");
    check_detail("\"\\u00\"", "offset 5: incomplete unicode escape");
    check_detail("[1,]", "offset 3: unexpected character");
    check_detail("[1 2]", "offset 3: expected ',' or ']'");
    check_detail("{\"a\" 1}", "offset 5: expected ':'");
    check_detail("{\"a\":1,}", "offset 7: expected a key");
    check_detail("1 2", "offset 2: trailing text");
    check_detail("[1] [2]", "offset 4: trailing text");
    check_detail("\"a\nb\"", "offset 2: control character in string");
    check_detail("[", "offset 1: expected a value");
    check_detail("{", "offset 1: expected a key");
}

static void nesting_to_the_cap_stays_clean(void)
{
    char text[1200];
    ek_val j;
    size_t i;

    for (i = 0; i < 500; i++)
        text[i] = '[';
    for (i = 0; i < 500; i++)
        text[500 + i] = ']';
    text[1000] = '\0';
    j = parse_ok(text, &ekT_std$Json$Arr);
    ek_release(j);
}

int main(void)
{
    null_and_bools_build_over_the_identities();
    integers_read_as_int();
    fractions_and_exponents_read_as_float();
    nonfinite_spellings_round_trip();
    strings_decode_the_escapes();
    surrogates_parse_as_pairs();
    arrays_build_the_list_spine();
    objects_build_the_dictionary();
    whitespace_around_every_token();
    empty_input_reads_empty();
    out_of_domain_integers_read_out_of_range();
    invalid_inputs_name_the_offset();
    nesting_to_the_cap_stays_clean();
    return ek_test_report("rt/json");
}
