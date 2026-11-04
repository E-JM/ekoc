#include <string.h>

#include "lower/internal.h"

/* Shared classification and plumbing for the derived services: the
   scalar slots a receiver can occupy, the service buffer's writer with
   its string-building helpers, and the shape and variant readers that
   feed the body emitters. */

lower_scalar lower_scalar_kind(const ek_type *t)
{
    static const struct {
        const char *name;
        lower_scalar slot;
    } names[] = {
        {"Int", EKSC_INT},   {"I8", EKSC_INT},    {"I16", EKSC_INT},
        {"I32", EKSC_INT},   {"I64", EKSC_INT},   {"UInt", EKSC_UINT},
        {"U8", EKSC_UINT},   {"U16", EKSC_UINT},  {"U32", EKSC_UINT},
        {"U64", EKSC_UINT},  {"Float", EKSC_FLOAT}, {"F32", EKSC_FLOAT},
        {"F64", EKSC_FLOAT}, {"Char", EKSC_CHAR}, {"Bool", EKSC_BOOL},
        {"String", EKSC_STRING}, {"Bytes", EKSC_BYTES}
    };
    size_t i;

    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || !t->u.nominal.sym->module
        || strcmp(t->u.nominal.sym->module, "std") != 0) {
        return EKSC_OTHER;
    }
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        if (strcmp(names[i].name, t->u.nominal.sym->name) == 0) {
            return names[i].slot;
        }
    }
    return EKSC_OTHER;
}

/* True when the nominal is the compiler-supplied Range placeholder:
   the type whose generated struct family the range operators build
   and the runtime traversals read. */
int lower_is_range(const ek_type *t)
{
    return t && t->kind == EK_T_NOMINAL && t->u.nominal.sym
        && t->u.nominal.sym->module
        && strcmp(t->u.nominal.sym->module, "std") == 0
        && strcmp(t->u.nominal.sym->name, "Range") == 0;
}

/* True when the nominal is a handle type of the system vocabulary:
   the closed three-name list the checker's own judgment reads, so the
   display channels and the halt rules stay one list apart. */
int lower_is_handle(const ek_type *t)
{
    static const char *handles[] = {"File", "Scope", "Task"};
    size_t i;

    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || t->u.nominal.argc > 1 || !t->u.nominal.sym->module
        || strcmp(t->u.nominal.sym->module, "sys") != 0) {
        return 0;
    }
    for (i = 0; i < sizeof handles / sizeof *handles; i++) {
        if (strcmp(handles[i], t->u.nominal.sym->name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Opens one service body's buffer. */
void lower_d_begin(lower_ctx *c, lower_demit *d, ek_buf *b)
{
    ek_buf_init(b);
    d->c = c;
    d->tmp = 0;
    d->buf = b;
}

/* Closes the body: its text joins the service buffer whole. */
void lower_d_end(lower_ctx *c, ek_buf *b)
{
    if (b->data && !ek_buf_append_bytes(&c->dsvc, b->data, b->len)) {
        c->oom = 1;
    }
    ek_buf_destroy(b);
}

/* Writes one line fragment into the body's buffer. */
void lower_d_printf(lower_demit *d, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (!ek_buf_vprintf(d->buf, fmt, ap)) {
        d->c->oom = 1;
    }
    va_end(ap);
}

/* A fresh temp of the service body, unique inside it. */
const char *lower_d_temp(lower_demit *d)
{
    d->tmp++;
    return lower_str(d->c, "s%u", d->tmp - 1);
}

/* The C string literal of a written text. */
const char *lower_d_cstr(lower_ctx *c, const char *s)
{
    ek_buf b;

    ek_buf_init(&b);
    lower_string_literal(c, &b, s, strlen(s));
    return lower_take(c, &b);
}

/* acc = acc ++ lit; each emitted line carries the block's indent. The
   literal reads its cache variable, kept for the join, and the old
   accumulator dies once the join exists. */
void lower_d_concat_lit(lower_demit *d, const char *indent, const char *acc,
                        const char *lit)
{
    const char *s = lower_d_temp(d);
    const char *n = lower_d_temp(d);
    const char *v = lower_str_cache(d->c, lit, strlen(lit));

    lower_d_printf(d, "%s{\n", indent);
    lower_d_printf(d, "%s    ek_obj *%s = ek_keep(%s).as.ref;\n", indent,
                   s, v);
    lower_d_printf(d, "%s    ek_obj *%s = ek_str_concat(%s, %s);\n", indent,
                   n, acc, s);
    lower_d_printf(d, "%s    ek_release_obj(%s);\n", indent, s);
    lower_d_printf(d, "%s    ek_release_obj(%s);\n", indent, acc);
    lower_d_printf(d, "%s    %s = %s;\n", indent, acc, n);
    lower_d_printf(d, "%s}\n", indent);
}

/* acc = acc ++ part; the part object and the old accumulator die. */
void lower_d_concat_obj(lower_demit *d, const char *indent, const char *acc,
                        const char *part)
{
    const char *n = lower_d_temp(d);

    lower_d_printf(d, "%s{\n", indent);
    lower_d_printf(d, "%s    ek_obj *%s = ek_str_concat(%s, %s);\n", indent,
                   n, acc, part);
    lower_d_printf(d, "%s    ek_release_obj(%s);\n", indent, part);
    lower_d_printf(d, "%s    ek_release_obj(%s);\n", indent, acc);
    lower_d_printf(d, "%s    %s = %s;\n", indent, acc, n);
    lower_d_printf(d, "%s}\n", indent);
}

/* The field spellings and applied types of a record- or tuple-shaped
   receiver: the row answers for the anonymous shapes, the declaration
   walk for nominals. Returns 0 when the shape has no lowering. */
static int shape_rows(lower_ctx *c, const ek_type *t, const char ***fnames,
                      const ek_type ***ftypes, size_t *nf, lower_gtype **g)
{
    const char **names = NULL;
    const ek_type **types = NULL;
    size_t ncap = 0, tcap = 0, n, i;

    if (t->kind == EK_T_RECORD) {
        n = t->u.record.count;
        *g = lower_anon_record(c, t);
    } else {
        n = t->u.tuple.count;
        *g = lower_anon_tuple(c, n);
    }
    if (!*g || !n) {
        *nf = n;
        return *g != NULL;
    }
    if (t->kind == EK_T_RECORD) {
        names = lower_grow(c, NULL, &ncap, n, sizeof *names);
        types = lower_grow(c, NULL, &tcap, n, sizeof *types);
        if (!names || !types) {
            return 0;
        }
        for (i = 0; i < n; i++) {
            names[i] = t->u.record.fields[i].name;
            types[i] = t->u.record.fields[i].type;
        }
        *fnames = names;
        *ftypes = types;
    } else {
        /* a positional tuple: fnames stays NULL, the items alias in
           directly, exactly as the nominal GT_TUP branch reports */
        *ftypes = t->u.tuple.items;
    }
    *nf = n;
    return 1;
}

int lower_shape_of(lower_ctx *c, const ek_node *at, const ek_type *t,
                   const char ***fnames, const ek_type ***ftypes, size_t *nf,
                   lower_gtype **g)
{
    const ek_node *dn;
    (void)at;
    const ek_type *repr;
    size_t cap = 0;

    *fnames = NULL;
    *ftypes = NULL;
    *nf = 0;
    *g = NULL;
    if (!t) {
        return 0;
    }
    if (t->kind == EK_T_RECORD || t->kind == EK_T_TUPLE) {
        return shape_rows(c, t, fnames, ftypes, nf, g);
    }
    if (t->kind != EK_T_NOMINAL || !t->u.nominal.sym) {
        return 0;
    }
    *g = lower_decl_repr(c, t->u.nominal.sym);
    dn = t->u.nominal.sym->decl;
    if (!*g || !dn) {
        return 0;
    }
    if (dn->kind == EK_N_CTYPE_DECL) {
        repr = lower_applied_type(c, dn->u.decl.body, dn, t);
    } else if (dn->kind == EK_N_TYPE_DECL) {
        repr = lower_applied_type(c, dn->u.type_decl.type, dn, t);
    } else {
        return 0;
    }
    if (!repr) {
        return 0;
    }
    if ((*g)->shape == GT_REC && repr->kind == EK_T_RECORD) {
        const ek_type **types = lower_grow(c, NULL, &cap, (*g)->nf,
                                           sizeof *types);
        size_t i, k;

        if (!types) {
            return 0;
        }
        /* The layout names the fields; the walked repr answers each
           name with its type, whichever order each reports. */
        for (i = 0; i < (*g)->nf; i++) {
            types[i] = NULL;
            for (k = 0; k < repr->u.record.count; k++) {
                if (strcmp((*g)->fnames[i], repr->u.record.fields[k].name)
                    == 0) {
                    types[i] = repr->u.record.fields[k].type;
                }
            }
            if (!types[i]) {
                return 0;
            }
        }
        *fnames = (*g)->fnames;
        *ftypes = types;
        *nf = (*g)->nf;
        return 1;
    }
    if ((*g)->shape == GT_TUP && repr->kind == EK_T_TUPLE
        && repr->u.tuple.count == (*g)->nf) {
        *ftypes = repr->u.tuple.items;
        *nf = (*g)->nf;
        return 1;
    }
    return 0;
}

/* The constructor symbol of one variant, matched on the symbol's own
   module, owner, and spelling. */
static const ek_symbol *ctor_symbol(lower_ctx *c, const char *mod,
                                    const char *owner, const char *name)
{
    size_t k, i;

    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_CONSTRUCTOR && m->owner && m->module
                && strcmp(m->owner, owner) == 0
                && strcmp(m->module, mod) == 0
                && strcmp(m->name, name) == 0) {
                return m;
            }
        }
    }
    return NULL;
}

/* The variants of an algebraic receiver, in declaration order, each
   with its registered family and applied payload. A nonzero construct
   registers every family as constructed, the way a construction site
   would: the decode side calls the variants' helpers to build the
   values it answers. Returns 0 when the receiver is not an algebraic
   type of this closure. */
int lower_variants_of(lower_ctx *c, const ek_node *at, const ek_type *t,
                      lower_variant **out, size_t *nv, int construct)
{
    const ek_node *dn;
    lower_variant *vs;
    size_t n, cap = 0, i;

    *out = NULL;
    *nv = 0;
    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || !t->u.nominal.sym->decl) {
        return 0;
    }
    dn = t->u.nominal.sym->decl;
    if (dn->kind != EK_N_TYPE_DECL
        || dn->u.type_decl.rhs != EK_TYPE_RHS_VARIANTS) {
        return 0;
    }
    n = dn->u.type_decl.variants.count;
    vs = lower_grow(c, NULL, &cap, n, sizeof *vs);
    if (!vs) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        const ek_node *v = dn->u.type_decl.variants.data[i];
        const ek_symbol *cs = ctor_symbol(c, t->u.nominal.sym->module,
                                          t->u.nominal.sym->name,
                                          v->u.variant.name);
        lower_gtype *g;

        if (!cs) {
            return 0;
        }
        g = lower_ctor_of_symbol(c, at, cs, construct);
        if (!g) {
            return 0;
        }
        g->shared = 1; /* the typeinfo the dispatch tests joins */
        vs[i].name = v->u.variant.name;
        vs[i].stem = g->stem;
        vs[i].payload = NULL;
        if (v->u.variant.payload) {
            vs[i].payload = lower_applied_type(c, v->u.variant.payload, dn,
                                               t);
            if (!vs[i].payload) {
                return 0;
            }
        }
    }
    *out = vs;
    *nv = n;
    return 1;
}

/* The construction helper of a std constructor: its family registers
   exactly as composite construction registers it. */
const char *lower_std_ctor(lower_ctx *c, const ek_node *at,
                           const char *owner, const char *name)
{
    size_t k, i;

    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        if (strcmp(s->module, "std") != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_CONSTRUCTOR && m->owner
                && strcmp(m->owner, owner) == 0
                && strcmp(m->name, name) == 0) {
                lower_gtype *g = lower_ctor_of_symbol(c, at, m, 1);

                return g ? g->helper : NULL;
            }
        }
    }
    lower_error(c, at, "the std constructor `%s.%s` has no lowering", owner,
                name);
    return NULL;
}

/* The decode service of one applied type. */
const char *lower_fromj_of(lower_ctx *c, const ek_type *t)
{
    return lower_dsvc_request(c, NULL, DSV_FROMJSON, t);
}

/* return Ok value; the expression is retained by the constructor. */
void lower_json_ok_emit(lower_demit *d, const char *indent, const char *expr)
{
    lower_d_printf(d, "%sreturn %s(%s);\n", indent,
                   lower_result_ctor(d->c, "Ok"), expr);
}

/* The key text test over a borrowed dictionary entry: the entry key is
   a String value of equal bytes. */
void lower_val_text_is(lower_demit *d, const char *k, const char *raw)
{
    const char *lit = lower_d_cstr(d->c, raw);
    size_t n = strlen(raw);

    d->c->uses_string_h = 1;
    lower_d_printf(d, "(%s.tag == EK_VAL_REF && %s.as.ref != (ek_obj *)0\n",
                   k, k);
    lower_d_printf(d, "    && %s.as.ref->info->kind == EK_KIND_STR\n", k);
    lower_d_printf(d, "    && ek_str_len(%s.as.ref) == %zu\n", k, n);
    lower_d_printf(d, "    && memcmp(ek_str_cstr(%s.as.ref), %s, %zu)"
                      " == 0)", k, lit, n);
}

/* The name test over a String object the Json Str slot carries. */
void lower_obj_text_is(lower_demit *d, const char *obj, const char *raw)
{
    const char *lit = lower_d_cstr(d->c, raw);
    size_t n = strlen(raw);

    d->c->uses_string_h = 1;
    lower_d_printf(d, "(ek_str_len(%s) == %zu\n", obj, n);
    lower_d_printf(d, "    && memcmp(ek_str_cstr(%s), %s, %zu) == 0)", obj,
                   lit, n);
}
