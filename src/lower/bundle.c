#include <string.h>

#include "lower/internal.h"

/* The bundle layout contract: the capability module's bundle records
   are built by the runtime over the slot order its header publishes,
   one enumeration per capability, and read by the unit through its own
   generated struct. The unit checks the two agree at compile time: one
   constant-expression array type per field, negative when the field's
   offset is not its slot's, and one for the record's size against the
   slot count. A field added, removed, or reordered on either side then
   stops the C build instead of misreading a closure. */

/* One bundle record: the type's name in the capability module and the
   enumeration prefix the runtime publishes for its slots. */
typedef struct {
    const char *type;
    const char *prefix;
} bundle_row;

static const bundle_row bundles[] = {
    {"_OutB", "EK_OUT"},   {"_InB", "EK_IN"},     {"_FsB", "EK_FS"},
    {"_EnvB", "EK_ENV"},   {"_ArgsB", "EK_ARGS"}, {"_ClockB", "EK_CLOCK"},
    {"_RandB", "EK_RAND"},
};

/* The row of one capability-module type name, or NULL. */
static const bundle_row *bundle_of(const lower_gtype *g)
{
    size_t i;

    if (!g->key_sym || !g->key_sym->module
        || strcmp(g->key_sym->module, "cap") != 0) {
        return NULL;
    }
    for (i = 0; i < sizeof bundles / sizeof *bundles; i++) {
        if (strcmp(g->key_sym->name, bundles[i].type) == 0) {
            return &bundles[i];
        }
    }
    return NULL;
}

/* The slot enumerator of one field: the prefix, the field's own
   spelling uppercased. */
static const char *slot_enum(lower_ctx *c, const bundle_row *row,
                             const char *field)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    if (!ek_buf_append_str(&b, row->prefix)
        || !ek_buf_append_char(&b, '_')) {
        c->oom = 1;
    }
    for (i = 0; field[i] && !c->oom; i++) {
        char ch = field[i] >= 'a' && field[i] <= 'z'
                      ? (char)(field[i] - 'a' + 'A')
                      : field[i];

        if (!ek_buf_append_char(&b, ch)) {
            c->oom = 1;
        }
    }
    return lower_take(c, &b);
}

/* Appends to the unit, recording exhaustion on the context. */
static int bprintf(lower_ctx *c, ek_buf *u, const char *fmt, ...)
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

/* The layout checks of one bundle record, after its typedef stands; a
   record the capability module does not name a bundle of checks
   nothing. */
void lower_bundle_checks(lower_ctx *c, ek_buf *u, const lower_gtype *g)
{
    const bundle_row *row = bundle_of(g);
    size_t i;

    if (!row) {
        return;
    }
    for (i = 0; i < g->nf; i++) {
        bprintf(c, u,
                "typedef char ekB_%s_%s[(offsetof(ekS_%s, %s) "
                "== sizeof(ek_obj) + %s * sizeof(ek_val)) ? 1 : -1];\n",
                g->stem, g->fnames[i], g->stem,
                lower_field_c_name(c, g, i), slot_enum(c, row, g->fnames[i]));
    }
    bprintf(c, u,
            "typedef char ekB_%s_slots[(sizeof(ekS_%s) == sizeof(ek_obj) "
            "+ %s_SLOTS * sizeof(ek_val)) ? 1 : -1];\n",
            g->stem, g->stem, row->prefix);
}
