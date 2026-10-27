#include <string.h>

#include "check/internal.h"

/* The compiler-provided members of the std namespaces the surface
   does not declare yet: the container and scalar namespaces whose
   signatures Annex C fixes. A declared namespace member wins over a
   row, and the derived services win over both, so a row is the
   default of a name no declaration owns. A namespace the table owns
   is complete: a name no station answers is absent, not bridged. */

/* A row signature is a type tree: a fresh variable slot, a std
   nominal, a list, an Option or Result application, a function, or
   the unit type. Slots are per-consultation: every use of a row
   instantiates its own variables. */
typedef struct ns_sig ns_sig;

struct ns_sig {
    int kind;
    const char *std;       /* NS_STD: the type's name */
    int slot;              /* NS_VAR: the variable's slot */
    const ns_sig **params; /* NS_FN */
    size_t nparams;
    const ns_sig *result;
    int arrow;             /* NS_FN: the arrow form */
    const ns_sig *arg;     /* NS_OPTION, NS_RESULT, NS_LIST */
};

enum { NS_VAR, NS_STD, NS_FN, NS_UNIT, NS_LIST, NS_OPTION, NS_RESULT };

typedef struct {
    const char *ns;
    const char *name;
    const ns_sig *sig;
} ns_row;

#define SIG_STD(n)  &(ns_sig){.kind = NS_STD, .std = (n)}
#define SIG_VAR(i)  &(ns_sig){.kind = NS_VAR, .slot = (i)}
#define SIG_UNIT    &(ns_sig){.kind = NS_UNIT}
#define SIG_LIST(a) &(ns_sig){.kind = NS_LIST, .arg = (a)}
#define SIG_OPTION(a) &(ns_sig){.kind = NS_OPTION, .arg = (a)}
#define SIG_RESULT(a) &(ns_sig){.kind = NS_RESULT, .arg = (a)}

/* (params...) arrow result */
#define SIG_FN(ar, res, ...)                                          \
    &(ns_sig){.kind = NS_FN, .params = (const ns_sig *[]){__VA_ARGS__}, \
              .nparams = sizeof((const ns_sig *[]){__VA_ARGS__})       \
                          / sizeof(const ns_sig *),                    \
              .result = (res), .arrow = (ar)}

static const ek_type *build(ck_ctx *c, const ns_sig *s, const ek_type **slots,
                            size_t nslots);

static const ek_type *build_fn(ck_ctx *c, const ns_sig *s,
                               const ek_type **slots, size_t nslots)
{
    const ek_type **ps = ek_arena_alloc(c->arena, s->nparams * sizeof *ps);
    const ek_type *r;
    size_t i;

    if (!ps) {
        c->oom = 1;
        return ck_ivar(c);
    }
    for (i = 0; i < s->nparams; i++) {
        ps[i] = build(c, s->params[i], slots, nslots);
    }
    r = build(c, s->result, slots, nslots);
    return ck_fn(c, ps, s->nparams, r, s->arrow);
}

static const ek_type *build(ck_ctx *c, const ns_sig *s, const ek_type **slots,
                            size_t nslots)
{
    const ek_symbol *sym;

    switch (s->kind) {
    case NS_VAR:
        return s->slot < (int)nslots && slots[s->slot]
                   ? slots[s->slot]
                   : (slots[s->slot] = ck_ivar(c));
    case NS_STD:
        return ck_std_type(c, s->std);
    case NS_UNIT:
        return ck_unit(c);
    case NS_FN:
        return build_fn(c, s, slots, nslots);
    case NS_LIST:
        return ck_list(c, build(c, s->arg, slots, nslots));
    case NS_OPTION:
        sym = ck_std_symbol(c, "Option");
        {
            const ek_type **args = ek_arena_alloc(c->arena, sizeof *args);
            const ek_type *a = build(c, s->arg, slots, nslots);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = a;
            return sym ? ck_nominal(c, sym, args, 1) : ck_ivar(c);
        }
    case NS_RESULT:
        sym = ck_std_symbol(c, "Result");
        {
            const ek_symbol *perr = ck_std_symbol(c, "ParseError");
            const ek_type **args = ek_arena_alloc(c->arena, 2 * sizeof *args);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = build(c, s->arg, slots, nslots);
            args[1] = perr ? ck_nominal(c, perr, NULL, 0) : ck_ivar(c);
            return sym ? ck_nominal(c, sym, args, 2) : ck_ivar(c);
        }
    default:
        return ck_ivar(c);
    }
}

/* The namespace a receiver or head type carries: the structural list
   is List; a nominal is named by its std symbol. NULL names a
   namespace the table does not own. */
static const char *namespace_of(const ek_type *t)
{
    if (t->kind == EK_T_LIST) {
        return "List";
    }
    if (t->kind == EK_T_NOMINAL && t->u.nominal.sym
        && t->u.nominal.sym->module
        && strcmp(t->u.nominal.sym->module, "std") == 0) {
        return t->u.nominal.sym->name;
    }
    return NULL;
}

/* The rows of Annex C.3: the List namespace over the structural list.
   The open arrows are the dispatchers of Clause 7.9.6. */
static const ns_row rows[] = {
    {"List", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"), SIG_LIST(SIG_VAR(0)))},
    {"List", "get", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_VAR(0)),
                           SIG_LIST(SIG_VAR(0)), SIG_STD("Int"))},
    {"List", "reverse", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                               SIG_LIST(SIG_VAR(0)))},
    {"List", "append", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                              SIG_LIST(SIG_VAR(0)), SIG_LIST(SIG_VAR(0)))},
    {"List", "map", SIG_FN(EK_ARROW_OPEN, SIG_LIST(SIG_VAR(1)),
                           SIG_LIST(SIG_VAR(0)),
                           SIG_FN(EK_ARROW_OPEN, SIG_VAR(1), SIG_VAR(0)))},
    {"List", "filter", SIG_FN(EK_ARROW_OPEN, SIG_LIST(SIG_VAR(0)),
                              SIG_LIST(SIG_VAR(0)),
                              SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"), SIG_VAR(0)))},
    {"List", "fold", SIG_FN(EK_ARROW_OPEN, SIG_VAR(1),
                            SIG_LIST(SIG_VAR(0)), SIG_VAR(1),
                            SIG_FN(EK_ARROW_OPEN, SIG_VAR(1), SIG_VAR(1), SIG_VAR(0)))},
    {"List", "each", SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                            SIG_LIST(SIG_VAR(0)),
                            SIG_FN(EK_ARROW_OPEN, SIG_UNIT, SIG_VAR(0)))},
    {"List", "each_with_index", SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                                       SIG_LIST(SIG_VAR(0)),
                                       SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                                              SIG_VAR(0), SIG_STD("Int")))},
    {"List", "any", SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"),
                           SIG_LIST(SIG_VAR(0)),
                           SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"), SIG_VAR(0)))},
    {"List", "all", SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"),
                           SIG_LIST(SIG_VAR(0)),
                           SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"), SIG_VAR(0)))},
    {"List", "find_by", SIG_FN(EK_ARROW_OPEN, SIG_OPTION(SIG_VAR(0)),
                               SIG_LIST(SIG_VAR(0)),
                               SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"), SIG_VAR(0)))},
    {"List", "sort", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                            SIG_LIST(SIG_VAR(0)))},
    {"List", "sort_by", SIG_FN(EK_ARROW_OPEN, SIG_LIST(SIG_VAR(0)),
                               SIG_LIST(SIG_VAR(0)),
                               SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"),
                                      SIG_VAR(0), SIG_VAR(0)))},
    {"List", "zip_with", SIG_FN(EK_ARROW_OPEN, SIG_LIST(SIG_VAR(2)),
                                SIG_LIST(SIG_VAR(0)), SIG_LIST(SIG_VAR(1)),
                                SIG_FN(EK_ARROW_OPEN, SIG_VAR(2),
                                       SIG_VAR(0), SIG_VAR(1)))},
    {"List", "take", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                            SIG_LIST(SIG_VAR(0)), SIG_STD("Int"))},
    {"List", "drop", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                            SIG_LIST(SIG_VAR(0)), SIG_STD("Int"))},
    {"List", "take_while", SIG_FN(EK_ARROW_OPEN, SIG_LIST(SIG_VAR(0)),
                                  SIG_LIST(SIG_VAR(0)),
                                  SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"), SIG_VAR(0)))},
    {"List", "drop_while", SIG_FN(EK_ARROW_OPEN, SIG_LIST(SIG_VAR(0)),
                                  SIG_LIST(SIG_VAR(0)),
                                  SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"), SIG_VAR(0)))},
    {"List", "join", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                            SIG_LIST(SIG_VAR(0)), SIG_STD("String"))},
    {"List", "slice", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_LIST(SIG_VAR(0))),
                             SIG_LIST(SIG_VAR(0)), SIG_STD("Int"),
                             SIG_STD("Int"))},
};

/* The one row a name carries over a namespace the table owns, with
   the row's variables instantiated fresh for this consultation. */
const ek_type *ck_ns_row(ck_ctx *c, const ek_type *recv, const char *name,
                         int *owned)
{
    const char *ns = namespace_of(recv);
    const ek_type *slots[4] = {0, 0, 0, 0};
    size_t i;

    *owned = 0;
    if (!ns) {
        return NULL;
    }
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        if (strcmp(ns, rows[i].ns) == 0) {
            if (strcmp(rows[i].name, name) == 0) {
                return build(c, rows[i].sig, slots, 4);
            }
            *owned = 1;
        }
    }
    return NULL;
}

/* True when the table carries rows over the receiver's namespace: the
   namespace is complete, so a name no station answered is absent. */
int ck_ns_owned(const ek_type *recv)
{
    const char *ns = namespace_of(recv);
    size_t i;

    if (!ns) {
        return 0;
    }
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        if (strcmp(ns, rows[i].ns) == 0) {
            return 1;
        }
    }
    return 0;
}
