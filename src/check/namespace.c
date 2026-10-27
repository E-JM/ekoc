#include <string.h>

#include "check/internal.h"

/* The compiler-provided members of the std namespaces the surface
   does not declare yet: the container, string, time, and scalar
   namespaces whose signatures Annex C fixes. A declared namespace member wins over a
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
    const ns_sig *arg;     /* NS_LIST, NS_OPTION, NS_RESULT, NS_APP*,
                              NS_DICT */
    const ns_sig *arg2;    /* NS_APP2, NS_DICT: the second argument */
};

enum { NS_VAR, NS_STD, NS_FN, NS_UNIT, NS_LIST, NS_OPTION, NS_RESULT,
       NS_APP1, NS_APP2, NS_DICT };

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

/* A std nominal applied to one type argument. */
#define SIG_APP1(n, a) &(ns_sig){.kind = NS_APP1, .std = (n), .arg = (a)}

/* A std nominal applied to two type arguments. */
#define SIG_APP2(n, a, b)                                             \
    &(ns_sig){.kind = NS_APP2, .std = (n), .arg = (a), .arg2 = (b)}

/* The structural dictionary [k]v. */
#define SIG_DICT(k, v)                                                \
    &(ns_sig){.kind = NS_DICT, .arg = (k), .arg2 = (v)}

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
    case NS_APP1:
        sym = ck_std_symbol(c, s->std);
        {
            const ek_type **args = ek_arena_alloc(c->arena, sizeof *args);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = build(c, s->arg, slots, nslots);
            return sym ? ck_nominal(c, sym, args, 1) : ck_ivar(c);
        }
    case NS_APP2:
        sym = ck_std_symbol(c, s->std);
        {
            const ek_type **args = ek_arena_alloc(c->arena, 2 * sizeof *args);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = build(c, s->arg, slots, nslots);
            args[1] = build(c, s->arg2, slots, nslots);
            return sym ? ck_nominal(c, sym, args, 2) : ck_ivar(c);
        }
    case NS_DICT:
        return ck_dict(c, build(c, s->arg, slots, nslots),
                       build(c, s->arg2, slots, nslots));
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
    if (t->kind == EK_T_DICT) {
        return "Dict";
    }
    if (t->kind == EK_T_NOMINAL && t->u.nominal.sym
        && t->u.nominal.sym->module
        && strcmp(t->u.nominal.sym->module, "std") == 0) {
        return t->u.nominal.sym->name;
    }
    return NULL;
}

/* The members the compiler supplies when no declaration owns the
   name. */
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

    /* The Dict namespace over the structural dictionary: the key and
       value variables stay paired across every member, and the
       callbacks take both. */
    {"Dict", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)))},
    {"Dict", "get", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_VAR(1)),
                           SIG_DICT(SIG_VAR(0), SIG_VAR(1)), SIG_VAR(0))},
    {"Dict", "insert", SIG_FN(EK_ARROW_PURE,
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)), SIG_VAR(0),
                              SIG_VAR(1))},
    {"Dict", "remove", SIG_FN(EK_ARROW_PURE,
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)), SIG_VAR(0))},
    {"Dict", "merge", SIG_FN(EK_ARROW_PURE,
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)))},
    {"Dict", "map", SIG_FN(EK_ARROW_OPEN,
                            SIG_DICT(SIG_VAR(0), SIG_VAR(2)),
                            SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                            SIG_FN(EK_ARROW_OPEN, SIG_VAR(2), SIG_VAR(0),
                                   SIG_VAR(1)))},
    {"Dict", "filter", SIG_FN(EK_ARROW_OPEN,
                               SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                               SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                               SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"),
                                      SIG_VAR(0), SIG_VAR(1)))},
    {"Dict", "fold", SIG_FN(EK_ARROW_OPEN, SIG_VAR(2),
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)), SIG_VAR(2),
                             SIG_FN(EK_ARROW_OPEN, SIG_VAR(2), SIG_VAR(2),
                                    SIG_VAR(0), SIG_VAR(1)))},
    {"Dict", "each", SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                             SIG_FN(EK_ARROW_OPEN, SIG_UNIT, SIG_VAR(0),
                                    SIG_VAR(1)))},

    /* The Set namespace: the element variable of the receiver ties
       the argument and the result. */
    {"Set", "of", SIG_FN(EK_ARROW_PURE, SIG_APP1("Set", SIG_VAR(0)),
                          SIG_LIST(SIG_VAR(0)))},
    {"Set", "to_list", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                               SIG_APP1("Set", SIG_VAR(0)))},
    {"Set", "contains", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                               SIG_APP1("Set", SIG_VAR(0)), SIG_VAR(0))},
    {"Set", "insert", SIG_FN(EK_ARROW_PURE,
                              SIG_APP1("Set", SIG_VAR(0)),
                              SIG_APP1("Set", SIG_VAR(0)), SIG_VAR(0))},
    {"Set", "remove", SIG_FN(EK_ARROW_PURE,
                              SIG_APP1("Set", SIG_VAR(0)),
                              SIG_APP1("Set", SIG_VAR(0)), SIG_VAR(0))},
    {"Set", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_APP1("Set", SIG_VAR(0)))},
    {"Set", "union", SIG_FN(EK_ARROW_PURE, SIG_APP1("Set", SIG_VAR(0)),
                             SIG_APP1("Set", SIG_VAR(0)),
                             SIG_APP1("Set", SIG_VAR(0)))},
    {"Set", "intersect", SIG_FN(EK_ARROW_PURE,
                                 SIG_APP1("Set", SIG_VAR(0)),
                                 SIG_APP1("Set", SIG_VAR(0)),
                                 SIG_APP1("Set", SIG_VAR(0)))},
    {"Set", "diff", SIG_FN(EK_ARROW_PURE, SIG_APP1("Set", SIG_VAR(0)),
                            SIG_APP1("Set", SIG_VAR(0)),
                            SIG_APP1("Set", SIG_VAR(0)))},

    /* The String namespace: the scalar sequence, its split and trim
       families, and the byte bridge. */
    {"String", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                SIG_STD("String"))},
    {"String", "at", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Char")),
                            SIG_STD("String"), SIG_STD("Int"))},
    {"String", "chars", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_STD("Char")),
                               SIG_STD("String"))},
    {"String", "from_char", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                   SIG_STD("Char"))},
    {"String", "from_chars", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                    SIG_LIST(SIG_STD("Char")))},
    {"String", "append", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                SIG_STD("String"), SIG_STD("String"))},
    {"String", "lines", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_STD("String")),
                               SIG_STD("String"))},
    {"String", "split", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_STD("String")),
                               SIG_STD("String"), SIG_STD("String"))},
    {"String", "contains", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                  SIG_STD("String"), SIG_STD("String"))},
    {"String", "starts_with", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                     SIG_STD("String"), SIG_STD("String"))},
    {"String", "ends_with", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                   SIG_STD("String"), SIG_STD("String"))},
    {"String", "replace", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("String"), SIG_STD("String"),
                                 SIG_STD("String"))},
    {"String", "replace_first", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                       SIG_STD("String"), SIG_STD("String"),
                                       SIG_STD("String"))},
    {"String", "slice", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("String")),
                               SIG_STD("String"), SIG_STD("Int"),
                               SIG_STD("Int"))},
    {"String", "trim", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                              SIG_STD("String"))},
    {"String", "trim_start", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                    SIG_STD("String"))},
    {"String", "trim_end", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                  SIG_STD("String"))},
    {"String", "to_bytes", SIG_FN(EK_ARROW_PURE, SIG_STD("Bytes"),
                                  SIG_STD("String"))},
    {"String", "pad_left", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                  SIG_STD("String"), SIG_STD("UInt"),
                                  SIG_STD("Char"))},
    {"String", "pad_right", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                   SIG_STD("String"), SIG_STD("UInt"),
                                   SIG_STD("Char"))},

    /* The Char namespace: the code-point bridge in both directions. */
    {"Char", "to_code", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("Char"))},
    {"Char", "from_code", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Char")),
                                 SIG_STD("Int"))},

    /* The Bytes namespace: the byte sequence, its integer and word
       bridges, and the hexadecimal and text decoders. */
    {"Bytes", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("Bytes"))},
    {"Bytes", "get", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Int")),
                            SIG_STD("Bytes"), SIG_STD("Int"))},
    {"Bytes", "append", SIG_FN(EK_ARROW_PURE, SIG_STD("Bytes"),
                               SIG_STD("Bytes"), SIG_STD("Bytes"))},
    {"Bytes", "to_ints", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_STD("Int")),
                                SIG_STD("Bytes"))},
    {"Bytes", "from_ints", SIG_FN(EK_ARROW_PURE,
                                  SIG_OPTION(SIG_STD("Bytes")),
                                  SIG_LIST(SIG_STD("Int")))},
    {"Bytes", "to_u8s", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_STD("U8")),
                               SIG_STD("Bytes"))},
    {"Bytes", "from_u8s", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Bytes")),
                                 SIG_LIST(SIG_STD("U8")))},
    {"Bytes", "to_hex", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                               SIG_STD("Bytes"))},
    {"Bytes", "from_hex", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Bytes")),
                                 SIG_STD("String"))},
    {"Bytes", "to_string", SIG_FN(EK_ARROW_PURE,
                                  SIG_RESULT(SIG_STD("String")),
                                  SIG_STD("Bytes"))},

    /* The Option namespace: the payload variable runs through the
       callback and the result; the nested-access helper narrows an
       Option of a list. */
    {"Option", "map", SIG_FN(EK_ARROW_OPEN, SIG_OPTION(SIG_VAR(1)),
                             SIG_OPTION(SIG_VAR(0)),
                             SIG_FN(EK_ARROW_OPEN, SIG_VAR(1), SIG_VAR(0)))},
    {"Option", "and_then", SIG_FN(EK_ARROW_OPEN, SIG_OPTION(SIG_VAR(1)),
                                  SIG_OPTION(SIG_VAR(0)),
                                  SIG_FN(EK_ARROW_OPEN,
                                         SIG_OPTION(SIG_VAR(1)), SIG_VAR(0)))},
    {"Option", "or_else", SIG_FN(EK_ARROW_OPEN, SIG_OPTION(SIG_VAR(0)),
                                 SIG_OPTION(SIG_VAR(0)),
                                 SIG_FN0(EK_ARROW_OPEN,
                                         SIG_OPTION(SIG_VAR(0))))},
    {"Option", "filter", SIG_FN(EK_ARROW_OPEN, SIG_OPTION(SIG_VAR(0)),
                                SIG_OPTION(SIG_VAR(0)),
                                SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"),
                                       SIG_VAR(0)))},
    {"Option", "is_some", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                 SIG_OPTION(SIG_VAR(0)))},
    {"Option", "is_none", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                 SIG_OPTION(SIG_VAR(0)))},
    {"Option", "unwrap", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_OPTION(SIG_VAR(0)))},
    {"Option", "unwrap_or", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                   SIG_OPTION(SIG_VAR(0)), SIG_VAR(0))},
    {"Option", "expect", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_OPTION(SIG_VAR(0)), SIG_STD("String"))},
    {"Option", "get", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_VAR(0)),
                             SIG_OPTION(SIG_LIST(SIG_VAR(0))),
                             SIG_STD("Int"))},
    {"Option", "to_result", SIG_FN(EK_ARROW_PURE,
                                   SIG_APP2("Result", SIG_VAR(0), SIG_VAR(1)),
                                   SIG_OPTION(SIG_VAR(0)), SIG_VAR(1))},

    /* The Result namespace: the payload and error variables run
       through the combinators, and traverse keeps the error while it
       lifts a list of results. */
    {"Result", "try", SIG_FN(EK_ARROW_OPEN,
                             SIG_APP2("Result", SIG_VAR(2), SIG_VAR(1)),
                             SIG_APP2("Result", SIG_VAR(0), SIG_VAR(1)),
                             SIG_FN(EK_ARROW_OPEN,
                                    SIG_APP2("Result", SIG_VAR(2),
                                             SIG_VAR(1)),
                                    SIG_VAR(0)))},
    {"Result", "map", SIG_FN(EK_ARROW_OPEN,
                             SIG_APP2("Result", SIG_VAR(2), SIG_VAR(1)),
                             SIG_APP2("Result", SIG_VAR(0), SIG_VAR(1)),
                             SIG_FN(EK_ARROW_OPEN, SIG_VAR(2), SIG_VAR(0)))},
    {"Result", "map_err", SIG_FN(EK_ARROW_OPEN,
                                 SIG_APP2("Result", SIG_VAR(0),
                                          SIG_VAR(2)),
                                 SIG_APP2("Result", SIG_VAR(0),
                                          SIG_VAR(1)),
                                 SIG_FN(EK_ARROW_OPEN, SIG_VAR(2),
                                        SIG_VAR(1)))},
    {"Result", "traverse", SIG_FN(EK_ARROW_OPEN,
                                  SIG_APP2("Result",
                                           SIG_LIST(SIG_VAR(2)),
                                           SIG_VAR(1)),
                                  SIG_LIST(SIG_VAR(0)),
                                  SIG_FN(EK_ARROW_OPEN,
                                         SIG_APP2("Result", SIG_VAR(2),
                                                  SIG_VAR(1)),
                                         SIG_VAR(0)))},
    {"Result", "is_ok", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                               SIG_APP2("Result", SIG_VAR(0),
                                        SIG_VAR(1)))},
    {"Result", "is_err", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                SIG_APP2("Result", SIG_VAR(0),
                                         SIG_VAR(1)))},
    {"Result", "unwrap", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_APP2("Result", SIG_VAR(0),
                                         SIG_VAR(1)))},
    {"Result", "unwrap_or", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                   SIG_APP2("Result", SIG_VAR(0),
                                            SIG_VAR(1)), SIG_VAR(0))},
    {"Result", "expect", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_APP2("Result", SIG_VAR(0),
                                         SIG_VAR(1)), SIG_STD("String"))},

    /* The Range namespace over the range ADT: the endpoint variable
       ties the receiver to the element of the produced list. */
    {"Range", "to_list", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                                SIG_APP1("Range", SIG_VAR(0)))},
    {"Range", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_APP1("Range", SIG_VAR(0)))},
    {"Range", "each", SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                             SIG_APP1("Range", SIG_VAR(0)),
                             SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                                    SIG_VAR(0)))},
    {"Range", "each_with_index", SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                                        SIG_APP1("Range", SIG_VAR(0)),
                                        SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                                               SIG_VAR(0), SIG_STD("Int")))},
    {"Range", "map", SIG_FN(EK_ARROW_OPEN, SIG_LIST(SIG_VAR(1)),
                            SIG_APP1("Range", SIG_VAR(0)),
                            SIG_FN(EK_ARROW_OPEN, SIG_VAR(1),
                                   SIG_VAR(0)))},
    {"Range", "fold", SIG_FN(EK_ARROW_OPEN, SIG_VAR(1),
                             SIG_APP1("Range", SIG_VAR(0)), SIG_VAR(1),
                             SIG_FN(EK_ARROW_OPEN, SIG_VAR(1), SIG_VAR(1),
                                    SIG_VAR(0)))},

    /* The wall-clock namespace: the duration arithmetic and the
       nanosecond payload, with the parser of the printed form. */
    {"Time", "sub", SIG_FN(EK_ARROW_PURE, SIG_STD("Duration"),
                            SIG_STD("Time"), SIG_STD("Time"))},
    {"Time", "add", SIG_FN(EK_ARROW_PURE, SIG_STD("Time"),
                            SIG_STD("Time"), SIG_STD("Duration"))},
    {"Time", "to_ns", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_STD("Time"))},
    {"Time", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Time")),
                              SIG_STD("String"))},

    /* The monotonic namespace: the same arithmetic over a reading
       with no printed form. */
    {"Mono", "sub", SIG_FN(EK_ARROW_PURE, SIG_STD("Duration"),
                            SIG_STD("Mono"), SIG_STD("Mono"))},
    {"Mono", "add", SIG_FN(EK_ARROW_PURE, SIG_STD("Mono"),
                            SIG_STD("Mono"), SIG_STD("Duration"))},
    {"Mono", "to_ns", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_STD("Mono"))},

    /* The duration namespace: construction from a count, the span
       arithmetic, the payload, and the parser. */
    {"Duration", "add", SIG_FN(EK_ARROW_PURE, SIG_STD("Duration"),
                                SIG_STD("Duration"), SIG_STD("Duration"))},
    {"Duration", "mul", SIG_FN(EK_ARROW_PURE, SIG_STD("Duration"),
                                SIG_STD("Duration"), SIG_STD("Int"))},
    {"Duration", "neg", SIG_FN(EK_ARROW_PURE, SIG_STD("Duration"),
                                SIG_STD("Duration"))},
    {"Duration", "secs", SIG_FN(EK_ARROW_PURE, SIG_STD("Duration"),
                                 SIG_STD("Int"))},
    {"Duration", "millis", SIG_FN(EK_ARROW_PURE, SIG_STD("Duration"),
                                   SIG_STD("Int"))},
    {"Duration", "to_ns", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                  SIG_STD("Duration"))},
    {"Duration", "parse", SIG_FN(EK_ARROW_PURE,
                                  SIG_RESULT(SIG_STD("Duration")),
                                  SIG_STD("String"))},

    /* The interchange namespace: parsing only; show and its JSON
       mapping are derived services over the seven variants. */
    {"Json", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Json")),
                              SIG_STD("String"))},

    /* The path namespace over String: lexical path vocabulary, with
       no file-system contact. */
    {"Path", "join", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                             SIG_STD("String"), SIG_STD("String"))},
    {"Path", "normalize", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                  SIG_STD("String"))},
    {"Path", "is_absolute", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                    SIG_STD("String"))},

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

    /* The machine words ferry, compare, show, and pad: the padding
       row is the only namespace member they carry; eq, the ordering
       family, and show derive. */
    {"I8", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                SIG_STD("I8"), SIG_STD("UInt"))},
    {"I16", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("I16"), SIG_STD("UInt"))},
    {"I32", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("I32"), SIG_STD("UInt"))},
    {"I64", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("I64"), SIG_STD("UInt"))},
    {"U8", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                SIG_STD("U8"), SIG_STD("UInt"))},
    {"U16", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("U16"), SIG_STD("UInt"))},
    {"U32", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("U32"), SIG_STD("UInt"))},
    {"U64", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("U64"), SIG_STD("UInt"))},
    {"F32", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("F32"), SIG_STD("UInt"))},
    {"F64", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("F64"), SIG_STD("UInt"))},

    /* A NULL name marks a namespace whose row set is complete: a
       name no station answers is absent. A partial namespace keeps
       the bridge for the names its rows do not carry yet. */
    {"Bytes", NULL, NULL},
    {"Char", NULL, NULL},
    {"Dict", NULL, NULL},
    {"Duration", NULL, NULL},
    {"F32", NULL, NULL},
    {"F64", NULL, NULL},
    {"Float", NULL, NULL},
    {"I16", NULL, NULL},
    {"I32", NULL, NULL},
    {"I64", NULL, NULL},
    {"I8", NULL, NULL},
    {"Int", NULL, NULL},
    {"Json", NULL, NULL},
    {"List", NULL, NULL},
    {"Mono", NULL, NULL},
    {"Option", NULL, NULL},
    {"Out", NULL, NULL},
    {"Path", NULL, NULL},
    {"Range", NULL, NULL},
    {"Result", NULL, NULL},
    {"Set", NULL, NULL},
    {"String", NULL, NULL},
    {"Time", NULL, NULL},
    {"U16", NULL, NULL},
    {"U32", NULL, NULL},
    {"U64", NULL, NULL},
    {"U8", NULL, NULL},
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
