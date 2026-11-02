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
    lower_emitf(c, "%s = %s(%s(%s));\n", t, okh, some, p);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", p);
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth++;
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s());\n", t, okh, none);
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "}\n");
}

/* One payload reason arm: the detail string moves into the value. */
static void emit_detail_arm(lower_ctx *c, const char *ioerr,
                            const char *y, const char *helper,
                            const char *dt)
{
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s != (ek_obj *)0"
                   " ? ek_val_ref(%s) : ek_val_ref(ek_str_from_cstr(\"\"));"
                   "\n", dt, y, y);
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

lower_val lower_wrap_io(lower_ctx *c, lower_scope *scope, const ek_node *e,
                        const char *sym, const char *args, lower_io_ok ok)
{
    unsigned d = c->depth;
    const char *t = lower_temp(c, scope);
    const char *o = lower_temp(c, scope);
    const char *y = lower_temp(c, scope);
    const char *k = lower_temp(c, scope);
    int nok = 0, nerr = 0, nsome = 0, nnone = 0;
    int nz[5] = {0, 0, 0, 0, 0};
    int ninv = 0, noth = 0;
    static const char *zero_names[5] = {"NotFound", "Permission", "Exists",
                                        "NotFile", "Closed"};
    const char *okh = lower_ctor_helper(c, e, "std", "Result", "Ok", &nok);
    const char *errh = lower_ctor_helper(c, e, "std", "Result", "Err", &nerr);
    const char *inv = lower_ctor_helper(c, e, "cap", "IoError",
                                        "InvalidData", &ninv);
    const char *oth = lower_ctor_helper(c, e, "cap", "IoError", "Other",
                                        &noth);
    const char *zero[5];
    const char *some = NULL;
    const char *none = NULL;
    size_t i;

    for (i = 0; i < 5; i++) {
        zero[i] = lower_ctor_helper(c, e, "cap", "IoError", zero_names[i],
                                    &nz[i]);
    }
    if (ok == LOWER_IO_OK_OPT) {
        some = lower_ctor_helper(c, e, "std", "Option", "Some", &nsome);
        none = lower_ctor_helper(c, e, "std", "Option", "None", &nnone);
    }
    for (i = 0; i < 5; i++) {
        if (!zero[i]) {
            return lower_val_unit();
        }
    }
    if (!okh || !errh || !inv || !oth
        || (ok == LOWER_IO_OK_OPT && (!some || !none))) {
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

/* The compiler-provided rows of the capability channel (Clause 10.8):
   the eight File members over the runtime's handle table, the process
   exit, the two advancing derivations, and the error channel. The file
   reads keep the text gate; a stale handle reports Err Closed on every
   operation, a second close included. The argument letters follow the
   intrinsic discipline: v the value slot, o the object. */
typedef enum {
    CAP_PLAIN, /* the call value is the result */
    CAP_IO_REF,
    CAP_IO_UNIT,
    CAP_IO_OPT,
    CAP_EXIT
} cap_shape;

typedef struct {
    const char *family;
    const char *member;
    const char *sym;
    const char *args;
    cap_shape shape;
} cap_row;

static const cap_row cap_rows[] = {
    {"File", "read", "ek_file_read", "vv", CAP_IO_REF},
    {"File", "read_bytes", "ek_file_read_bytes", "vv", CAP_IO_REF},
    {"File", "read_line", "ek_file_read_line", "v", CAP_IO_OPT},
    {"File", "seek", "ek_file_seek", "vv", CAP_IO_UNIT},
    {"File", "write", "ek_file_write", "vo", CAP_IO_UNIT},
    {"File", "write_bytes", "ek_file_write_bytes", "vo", CAP_IO_UNIT},
    {"File", "flush", "ek_file_flush", "v", CAP_IO_UNIT},
    {"File", "close", "ek_file_close", "v", CAP_IO_UNIT},
    {"In", "from_lines", "ek_in_from_lines", "o", CAP_PLAIN},
    {"Rand", "with_seed", "ek_rand_with_seed", "v", CAP_PLAIN},
    {"Out", "err", "ek_cap_out_stderr", "", CAP_PLAIN},
    {"Proc", "exit", "ek_proc_exit", "vv", CAP_EXIT},
};

/* The runtime's value builders join the unit with the advancing
   derivation: the closure constructs over the shared identities. */
static int registers_families(const cap_row *row)
{
    return strcmp(row->member, "from_lines") == 0;
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

lower_val lower_cap_call(lower_ctx *c, lower_scope *scope,
                         const ek_node *e, const ek_node *callee,
                         const char *family, int has_recv)
{
    const cap_row *row = NULL;
    const char **as;
    size_t cap = 0, na, written, i;
    lower_val out;

    for (i = 0; i < sizeof cap_rows / sizeof *cap_rows; i++) {
        if (strcmp(cap_rows[i].family, family) == 0
            && strcmp(cap_rows[i].member, callee->u.field.name) == 0) {
            row = &cap_rows[i];
        }
    }
    if (!row) {
        return (lower_val){ NULL, 0 };
    }
    na = strlen(row->args);
    written = na - (size_t)has_recv;
    if (e->u.call.args.count != written) {
        lower_error(c, e, "the call of `%s.%s` is malformed", family,
                    callee->u.field.name);
        return lower_val_unit();
    }
    if (registers_families(row)) {
        lower_cap_families(c, e);
    }
    as = lower_grow(c, NULL, &cap, na ? na : 1, sizeof *as);
    if (!as) {
        return lower_val_unit();
    }
    if (has_recv) {
        lower_val a = lower_sub(c, scope, callee->u.field.recv);

        as[0] = lower_as_temp(c, scope, &a);
    }
    for (i = 0; i < written; i++) {
        lower_val a = lower_sub(c, scope, e->u.call.args.data[i]);

        as[has_recv + i] = lower_as_temp(c, scope, &a);
    }
    {
        const char *args = value_args(c, row->args, as, na);

        switch (row->shape) {
        case CAP_PLAIN: {
            const char *t = lower_temp(c, scope);

            lower_pad(c);
            lower_emitf(c, "ek_val %s = %s(%s);\n", t, row->sym, args);
            out = (lower_val){ t, 1 };
            break;
        }
        case CAP_IO_REF:
            out = lower_wrap_io(c, scope, e, row->sym, args,
                                LOWER_IO_OK_REF);
            break;
        case CAP_IO_UNIT:
            out = lower_wrap_io(c, scope, e, row->sym, args,
                                LOWER_IO_OK_UNIT);
            break;
        case CAP_IO_OPT:
            out = lower_wrap_io(c, scope, e, row->sym, args,
                                LOWER_IO_OK_OPT);
            break;
        default:
            out = lower_wrap_exit(c, scope, row->sym, args);
            break;
        }
    }
    for (i = na; i-- > 0;) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", as[i]);
    }
    return out;
}
