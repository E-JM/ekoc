#include <string.h>

#include "lower/internal.h"

/* The shape side of the marshalling whitelist: the machine words'
   exact widths and slots, the scalar expressions crossing in either
   direction, and the classification of a written annotation node.
   The C namespace rows live only in the written form, so the shape is
   re-derived at the boundary rather than read off the check row. */

/* The C type of each machine word's spelling. */
const char *lower_word_ctype(const char *name)
{
    static const struct {
        const char *name;
        const char *ctype;
    } words[] = {
        {"I8", "int8_t"},    {"I16", "int16_t"},  {"I32", "int32_t"},
        {"I64", "int64_t"},  {"U8", "uint8_t"},   {"U16", "uint16_t"},
        {"U32", "uint32_t"}, {"U64", "uint64_t"}, {"F32", "float"},
        {"F64", "double"},
    };
    size_t i;

    for (i = 0; i < sizeof words / sizeof *words; i++) {
        if (strcmp(words[i].name, name) == 0) {
            return words[i].ctype;
        }
    }
    return NULL;
}

/* The signedness of a machine word: 'i' the Int slot, 'u' the UInt
   slot, 'f' the Float slot. */
static char word_slot(const char *name)
{
    if (name[0] == 'I') {
        return 'i';
    }
    if (name[0] == 'U') {
        return 'u';
    }
    return 'f';
}

/* The C expression reading the word's payload out of the value slot,
   narrowed to the exact width. */
const char *lower_word_out(lower_ctx *c, const char *word, const char *v)
{
    switch (word_slot(word)) {
    case 'i':
        return lower_str(c, "(%s)(%s).as.i", lower_word_ctype(word), v);
    case 'u':
        return lower_str(c, "(%s)(%s).as.u", lower_word_ctype(word), v);
    default:
        return lower_str(c, "(%s)(%s).as.f", lower_word_ctype(word), v);
    }
}

/* The C expression rebuilding the value slot from the exact-width C
   value, widened back. */
const char *lower_word_in(lower_ctx *c, const char *word, const char *r)
{
    switch (word_slot(word)) {
    case 'i':
        return lower_str(c, "ek_val_int((int64_t)(%s))", r);
    case 'u':
        return lower_str(c, "ek_val_uint((uint64_t)(%s))", r);
    default:
        return lower_str(c, "ek_val_float((double)(%s))", r);
    }
}

/* The C expression reading one scalar shape out of the value temp v;
   NULL for the shapes that marshal as structs or callbacks. Shared by
   the extern call path and the callback adapters. */
const char *lower_scalar_out_expr(lower_ctx *c, const lower_mshape *s,
                                  const char *v)
{
    switch (s->kind) {
    case MS_BOOL:
        return lower_str(c, "(int)((%s).as.i != 0)", v);
    case MS_INT:
        return lower_str(c, "(int64_t)(%s).as.i", v);
    case MS_UINT:
        return lower_str(c, "(uint64_t)(%s).as.u", v);
    case MS_FLOAT:
        return lower_str(c, "(double)(%s).as.f", v);
    case MS_WORD:
        return lower_word_out(c, s->word, v);
    case MS_SIZE:
        return lower_str(c, "(size_t)(%s).as.u", v);
    case MS_PTR:
        return lower_str(c, "ek_ptr_unwrap(%s)", v);
    case MS_STRING:
        return lower_str(c, "ek_str_cstr((%s).as.ref)", v);
    case MS_BYTES:
        return lower_str(c, "ek_bytes_data((%s).as.ref)", v);
    default:
        return NULL;
    }
}

/* The marshalling shape of one written annotation node, classified
   from the annotation itself: the whitelist rows carry their C
   spelling, and the C namespace rows live only in the written form.
   The ctype rows register their two shapes on the way through. */
/* The shape of a one-segment name: the scalars by spelling, anything
   else through its ctype declaration. */
static lower_mshape ms_of_name(lower_ctx *c, const ek_node *tn)
{
    lower_mshape s;

    memset(&s, 0, sizeof s);
    s.kind = MS_BAD;
    {
        const char *n = tn->u.path.data[0];

        if (strcmp(n, "_") == 0) {
            s.kind = MS_UNIT;
        } else if (strcmp(n, "Bool") == 0) {
            s.kind = MS_BOOL;
        } else if (strcmp(n, "Int") == 0) {
            s.kind = MS_INT;
        } else if (strcmp(n, "UInt") == 0) {
            s.kind = MS_UINT;
        } else if (strcmp(n, "Float") == 0) {
            s.kind = MS_FLOAT;
        } else if (strcmp(n, "String") == 0) {
            s.kind = MS_STRING;
        } else if (strcmp(n, "Bytes") == 0) {
            s.kind = MS_BYTES;
        } else if (lower_word_ctype(n)) {
            s.kind = MS_WORD;
            s.word = n;
        } else {
            const ek_symbol *ct = lower_ctype_of_node(c, tn);

            if (ct) {
                s.kind = MS_CTYPE;
                s.ctype = ct;
            }
        }
    }
    return s;
}

/* The shape of a C namespace spelling: the size ferry and the opaque
   pointer, nothing else. */
static lower_ms_kind ms_of_c_name(const ek_node *tn)
{
    if (tn->u.path.count == 2 && strcmp(tn->u.path.data[0], "C") == 0
        && strcmp(tn->u.path.data[1], "Size") == 0) {
        return MS_SIZE;
    }
    if (tn->u.path.count == 3 && strcmp(tn->u.path.data[0], "C") == 0
        && strcmp(tn->u.path.data[1], "Ptr") == 0
        && strcmp(tn->u.path.data[2], "Void") == 0) {
        return MS_PTR;
    }
    return MS_BAD;
}

lower_mshape lower_ms_of(lower_ctx *c, const ek_node *tn)
{
    lower_mshape s;

    memset(&s, 0, sizeof s);
    s.kind = MS_BAD;
    tn = tn && tn->kind == EK_N_T_PAREN ? tn->u.wrap.inner : tn;
    if (!tn) {
        s.kind = MS_UNIT; /* an elided result is the unit */
        return s;
    }
    if (tn->kind == EK_N_T_FN) {
        s.kind = MS_CLOSURE;
        s.fn = tn;
        return s;
    }
    if (tn->kind == EK_N_T_APPLY) {
        const ek_node *head = tn->u.t_apply.head;
        const ek_symbol *inner;

        if (!head || head->kind != EK_N_T_NAME || head->u.path.count != 2
            || strcmp(head->u.path.data[0], "C") != 0
            || strcmp(head->u.path.data[1], "In") != 0
            || tn->u.t_apply.args.count != 1) {
            return s;
        }
        inner = lower_ctype_of_node(c, tn->u.t_apply.args.data[0]);
        if (!inner) {
            return s;
        }
        s.kind = MS_CIN;
        s.ctype = inner;
        return s;
    }
    if (tn->kind != EK_N_T_NAME) {
        return s;
    }
    if (tn->u.path.count == 1) {
        return ms_of_name(c, tn);
    }
    s.kind = ms_of_c_name(tn);
    return s;
}

/* The parameter's C type text; NULL where the shape has no
   parameter lowering. */
const char *lower_ms_param_type(lower_ctx *c, const lower_mshape *s)
{
    switch (s->kind) {
    case MS_BOOL:
        return "int";
    case MS_INT:
        return "int64_t";
    case MS_UINT:
        return "uint64_t";
    case MS_FLOAT:
        return "double";
    case MS_WORD:
        return lower_word_ctype(s->word);
    case MS_SIZE:
        return "size_t";
    case MS_PTR:
        return "void *";
    case MS_STRING:
        return "const char *";
    case MS_BYTES:
        return "const void *";
    case MS_CTYPE:
        return lower_ctype_shape(c, s->ctype)
                   ? lower_str(c, "ekC_%s$%s", s->ctype->module,
                               s->ctype->name)
                   : NULL;
    case MS_CIN:
        return lower_ctype_shape(c, s->ctype)
                   ? lower_str(c, "const ekC_%s$%s *", s->ctype->module,
                               s->ctype->name)
                   : NULL;
    default:
        return NULL;
    }
}

/* The callback parameter's C type: the whitelist rows in the C-to-Eko
   direction, where a byte block or a copy-in struct has no shape and
   no closure nests. */
const char *lower_cbr_param_type(lower_ctx *c, const lower_mshape *s)
{
    switch (s->kind) {
    case MS_BYTES:
    case MS_CIN:
    case MS_CLOSURE:
    case MS_UNIT:
        return NULL;
    default:
        return lower_ms_param_type(c, s);
    }
}

/* The callback result's C type: the whitelist rows in the Eko-to-C
   direction. */
const char *lower_cbr_ret_type(lower_ctx *c, const lower_mshape *s)
{
    switch (s->kind) {
    case MS_UNIT:
        return "void";
    case MS_STRING:
        return "const char *";
    case MS_BYTES:
        return "const void *";
    case MS_CIN:
    case MS_CLOSURE:
        return NULL;
    default:
        return lower_ms_param_type(c, s);
    }
}

/* The ek_val expression rebuilding one scalar shape from the C value
   r; NULL for the shapes that marshal as statements or structs. */
const char *lower_scalar_in_expr(lower_ctx *c, const lower_mshape *s,
                                 const char *r)
{
    switch (s->kind) {
    case MS_BOOL:
        return lower_str(c, "ek_bool(%s)", r);
    case MS_INT:
        return lower_str(c, "ek_val_int(%s)", r);
    case MS_UINT:
        return lower_str(c, "ek_val_uint(%s)", r);
    case MS_FLOAT:
        return lower_str(c, "ek_val_float(%s)", r);
    case MS_WORD:
        return lower_word_in(c, s->word, r);
    case MS_SIZE:
        return lower_str(c, "ek_val_uint((uint64_t)(%s))", r);
    case MS_PTR:
        return lower_str(c, "ek_ptr_wrap(%s)", r);
    case MS_CTYPE:
        return lower_ctype_unpack(c, s->ctype, r);
    default:
        return NULL;
    }
}
