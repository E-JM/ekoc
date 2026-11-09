#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lower/internal.h"
#include "support/vocab.h"

/* The derived service registry: one body per family and applied type,
   emitted into the service buffer on first use, with the applied
   arguments spelled into the name so generic instantiations are
   distinct services. Every body joins the unit as a forward declaration
   ahead of the function bodies, so a recursive type's service may call
   its own name. */

static void key_into(lower_ctx *c, ek_buf *b, const ek_type *t);

/* The key of a tuple or record shape: one slot per component. */
static void key_fields(lower_ctx *c, ek_buf *b, const ek_type *t)
{
    size_t n = t->kind == EK_T_TUPLE ? t->u.tuple.count
                                     : t->u.record.count;

    ek_buf_append_str(b, t->kind == EK_T_TUPLE ? "T(" : "R(");
    for (size_t i = 0; i < n; i++) {
        if (i) {
            ek_buf_append_char(b, ',');
        }
        if (t->kind == EK_T_RECORD) {
            ek_buf_printf(b, "%s:", t->u.record.fields[i].name);
            key_into(c, b, t->u.record.fields[i].type);
        } else {
            key_into(c, b, t->u.tuple.items[i]);
        }
    }
    ek_buf_append_str(b, ")");
}

/* The key of a nominal and its applied arguments. */
static void key_nominal(lower_ctx *c, ek_buf *b, const ek_type *t)
{
    ek_buf_printf(b, "N%s:%s(", t->u.nominal.sym ? t->u.nominal.sym->module
                                                 : "?",
                  t->u.nominal.sym ? t->u.nominal.sym->name : "?");
    for (size_t i = 0; i < t->u.nominal.argc; i++) {
        if (i) {
            ek_buf_append_char(b, ',');
        }
        key_into(c, b, t->u.nominal.args[i]);
    }
    ek_buf_append_str(b, ")");
}

/* The key pieces of the compound shapes. */
static void key_shape(lower_ctx *c, ek_buf *b, const ek_type *t)
{
    switch (t->kind) {
    case EK_T_LIST:
        ek_buf_append_str(b, "L(");
        key_into(c, b, t->u.list.elem);
        ek_buf_append_str(b, ")");
        break;
    case EK_T_DICT:
        ek_buf_append_str(b, "D(");
        key_into(c, b, t->u.dict.key);
        ek_buf_append_str(b, ")(");
        key_into(c, b, t->u.dict.val);
        ek_buf_append_str(b, ")");
        break;
    case EK_T_TUPLE:
    case EK_T_RECORD:
        key_fields(c, b, t);
        break;
    case EK_T_NOMINAL:
        key_nominal(c, b, t);
        break;
    default:
        break;
    }
}
/* The structural key of an applied type: equal shapes share, distinct
   shapes never collide. */
static void key_into(lower_ctx *c, ek_buf *b, const ek_type *t)
{
    if (!t || !ek_buf_append_char(b, '?')) {
        c->oom = 1;
        return;
    }
    switch (t->kind) {
    case EK_T_UNIT:
        ek_buf_append_str(b, "U");
        break;
    case EK_T_NEVER:
        ek_buf_append_str(b, "X");
        break;
    case EK_T_FN:
        ek_buf_append_str(b, "F");
        break;
    case EK_T_TVAR:
        ek_buf_printf(b, "V%s", t->u.tvar.name);
        break;
    case EK_T_IVAR:
        ek_buf_append_str(b, "I");
        break;
    default:
        key_shape(c, b, t);
        break;
    }
}

/* The capability channel's nine types: std placeholder nominals with
   no declaration behind them, whose values the entry shim and the
   runtime's bundle closures build. */
static int is_cap_type(const ek_type *t)
{
    const ek_symbol *sym;

    if (t->kind != EK_T_NOMINAL || t->u.nominal.argc != 0) {
        return 0;
    }
    sym = t->u.nominal.sym;
    if (!sym || !sym->module || strcmp(sym->module, "std") != 0
        || sym->decl) {
        return 0;
    }
    return ek_is_capability_name(sym->name);
}

/* The nominal rows: the scalars order only as text, the range carries
   the operator services, the capabilities and handles render only, the
   runtime containers walk their elements, and a declared type derives
   everything but the ordering. */
static int supported_nominal(lower_dsvc_kind kind, const ek_type *t)
{
    lower_scalar sk = lower_scalar_kind(t);

    if (sk != EKSC_OTHER) {
        return kind != DSV_ORD || sk == EKSC_STRING || sk == EKSC_BYTES;
    }
    if (lower_is_range(t)) {
        /* The compiler supplies the display, comparison, and keying
           services over the range's struct: the operator form, the
           endpoint order, and the keying walk. */
        return kind == DSV_SHOW || kind == DSV_INSPECT
            || kind == DSV_EQ || kind == DSV_ORD || kind == DSV_HASH;
    }
    if (is_cap_type(t)) {
        /* The capability value reads through no structure: the two
           display channels render the <cap> form, every other family
           stays unsupported. */
        return kind == DSV_SHOW || kind == DSV_INSPECT;
    }
    if (lower_is_handle(t)) {
        /* A handle renders its own form on the display channels;
           equality, ordering, keying, and JSON stay derived-free the
           way the checker's halt rules read them. */
        return kind == DSV_SHOW || kind == DSV_INSPECT;
    }
    if (lower_is_set(t)) {
        /* The set's services walk its elements in insertion order
           through the runtime's list; the order of the walk is not
           the equality, which is set equality. */
        return kind != DSV_ORD;
    }
    if (lower_is_vector(t)) {
        /* The vector's services walk its elements in index order
           through the runtime's list; the order of the walk is the
           equality, which compares index for index. */
        return kind != DSV_ORD;
    }
    return t->u.nominal.sym && t->u.nominal.sym->decl != NULL;
}

/* Which families a service renders at all. */
static int supported(lower_dsvc_kind kind, const ek_type *t)
{
    if (!t) {
        return 0;
    }
    switch (t->kind) {
    case EK_T_FN:
        return kind == DSV_SHOW || kind == DSV_INSPECT;
    case EK_T_NEVER:
        return 0;
    case EK_T_DICT:
        if (kind == DSV_ORD) {
            return 0;
        }
        if (kind == DSV_TOJSON || kind == DSV_FROMJSON) {
            return lower_scalar_kind(t->u.dict.key) == EKSC_STRING;
        }
        return 1;
    case EK_T_NOMINAL:
        return supported_nominal(kind, t);
    default:
        return kind != DSV_ORD;
    }
}

static const char *kind_prefix(lower_dsvc_kind kind)
{
    switch (kind) {
    case DSV_EQ: return "ekq_";
    case DSV_ORD: return "eko_";
    case DSV_HASH: return "ekh_";
    case DSV_SHOW: return "ekd_show_";
    case DSV_INSPECT: return "ekd_inspect_";
    case DSV_TOJSON: return "ekd_to_json_";
    default: return "ekd_from_json_";
    }
}

/* The service name of one family and applied type, suffixed when two
   distinct shapes would spell alike. */
static const char *mint_name(lower_ctx *c, lower_dsvc_kind kind,
                             const ek_type *t)
{
    ek_buf b;
    const char *spell;
    const char *name;
    unsigned n = 0;
    size_t i;

    ek_buf_init(&b);
    spell_into(c, &b, t);
    if (c->oom) {
        ek_buf_destroy(&b);
        return NULL;
    }
    spell = lower_take(c, &b);
    name = lower_str(c, "%s%s", kind_prefix(kind), spell);
    while (name) {
        int taken = 0;

        for (i = 0; i < c->ndsvc; i++) {
            if (strcmp(c->dsvcs[i].name, name) == 0) {
                taken = 1;
            }
        }
        if (!taken) {
            break;
        }
        n++;
        name = lower_str(c, "%s%s_%u", kind_prefix(kind), spell, n);
    }
    return name;
}

/* Emits the signature and the body of one service. */
static void emit_service(lower_ctx *c, const lower_dsvc *s,
                         const ek_type *t)
{
    switch (s->kind) {
    case DSV_EQ:
    case DSV_ORD:
        lower_push_sig(c, lower_str(c, "static int %s(ek_val a, ek_val b)",
                                    s->name));
        if (s->kind == DSV_EQ) {
            lower_dsvc_eq_emit(c, s->name, t);
        } else {
            lower_dsvc_ord_emit(c, s->name, t);
        }
        break;
    case DSV_HASH:
        lower_push_sig(c, lower_str(c, "static unsigned long %s(ek_val v)",
                                    s->name));
        lower_dsvc_hash_emit(c, s->name, t);
        break;
    case DSV_SHOW:
    case DSV_INSPECT:
        lower_push_sig(c, lower_str(c, "static ek_obj *%s(ek_val v)",
                                    s->name));
        lower_dsvc_show_emit(c, s->name, t, s->kind == DSV_INSPECT);
        break;
    default:
        lower_push_sig(c, lower_str(c, "static ek_val %s(ek_val v)",
                                    s->name));
        lower_dsvc_json_emit(c, s->name, t, s->kind == DSV_FROMJSON);
        break;
    }
}

/* Requests the service of one family over an applied type: the name of
   the existing body, or a freshly emitted one. NULL when the family
   has no service for the type. */
const char *lower_dsvc_request(lower_ctx *c, const ek_node *at,
                               lower_dsvc_kind kind, const ek_type *t)
{
    ek_buf kb;
    const char *key;
    const char *name;
    lower_dsvc *slots;
    lower_dsvc *s;
    size_t i;

    (void)at;
    if (!supported(kind, t)) {
        return NULL;
    }
    ek_buf_init(&kb);
    key_into(c, &kb, t);
    key = c->oom ? NULL : lower_take(c, &kb);
    if (!key) {
        return NULL;
    }
    for (i = 0; i < c->ndsvc; i++) {
        if (c->dsvcs[i].kind == (unsigned char)kind
            && strcmp(c->dsvcs[i].key, key) == 0) {
            return c->dsvcs[i].name;
        }
    }
    name = mint_name(c, kind, t);
    slots = name ? lower_grow(c, c->dsvcs, &c->cdsvc, c->ndsvc + 1,
                              sizeof *slots) : NULL;
    if (!name || !slots) {
        return NULL;
    }
    c->dsvcs = slots;
    s = &slots[c->ndsvc++];
    s->key = key;
    s->name = name;
    s->kind = (unsigned char)kind;
    emit_service(c, s, t);
    return s->name;
}

/* The result type of a derived-service call at a receiver the
   instantiation substituted concrete: the fixed result of the
   service, the receiver itself for `with`. NULL when the site is not
   such a call. */
const ek_type *lower_open_call_result(lower_ctx *c, const ek_node *e)
{
    const ek_node *callee;
    const char *name = NULL;
    const ek_node *recv = NULL;
    lower_dsvc_kind kind;
    const ek_type *rs;

    if (!e || e->kind != EK_N_CALL) {
        return NULL;
    }
    callee = e->u.call.callee;
    if (callee && callee->kind == EK_N_FIELD) {
        name = callee->u.field.name;
        recv = callee->u.field.recv;
    } else if (callee && callee->kind == EK_N_NAME
               && callee->u.path.count == 1) {
        name = callee->u.path.data[0];
        recv = e->u.call.args.count ? e->u.call.args.data[0] : NULL;
    }
    if (!name || !recv) {
        return NULL;
    }
    rs = lower_row_type(c, recv);
    if (!rs || lower_type_has_var(rs)) {
        return NULL;
    }
    if (lower_service_kind(name, &kind)) {
        switch (kind) {
        case DSV_EQ:
        case DSV_ORD:
            return lower_std_nominal(c, "Bool");
        case DSV_SHOW:
        case DSV_INSPECT:
            return lower_std_nominal(c, "String");
        case DSV_TOJSON:
            return lower_std_nominal(c, "Json");
        default:
            return NULL;
        }
    }
    return strcmp(name, "with") == 0 ? rs : NULL;
}
