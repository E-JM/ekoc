#include <string.h>

#include "lower/internal.h"

/* The capability channel's wrapping: the runtime's status contracts
   over the file family and the process exit become the std Result and
   IoError values the signatures carry. The success arm wraps by the
   row's shape; the failure arm maps the reason kind onto the IoError
   constructors, whose families register exactly as composite
   construction registers them. Every emitter builds one owned result
   temp; the detail and payload objects the runtime handed over die
   with the wrapping. */

/* The reason kinds the runtime reports, in the file family ABI's
   order: the two payload kinds close the list. */
enum { IO_INVALID_DATA = 5, IO_OTHER = 6 };

static void emit_ok_arm(lower_ctx *c, lower_scope *scope, lower_io_ok ok,
                        const char *t, const char *o, const char *okh,
                        const char *some, const char *none)
{
    const char *p = lower_temp(c, scope);
    const char *q = lower_temp(c, scope);

    if (ok == LOWER_IO_OK_UNIT) {
        lower_pad(c);
        lower_emitf(c, "%s = %s(ek_val_unit());\n", t, okh);
        return;
    }
    if (ok == LOWER_IO_OK_REF) {
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_val_ref(%s);\n", p, o);
        lower_pad(c);
        lower_emitf(c, "%s = %s(%s);\n", t, okh, p);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", p);
        return;
    }
    lower_pad(c);
    lower_emitf(c, "if (%s != (ek_obj *)0) {\n", o);
    c->depth++;
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_val_ref(%s);\n", p, o);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(%s);\n", q, some, p);
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s);\n", t, okh, q);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", q);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", p);
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth++;
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s();\n", q, none);
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s);\n", t, okh, q);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", q);
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "}\n");
}

/* One payload reason arm: the detail string moves into the value, the
   missing detail falling back to the cached empty literal. */
static void emit_detail_arm(lower_ctx *c, const char *ioerr,
                            const char *y, const char *helper,
                            const char *dt)
{
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s != (ek_obj *)0"
                   " ? ek_val_ref(%s) : ek_keep(%s);\n", dt, y, y,
                lower_str_cache(c, "", 0));
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s);\n", ioerr, helper, dt);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", dt);
}

/* The failure arm: the reason kind selects the IoError constructor,
   the payload kinds carrying the detail the runtime wrote. */
static void emit_err_arm(lower_ctx *c, lower_scope *scope, const char *t,
                         const char *k, const char *y, const char *errh,
                         const char **zero, const char *inv,
                         const char *oth)
{
    static const int kinds[5] = {0, 1, 2, 3, 4};
    const char *ioerr = lower_temp(c, scope);
    const char *dt = lower_temp(c, scope);
    size_t i;

    lower_pad(c);
    lower_emitf(c, "ek_val %s;\n", ioerr);
    for (i = 0; i < sizeof kinds / sizeof *kinds; i++) {
        lower_pad(c);
        lower_emitf(c, "%s (%s == %d) {\n", i ? "} else if" : "if", k,
                    kinds[i]);
        c->depth++;
        lower_pad(c);
        lower_emitf(c, "%s = %s();\n", ioerr, zero[i]);
        c->depth--;
    }
    lower_pad(c);
    lower_emitf(c, "} else if (%s == %d) {\n", k, IO_INVALID_DATA);
    c->depth++;
    emit_detail_arm(c, ioerr, y, inv, dt);
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth++;
    emit_detail_arm(c, ioerr, y, oth, dt);
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "}\n");
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s);\n", t, errh, ioerr);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", ioerr);
}

/* The constructors of a wrap: the Result arms, the five zero-payload
   IoError reasons, the two payload reasons, and the Option pair at the
   OPT shape. 0 when any is missing. */
static int io_ctors(lower_ctx *c, const ek_node *e, lower_io_ok ok,
                    const char **okh, const char **errh, const char **inv,
                    const char **oth, const char **zero,
                    const char **some, const char **none)
{
    static const char *zero_names[5] = {"NotFound", "Permission", "Exists",
                                        "NotFile", "Closed"};
    int nok = 0, nerr = 0, nsome = 0, nnone = 0;
    int nz[5] = {0, 0, 0, 0, 0};
    int ninv = 0, noth = 0;
    size_t i;

    *okh = lower_ctor_helper(c, e, "std", "Result", "Ok", &nok);
    *errh = lower_ctor_helper(c, e, "std", "Result", "Err", &nerr);
    *inv = lower_ctor_helper(c, e, "sys", "IoError", "InvalidData", &ninv);
    *oth = lower_ctor_helper(c, e, "sys", "IoError", "Other", &noth);
    for (i = 0; i < 5; i++) {
        zero[i] = lower_ctor_helper(c, e, "sys", "IoError", zero_names[i],
                                    &nz[i]);
    }
    if (ok == LOWER_IO_OK_OPT) {
        *some = lower_ctor_helper(c, e, "std", "Option", "Some", &nsome);
        *none = lower_ctor_helper(c, e, "std", "Option", "None", &nnone);
    }
    for (i = 0; i < 5; i++) {
        if (!zero[i]) {
            return 0;
        }
    }
    return *okh && *errh && *inv && *oth
        && (ok != LOWER_IO_OK_OPT || (*some && *none));
}

lower_val lower_wrap_io(lower_ctx *c, lower_scope *scope, const ek_node *e,
                        const char *sym, const char *args, lower_io_ok ok)
{
    unsigned d = c->depth;
    const char *t = lower_temp(c, scope);
    const char *o = lower_temp(c, scope);
    const char *y = lower_temp(c, scope);
    const char *k = lower_temp(c, scope);
    const char *zero[5];
    const char *okh, *errh, *inv, *oth;
    const char *some = NULL;
    const char *none = NULL;

    if (!io_ctors(c, e, ok, &okh, &errh, &inv, &oth, zero, &some, &none)) {
        return lower_val_unit();
    }
    lower_pad(c);
    if (ok != LOWER_IO_OK_UNIT) {
        lower_emitf(c, "ek_obj *%s = (ek_obj *)0;\n", o);
    }
    lower_emitf(c, "ek_obj *%s = (ek_obj *)0;\n", y);
    lower_pad(c);
    lower_emitf(c, "int %s = 0;\n", k);
    lower_pad(c);
    lower_emitf(c, "ek_val %s;\n", t);
    lower_pad(c);
    if (ok != LOWER_IO_OK_UNIT) {
        lower_emitf(c, "if (%s(%s, &%s, &%s, &%s)) {\n", sym, args, o, k, y);
    } else {
        lower_emitf(c, "if (%s(%s, &%s, &%s)) {\n", sym, args, k, y);
    }
    c->depth = d + 1;
    emit_ok_arm(c, scope, ok, t, o, okh, some, none);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth = d + 1;
    emit_err_arm(c, scope, t, k, y, errh, zero, inv, oth);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "}\n");
    return (lower_val){ t, 1 };
}

/* The exit call: the code passes through and the process ends there,
   so the call carries no result temp. */
lower_val lower_wrap_exit(lower_ctx *c, lower_scope *scope, const char *sym,
                          const char *args)
{
    (void)scope;
    lower_pad(c);
    lower_emitf(c, "%s(%s);\n", sym, args);
    return lower_val_unit();
}

/* The pure construction of a capability value pulls the constructor's
   own object file in through archive resolution, so the row's
   capability registers the families that pull names. The File handle
   family is no manifest capability and has no bundle behind it, so its
   rows never register -- the row's own flag carries that decision,
   whatever shape a future member dispatches over. The handle methods
   build their Result values over the unit's own helpers and stamp
   their families through that construction instead. */
static void register_static_families(lower_ctx *c, const ek_node *e,
                                     const ek_surface_row *row)
{
    if (row->registers) {
        lower_cap_families_of(c, e, row->ns);
    }
}

/* The joined runtime arguments: object slots pass the reference, the
   int64 slots the raw word, the value slots the value. */
static const char *value_args(lower_ctx *c, const char *letters,
                              const char **as, size_t na)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; i < na; i++) {
        int ok;

        if (i && !ek_buf_append_str(&b, ", ")) {
            c->oom = 1;
        }
        if (letters[i] == 'o') {
            ok = ek_buf_printf(&b, "%s.as.ref", as[i]);
        } else if (letters[i] == 'i') {
            ok = ek_buf_printf(&b, "%s.as.i", as[i]);
        } else {
            ok = ek_buf_append_str(&b, as[i]);
        }
        if (!ok) {
            c->oom = 1;
        }
    }
    return lower_take(c, &b);
}

/* The written slots of the call: the receiver first at a member link,
   then the call's arguments, each a temp in source order. */
static const char **cap_args(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             size_t na, size_t written, int has_recv,
                             size_t *cap)
{
    const char **as = lower_grow(c, NULL, cap, na ? na : 1, sizeof *as);
    size_t i;

    if (!as) {
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

/* The row's shape picks the wrap: the plain call, the three status
   contracts, or the bare exit. */
static lower_val cap_shape_call(lower_ctx *c, lower_scope *scope,
                                const ek_node *e, const ek_surface_row *row,
                                const char *args)
{
    switch (row->shape) {
    case EK_CAP_PLAIN: {
        const char *t = lower_temp(c, scope);

        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s(%s);\n", t, row->sym, args);
        return (lower_val){ t, 1 };
    }
    case EK_CAP_IO_REF:
        return lower_wrap_io(c, scope, e, row->sym, args, LOWER_IO_OK_REF);
    case EK_CAP_IO_UNIT:
        return lower_wrap_io(c, scope, e, row->sym, args,
                             LOWER_IO_OK_UNIT);
    case EK_CAP_IO_OPT:
        return lower_wrap_io(c, scope, e, row->sym, args, LOWER_IO_OK_OPT);
    default:
        return lower_wrap_exit(c, scope, row->sym, args);
    }
}

lower_val lower_cap_call(lower_ctx *c, lower_scope *scope,
                         const ek_node *e, const ek_node *callee,
                         const char *family, int has_recv)
{
    const ek_surface_row *row;
    const char **as;
    size_t cap = 0, na, written, i;
    lower_val out;

    row = ek_surface_find(family, callee->u.field.name);
    if (!row || row->route != EK_ROUTE_CAP) {
        return (lower_val){ NULL, 0 };
    }
    na = strlen(row->args ? row->args : "v");
    written = na - (size_t)has_recv;
    if (e->u.call.args.count != written) {
        lower_error(c, e, "the call of `%s.%s` is malformed", family,
                    callee->u.field.name);
        return lower_val_unit();
    }
    register_static_families(c, e, row);
    as = cap_args(c, scope, e, callee, na, written, has_recv, &cap);
    if (!as) {
        return lower_val_unit();
    }
    out = cap_shape_call(c, scope, e, row,
                         value_args(c, row->args ? row->args : "v", as, na));
    for (i = na; i-- > 0;) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", as[i]);
    }
    return out;
}
