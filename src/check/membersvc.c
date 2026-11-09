#include <string.h>

#include "check/internal.h"

/* The derived services a receiver's namespace carries by default: the
   equality pair, the ordering family, the display pair, the JSON pair,
   with, and the panic intrinsic. A service either derives over the
   receiver or the miss is diagnosed here; the member-link dispatch of
   member.c consults them after the declared members. */
/* The two-receiver arrow of a derived service. */
static const ek_type *derived_fn(ck_ctx *c, const ek_type *r,
                                 const ek_type *bool_t)
{
    const ek_type **ps = ek_arena_alloc(c->arena, 2 * sizeof *ps);

    if (ps) {
        ps[0] = r;
        ps[1] = r;
        return ck_fn(c, ps, 2, bool_t, EK_ARROW_PURE);
    }
    c->oom = 1;
    return ck_ivar(c);
}

/* The one-parameter arrow of a derived service. */
static const ek_type *service_fn(ck_ctx *c, const ek_type *param,
                                 const ek_type *result)
{
    const ek_type **ps = ek_arena_alloc(c->arena, sizeof *ps);

    if (ps) {
        ps[0] = param;
        return ck_fn(c, ps, 1, result, EK_ARROW_PURE);
    }
    c->oom = 1;
    return ck_ivar(c);
}

/* A member the receiver does not carry, in a namespace where absence is
   final: reported regardless of who else could have supplied it. */
static void absent(ck_ctx *c, const ek_node *at, const ek_type *r,
                   const char *name)
{
    ck_error(c, at, "E-unbound-member",
             "the receiver type %s has no member `%s` (Clause 7.4.3)",
             ck_type_name(c, r), name);
    /* The link is reported: the call over it makes no bit claim. */
    ck_map_put(c, &c->absent, at, (const void *)1);
}

/* The derived equality pair: E-no-eq where the derivation halts, the
   service where it does not. NULL falls through. */
const ek_type *ck_derive_eq(ck_ctx *c, const ek_node *at,
                                const ek_type *r, const char *name,
                                ek_reading *rd, const ek_type *bool_t)
{
    if (strcmp(name, "eq") != 0 && strcmp(name, "ne") != 0) {
        return NULL;
    }
    ck_req_note(c, at, r, name);
    if (ck_no_eq(c, r)) {
        /* The derivation halts here: equality over the type is
           E-no-eq and the member is absent. */
        ck_no_eq_error(c, at, r);
        return NULL;
    }
    *rd = EK_RD_DERIVED;
    return derived_fn(c, r, bool_t);
}

/* The value layer: the types whose equality and ordering the compiler
   owns. `Bool` is not among them; it has no ordering. `Range` orders
   over its endpoints. */
static int value_layer(const ek_type *r)
{
    static const char *names[] = {
        "Int",   "UInt", "Float", "Char", "String", "Bytes",
        "Range", "I8",   "I16",  "I32",  "I64",  "U8",
        "U16",   "U32",  "U64",  "F32",   "F64"
    };
    size_t i;

    if (!r || r->kind != EK_T_NOMINAL || !r->u.nominal.sym) {
        return 0;
    }
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        if (ck_is_std(r, names[i])) {
            return 1;
        }
    }
    return 0;
}

/* The ordering family over a receiver that has an order at all:
   capabilities and functions have none, and the halt is reported as
   the equality halt that it follows from. */
static void no_order_error(ck_ctx *c, const ek_node *at, const ek_type *r)
{
    ck_error(c, at, "E-no-eq",
             "ordering over %s has no derivation; capabilities, "
             "handles, and functions have no `lt` (Clause 9.8.5)",
             ck_type_name(c, r));
}

/* The derived ordering family: the value layer, and declared ADTs
   other than Bool by declaration order. A receiver with no order at
   all is reported rather than bridged, because the family is closed.
   NULL falls through. */
const ek_type *ck_derive_lt(ck_ctx *c, const ek_node *at,
                                const ek_type *r, const char *name,
                                ek_reading *rd, const ek_type *bool_t)
{
    const ck_decl *d;

    if (strcmp(name, "lt") != 0 && strcmp(name, "le") != 0
        && strcmp(name, "gt") != 0 && strcmp(name, "ge") != 0) {
        return NULL;
    }
    ck_req_note(c, at, r, name);
    if (ck_is_capability(r) || ck_is_handle(r) || r->kind == EK_T_FN) {
        no_order_error(c, at, r);
        return NULL;
    }
    if (!value_layer(r)) {
        if (r->kind != EK_T_NOMINAL || !r->u.nominal.sym
            || !r->u.nominal.sym->decl) {
            absent(c, at, r, name);
            return NULL;
        }
        d = ck_decl_of(c, r->u.nominal.sym);
        if (!d || d->rhs != EK_TYPE_RHS_VARIANTS || ck_is_std(r, "Bool")) {
            absent(c, at, r, name);
            return NULL;
        }
        if (ck_no_eq(c, r)) {
            ck_no_eq_error(c, at, r);
            return NULL;
        }
    }
    *rd = EK_RD_DERIVED;
    return derived_fn(c, r, bool_t);
}

/* True when a display service derives over the receiver: its type is
   determined, so the arrow carries real parameter and result types. A
   capability, a value-layer type, and a placeholder all display; a
   type variable and an inference variable stay the opaque bridge. */
static int display_receiver(const ek_type *r)
{
    switch (r->kind) {
    case EK_T_RECORD:
    case EK_T_TUPLE:
    case EK_T_LIST:
    case EK_T_DICT:
    case EK_T_UNIT:
    case EK_T_FN:
    case EK_T_NOMINAL:
        return 1;
    default:
        return 0;
    }
}

/* The derived display services over a determined receiver: show and
   inspect are pure `(t) -> String` members, inspect never overridable
   and show losing to a declared member (the namespace is consulted
   first). NULL falls through to opacity. */
const ek_type *ck_derive_display(ck_ctx *c, const ek_type *r,
                                     const char *name, ek_reading *rd)
{
    const ek_type *string_t;

    if (strcmp(name, "show") != 0 && strcmp(name, "inspect") != 0) {
        return NULL;
    }
    if (!display_receiver(r)) {
        return NULL;
    }
    *rd = EK_RD_DERIVED;
    string_t = ck_std_type(c, "String");
    return service_fn(c, r, string_t);
}

/* The panic intrinsic is a compiler-generated service of String's
   namespace: the message receiver, the never result. */
const ek_type *ck_derive_panic(ck_ctx *c, const ek_type *r,
                                   const char *name, ek_reading *rd)
{
    if (strcmp(name, "panic") != 0 || !ck_is_std(r, "String")) {
        return NULL;
    }
    *rd = EK_RD_DERIVED;
    return service_fn(c, ck_std_type(c, "String"), ck_never(c));
}

/* The structural receiver of a compiler-supplied container head: List
   and Dict are the unapplied spellings of the structural composites,
   so a service written at such a head is written at the composite over
   fresh variables the arguments and the expected type then determine.
   NULL for any other head. */
static const ek_type *container_receiver(ck_ctx *c, const ek_type *r)
{
    if (ck_is_std(r, "List")) {
        return ck_list(c, ck_ivar(c));
    }
    if (ck_is_std(r, "Dict")) {
        return ck_dict(c, ck_ivar(c), ck_ivar(c));
    }
    return NULL;
}

/* The receiver a derived service is written at: an unapplied generic
   type head takes fresh variables, which the arguments and the
   expected type then determine. A declared head counts its binders; a
   head with no declaration behind it is compiler-supplied, so its
   count comes from the surface table, and the two container heads
   take their structural shapes directly. */
const ek_type *ck_service_receiver(ck_ctx *c, const ek_type *r)
{
    const ek_symbol *sym;
    const ek_type *shape;
    size_t nb;

    if (r->kind != EK_T_NOMINAL || !(sym = r->u.nominal.sym)
        || r->u.nominal.argc != 0) {
        return r;
    }
    if (!sym->decl) {
        shape = container_receiver(c, r);
        if (shape) {
            return shape;
        }
    }
    nb = sym->decl ? ck_decl_of(c, sym)->nbinders
                   : ek_surface_generic_arity(sym->module, sym->name);
    return nb ? ck_nominal_fresh_n(c, sym, nb) : r;
}

/* The derived JSON pair over a receiver whose form exists: the
   mapping halts at a capability, a function, and a non-string-keyed
   dictionary. `to_json` is `(t) -> Json`; `from_json` takes the JSON
   value and yields the receiver type in a `Result`. NULL falls
   through, and the halt is diagnosed at the link. */
const ek_type *ck_derive_json(ck_ctx *c, const ek_node *at,
                                  const ek_type *r, const char *name,
                                  ek_reading *rd)
{
    const ek_type *json_t;
    const ek_type *perr;
    const ek_symbol *result;

    if (strcmp(name, "to_json") != 0 && strcmp(name, "from_json") != 0) {
        return NULL;
    }
    ck_req_note(c, at, r, name);
    if (r->kind == EK_T_IVAR || r->kind == EK_T_TVAR) {
        return NULL; /* an undetermined receiver: the bridge */
    }
    if (ck_no_json(c, r)) {
        ck_json_shape_error(c, at, r);
        return NULL;
    }
    *rd = EK_RD_DERIVED;
    json_t = ck_std_type(c, "Json");
    if (strcmp(name, "to_json") == 0) {
        return service_fn(c, r, json_t);
    }
    perr = ck_std_type(c, "ParseError");
    result = ck_std_symbol(c, "Result");
    if (!result || !perr) {
        return ck_opaque(c);
    }
    {
        const ek_type **args = ek_arena_alloc(c->arena, 2 * sizeof *args);

        if (!args) {
            c->oom = 1;
            return ck_ivar(c);
        }
        args[0] = r;
        args[1] = perr;
        return service_fn(c, json_t, ck_nominal(c, result, args, 2));
    }
}

/* The closest expressible arrow of the `with` service: the service's
   second parameter (the partial fields) has no type-syntax name, so
   the link row carries (t, t) -> t as a stand-in; the call itself is
   typed in call.c, which checks the literal against t directly. */
static const ek_type *with_fn(ck_ctx *c, const ek_type *r)
{
    const ek_type **ps = ek_arena_alloc(c->arena, 2 * sizeof *ps);

    if (ps) {
        ps[0] = r;
        ps[1] = r;
        return ck_fn(c, ps, 2, r, EK_ARROW_PURE);
    }
    c->oom = 1;
    return ck_ivar(c);
}

/* True when the nominal is a std placeholder: it has no declaration,
   so its shape is opaque and nothing is diagnosed about it. */
static int std_placeholder(const ek_type *r)
{
    return r->u.nominal.sym->decl == NULL;
}

/* The derived with member over a record receiver: an anonymous record
   or a record-shaped nominal. A receiver whose type is otherwise known
   is rejected here; an opaque placeholder other than a value-layer
   type falls through to opacity. NULL falls through. */
const ek_type *ck_derive_with(ck_ctx *c, const ek_node *at,
                                  const ek_type *r, const char *name,
                                  ek_reading *rd)
{
    const ek_type *rec = r;

    if (strcmp(name, "with") != 0) {
        return NULL;
    }
    if (rec->kind == EK_T_NOMINAL) {
        if (std_placeholder(rec) && !value_layer(rec)) {
            return NULL;
        }
        rec = ck_decl_repr(c, rec);
        if (!rec || rec->kind != EK_T_RECORD) {
            ck_error(c, at, "E-type-mismatch",
                     "`with` constructs a record from a record, not %s "
                     "(Clause 9.8.6)", ck_type_name(c, r));
            return NULL;
        }
    } else if (rec->kind != EK_T_RECORD) {
        ck_error(c, at, "E-type-mismatch",
                 "`with` constructs a record from a record, not %s "
                 "(Clause 9.8.6)", ck_type_name(c, r));
        return NULL;
    }
    *rd = EK_RD_DERIVED;
    return with_fn(c, r);
}

/* True when the receiver's namespace is one the compiler still supplies
   members for: the std surface, the shipped companions, and the two
   container namespaces. A declared module's own namespace is complete
   and never bridged. */
static int std_namespace(const ek_type *r)
{
    switch (r->kind) {
    case EK_T_LIST:
    case EK_T_DICT:
        return 1;
    case EK_T_NOMINAL:
        return r->u.nominal.sym && r->u.nominal.sym->module
            && (strcmp(r->u.nominal.sym->module, "std") == 0
                || strcmp(r->u.nominal.sym->module, "time") == 0);
    default:
        return 0;
    }
}

/* A name no station answered. A derived service is reported by its own
   derivation; any other name is reported too, unless the namespace is
   one the compiler still supplies, where the opaque bridge stands. */
const ek_type *ck_no_member(ck_ctx *c, const ek_node *at,
                                const ek_type *r, const char *name,
                                const ek_symbol *mod)
{
    if (ck_service_name(name)) {
        return ck_opaque(c);
    }
    if (mod) {
        /* The chain named a module that holds no member of this
           spelling for this receiver: the deferral resolve made ends
           here, with the module's own words. */
        ck_error(c, at, "E-unbound-member",
                 "module `%s` declares no member `%s` for the receiver %s "
                 "(Clauses 7.4.3, 7.4.4)", mod->name, name,
                 ck_type_name(c, r));
        ck_map_put(c, &c->absent, at, (const void *)1);
        return ck_opaque(c);
    }
    /* A namespace the compiler rows complete has no opaque names
       left; every other std namespace keeps the bridge. */
    if (!std_namespace(r) || ck_ns_owned(r)) {
        absent(c, at, r, name);
    }
    return ck_opaque(c);
}
