#include <string.h>

#include "lower/internal.h"

/* The emission of the generated struct families: the typedefs, the
   drop and typeinfo pair, the construction helpers, and the list fold.
   The families register while the bodies lower; this text joins the
   unit before the functions. */

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
    /* External linkage and const: the runtime's bundle closures build
       values over the unit's own typeinfo identity, so the symbol is
       shared with rt exactly as a generated struct shares it with the
       patterns that read it. A family carrying the structural keying
       hooks names them in the initializer, with their signatures
       ahead of it: the definitions stand in the service buffer. */
    if (g->eq_name) {
        uprintf(c, u, "static int %s(ek_val a, ek_val b);\n", g->eq_name);
    }
    if (g->hash_name) {
        uprintf(c, u, "static unsigned long %s(ek_val v);\n", g->hash_name);
    }
    uprintf(c, u, "const ek_typeinfo ekT_%s = { EK_KIND_STRUCT, "
                  "drop_%s, %s, %s };\n\n", g->stem, g->stem,
                  g->eq_name ? g->eq_name : "NULL",
                  g->hash_name ? g->hash_name : "NULL");
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
   constructs, in an order the C compiler reads top to bottom. A family
   the runtime shares keeps its typeinfo and drop without the helper:
   the runtime builds its values directly over the identity. */
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
        if (c->gtypes[i].constructed || c->gtypes[i].shared) {
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
