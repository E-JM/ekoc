#include <string.h>

#include "lower/internal.h"

/* The intrinsic dispatch: a member call whose reading is a compiler
   row of the std namespaces routes to its runtime symbol over the
   receiver and the written arguments, and the status+out contracts
   wrap into the std Option and Result values. The table carries the
   rows the runtime surface backs; anything else on these receivers
   keeps the ordinary member path. */

/* The result wrapping of one row. */
typedef enum {
    W_LEN,        /* a size_t count: the Int value of it */
    W_PLAIN,      /* the call value is the result */
    W_REF,        /* an owned object result: the reference value of it */
    W_KEEP,       /* the widening identity: the argument kept */
    W_OPTION,     /* status + out slot: Some payload or None */
    W_OPTION_OBJ, /* an owned object or NULL: Some payload or None */
    W_PARSE,      /* parse status + out slot: Ok payload or Err the
                     ParseError of the kind */
    W_DECODE      /* ok flag + out object + detail: Ok payload or
                     Err (Invalid detail) */
} wrap_shape;

/* One routed row: the receiver family and member spelling, the
   runtime symbol (NULL is the widening identity), one letter per
   runtime argument -- v the value slot, o the object -- the result
   wrapping, the out slot, and the radix parsers' base. */
typedef struct {
    const char *family;
    const char *member;
    const char *sym;
    const char *args;
    wrap_shape wrap;
    out_kind out;
    int radix;
} intrinsic_row;

static const intrinsic_row rows[] = {
    /* The checked arithmetic of the domain words: quot and rem are
       the truncating pair the runtime divides with. */
    {"Int", "add", "ek_num_add", "vv", W_PLAIN, OUT_I64, 0},
    {"Int", "sub", "ek_num_sub", "vv", W_PLAIN, OUT_I64, 0},
    {"Int", "mul", "ek_num_mul", "vv", W_PLAIN, OUT_I64, 0},
    {"Int", "quot", "ek_num_div", "vv", W_PLAIN, OUT_I64, 0},
    {"Int", "rem", "ek_num_mod", "vv", W_PLAIN, OUT_I64, 0},
    {"Int", "neg", "ek_num_neg", "v", W_PLAIN, OUT_I64, 0},
    {"UInt", "add", "ek_num_add", "vv", W_PLAIN, OUT_I64, 0},
    {"UInt", "sub", "ek_num_sub", "vv", W_PLAIN, OUT_I64, 0},
    {"UInt", "mul", "ek_num_mul", "vv", W_PLAIN, OUT_I64, 0},
    {"UInt", "quot", "ek_num_div", "vv", W_PLAIN, OUT_I64, 0},
    {"UInt", "rem", "ek_num_mod", "vv", W_PLAIN, OUT_I64, 0},
    {"Float", "add", "ek_num_add", "vv", W_PLAIN, OUT_I64, 0},
    {"Float", "sub", "ek_num_sub", "vv", W_PLAIN, OUT_I64, 0},
    {"Float", "mul", "ek_num_mul", "vv", W_PLAIN, OUT_I64, 0},
    {"Float", "div", "ek_num_div", "vv", W_PLAIN, OUT_I64, 0},
    {"Float", "rem", "ek_num_mod", "vv", W_PLAIN, OUT_I64, 0},
    {"Float", "neg", "ek_num_neg", "v", W_PLAIN, OUT_I64, 0},

    /* The domain conversions: the narrowing rows report through
       Option, the total rows plain. */
    {"Int", "to_uint", "ek_try_to_uint", "v", W_OPTION, OUT_U64, 0},
    {"Int", "to_float", "ek_to_float", "v", W_PLAIN, OUT_I64, 0},
    {"Int", "to_i8", "ek_try_to_i8", "v", W_OPTION, OUT_I64, 0},
    {"Int", "to_i16", "ek_try_to_i16", "v", W_OPTION, OUT_I64, 0},
    {"Int", "to_i32", "ek_try_to_i32", "v", W_OPTION, OUT_I64, 0},
    {"Int", "to_i64", "ek_try_to_i64", "v", W_OPTION, OUT_I64, 0},
    {"Int", "to_u8", "ek_try_to_u8", "v", W_OPTION, OUT_U64, 0},
    {"Int", "to_u16", "ek_try_to_u16", "v", W_OPTION, OUT_U64, 0},
    {"Int", "to_u32", "ek_try_to_u32", "v", W_OPTION, OUT_U64, 0},
    {"Int", "to_u64", "ek_try_to_u64", "v", W_OPTION, OUT_U64, 0},
    {"Int", "to_f32", "ek_to_f32", "v", W_PLAIN, OUT_I64, 0},
    {"Int", "to_f64", "ek_to_f64", "v", W_PLAIN, OUT_I64, 0},
    /* The widening identities: the source word already travels in the
       slot of its domain. */
    {"Int", "from_i8", NULL, "v", W_KEEP, OUT_I64, 0},
    {"Int", "from_i16", NULL, "v", W_KEEP, OUT_I64, 0},
    {"Int", "from_i32", NULL, "v", W_KEEP, OUT_I64, 0},
    {"Int", "from_i64", NULL, "v", W_KEEP, OUT_I64, 0},
    {"UInt", "to_int", "ek_try_to_int", "v", W_OPTION, OUT_I64, 0},
    {"UInt", "to_float", "ek_to_float", "v", W_PLAIN, OUT_I64, 0},
    {"UInt", "to_i8", "ek_try_to_i8", "v", W_OPTION, OUT_I64, 0},
    {"UInt", "to_i16", "ek_try_to_i16", "v", W_OPTION, OUT_I64, 0},
    {"UInt", "to_i32", "ek_try_to_i32", "v", W_OPTION, OUT_I64, 0},
    {"UInt", "to_i64", "ek_try_to_i64", "v", W_OPTION, OUT_I64, 0},
    {"UInt", "to_u8", "ek_try_to_u8", "v", W_OPTION, OUT_U64, 0},
    {"UInt", "to_u16", "ek_try_to_u16", "v", W_OPTION, OUT_U64, 0},
    {"UInt", "to_u32", "ek_try_to_u32", "v", W_OPTION, OUT_U64, 0},
    {"UInt", "to_u64", "ek_try_to_u64", "v", W_OPTION, OUT_U64, 0},
    {"UInt", "to_f32", "ek_to_f32", "v", W_PLAIN, OUT_I64, 0},
    {"UInt", "to_f64", "ek_to_f64", "v", W_PLAIN, OUT_I64, 0},
    {"UInt", "from_u8", NULL, "v", W_KEEP, OUT_I64, 0},
    {"UInt", "from_u16", NULL, "v", W_KEEP, OUT_I64, 0},
    {"UInt", "from_u32", NULL, "v", W_KEEP, OUT_I64, 0},
    {"UInt", "from_u64", NULL, "v", W_KEEP, OUT_I64, 0},
    {"Float", "to_int", "ek_try_to_int", "v", W_OPTION, OUT_I64, 0},
    {"Float", "to_uint", "ek_try_to_uint", "v", W_OPTION, OUT_U64, 0},
    {"Float", "to_i8", "ek_try_to_i8", "v", W_OPTION, OUT_I64, 0},
    {"Float", "to_i16", "ek_try_to_i16", "v", W_OPTION, OUT_I64, 0},
    {"Float", "to_i32", "ek_try_to_i32", "v", W_OPTION, OUT_I64, 0},
    {"Float", "to_i64", "ek_try_to_i64", "v", W_OPTION, OUT_I64, 0},
    {"Float", "to_u8", "ek_try_to_u8", "v", W_OPTION, OUT_U64, 0},
    {"Float", "to_u16", "ek_try_to_u16", "v", W_OPTION, OUT_U64, 0},
    {"Float", "to_u32", "ek_try_to_u32", "v", W_OPTION, OUT_U64, 0},
    {"Float", "to_u64", "ek_try_to_u64", "v", W_OPTION, OUT_U64, 0},
    {"Float", "to_f32", "ek_to_f32", "v", W_PLAIN, OUT_I64, 0},
    {"Float", "to_f64", "ek_to_f64", "v", W_PLAIN, OUT_I64, 0},
    {"Float", "from_f32", NULL, "v", W_KEEP, OUT_I64, 0},
    {"Float", "from_f64", NULL, "v", W_KEEP, OUT_I64, 0},

    /* The parse family: the kind wraps into the Result's ParseError. */
    {"Int", "parse", "ek_parse_int", "o", W_PARSE, OUT_I64, 0},
    {"Int", "parse_hex", "ek_parse_int_radix", "o", W_PARSE, OUT_I64, 16},
    {"Int", "parse_oct", "ek_parse_int_radix", "o", W_PARSE, OUT_I64, 8},
    {"Int", "parse_bin", "ek_parse_int_radix", "o", W_PARSE, OUT_I64, 2},
    {"UInt", "parse", "ek_parse_uint", "o", W_PARSE, OUT_U64, 0},
    {"UInt", "parse_hex", "ek_parse_uint_radix", "o", W_PARSE, OUT_U64, 16},
    {"UInt", "parse_oct", "ek_parse_uint_radix", "o", W_PARSE, OUT_U64, 8},
    {"UInt", "parse_bin", "ek_parse_uint_radix", "o", W_PARSE, OUT_U64, 2},
    {"Float", "parse", "ek_parse_float", "o", W_PARSE, OUT_F64, 0},

    /* Char's code-point bridge. */
    {"Char", "to_code", "ek_char_to_code", "v", W_PLAIN, OUT_I64, 0},
    {"Char", "from_code", "ek_try_char_from_code", "v", W_OPTION,
     OUT_U32, 0},

    /* The string's scalar sequence and its byte form. */
    {"String", "length", "ek_str_length", "o", W_LEN, OUT_I64, 0},
    {"String", "chars", "ek_str_chars", "o", W_PLAIN, OUT_I64, 0},
    {"String", "from_char", "ek_str_from_char", "v", W_REF, OUT_I64, 0},
    {"String", "from_chars", "ek_str_from_chars", "o", W_REF, OUT_I64, 0},
    {"String", "append", "ek_str_concat", "oo", W_REF, OUT_I64, 0},
    {"String", "to_bytes", "ek_str_to_bytes", "o", W_REF, OUT_I64, 0},

    /* The byte sequence, its integer bridge, and its text forms. */
    {"Bytes", "length", "ek_bytes_len", "o", W_LEN, OUT_I64, 0},
    {"Bytes", "get", "ek_bytes_get", "ov", W_OPTION, OUT_I64, 0},
    {"Bytes", "append", "ek_bytes_concat", "oo", W_REF, OUT_I64, 0},
    {"Bytes", "to_ints", "ek_bytes_to_ints", "o", W_PLAIN, OUT_I64, 0},
    {"Bytes", "from_ints", "ek_bytes_from_ints", "v", W_OPTION_OBJ,
     OUT_I64, 0},
    {"Bytes", "to_hex", "ek_bytes_to_hex", "o", W_REF, OUT_I64, 0},
    {"Bytes", "from_hex", "ek_bytes_from_hex", "o", W_DECODE, OUT_I64, 0},
    {"Bytes", "to_string", "ek_bytes_to_string", "o", W_DECODE, OUT_I64,
     0},

    /* The containers: the append the operator builds on, the
       dictionary's persistent updates, and the counts. */
    {"List", "append", "ek_list_append", "vv", W_PLAIN, OUT_I64, 0},
    {"Dict", "length", "ek_dict_len", "v", W_LEN, OUT_I64, 0},
    {"Dict", "get", "ek_dict_get", "vv", W_OPTION, OUT_VAL, 0},
    {"Dict", "insert", "ek_dict_set", "vvv", W_PLAIN, OUT_I64, 0},
    {"Dict", "remove", "ek_dict_remove", "vv", W_PLAIN, OUT_I64, 0},
    {"Dict", "merge", "ek_dict_merge", "vv", W_PLAIN, OUT_I64, 0},

    /* The range's namespace: the struct value and the callback ride
       as values, and the traversals run natively in the runtime. */
    {"Range", "length", "ek_range_length", "v", W_PLAIN, OUT_I64, 0},
    {"Range", "to_list", "ek_range_to_list", "v", W_PLAIN, OUT_I64, 0},
    {"Range", "each", "ek_range_each", "vv", W_PLAIN, OUT_I64, 0},
    {"Range", "each_with_index", "ek_range_each_index", "vv", W_PLAIN,
     OUT_I64, 0},
    {"Range", "map", "ek_range_map", "vv", W_PLAIN, OUT_I64, 0},
    {"Range", "fold", "ek_range_fold", "vvv", W_PLAIN, OUT_I64, 0},

    /* The lexical path vocabulary over String. */
    {"Path", "join", "ek_path_join", "oo", W_REF, OUT_I64, 0},
    {"Path", "normalize", "ek_path_normalize", "o", W_REF, OUT_I64, 0},
    {"Path", "is_absolute", "ek_path_is_absolute", "o", W_PLAIN, OUT_I64,
     0},

    /* The floor family of the integers, the division the time
       companion's civil arithmetic builds on, and the absolute value
       under the arithmetic rule. */
    {"Int", "div", "ek_num_fdiv", "vv", W_PLAIN, OUT_I64, 0},
    {"Int", "mod", "ek_num_fmod", "vv", W_PLAIN, OUT_I64, 0},
    {"Int", "abs", "ek_num_abs", "v", W_PLAIN, OUT_I64, 0},
    {"Float", "abs", "ek_num_abs", "v", W_PLAIN, OUT_I64, 0},
};

/* The capability channel owns the File, In, Rand, and Proc families:
   the compiler-provided rows of Clause 10.8 route through capwrap.c. */
static int cap_family(const char *family)
{
    return strcmp(family, "File") == 0 || strcmp(family, "In") == 0
        || strcmp(family, "Rand") == 0 || strcmp(family, "Proc") == 0;
}

/* The row of one family and member spelling. */
static const intrinsic_row *lookup(const char *family, const char *member)
{
    size_t i;

    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        if (strcmp(rows[i].family, family) == 0
            && strcmp(rows[i].member, member) == 0) {
            return &rows[i];
        }
    }
    return NULL;
}

/* The family a receiver type names: the structural containers, the
   std placeholder scalars, and the capability module's own types --
   the File family's rows route through the same table. */
const char *lower_intrinsic_family(const ek_type *t)
{
    if (!t) {
        return NULL;
    }
    if (t->kind == EK_T_LIST) {
        return "List";
    }
    if (t->kind == EK_T_DICT) {
        return "Dict";
    }
    if (t->kind == EK_T_NOMINAL && t->u.nominal.sym && t->u.nominal.sym->module
        && (strcmp(t->u.nominal.sym->module, "std") == 0
            || strcmp(t->u.nominal.sym->module, "cap") == 0)) {
        return t->u.nominal.sym->name;
    }
    return NULL;
}

/* The joined runtime arguments: object slots pass the reference, the
   value slots the value. */
static const char *value_args(lower_ctx *c, const intrinsic_row *row,
                              const char **as, size_t na)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; i < na; i++) {
        if (i && !ek_buf_append_str(&b, ", ")) {
            c->oom = 1;
        }
        if (row->args[i] == 'o'
            ? !ek_buf_printf(&b, "%s.as.ref", as[i])
            : !ek_buf_append_str(&b, as[i])) {
            c->oom = 1;
        }
    }
    return lower_take(c, &b);
}

/* The unsupported arity: the receiver and the arguments still evaluate
   and die. */
static lower_val discard_call(lower_ctx *c, lower_scope *scope,
                              const ek_node *e, int has_recv)
{
    const ek_node *recv = e->u.call.callee->u.field.recv;
    size_t i;

    if (has_recv && recv) {
        lower_val a = lower_sub(c, scope, recv);
        const char *t = lower_as_temp(c, scope, &a);

        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
    for (i = 0; i < e->u.call.args.count; i++) {
        lower_val a = lower_sub(c, scope, e->u.call.args.data[i]);
        const char *t = lower_as_temp(c, scope, &a);

        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
    return lower_val_unit();
}

/* The plain shapes: the count, the call value, the owned object, and
   the widening identity. */
static lower_val emit_plain(lower_ctx *c, lower_scope *scope,
                            const intrinsic_row *row, const char *args,
                            const char **as)
{
    const char *t = lower_temp(c, scope);

    lower_pad(c);
    if (!row->sym) {
        lower_emitf(c, "ek_val %s = ek_keep(%s);\n", t, as[0]);
    } else if (row->wrap == W_LEN) {
        lower_emitf(c, "ek_val %s = ek_val_int((int64_t)%s(%s));\n", t,
                    row->sym, args);
    } else if (row->wrap == W_REF) {
        lower_emitf(c, "ek_val %s = ek_val_ref(%s(%s));\n", t, row->sym,
                    args);
    } else {
        lower_emitf(c, "ek_val %s = %s(%s);\n", t, row->sym, args);
    }
    return (lower_val){ t, 1 };
}

/* The receiver and written arguments as the runtime argument temps,
   in source order; NULL where the arity does not match. */
static const char **prep_temps(lower_ctx *c, lower_scope *scope,
                               const ek_node *e, const ek_node *callee,
                               size_t na, int has_recv)
{
    const char **as;
    size_t cap = 0, written = na - (size_t)has_recv, i;

    as = lower_grow(c, NULL, &cap, na ? na : 1, sizeof *as);
    if (!as) {
        c->oom = 1;
        return NULL;
    }
    if (has_recv) {
        lower_val a = lower_sub(c, scope, callee->u.field.recv);

        as[0] = lower_as_temp(c, scope, &a);
    }
    for (i = 0; i < written; i++) {
        lower_val a = lower_sub(c, scope, e->u.call.args.data[i]);

        as[has_recv + i] = lower_as_temp(c, scope, &a);
    }
    return as;
}

/* Releases the runtime argument temps, most recent first. */
static void release_temps(lower_ctx *c, const char **as, size_t na)
{
    size_t i = na;

    while (i > 0) {
        i--;
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", as[i]);
    }
}

/* The row's result wrapping, dispatched over its shape; the plain
   shapes build in emit_plain over the argument temps. */
static lower_val emit_wrap(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const intrinsic_row *row,
                           const char *args, const char **as)
{
    switch (row->wrap) {
    case W_OPTION:
        return lower_wrap_option(c, scope, e, row->sym, args, row->out);
    case W_OPTION_OBJ:
        return lower_wrap_option_obj(c, scope, e, row->sym, args);
    case W_PARSE:
        return lower_wrap_parse(c, scope, e, row->sym, args, row->out,
                                row->radix);
    case W_DECODE:
        return lower_wrap_decode(c, scope, e, row->sym, args);
    default:
        return emit_plain(c, scope, row, args, as);
    }
}

lower_val lower_intrinsic_call(lower_ctx *c, lower_scope *scope,
                               const ek_node *e, const ek_node *callee,
                               const char *family, int has_recv)
{
    const intrinsic_row *row = lookup(family, callee->u.field.name);
    const char **as;
    size_t na, written;
    lower_val out;

    if (!row && cap_family(family)) {
        return lower_cap_call(c, scope, e, callee, family, has_recv);
    }
    if (!row) {
        return (lower_val){ NULL, 0 };
    }
    na = strlen(row->args);
    written = na - (size_t)has_recv;
    if (e->u.call.args.count != written) {
        lower_error(c, e, "the call of `%s.%s` is malformed", family,
                    callee->u.field.name);
        return discard_call(c, scope, e, has_recv);
    }
    as = prep_temps(c, scope, e, callee, na, has_recv);
    if (!as) {
        return lower_val_unit();
    }
    {
        const char *args = value_args(c, row, as, na);

        out = emit_wrap(c, scope, e, row, args, as);
    }
    release_temps(c, as, na);
    return out;
}
