#include "internal.h"

#include "json.h"

#include <math.h>
#include <stdlib.h>

/* The JSON document reader. One recursive descent over the stored
   bytes builds the std Json ADT and wraps it in the Result the parse
   family returns: Ok the value, Err Empty when the document holds no
   value at all, Err (Invalid detail) naming the offset and the context
   of the first malformed piece, and Err OutOfRange when integer syntax
   leaves the Int domain. The scanner also accepts the three non-finite
   spellings the Float display channel produces, so parse and show
   round-trip over them. The string decoder and the number scanner live
   in jsontext.c.

   The builders stamp the emitted unit's Json and ParseError identities
   onto the values, the way the capability builders do; this file is
   their only reference, so a program that never parses JSON links
   without the unit defining the identities at all. */

static json_status parse_value(json_scan *s, ek_val *out, ek_obj **detail);
static json_status parse_array(json_scan *s, ek_val *out, ek_obj **detail);
static json_status parse_object(json_scan *s, ek_val *out, ek_obj **detail);

/* The std Json constructors: the seven variants of the interchange
   type, and the ParseError of the parse family. A payload argument is
   owned by the caller and owned by the built value afterwards. */

/* Wraps an owned payload into its one-payload variant. */
static ek_val json_wrapped(const ek_typeinfo *info, ek_val payload)
{
    ek_val v = ek_gen_one(info, payload);

    ek_release(payload);
    return v;
}

ek_val json_null(void)
{
    return ek_gen_zero(&ekT_std$Json$Null);
}

ek_val json_bool(int b)
{
    return ek_gen_one(&ekT_std$Json$Bool, ek_bool(b));
}

ek_val json_int(int64_t n)
{
    return ek_gen_one(&ekT_std$Json$Int, ek_val_int(n));
}

ek_val json_float(double x)
{
    return ek_gen_one(&ekT_std$Json$Float, ek_val_float(x));
}

ek_val json_str(ek_val s)
{
    return json_wrapped(&ekT_std$Json$Str, s);
}

ek_val json_arr(ek_val list)
{
    return json_wrapped(&ekT_std$Json$Arr, list);
}

ek_val json_obj(ek_val dict)
{
    return json_wrapped(&ekT_std$Json$Obj, dict);
}

ek_val parse_error_empty(void)
{
    return ek_gen_zero(&ekT_std$ParseError$Empty);
}

ek_val parse_error_invalid(ek_val detail)
{
    return json_wrapped(&ekT_std$ParseError$Invalid, detail);
}

ek_val parse_error_range(void)
{
    return ek_gen_zero(&ekT_std$ParseError$OutOfRange);
}

/* The keyword spellings a value position can open with: the three JSON
   literals and the three non-finite spellings, then the signed number
   scan. */
static json_status parse_keyword(json_scan *s, ek_val *out, ek_obj **detail)
{
    switch (json_peek(s)) {
    case 't':
        if (json_match_lit(s, "true")) {
            *out = json_bool(1);
            return JSON_OK;
        }
        return json_fail_at(s, detail, "bad literal");
    case 'f':
        if (json_match_lit(s, "false")) {
            *out = json_bool(0);
            return JSON_OK;
        }
        return json_fail_at(s, detail, "bad literal");
    case 'n':
        if (json_match_lit(s, "null")) {
            *out = json_null();
            return JSON_OK;
        }
        return json_fail_at(s, detail, "bad literal");
    case 'N':
        if (json_match_lit(s, "NaN")) {
            *out = json_float(NAN);
            return JSON_OK;
        }
        return json_fail_at(s, detail, "bad literal");
    case 'I':
        if (json_match_lit(s, "Inf")) {
            *out = json_float(INFINITY);
            return JSON_OK;
        }
        return json_fail_at(s, detail, "bad literal");
    case '-':
        if (json_match_lit(s, "-Inf")) {
            *out = json_float(-INFINITY);
            return JSON_OK;
        }
        return json_parse_number(s, out, detail);
    default:
        return json_parse_number(s, out, detail);
    }
}

/* One value, dispatched on the first byte. */
static json_status parse_value(json_scan *s, ek_val *out, ek_obj **detail)
{
    int c = json_peek(s);

    switch (c) {
    case '{':
        return parse_object(s, out, detail);
    case '[':
        return parse_array(s, out, detail);
    case '"':
        {
            ek_val str;
            json_status st = json_parse_string(s, &str, detail);

            if (st != JSON_OK)
                return st;
            *out = json_str(str); /* takes the reference over */
            return JSON_OK;
        }
    case 't':
    case 'f':
    case 'n':
    case 'N':
    case 'I':
    case '-':
        return parse_keyword(s, out, detail);
    default:
        if (c >= '0' && c <= '9')
            return json_parse_number(s, out, detail);
        return json_fail_at(s, detail, c < 0 ? "expected a value"
                                             : "unexpected character");
    }
}

/* One array: the elements collect into a buffer, and the spine builds
   from the right once the scan succeeds, so the walk stays
   constant-stack per element. */
static json_status parse_array(json_scan *s, ek_val *out, ek_obj **detail)
{
    ek_elems elems;
    ek_val acc = ek_nil_val();
    json_status st = JSON_OK;

    elems.data = NULL;
    elems.len = 0;
    elems.cap = 0;
    json_nest_in(s);
    s->pos++;
    json_skip_ws(s);
    if (json_peek(s) != ']') {
        for (;;) {
            ek_val elem;

            st = parse_value(s, &elem, detail);
            if (st != JSON_OK)
                break;
            ek_elems_add(&elems, elem);
            json_skip_ws(s);
            if (json_peek(s) == ',') {
                s->pos++;
                json_skip_ws(s);
                continue;
            }
            if (json_peek(s) == ']')
                break;
            st = json_fail_at(s, detail, "expected ',' or ']'");
            break;
        }
    }
    if (st == JSON_OK) {
        s->pos++;
        acc = ek_elems_list(&elems);
        *out = json_arr(acc); /* takes the spine's reference over */
    } else {
        ek_elems_drain(&elems);
        ek_release(acc);
    }
    s->depth--;
    return st;
}

/* One member: a string key, a colon, the value, and the entry landed
   in the dictionary under construction. A duplicate key replaces the
   value and keeps its place. */
static json_status parse_member(json_scan *s, ek_val *dict, ek_obj **detail)
{
    ek_val key;
    ek_val val;
    ek_val fresh;
    json_status st;

    if (json_peek(s) != '"')
        return json_fail_at(s, detail, "expected a key");
    st = json_parse_string(s, &key, detail);
    if (st != JSON_OK)
        return st;
    json_skip_ws(s);
    if (!json_expect(s, ':', detail, "expected ':'")) {
        ek_release(key);
        return JSON_INVALID;
    }
    json_skip_ws(s);
    st = parse_value(s, &val, detail);
    if (st != JSON_OK) {
        ek_release(key);
        return st;
    }
    fresh = ek_dict_set(*dict, key, val);
    ek_release(key);
    ek_release(val);
    ek_release(*dict);
    *dict = fresh;
    return JSON_OK;
}

/* One object: the entries land in a dictionary that preserves
   insertion order. */
static json_status parse_object(json_scan *s, ek_val *out, ek_obj **detail)
{
    ek_val dict = ek_dict_new();
    json_status st = JSON_OK;

    json_nest_in(s);
    s->pos++;
    json_skip_ws(s);
    if (json_peek(s) != '}') {
        for (;;) {
            st = parse_member(s, &dict, detail);
            if (st != JSON_OK)
                break;
            json_skip_ws(s);
            if (json_peek(s) == ',') {
                s->pos++;
                json_skip_ws(s);
                continue;
            }
            if (json_peek(s) == '}')
                break;
            st = json_fail_at(s, detail, "expected ',' or '}'");
            break;
        }
    }
    if (st == JSON_OK) {
        s->pos++;
        *out = json_obj(dict);
    } else {
        ek_release(dict);
    }
    s->depth--;
    return st;
}

/* The Result the parse family returns: Ok the value, or Err the
   ParseError. Both take ownership of the argument. */
static ek_val parse_result(int ok, ek_val value, ek_val reason)
{
    ek_val result = ok ? ek_gen_result_ok(value) : ek_gen_result_err(reason);

    ek_release(value);
    ek_release(reason);
    return result;
}

/* The Err side of a failed value scan: the kind decides the ParseError
   variant, and the owned detail moves into the Invalid one, whose
   wrapping releases it. */
static ek_val parse_failed(json_status st, ek_obj *detail)
{
    if (st == JSON_RANGE)
        return parse_error_range();
    return parse_error_invalid(ek_val_ref(detail));
}

/* The parse family's Json entry: the whole text as one document. The
   returned Result is the caller's to release. */
ek_val ek_json_parse(ek_obj *text)
{
    json_scan s;
    ek_obj *detail = NULL;
    ek_val out;
    ek_val reason;
    json_status st;

    s.p = ek_str_cstr(text);
    s.len = ek_str_len(text);
    s.pos = 0;
    s.depth = 0;
    json_skip_ws(&s);
    if (json_peek(&s) < 0) {
        return parse_result(0, ek_val_unit(), parse_error_empty());
    }
    st = parse_value(&s, &out, &detail);
    if (st != JSON_OK) {
        reason = parse_failed(st, detail);
        return parse_result(0, ek_val_unit(), reason);
    }
    json_skip_ws(&s);
    if (json_peek(&s) >= 0) {
        ek_release(out);
        json_fail_at(&s, &detail, "trailing text");
        reason = parse_failed(JSON_INVALID, detail);
        return parse_result(0, ek_val_unit(), reason);
    }
    return parse_result(1, out, ek_val_unit());
}
