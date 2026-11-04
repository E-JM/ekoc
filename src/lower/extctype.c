#include <string.h>

#include "lower/internal.h"

/* The frozen C struct of a ctype and the pack and unpack walks
   between its two shapes: the exact-width fields in declaration order
   on the C side, the ek_val slots behind the header on the value
   side. The C structs join the unit beside the preamble, so every
   prototype and adapter reads them; the value families register
   through the ordinary machinery. */
/* The declaration record of a ctype symbol: the written field list,
   in declaration order. */
static const ek_node *ctype_record(const ek_symbol *tsym)
{
    const ek_node *dn = tsym ? tsym->decl : NULL;

    if (!dn || dn->kind != EK_N_CTYPE_DECL) {
        return NULL;
    }
    return dn->u.decl.body;
}

/* The declaration symbol a written name denotes when it names a
   ctype: shared with the shape classification and the adapters. */
const ek_symbol *lower_ctype_of_node(lower_ctx *c, const ek_node *tn)
{
    const ek_ck_row *row;
    const ek_type *t;

    if (!tn || tn->kind != EK_N_T_NAME || tn->u.path.count != 1) {
        return NULL;
    }
    row = lower_row(c, tn);
    t = row ? row->type : NULL;
    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym) {
        return NULL;
    }
    return lower_ctype_shape(c, t->u.nominal.sym) ? t->u.nominal.sym : NULL;
}

/* The written field list of a registered ctype: NULL when the
   declaration has no record body. */
static const ek_nlist *ctype_fields(const ek_symbol *tsym)
{
    const ek_node *rec = ctype_record(tsym);

    return rec && rec->kind == EK_N_T_RECORD ? &rec->u.kids : NULL;
}

/* The C struct name of a ctype: the module and the declaration's
   spelling. */
static const char *typedef_name(lower_ctx *c, const ek_symbol *tsym)
{
    return lower_str(c, "ekC_%s$%s", tsym->module, tsym->name);
}

/* The frozen C struct of a ctype: exact-width fields in declaration
   order, emitted once per declaration beside the preamble. The value
   family registers with it, so the pack and unpack walks see both
   shapes. */
/* The opaque pointer field spelling, the one multi-segment name a
   field may carry. */
static int ptr_field(const ek_node *tn)
{
    return tn && tn->kind == EK_N_T_NAME && tn->u.path.count == 3
        && strcmp(tn->u.path.data[0], "C") == 0
        && strcmp(tn->u.path.data[1], "Ptr") == 0
        && strcmp(tn->u.path.data[2], "Void") == 0;
}

/* The field node without its parenthesization. */
static const ek_node *bare_field(const ek_node *tn)
{
    return tn && tn->kind == EK_N_T_PAREN ? tn->u.wrap.inner : tn;
}

/* Every field is a machine word, a nested ctype, or the opaque
   pointer, and a nested one registers first, so its struct text
   precedes the outer's. */
static int fields_register(lower_ctx *c, const ek_nlist *kids)
{
    size_t i;

    for (i = 0; i < kids->count; i++) {
        const ek_node *tn = bare_field(kids->data[i]->u.t_field.type);
        const char *w;

        if (!tn || tn->kind != EK_N_T_NAME || tn->u.path.count != 1) {
            return ptr_field(tn);
        }
        w = lower_word_ctype(tn->u.path.data[0]);
        if (w) {
            continue;
        }
        if (!lower_ctype_of_node(c, tn)) {
            return 0;
        }
    }
    return 1;
}

/* The typedef text of the registered ctype, into the preamble block. */
static void emit_ctype_typedef(lower_ctx *c, const ek_symbol *tsym,
                               const ek_nlist *kids)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    if (!ek_buf_printf(&b, "typedef struct {")) {
        c->oom = 1;
    }
    for (i = 0; i < kids->count; i++) {
        const ek_node *tn = bare_field(kids->data[i]->u.t_field.type);
        const char *w = tn && tn->kind == EK_N_T_NAME
                            && tn->u.path.count == 1
                            ? lower_word_ctype(tn->u.path.data[0]) : NULL;
        const char *ct;
        const ek_symbol *nested = NULL;

        if (w) {
            ct = w;
        } else if (ptr_field(tn)) {
            ct = "void *";
        } else {
            nested = lower_ctype_of_node(c, tn);
            ct = nested ? typedef_name(c, nested) : NULL;
        }
        if (!ct || !ek_buf_printf(&b, " %s f_%s;", ct,
                                  kids->data[i]->u.t_field.name)) {
            c->oom = 1;
        }
    }
    if (!ek_buf_printf(&b, " } %s;\n\n", typedef_name(c, tsym))) {
        c->oom = 1;
    }
    if (b.data && !ek_buf_append_bytes(&c->cext, b.data, b.len)) {
        c->oom = 1;
    }
    ek_buf_destroy(&b);
}

lower_gtype *lower_ctype_shape(lower_ctx *c, const ek_symbol *tsym)
{
    lower_gtype *g;
    const ek_nlist *kids;
    const ek_symbol **slots;
    size_t i;

    if (!tsym) {
        return NULL;
    }
    g = lower_decl_repr(c, tsym);
    if (!g) {
        return NULL;
    }
    for (i = 0; i < c->nct; i++) {
        if (c->ctypedefs[i] == tsym) {
            return g;
        }
    }
    kids = ctype_fields(tsym);
    if (!kids || !fields_register(c, kids)) {
        return NULL;
    }
    slots = lower_grow(c, c->ctypedefs, &c->cct, c->nct + 1, sizeof *slots);
    if (!slots) {
        return NULL;
    }
    c->ctypedefs = slots;
    c->ctypedefs[c->nct++] = tsym;
    emit_ctype_typedef(c, tsym, kids);
    /* the pack and unpack walks both stand behind the family: the
       helper joins the unit with the typedef */
    g->constructed = 1;
    return g;
}

/* The compound literal of one struct level: base is the C expression
   of the value layer's struct pointer; a word field widens out of its
   slot, a nested ctype field recurses into its own literal. */
static const char *pack_fields(lower_ctx *c, const ek_symbol *tsym,
                               const char *base)
{
    const ek_nlist *kids = ctype_fields(tsym);
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    if (!ek_buf_printf(&b, "(ekC_%s$%s){", tsym->module, tsym->name)) {
        c->oom = 1;
    }
    for (i = 0; i < (kids ? kids->count : 0); i++) {
        const char *name = kids->data[i]->u.t_field.name;
        const ek_node *tn = kids->data[i]->u.t_field.type;
        const char *w;
        const char *field = lower_str(c, "%s->f_%s", base, name);
        const char *piece;

        tn = bare_field(tn);
        w = tn && tn->kind == EK_N_T_NAME && tn->u.path.count == 1
                ? lower_word_ctype(tn->u.path.data[0]) : NULL;
        if (w) {
            piece = lower_word_out(c, tn->u.path.data[0], field);
        } else if (ptr_field(tn)) {
            piece = lower_str(c, "ek_ptr_unwrap(%s)", field);
        } else {
            const ek_symbol *nested = lower_ctype_of_node(c, tn);

            piece = nested ? pack_fields(c, nested,
                                         lower_str(c,
                                             "((ekS_%s$%s *)(%s).as.ref)",
                                             nested->module, nested->name,
                                             field))
                           : "0";
        }
        if (!ek_buf_printf(&b, "%s.f_%s = %s", i ? ", " : "", name,
                           piece)) {
            c->oom = 1;
        }
    }
    if (!ek_buf_append_char(&b, '}')) {
        c->oom = 1;
    }
    return lower_take(c, &b);
}

/* The compound literal carrying the ek value expression src across
   the boundary by value; NULL when the declaration has no lowering. */
const char *lower_ctype_pack_expr(lower_ctx *c, const ek_symbol *tsym,
                                  const char *src)
{
    if (!lower_ctype_shape(c, tsym)) {
        return NULL;
    }
    return pack_fields(c, tsym, lower_str(c, "((ekS_%s$%s *)(%s).as.ref)",
                                          tsym->module, tsym->name, src));
}

/* The ek_val builder expression over the C struct expression src:
   the construction helper over the widened fields, a nested ctype
   field recursing into its own builder. */
static const char *unpack_fields(lower_ctx *c, const ek_symbol *tsym,
                                 const char *src)
{
    const ek_nlist *kids = ctype_fields(tsym);
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    if (!ek_buf_printf(&b, "ekn_%s$%s$rec(", tsym->module, tsym->name)) {
        c->oom = 1;
    }
    for (i = 0; i < (kids ? kids->count : 0); i++) {
        const char *name = kids->data[i]->u.t_field.name;
        const ek_node *tn = kids->data[i]->u.t_field.type;
        const char *w;
        const char *field = lower_str(c, "(%s).f_%s", src, name);
        const char *piece;

        tn = bare_field(tn);
        w = tn && tn->kind == EK_N_T_NAME && tn->u.path.count == 1
                ? lower_word_ctype(tn->u.path.data[0]) : NULL;
        if (w) {
            piece = lower_word_in(c, tn->u.path.data[0], field);
        } else if (ptr_field(tn)) {
            piece = lower_str(c, "ek_ptr_wrap(%s)", field);
        } else {
            const ek_symbol *nested = lower_ctype_of_node(c, tn);

            piece = nested ? unpack_fields(c, nested, field)
                           : "ek_val_unit()";
        }
        if (!ek_buf_printf(&b, "%s%s", i ? ", " : "", piece)) {
            c->oom = 1;
        }
    }
    if (!ek_buf_append_char(&b, ')')) {
        c->oom = 1;
    }
    return lower_take(c, &b);
}

const char *lower_ctype_unpack(lower_ctx *c, const ek_symbol *tsym,
                               const char *src)
{
    return lower_ctype_shape(c, tsym) ? unpack_fields(c, tsym, src) : NULL;
}



