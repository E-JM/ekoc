#include <string.h>

#include "lower/internal.h"

/* The applied types a declared type's own syntax names: a variant
   payload or a record field type is written against the declaration's
   binders, so the receiver's instantiation supplies the arguments and
   every other name resolves through the module closure. Undetermined
   shapes come back as inference variables, which the service emitters
   render as opaque leaves. */

/* An inference variable stands for a shape the walk could not
   determine. */
static const ek_type *opaque_type(lower_ctx *c)
{
    ek_type *t = ek_arena_alloc(c->arena, sizeof *t);

    if (!t) {
        c->oom = 1;
        return NULL;
    }
    t->kind = EK_T_IVAR;
    t->u.ivar.id = 0;
    t->u.ivar.bound = NULL;
    return t;
}

static const ek_type *mk_nominal(lower_ctx *c, const ek_symbol *sym,
                                 const ek_type **args, size_t argc)
{
    ek_type *t = ek_arena_alloc(c->arena, sizeof *t);

    if (!t) {
        c->oom = 1;
        return NULL;
    }
    t->kind = EK_T_NOMINAL;
    t->u.nominal.sym = sym;
    t->u.nominal.args = args;
    t->u.nominal.argc = argc;
    return t;
}

/* The type symbol of one written name: the match reads the symbol's
   own module, since the std placeholder rows stand in the file's
   surface with the std module recorded on the symbol. */
const ek_symbol *lower_type_symbol(lower_ctx *c, const char *mod,
                                   const char *name)
{
    size_t k, i;

    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_TYPE && strcmp(m->name, name) == 0
                && (!mod || (m->module && strcmp(m->module, mod) == 0))) {
                return m;
            }
        }
    }
    return NULL;
}

/* The binder index a written name denotes, -1 when the name is no
   binder of the declaration. */
static int binder_index(const ek_node *decl, const char *name)
{
    size_t i;

    if (!decl || decl->kind != EK_N_TYPE_DECL) {
        return -1;
    }
    for (i = 0; i < decl->u.type_decl.params.count; i++) {
        if (strcmp(decl->u.type_decl.params.data[i], name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* The instantiation's argument for one binder, NULL when the receiver
   carries no argument array of the declaration's width. */
static const ek_type *binder_arg(const ek_type *owner, int index)
{
    if (!owner || owner->kind != EK_T_NOMINAL) {
        return NULL;
    }
    if (index < 0 || (size_t)index >= owner->u.nominal.argc) {
        return NULL;
    }
    return owner->u.nominal.args[index];
}

static const ek_type *walk_type(lower_ctx *c, const ek_node *t,
                                const ek_node *decl, const ek_type *owner);

/* A written name: a binder reads the owner's argument, anything else
   resolves through the surfaces; an unapplied generic head keeps its
   zero-argument nominal, which the emitters treat as opaque. */
static const ek_type *walk_name(lower_ctx *c, const ek_node *t,
                                const ek_node *decl, const ek_type *owner)
{
    const ek_slist *path = &t->u.path;
    const char *name = path->count ? path->data[path->count - 1] : NULL;
    const char *mod = path->count > 1 ? path->data[0] : NULL;
    int index;
    const ek_symbol *sym;

    index = path->count == 1 ? binder_index(decl, name) : -1;
    if (index >= 0) {
        const ek_type *arg = binder_arg(owner, index);

        return arg ? arg : opaque_type(c);
    }
    if (!name) {
        return opaque_type(c);
    }
    sym = lower_type_symbol(c, mod, name);
    if (!sym) {
        return opaque_type(c);
    }
    return mk_nominal(c, sym, NULL, 0);
}

/* A written application: the head names the type, the arguments walk
   against the same declaration. */
static const ek_type *walk_apply(lower_ctx *c, const ek_node *t,
                                 const ek_node *decl, const ek_type *owner)
{
    const ek_node *head = t->u.t_apply.head;
    const ek_symbol *sym;
    const ek_type **args;
    size_t n = t->u.t_apply.args.count, cap = 0, i;

    if (!head || head->kind != EK_N_T_NAME || head->u.path.count == 0) {
        return opaque_type(c);
    }
    sym = lower_type_symbol(c, NULL,
                            head->u.path.data[head->u.path.count - 1]);
    if (!sym) {
        return opaque_type(c);
    }
    args = n ? lower_grow(c, NULL, &cap, n, sizeof *args) : NULL;
    if (n && !args) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        args[i] = walk_type(c, t->u.t_apply.args.data[i], decl, owner);
        if (!args[i]) {
            return NULL;
        }
    }
    return mk_nominal(c, sym, args, n);
}

/* A written list type: the element walks against the same
   declaration. */
static const ek_type *walk_list(lower_ctx *c, const ek_node *t,
                                const ek_node *decl, const ek_type *owner)
{
    const ek_type *elem = walk_type(c, t->u.wrap.inner, decl, owner);

    if (!elem) {
        return NULL;
    }
    {
        ek_type *out = ek_arena_alloc(c->arena, sizeof *out);

        if (!out) {
            c->oom = 1;
            return NULL;
        }
        out->kind = EK_T_LIST;
        out->u.list.elem = elem;
        return out;
    }
}

/* A written dictionary type: the key and the value walk against the
   same declaration. */
static const ek_type *walk_dict(lower_ctx *c, const ek_node *t,
                                const ek_node *decl, const ek_type *owner)
{
    const ek_type *key = walk_type(c, t->u.pair.a, decl, owner);
    const ek_type *val = walk_type(c, t->u.pair.b, decl, owner);
    ek_type *out;

    if (!key || !val) {
        return NULL;
    }
    out = ek_arena_alloc(c->arena, sizeof *out);
    if (!out) {
        c->oom = 1;
        return NULL;
    }
    out->kind = EK_T_DICT;
    out->u.dict.key = key;
    out->u.dict.val = val;
    return out;
}

/* A written tuple type: the components walk against the same
   declaration. */
static const ek_type *walk_tuple(lower_ctx *c, const ek_node *t,
                                 const ek_node *decl, const ek_type *owner)
{
    const ek_type **items;
    ek_type *out;
    size_t n = t->u.kids.count, cap = 0, i;

    items = n ? lower_grow(c, NULL, &cap, n, sizeof *items) : NULL;
    if (n && !items) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        items[i] = walk_type(c, t->u.kids.data[i], decl, owner);
        if (!items[i]) {
            return NULL;
        }
    }
    out = ek_arena_alloc(c->arena, sizeof *out);
    if (!out) {
        c->oom = 1;
        return NULL;
    }
    out->kind = EK_T_TUPLE;
    out->u.tuple.items = items;
    out->u.tuple.count = n;
    return out;
}

/* A written record type: the fields keep their written names, their
   types walk against the same declaration. */
static const ek_type *walk_record(lower_ctx *c, const ek_node *t,
                                  const ek_node *decl, const ek_type *owner)
{
    ek_tfield *fields;
    ek_type *out;
    size_t n = t->u.kids.count, cap = 0, i;

    fields = n ? lower_grow(c, NULL, &cap, n, sizeof *fields) : NULL;
    if (n && !fields) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const ek_node *f = t->u.kids.data[i];

        fields[i].name = f->u.t_field.name;
        fields[i].type = walk_type(c, f->u.t_field.type, decl, owner);
        if (!fields[i].type) {
            return NULL;
        }
    }
    out = ek_arena_alloc(c->arena, sizeof *out);
    if (!out) {
        c->oom = 1;
        return NULL;
    }
    out->kind = EK_T_RECORD;
    out->u.record.fields = fields;
    out->u.record.count = n;
    return out;
}

/* A written function type: the parameters and the result walk against
   the same declaration, and an elided result is the unit type. */
static const ek_type *walk_fn(lower_ctx *c, const ek_node *t,
                              const ek_node *decl, const ek_type *owner)
{
    const ek_type **params;
    const ek_type *result;
    ek_type *out;
    size_t n = t->u.t_fn.params.count, cap = 0, i;

    params = n ? lower_grow(c, NULL, &cap, n, sizeof *params) : NULL;
    if (n && !params) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const ek_node *pn = t->u.t_fn.params.data[i];

        params[i] = walk_type(c, pn->u.t_field.type, decl, owner);
        if (!params[i]) {
            return NULL;
        }
    }
    /* an elided result is the unit type */
    if (!t->u.t_fn.result) {
        ek_type *unit = ek_arena_alloc(c->arena, sizeof *unit);

        if (!unit) {
            c->oom = 1;
            return NULL;
        }
        unit->kind = EK_T_UNIT;
        result = unit;
    } else {
        result = walk_type(c, t->u.t_fn.result, decl, owner);
        if (!result) {
            return NULL;
        }
    }
    out = ek_arena_alloc(c->arena, sizeof *out);
    if (!out) {
        c->oom = 1;
        return NULL;
    }
    out->kind = EK_T_FN;
    out->u.fn.params = params;
    out->u.fn.count = n;
    out->u.fn.result = result;
    out->u.fn.arrow = t->u.t_fn.arrow;
    return out;
}

static const ek_type *walk_type(lower_ctx *c, const ek_node *t,
                                const ek_node *decl, const ek_type *owner)
{
    if (!t) {
        return opaque_type(c);
    }
    switch (t->kind) {
    case EK_N_T_PAREN:
        return walk_type(c, t->u.wrap.inner, decl, owner);
    case EK_N_T_NAME:
        return walk_name(c, t, decl, owner);
    case EK_N_T_APPLY:
        return walk_apply(c, t, decl, owner);
    case EK_N_T_LIST:
        return walk_list(c, t, decl, owner);
    case EK_N_T_DICT:
        return walk_dict(c, t, decl, owner);
    case EK_N_T_TUPLE:
        return walk_tuple(c, t, decl, owner);
    case EK_N_T_RECORD:
        return walk_record(c, t, decl, owner);
    case EK_N_T_FN:
        return walk_fn(c, t, decl, owner);
    default:
        return opaque_type(c);
    }
}

/* The applied type of a type-syntax node under a declared type's
   instantiation: binder names read the owner's arguments, other names
   resolve through the module closure. */
const ek_type *lower_applied_type(lower_ctx *c, const ek_node *tnode,
                                  const ek_node *decl, const ek_type *owner)
{
    const ek_type *t = walk_type(c, tnode, decl, owner);

    return t ? t : opaque_type(c);
}
