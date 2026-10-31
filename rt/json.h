#ifndef EK_RT_JSON_H
#define EK_RT_JSON_H

#include <stddef.h>

#include "ekort.h"

/* The scan state and helpers shared by the JSON reader's files. Not
   part of the public surface and not visible to compiled programs. */

/* The deepest container nesting one document may reach; past it the
   scan panics rather than exhausting the C stack. */
#define JSON_MAX_DEPTH 512

/* The scan: the input bytes, the position, and the nesting depth. */
typedef struct {
    const char *p;
    size_t len;
    size_t pos;
    int depth;
} json_scan;

/* The outcome of one parse step. */
typedef enum {
    JSON_OK,
    JSON_INVALID,
    JSON_RANGE
} json_status;

/* The byte at the position, or -1 past the end. */
int json_peek(const json_scan *s);

/* The four JSON whitespace characters. */
void json_skip_ws(json_scan *s);

/* Writes the failure detail naming the offset and returns INVALID. */
json_status json_fail_at(const json_scan *s, ek_obj **detail,
                         const char *why);

/* Consumes the expected byte, or writes the failure and returns zero. */
int json_expect(json_scan *s, char c, ek_obj **detail, const char *why);

/* Matches the exact spelling at the position and moves past it. */
int json_match_lit(json_scan *s, const char *word);

/* Enters one container level, panicking past the cap. */
void json_nest_in(json_scan *s);

/* One string: the opening quote stands at the position. OK writes an
   owned String value into *out. */
json_status json_parse_string(json_scan *s, ek_val *out, ek_obj **detail);

/* One number at the position: syntax without a fraction or an exponent
   reads as Int (JSON_RANGE when the digits leave the Int domain),
   anything else as Float. */
json_status json_parse_number(json_scan *s, ek_val *out, ek_obj **detail);

/* The std Json constructors and the ParseError of the parse family,
   defined in json.c and shared with jsontext.c. A payload argument is
   owned by the caller and owned by the built value afterwards. */
ek_val json_null(void);
ek_val json_bool(int b);
ek_val json_int(int64_t n);
ek_val json_float(double x);
ek_val json_str(ek_val s);
ek_val json_arr(ek_val list);
ek_val json_obj(ek_val dict);
ek_val parse_error_empty(void);
ek_val parse_error_invalid(ek_val detail);
ek_val parse_error_range(void);

#endif
