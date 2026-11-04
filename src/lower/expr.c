#include <string.h>

#include "lower/internal.h"

/* Expressions other than literals and calls: names, operators, and
   fields. Every result is an owned value: operator results are born
   owned, a name read is an owned copy through the helper, and a name
   holding a function is a fresh closure over its static trampoline.
   Operands evaluate to temps in source order and are released once
   their use is emitted. */

/* A descent that is not a tail position of the function being
   emitted. */
lower_val lower_sub(lower_ctx *c, lower_scope *scope, const ek_node *e)
{
    int saved = c->in_tail;
    lower_val v;

    c->in_tail = 0;
    v = lower_expr(c, scope, e);
    c->in_tail = saved;
    return v;
}

/* True and False are scalar-represented; the check row's Bool decides.
   Every other constructor lands through the generated struct layer. */
static lower_val ctor_expr(lower_ctx *c, const ek_node *e, const ek_ref *ref)
{
    const ek_type *t = lower_row_type(c, e);
    const ek_symbol *sym = ref->symbol;
    int is_bool = t && t->kind == EK_T_NOMINAL && t->u.nominal.sym
        && strcmp(t->u.nominal.sym->name, "Bool") == 0;

    if (is_bool && sym && sym->owner && strcmp(sym->owner, "Bool") == 0) {
        if (strcmp(sym->name, "True") == 0) {
            return (lower_val){ "ek_bool(1)", 0 };
        }
        if (strcmp(sym->name, "False") == 0) {
            return (lower_val){ "ek_bool(0)", 0 };
        }
    }
    return lower_ctor_value(c, e, ref);
}

/* The sole constructor a bridged chain head names, over the nominal
   check anchored at it: the same sole-candidate rule check applied, read
   off the closure's surfaces -- check already refused the generic and
   the ambiguous heads by leaving the row opaque. NULL keeps the plain
   refusal. */
static const ek_symbol *bridged_ctor(lower_ctx *c, const ek_node *e)
{
    const ek_type *t = lower_row_type(c, e);
    const ek_symbol *tsym = t && t->kind == EK_T_NOMINAL
                                ? t->u.nominal.sym : NULL;
    const char *name = e->u.path.count == 1 ? e->u.path.data[0] : NULL;
    const ek_symbol *cand = NULL;
    size_t k, i;

    if (!tsym || !name) {
        return NULL;
    }
    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind != EK_SYM_CONSTRUCTOR || !m->owner
                || strcmp(m->name, name) != 0
                || strcmp(m->owner, tsym->name) != 0) {
                continue;
            }
            if (cand && cand != m) {
                return NULL; /* an ambiguity keeps the refusal */
            }
            cand = m;
        }
    }
    return cand;
}

static lower_val name_expr(lower_ctx *c, lower_scope *scope, const ek_node *e)
{
    const ek_ref *ref;
    const lower_local *loc;

    if (e->u.path.count == 1 && strcmp(e->u.path.data[0], "_") == 0) {
        return lower_val_unit(); /* the unit value at value position */
    }
    ref = lower_ref(c, e);
    if (!ref) {
        return lower_val_unit(); /* resolve diagnosed the reference */
    }
    if (ref->kind == EK_SYM_LOCAL || ref->kind == EK_SYM_PARAM) {
        if (!ref->symbol) {
            return lower_val_unit();
        }
        loc = scope_find_local(scope, ref->symbol->name);
        if (!loc) {
            lower_error(c, e, "the binding of `%s` has no lowering",
                        ref->symbol->name);
            return lower_val_unit();
        }
        return (lower_val){ lower_str(c, "ek_keep(%s)", loc->cname), 0 };
    }
    if (ref->kind == EK_SYM_CONSTRUCTOR) {
        return ctor_expr(c, e, ref);
    }
    if (ref->kind == EK_SYM_FUNC && ref->symbol) {
        if (lower_is_value(ref->symbol)) {
            lower_enqueue(c, ref->symbol);
            return (lower_val){ lower_str(c, "%s()",
                                            lower_value_getter(c,
                                                               ref->symbol)),
                                0 };
        }
        {
            const char *ekt = lower_trampoline(c, ref->symbol);

            return (lower_val){ lower_str(c, "ek_closure_new(%s, "
                                             "ek_val_unit())", ekt), 0 };
        }
    }
    if (ref->kind == EK_SYM_EXTERNAL && !ref->ncands) {
        /* a receiver-form head the resolver bridged: check anchored a
           sole constructor's nominal over it, so the head reads as that
           constructor's own value */
        const ek_symbol *ctor = bridged_ctor(c, e);

        if (ctor) {
            ek_ref cons = *ref;

            cons.kind = EK_SYM_CONSTRUCTOR;
            cons.station = EK_ST_CONS;
            cons.symbol = ctor;
            return ctor_expr(c, e, &cons);
        }
    }
    lower_error(c, e, "the reference to `%s` is not supported by this "
                      "lowerer",
                e->u.path.count == 1 ? e->u.path.data[0] : "?");
    return lower_val_unit();
}

/* A module-qualified read of a top-level binding or function: the
   value the name read gives, spelled through the qualified path. */
static lower_val qualified_read(lower_ctx *c, const ek_node *e)
{
    const ek_ref *ref = lower_ref(c, e);

    if (!ref || ref->kind != EK_SYM_FUNC || !ref->symbol) {
        return (lower_val){ NULL, 0 };
    }
    if (lower_is_value(ref->symbol)) {
        lower_enqueue(c, ref->symbol);
        return (lower_val){ lower_str(c, "%s()",
                                      lower_value_getter(c, ref->symbol)),
                            0 };
    }
    {
        const char *ekt = lower_trampoline(c, ref->symbol);

        return (lower_val){ lower_str(c, "ek_closure_new(%s, "
                                         "ek_val_unit())", ekt), 0 };
    }
}

/* A compiler row constant of the std scalar namespaces read through
   the type's name: the domain bounds, born unowned over the value slot
   the domain travels in. */
static lower_val ns_constant_read(lower_ctx *c, const ek_node *e)
{
    static const struct {
        const char *family;
        const char *name;
        const char *text;
    } rows[] = {
        {"Int", "min", "ek_val_int(INT64_MIN)"},
        {"Int", "max", "ek_val_int(INT64_MAX)"},
        {"UInt", "max", "ek_val_uint(UINT64_MAX)"},
    };
    const ek_ck_row *row = lower_row(c, e);
    const ek_ref *head;
    size_t i;

    if (!row || row->reading != EK_RD_NAMESPACE) {
        return (lower_val){ NULL, 0 };
    }
    head = lower_ref(c, e->u.field.recv);
    if (!head || head->kind != EK_SYM_TYPE || !head->symbol) {
        return (lower_val){ NULL, 0 };
    }
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        if (strcmp(rows[i].family, head->symbol->name) == 0
            && strcmp(rows[i].name, e->u.field.name) == 0) {
            return (lower_val){ rows[i].text, 0 };
        }
    }
    return (lower_val){ NULL, 0 };
}

/* A field access: the receiver lowers to a temp, the read is an owned
   copy out of the generated struct, and the receiver temp dies. The
   receiver borrows the frame local it reads -- the slot read is
   immediate and keeps nothing but the field's own copy -- so a local
   receiver costs no keep and no release. */
static lower_val field_expr(lower_ctx *c, lower_scope *scope,
                            const ek_node *e)
{
    const ek_ck_row *row = lower_row(c, e);
    const ek_type *rt = lower_row_type(c, e->u.field.recv);
    const char *bn;
    lower_val v;
    const char *t, *r, *slot;
    int owned;

    if (!row || row->reading != EK_RD_FIELD || !rt) {
        lower_val q = qualified_read(c, e);

        if (!q.text) {
            q = ns_constant_read(c, e);
        }
        if (q.text) {
            return q;
        }
        lower_error(c, e, "the %s is not supported by this lowerer",
                    lower_construct(e));
        return lower_val_unit();
    }
    bn = lower_borrow_local(c, scope, e->u.field.recv);
    if (bn) {
        t = bn;
        owned = 0;
    } else {
        v = lower_sub(c, scope, e->u.field.recv);
        t = lower_as_temp(c, scope, &v);
        owned = 1;
    }
    slot = lower_field_slot(c, e, rt, t, e->u.field.name);
    if (!slot) {
        if (owned) {
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", t);
        }
        return lower_val_unit();
    }
    r = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_keep(%s);\n", r, slot);
    if (owned) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
    return (lower_val){ r, 1 };
}

/* The written name of a construct, for the unsupported-construct
   diagnostics. */
const char *lower_construct(const ek_node *n)
{
    switch (n->kind) {
    case EK_N_CASE: return "`case` expression";
    case EK_N_LAMBDA: return "lambda";
    case EK_N_COMPOSITE: return "composite literal";
    case EK_N_APPLY: return "constructor application";
    case EK_N_FIELD: return "field access";
    case EK_N_INVERT: return "`<-` binding";
    default: return "construct";
    }
}

static lower_val lower_block_expr(lower_ctx *c, lower_scope *scope,
                                  const ek_node *e)
{
    lower_scope s;
    lower_val v;
    unsigned d = c->depth;

    memset(&s, 0, sizeof s);
    s.parent = scope;
    c->depth = d + 1;
    v = lower_block_value(c, &s, e);
    c->depth = d;
    lower_release_scope(c, &s);
    return v;
}

lower_val lower_expr(lower_ctx *c, lower_scope *scope, const ek_node *e)
{
    if (!e) {
        return lower_val_unit();
    }
    switch (e->kind) {
    case EK_N_INT_LIT:
        return lower_int_lit(c, e);
    case EK_N_FLOAT_LIT:
        return lower_float_lit(c, e);
    case EK_N_CHAR_LIT:
        return lower_char_lit(c, e);
    case EK_N_STRING_LIT:
        return lower_string_expr(c, scope, e);
    case EK_N_NAME:
        return name_expr(c, scope, e);
    case EK_N_PAREN:
        return lower_expr(c, scope, e->u.wrap.inner);
    case EK_N_UNARY:
        return lower_unary_expr(c, scope, e);
    case EK_N_BINARY:
        return lower_binary_expr(c, scope, e);
    case EK_N_CALL:
        return lower_call_expr(c, scope, e);
    case EK_N_APPLY:
        return lower_ctor_apply(c, scope, e);
    case EK_N_COMPOSITE:
        return lower_composite(c, scope, e);
    case EK_N_FIELD:
        return field_expr(c, scope, e);
    case EK_N_IF: {
        lower_val v;

        lower_if(c, scope, e, &v);
        return v;
    }
    case EK_N_CASE:
        return lower_case(c, scope, e);
    case EK_N_LAMBDA:
        return lower_lambda(c, scope, e);
    case EK_N_INVERT:
        return lower_invert(c, scope, e, NULL, 0);
    case EK_N_BLOCK:
        return lower_block_expr(c, scope, e);
    default:
        lower_error(c, e, "the %s is not supported by this lowerer",
                    lower_construct(e));
        return lower_val_unit();
    }
}
