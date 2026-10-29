#include <string.h>

#include "lower/internal.h"

/* The generated struct layer: every composite value lives behind a
   reference to a struct that embeds the runtime object header first.
   A declared type's record or tuple representation, each constructed
   constructor of an algebraic type, and the anonymous record and tuple
   shapes a body builds or reads register here while the bodies lower;
   the families themselves are emitted into the unit before the
   functions. Registry identity is the symbol pointer for the declared
   families and the shape key for the anonymous ones, so two spellings
   of one shape share a struct and two constructors never do. */

/* Appends to the unit, recording exhaustion on the context. */
static int uputs(lower_ctx *c, ek_buf *u, const char *s)
{
    if (!ek_buf_append_str(u, s)) {
        c->oom = 1;
        return 0;
    }
    return 1;
}

static int uprintf(lower_ctx *c, ek_buf *u, const char *fmt, ...)
{
    va_list ap;
    int ok;

    va_start(ap, fmt);
    ok = ek_buf_vprintf(u, fmt, ap);
    va_end(ap);
    if (!ok) {
        c->oom = 1;
    }
    return ok;
}

/* The C spelling of a struct field: a record field carries its
   written name, a tuple field its position, a constructor payload its
   position without the value-layer prefix. */
static const char *field_c_name(lower_ctx *c, const lower_gtype *g,
                                size_t i)
{
    if (g->shape == GT_REC) {
        return lower_str(c, "f_%s", g->fnames[i]);
    }
    if (g->shape == GT_TUP) {
        return lower_str(c, "f_%zu", i);
    }
    return lower_str(c, "p%zu", i);
}

/* The C spelling of a helper parameter: positional in every family, so
   a written field name can never collide with a C keyword. */
static const char *param_c_name(lower_ctx *c, size_t i)
{
    return lower_str(c, "p%zu", i);
}

/* Finds a registered family by symbol or shape key. */
static lower_gtype *find_family(const lower_ctx *c, const ek_symbol *sym,
                                const char *key)
{
    size_t i;

    for (i = 0; i < c->ngt; i++) {
        const lower_gtype *g = &c->gtypes[i];

        if (sym ? g->key_sym == sym
                : (g->key_str && key && strcmp(g->key_str, key) == 0)) {
            return &c->gtypes[i];
        }
    }
    return NULL;
}

static lower_gtype *add_family(lower_ctx *c)
{
    lower_gtype *slots = lower_grow(c, c->gtypes, &c->cgt, c->ngt + 1,
                                    sizeof *slots);

    if (!slots) {
        return NULL;
    }
    c->gtypes = slots;
    memset(&slots[c->ngt], 0, sizeof slots[c->ngt]);
    return &slots[c->ngt++];
}

/* Registers the record or tuple representation of a declared type:
   the layout follows the declaration's right-hand side, records in
   declaration order. A std placeholder or an algebraic type has no
   representation and registers nothing. */
lower_gtype *lower_decl_repr(lower_ctx *c, const ek_symbol *tsym)
{
    lower_gtype *g;
    const ek_node *dn;
    const ek_nlist *kids;
    size_t cap = 0, i;

    if (!tsym) {
        return NULL;
    }
    g = find_family(c, tsym, NULL);
    if (g) {
        g->used = 1;
        return g;
    }
    dn = tsym->decl;
    if (!dn || dn->kind != EK_N_TYPE_DECL) {
        return NULL;
    }
    if (dn->u.type_decl.rhs != EK_TYPE_RHS_RECORD
        && dn->u.type_decl.rhs != EK_TYPE_RHS_TUPLE) {
        return NULL;
    }
    kids = &dn->u.type_decl.type->u.kids;
    g = add_family(c);
    if (!g) {
        return NULL;
    }
    g->key_sym = tsym;
    g->stem = lower_str(c, "%s_%s", tsym->module, tsym->name);
    g->helper = lower_str(c, "ekn_%s_%s_rec", tsym->module, tsym->name);
    g->shape = dn->u.type_decl.rhs == EK_TYPE_RHS_TUPLE ? GT_TUP : GT_REC;
    g->nf = kids->count;
    if (g->shape == GT_REC) {
        g->fnames = lower_grow(c, NULL, &cap, g->nf, sizeof *g->fnames);
        if (!g->fnames) {
            c->ngt--;
            return NULL;
        }
        for (i = 0; i < g->nf; i++) {
            g->fnames[i] = kids->data[i]->u.t_field.name;
        }
    }
    g->used = 1;
    return g;
}

/* The registry key of an anonymous record: its field names in the
   canonical sorted order the checker reports. */
static const char *record_key(lower_ctx *c, const ek_type *rec)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; i < rec->u.record.count; i++) {
        if (i && !ek_buf_append_char(&b, ',')) {
            c->oom = 1;
        }
        if (!ek_buf_append_str(&b, rec->u.record.fields[i].name)) {
            c->oom = 1;
        }
    }
    return lower_take(c, &b);
}

/* Registers the anonymous record shape of a record type: fields in
   the checker's sorted order, shared with every equal field set. */
lower_gtype *lower_anon_record(lower_ctx *c, const ek_type *rec)
{
    const char *key = record_key(c, rec);
    lower_gtype *g;
    size_t cap = 0, i;

    if (c->oom) {
        return NULL;
    }
    g = find_family(c, NULL, key);
    if (g) {
        g->used = 1;
        return g;
    }
    g = add_family(c);
    if (!g) {
        return NULL;
    }
    g->key_str = key;
    g->stem = lower_str(c, "rec%u", c->anon_rec++);
    g->helper = lower_str(c, "ekn_%s", g->stem);
    g->shape = GT_REC;
    g->nf = rec->u.record.count;
    if (g->nf) {
        g->fnames = lower_grow(c, NULL, &cap, g->nf, sizeof *g->fnames);
        if (!g->fnames) {
            c->ngt--;
            return NULL;
        }
        for (i = 0; i < g->nf; i++) {
            g->fnames[i] = rec->u.record.fields[i].name;
        }
    }
    g->used = 1;
    return g;
}

/* Registers the anonymous tuple shape of an arity: same arity, same
   layout, one family. */
lower_gtype *lower_anon_tuple(lower_ctx *c, size_t n)
{
    const char *key = lower_str(c, "tup:%zu", n);
    lower_gtype *g = find_family(c, NULL, key);

    if (g) {
        g->used = 1;
        return g;
    }
    g = add_family(c);
    if (!g) {
        return NULL;
    }
    g->key_str = key;
    g->stem = lower_str(c, "tup_%zu", n);
    g->helper = lower_str(c, "ekn_%s", g->stem);
    g->shape = GT_TUP;
    g->nf = n;
    g->used = 1;
    return g;
}

/* The constructor a reference denotes, re-derived the way check picks
   one: the candidate owned by the expected type's declaration. */
static const ek_symbol *pick_ctor(const ek_ref *ref, const ek_symbol *tsym)
{
    size_t i;

    for (i = 0; i < ref->ncands; i++) {
        const ek_symbol *cand = ref->cands[i];

        if (cand && cand->owner && cand->module && tsym->module
            && strcmp(cand->owner, tsym->name) == 0
            && strcmp(cand->module, tsym->module) == 0) {
            return cand;
        }
    }
    if (ref->ncands == 0 && ref->symbol && ref->symbol->owner
        && strcmp(ref->symbol->owner, tsym->name) == 0) {
        return ref->symbol;
    }
    return NULL;
}

/* Registers the constructor family of an algebraic type: one struct
   per constructor, its payload the variant's own. */
lower_gtype *lower_ctor_family(lower_ctx *c, const ek_node *at,
                               const ek_ref *ref, const ek_symbol *tsym)
{
    const ek_symbol *cs = tsym ? pick_ctor(ref, tsym) : NULL;
    const ek_node *dn = tsym ? tsym->decl : NULL;
    const char **slots;
    lower_gtype *g;
    size_t cap = 0, i;

    if (!cs || !dn || dn->kind != EK_N_TYPE_DECL) {
        lower_error(c, at, "the constructor `%s` is not supported by "
                          "this lowerer",
                    ref && ref->symbol ? ref->symbol->name : "?");
        return NULL;
    }
    g = find_family(c, cs, NULL);
    if (g) {
        return g;
    }
    for (i = 0; i < dn->u.type_decl.variants.count; i++) {
        const ek_node *v = dn->u.type_decl.variants.data[i];

        if (strcmp(v->u.variant.name, cs->name) != 0) {
            continue;
        }
        g = add_family(c);
        if (!g) {
            return NULL;
        }
        g->key_sym = cs;
        g->stem = lower_str(c, "%s_%s_%s", tsym->module, tsym->name,
                            cs->name);
        g->helper = lower_str(c, "ekn_%s", g->stem);
        g->shape = GT_CTOR;
        g->nf = v->u.variant.payload ? 1 : 0;
        g->used = 1;
        g->constructed = 1;
        if (g->nf) {
            slots = lower_grow(c, NULL, &cap, 1, sizeof *slots);
            if (slots) {
                slots[0] = "";
                g->fnames = slots;
            }
        }
        return g;
    }
    lower_error(c, at, "the constructor `%s` is not supported by this "
                       "lowerer", cs->name);
    return NULL;
}

/* The C text reading one field of a receiver temp: the receiver's
   type decides the family, and the read is an owned copy through the
   retention helper. */
const char *lower_field_slot(lower_ctx *c, const ek_node *at,
                             const ek_type *recv, const char *temp,
                             const char *name)
{
    lower_gtype *g = NULL;

    if (recv->kind == EK_T_NOMINAL) {
        if (recv->u.nominal.sym) {
            g = lower_decl_repr(c, recv->u.nominal.sym);
        }
    } else if (recv->kind == EK_T_RECORD) {
        g = lower_anon_record(c, recv);
    }
    if (!g || g->shape != GT_REC) {
        lower_error(c, at, "the field `%s` has no lowering", name);
        return NULL;
    }
    return lower_str(c, "((ekS_%s *)%s.as.ref)->f_%s", g->stem, temp,
                     name);
}

void lower_use_list(lower_ctx *c)
{
    c->uses_list = 1;
}

/* The shared allocation of the generated structs: zeroed storage, or
   the exhaustion abort of the runtime. */
static void emit_alloc_helper(lower_ctx *c, ek_buf *u)
{
    if (uputs(c, u, "static void *ek_struct_alloc(size_t size)\n{\n")
        && uputs(c, u, "    void *p = calloc(1, size);\n\n")
        && uputs(c, u, "    if (!p) {\n")
        && uputs(c, u, "        ek_abort(\"out of memory\");\n")
        && uputs(c, u, "    }\n")
        && uputs(c, u, "    return p;\n")) {
        uputs(c, u, "}\n\n");
    }
}

static void emit_typedef(lower_ctx *c, ek_buf *u, const lower_gtype *g)
{
    size_t i;

    if (!uprintf(c, u, "typedef struct { ek_obj obj;")) {
        return;
    }
    for (i = 0; i < g->nf; i++) {
        if (!uprintf(c, u, " ek_val %s;", field_c_name(c, g, i))) {
            return;
        }
    }
    uprintf(c, u, " } ekS_%s;\n\n", g->stem);
}

static void emit_drop(lower_ctx *c, ek_buf *u, const lower_gtype *g)
{
    size_t i;

    if (!uprintf(c, u, "static void drop_%s(ek_obj *o)\n{\n", g->stem)) {
        return;
    }
    if (g->nf == 0 && !uputs(c, u, "    (void)o;\n")) {
        return;
    }
    for (i = 0; i < g->nf; i++) {
        if (!uprintf(c, u, "    ek_release(((ekS_%s *)o)->%s);\n", g->stem,
                     field_c_name(c, g, i))) {
            return;
        }
    }
    uputs(c, u, "}\n\n");
}

static void emit_typeinfo(lower_ctx *c, ek_buf *u, const lower_gtype *g)
{
    uprintf(c, u, "static ek_typeinfo ekT_%s = { EK_KIND_STRUCT, "
                  "drop_%s };\n\n", g->stem, g->stem);
}

/* One construction helper: allocates the zeroed struct, marks it born
   with one reference, and retains every payload it stores. */
static void emit_helper(lower_ctx *c, ek_buf *u, const lower_gtype *g)
{
    const char *sep = "";
    size_t i;

    if (!uprintf(c, u, "static ek_val %s(", g->helper)) {
        return;
    }
    if (g->nf == 0 && !uputs(c, u, "void")) {
        return;
    }
    for (i = 0; i < g->nf; i++) {
        if (!uprintf(c, u, "%sek_val %s", sep, param_c_name(c, i))) {
            return;
        }
        sep = ", ";
    }
    if (!uputs(c, u, ")\n{\n")
        || !uprintf(c, u, "    ekS_%s *o = ek_struct_alloc(sizeof *o);\n\n",
                    g->stem)
        || !uprintf(c, u, "    o->obj.info = &ekT_%s;\n", g->stem)
        || !uputs(c, u, "    o->obj.rc = 1;\n")) {
        return;
    }
    for (i = 0; i < g->nf; i++) {
        if (!uprintf(c, u, "    o->%s = ek_keep(%s);\n",
                     field_c_name(c, g, i), param_c_name(c, i))) {
            return;
        }
    }
    uputs(c, u, "    return ek_val_ref(&o->obj);\n}\n\n");
}

/* The list fold: right-to-left over the element temps, so the written
   order survives; every intermediate cell dies inside the fold, and
   the spine release at the end costs no C stack. */
static void emit_list_helper(lower_ctx *c, ek_buf *u)
{
    if (uputs(c, u, "static ek_val ekn_list(ek_val *elems, size_t n)\n{\n")
        && uputs(c, u, "    ek_val out = ek_nil_val();\n")
        && uputs(c, u, "    size_t i = n;\n\n")
        && uputs(c, u, "    while (i > 0) {\n")
        && uputs(c, u, "        ek_val cell;\n\n")
        && uputs(c, u, "        i--;\n")
        && uputs(c, u, "        cell = ek_cons(elems[i], out);\n")
        && uputs(c, u, "        ek_release(out);\n")
        && uputs(c, u, "        out = cell;\n")
        && uputs(c, u, "    }\n")
        && uputs(c, u, "    return out;\n")) {
        uputs(c, u, "}\n\n");
    }
}

/* Emits the registered families: typedefs of everything a body
   references, and typeinfo, drop, and helper of everything a body
   constructs, in an order the C compiler reads top to bottom. */
void lower_emit_types(lower_ctx *c, ek_buf *unit)
{
    int constructed = 0;
    size_t i;

    for (i = 0; i < c->ngt; i++) {
        constructed = constructed || c->gtypes[i].constructed;
    }
    if (!c->ngt && !c->uses_list) {
        return;
    }
    if (constructed) {
        emit_alloc_helper(c, unit);
    }
    for (i = 0; i < c->ngt; i++) {
        if (c->gtypes[i].used) {
            emit_typedef(c, unit, &c->gtypes[i]);
        }
    }
    for (i = 0; i < c->ngt; i++) {
        if (c->gtypes[i].constructed) {
            emit_drop(c, unit, &c->gtypes[i]);
            emit_typeinfo(c, unit, &c->gtypes[i]);
        }
    }
    if (c->uses_list) {
        emit_list_helper(c, unit);
    }
    for (i = 0; i < c->ngt; i++) {
        if (c->gtypes[i].constructed) {
            emit_helper(c, unit, &c->gtypes[i]);
        }
    }
}
