#include <string.h>

#include "lower/internal.h"

/* The intrinsic dispatch: a member call whose reading is a compiler
   row of the std namespaces routes to its runtime symbol over the
   receiver and the written arguments, and the status+out contracts
   wrap into the std Option and Result values. The surface table
   carries the rows the runtime surface backs; anything else on these
   receivers keeps the ordinary member path. */

/* The family a receiver type names: the structural containers, the
   std placeholder scalars, the capability module's own types, and the
   time companion's rows -- the File family's rows route through the
   same table. */
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
            || strcmp(t->u.nominal.sym->module, "sys") == 0
            || strcmp(t->u.nominal.sym->module, "cap") == 0
            || strcmp(t->u.nominal.sym->module, "time") == 0)) {
        return t->u.nominal.sym->name;
    }
    return NULL;
}

/* The joined runtime arguments: object slots pass the reference, the
   value slots the value. */
static const char *value_args(lower_ctx *c, const char *letters,
                              const char **as, size_t na)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; i < na; i++) {
        if (i && !ek_buf_append_str(&b, ", ")) {
            c->oom = 1;
        }
        if (letters[i] == 'o'
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

/* The plain shapes: the count, the call value, the owned object, the
   Bool word, the display object, and the widening identity. */
static lower_val emit_plain(lower_ctx *c, lower_scope *scope,
                            const ek_surface_row *row, const char *args,
                            const char **as)
{
    const char *t = lower_temp(c, scope);

    lower_pad(c);
    if (!row->sym) {
        lower_emitf(c, "ek_val %s = ek_keep(%s);\n", t, as[0]);
    } else if (row->wrap == EK_WRAP_LEN) {
        lower_emitf(c, "ek_val %s = ek_val_int((int64_t)%s(%s));\n", t,
                    row->sym, args);
    } else if (row->wrap == EK_WRAP_REF) {
        lower_emitf(c, "ek_val %s = ek_val_ref(%s(%s));\n", t, row->sym,
                    args);
    } else if (row->wrap == EK_WRAP_BOOL) {
        lower_emitf(c, "ek_val %s = ek_bool(%s(%s));\n", t, row->sym,
                    args);
    } else if (row->wrap == EK_WRAP_SHOW && row->radix) {
        lower_emitf(c, "ek_val %s = ek_val_ref(%s(%s, %d));\n", t,
                    row->sym, args, row->radix);
    } else if (row->wrap == EK_WRAP_SHOW) {
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
                           const ek_node *e, const ek_surface_row *row,
                           const char *args, const char **as)
{
    switch (row->wrap) {
    case EK_WRAP_OPTION:
        return lower_wrap_option(c, scope, e, row->sym, args, row->out);
    case EK_WRAP_OPTION_OBJ:
        return lower_wrap_option_obj(c, scope, e, row->sym, args);
    case EK_WRAP_PARSE:
        return lower_wrap_parse(c, scope, e, row->sym, args, row->out,
                                row->radix);
    case EK_WRAP_DECODE:
        return lower_wrap_decode(c, scope, e, row->sym, args);
    default:
        return emit_plain(c, scope, row, args, as);
    }
}

/* Whether the row carries a key or element of the receiver's own key
   domain at all. */
static int keys_over_domain(const char *family, const char *member)
{
    if (strcmp(family, "Dict") == 0) {
        return strcmp(member, "get") == 0 || strcmp(member, "insert") == 0
            || strcmp(member, "remove") == 0
            || strcmp(member, "merge") == 0;
    }
    if (strcmp(family, "Set") == 0) {
        return strcmp(member, "of") == 0 || strcmp(member, "insert") == 0
            || strcmp(member, "remove") == 0
            || strcmp(member, "contains") == 0;
    }
    return 0;
}

/* Stamps the structural keying of the domain the row builds over: the
   dictionary's key or the set's element, read off the receiver's type
   or, at the qualified form, off the list argument's element type. */
static void stamp_key_domain(lower_ctx *c, const ek_node *e,
                             const ek_node *callee, const char *family,
                             int has_recv)
{
    const ek_type *rt;
    const ek_type *kt = NULL;

    if (has_recv && callee->u.field.recv) {
        rt = lower_row_type(c, callee->u.field.recv);
    } else if (e->u.call.args.count) {
        rt = lower_row_type(c, e->u.call.args.data[0]);
    } else {
        rt = NULL;
    }
    if (!rt) {
        return;
    }
    if (strcmp(family, "Dict") == 0 && rt->kind == EK_T_DICT) {
        kt = rt->u.dict.key;
    } else if (strcmp(family, "Set") == 0) {
        if (rt->kind == EK_T_LIST) {
            kt = rt->u.list.elem;
        } else if (rt->kind == EK_T_NOMINAL && rt->u.nominal.argc == 1) {
            kt = rt->u.nominal.args[0];
        }
    }
    if (kt) {
        lower_key_hooks(c, kt);
    }
}

/* The element type of the domain the Set.of row builds over, read off
   the list the same way the stamping reads it. */
static const ek_type *set_of_element(lower_ctx *c, const ek_node *e,
                                     const ek_node *callee, int has_recv)
{
    const ek_type *rt = has_recv && callee->u.field.recv
                            ? lower_row_type(c, callee->u.field.recv)
                            : e->u.call.args.count
                                  ? lower_row_type(c,
                                                   e->u.call.args.data[0])
                                  : NULL;

    if (!rt) {
        return NULL;
    }
    if (rt->kind == EK_T_LIST) {
        return rt->u.list.elem;
    }
    if (rt->kind == EK_T_NOMINAL && rt->u.nominal.argc == 1) {
        return rt->u.nominal.args[0];
    }
    return NULL;
}

/* The Set.of row: the element domain's hook pair rides with the list,
   so the set keys its elements by the domain's own equality. */
static lower_val emit_set_of(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             int has_recv, const char *list)
{
    const ek_type *kt = set_of_element(c, e, callee, has_recv);
    const char *kq = "ek_key_eq";
    const char *kh = "ek_key_hash";
    const char *t = lower_temp(c, scope);

    if (kt && !lower_key_pair(c, kt, &kq, &kh)) {
        lower_error(c, e, "the set element is not a key domain this "
                          "lowerer supports");
    }
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_set_of(%s, %s, %s);\n", t, list, kq, kh);
    return (lower_val){ t, 1 };
}

/* The notes a routed call stamps beside its emission: the Json and
   time constructor families join the unit, and a row that builds over
   a key domain stamps the domain's hooks. */
static void stamp_family_notes(lower_ctx *c, const ek_node *e,
                               const ek_node *callee, const char *family,
                               int has_recv)
{
    if (strcmp(family, "Json") == 0) {
        lower_json_families(c, e);
    } else if (strcmp(family, "Time") == 0
               || strcmp(family, "Duration") == 0) {
        lower_time_families(c, e);
    } else if (keys_over_domain(family, callee->u.field.name)) {
        stamp_key_domain(c, e, callee, family, has_recv);
    }
}

lower_val lower_intrinsic_call(lower_ctx *c, lower_scope *scope,
                               const ek_node *e, const ek_node *callee,
                               const char *family, int has_recv)
{
    const ek_surface_row *row = ek_surface_find(family, callee->u.field.name);
    const char *letters;
    const char **as;
    size_t na, written;
    lower_val out;

    if (row && row->route == EK_ROUTE_CAP) {
        return lower_cap_call(c, scope, e, callee, family, has_recv);
    }
    if (!row || row->route != EK_ROUTE_CALL) {
        lower_val v = lower_unwrap_call(c, scope, e, callee, family,
                                        callee->u.field.name, has_recv);

        return v.text ? v : (lower_val){ NULL, 0 };
    }
    letters = row->args ? row->args : "v";
    na = strlen(letters);
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
    stamp_family_notes(c, e, callee, family, has_recv);
    if (strcmp(family, "Set") == 0 && strcmp(callee->u.field.name, "of") == 0) {
        out = emit_set_of(c, scope, e, callee, has_recv, as[0]);
    } else {
        const char *args = value_args(c, letters, as, na);

        out = emit_wrap(c, scope, e, row, args, as);
    }
    release_temps(c, as, na);
    return out;
}
