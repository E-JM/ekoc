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
    const char *name; /* NULL marks the namespace's row set complete */
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

/* () arrow result */
#define SIG_FN0(ar, res)                                              \
    &(ns_sig){.kind = NS_FN, .params = NULL, .nparams = 0,            \
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

/* The scalar namespaces: the members the compiler supplies when no
   declaration owns the name. */
static const ns_row rows[] = {
    {"Float", "add", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "div", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "mul", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "neg", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "rem", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "sub", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},

    /* Conversions across domains, the parse family, and formatting. */
    {"Float", "to_int", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Int")),
                              SIG_STD("Float"))},
    {"Float", "to_uint", SIG_FN(EK_ARROW_PURE,
                               SIG_OPTION(SIG_STD("UInt")),
                               SIG_STD("Float"))},
    {"Float", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Float")),
                             SIG_STD("String"))},
    {"Float", "to_fixed", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                SIG_STD("Float"), SIG_STD("UInt"))},
    {"Float", "to_exp", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                              SIG_STD("Float"), SIG_STD("UInt"))},

    /* The floating-point mathematics surface. */
    {"Float", "abs", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "floor", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"))},
    {"Float", "ceil", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "trunc", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"))},
    {"Float", "round", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"))},
    {"Float", "pow", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "sqrt", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "exp", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "log", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "log2", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "log10", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"))},
    {"Float", "sin", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "cos", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "tan", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "asin", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "acos", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "atan", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "atan2", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "is_nan", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                              SIG_STD("Float"))},
    {"Float", "is_inf", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                              SIG_STD("Float"))},
    {"Float", "is_finite", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                 SIG_STD("Float"))},

    /* The machine-word sizes: narrowing is checked and the float
       widths are total. */
    {"Float", "to_i8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I8")),
                             SIG_STD("Float"))},
    {"Float", "to_i16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I16")),
                              SIG_STD("Float"))},
    {"Float", "to_i32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I32")),
                              SIG_STD("Float"))},
    {"Float", "to_i64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I64")),
                              SIG_STD("Float"))},
    {"Float", "to_u8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U8")),
                             SIG_STD("Float"))},
    {"Float", "to_u16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U16")),
                              SIG_STD("Float"))},
    {"Float", "to_u32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U32")),
                              SIG_STD("Float"))},
    {"Float", "to_u64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U64")),
                              SIG_STD("Float"))},
    {"Float", "to_f32", SIG_FN(EK_ARROW_PURE, SIG_STD("F32"),
                              SIG_STD("Float"))},
    {"Float", "to_f64", SIG_FN(EK_ARROW_PURE, SIG_STD("F64"),
                              SIG_STD("Float"))},
    {"Float", "from_f32", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                                SIG_STD("F32"))},
    {"Float", "from_f64", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                                SIG_STD("F64"))},
    {"Int", "abs", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"))},
    {"Int", "add", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "div", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "max", SIG_STD("Int")},
    {"Int", "min", SIG_STD("Int")},
    {"Int", "mod", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "mul", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "neg", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"))},
    {"Int", "pow", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("UInt"))},
    {"Int", "quot", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                           SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "rem", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "sub", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},

    /* Conversions across domains, the parse family, and formatting. */
    {"Int", "to_uint", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("UInt")),
                             SIG_STD("Int"))},
    {"Int", "to_float", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                              SIG_STD("Int"))},
    {"Int", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Int")),
                           SIG_STD("String"))},
    {"Int", "to_hex", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                            SIG_STD("Int"))},
    {"Int", "to_oct", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                            SIG_STD("Int"))},
    {"Int", "to_bin", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                            SIG_STD("Int"))},
    {"Int", "parse_hex", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Int")),
                               SIG_STD("String"))},
    {"Int", "parse_oct", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Int")),
                               SIG_STD("String"))},
    {"Int", "parse_bin", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Int")),
                               SIG_STD("String"))},

    /* Wrapping arithmetic on the machine ring. */
    {"Int", "wrapping_add", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                  SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "wrapping_sub", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                  SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "wrapping_mul", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                  SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "wrapping_neg", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                  SIG_STD("Int"))},

    /* The machine-word sizes, total in both directions. */
    {"Int", "to_i8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I8")),
                            SIG_STD("Int"))},
    {"Int", "to_i16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I16")),
                             SIG_STD("Int"))},
    {"Int", "to_i32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I32")),
                             SIG_STD("Int"))},
    {"Int", "to_i64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I64")),
                             SIG_STD("Int"))},
    {"Int", "to_u8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U8")),
                            SIG_STD("Int"))},
    {"Int", "to_u16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U16")),
                             SIG_STD("Int"))},
    {"Int", "to_u32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U32")),
                             SIG_STD("Int"))},
    {"Int", "to_u64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U64")),
                             SIG_STD("Int"))},
    {"Int", "to_f32", SIG_FN(EK_ARROW_PURE, SIG_STD("F32"),
                             SIG_STD("Int"))},
    {"Int", "to_f64", SIG_FN(EK_ARROW_PURE, SIG_STD("F64"),
                             SIG_STD("Int"))},
    {"Int", "from_i8", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_STD("I8"))},
    {"Int", "from_i16", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("I16"))},
    {"Int", "from_i32", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("I32"))},
    {"Int", "from_i64", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("I64"))},
    {"Int", "from_u8", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_STD("U8"))},
    {"Int", "from_u16", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("U16"))},
    {"Int", "from_u32", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("U32"))},
    {"Int", "from_u64", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("U64"))},

    /* The List namespace over the structural list. The open arrows are
       the dispatchers. */
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

    /* The Out capability: printing touches the world; null is pure,
       and tee is a pure derivation whose observer may touch the world. */
    {"Out", "null", SIG_FN0(EK_ARROW_PURE, SIG_STD("Out"))},
    {"Out", "print", SIG_FN(EK_ARROW_WORLD, SIG_UNIT,
                            SIG_STD("Out"), SIG_STD("String"))},
    {"Out", "println", SIG_FN(EK_ARROW_WORLD, SIG_UNIT,
                              SIG_STD("Out"), SIG_STD("String"))},
    {"Out", "tee", SIG_FN(EK_ARROW_PURE, SIG_STD("Out"),
                          SIG_STD("Out"),
                          SIG_FN(EK_ARROW_WORLD, SIG_UNIT,
                                 SIG_STD("String")))},

    /* The unsigned scalar namespace: arithmetic without negation. */
    {"UInt", "add", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "max", SIG_STD("UInt")},
    {"UInt", "mul", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "quot", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                            SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "rem", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "sub", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},

    /* Conversions across domains, the parse family, and formatting. */
    {"UInt", "to_int", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Int")),
                              SIG_STD("UInt"))},
    {"UInt", "to_float", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                               SIG_STD("UInt"))},
    {"UInt", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("UInt")),
                            SIG_STD("String"))},
    {"UInt", "to_hex", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                             SIG_STD("UInt"))},
    {"UInt", "to_oct", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                             SIG_STD("UInt"))},
    {"UInt", "to_bin", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                             SIG_STD("UInt"))},
    {"UInt", "parse_hex", SIG_FN(EK_ARROW_PURE,
                                SIG_RESULT(SIG_STD("UInt")),
                                SIG_STD("String"))},
    {"UInt", "parse_oct", SIG_FN(EK_ARROW_PURE,
                                SIG_RESULT(SIG_STD("UInt")),
                                SIG_STD("String"))},
    {"UInt", "parse_bin", SIG_FN(EK_ARROW_PURE,
                                SIG_RESULT(SIG_STD("UInt")),
                                SIG_STD("String"))},

    /* The bitwise family over the unsigned ring. */
    {"UInt", "band", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                            SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "bor", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "bxor", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                            SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "bnot", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                            SIG_STD("UInt"))},
    {"UInt", "shl", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "shr", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},

    /* Wrapping arithmetic on the machine ring. */
    {"UInt", "wrapping_add", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                   SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "wrapping_sub", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                   SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "wrapping_mul", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                   SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "wrapping_neg", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                   SIG_STD("UInt"))},

    /* The machine-word sizes: unsigned sources narrow to both signed
       and unsigned widths, and the float widths are total. */
    {"UInt", "to_i8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I8")),
                             SIG_STD("UInt"))},
    {"UInt", "to_i16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I16")),
                              SIG_STD("UInt"))},
    {"UInt", "to_i32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I32")),
                              SIG_STD("UInt"))},
    {"UInt", "to_i64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I64")),
                              SIG_STD("UInt"))},
    {"UInt", "to_u8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U8")),
                             SIG_STD("UInt"))},
    {"UInt", "to_u16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U16")),
                              SIG_STD("UInt"))},
    {"UInt", "to_u32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U32")),
                              SIG_STD("UInt"))},
    {"UInt", "to_u64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U64")),
                              SIG_STD("UInt"))},
    {"UInt", "to_f32", SIG_FN(EK_ARROW_PURE, SIG_STD("F32"),
                              SIG_STD("UInt"))},
    {"UInt", "to_f64", SIG_FN(EK_ARROW_PURE, SIG_STD("F64"),
                              SIG_STD("UInt"))},
    {"UInt", "from_u8", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                               SIG_STD("U8"))},
    {"UInt", "from_u16", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                SIG_STD("U16"))},
    {"UInt", "from_u32", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                SIG_STD("U32"))},
    {"UInt", "from_u64", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                SIG_STD("U64"))},

    /* A NULL name marks a namespace whose row set is complete: a
       name no station answers is absent. A partial namespace keeps
       the bridge for the names its rows do not carry yet. */
    {"Float", NULL, NULL},
    {"Int", NULL, NULL},
    {"List", NULL, NULL},
    {"Out", NULL, NULL},
    {"UInt", NULL, NULL},
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
        if (rows[i].name && strcmp(ns, rows[i].ns) == 0) {
            if (strcmp(rows[i].name, name) == 0) {
                return build(c, rows[i].sig, slots, 4);
            }
            *owned = 1;
        }
    }
    return NULL;
}

/* True when the receiver's namespace row set is complete: a name no
   station answered is absent, not bridged. */
int ck_ns_owned(const ek_type *recv)
{
    const char *ns = namespace_of(recv);
    size_t i;

    if (!ns) {
        return 0;
    }
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        if (rows[i].name == NULL && strcmp(ns, rows[i].ns) == 0) {
            return 1;
        }
    }
    return 0;
}
