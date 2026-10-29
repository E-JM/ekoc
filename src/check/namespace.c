#include <string.h>

#include "check/internal.h"

/* The compiler-provided members of the std namespaces: the intrinsic
   boundary of the shipped surface. A declared namespace member wins
   over a row, and the derived services win over both, so a row is the
   default of a name no declaration owns. A namespace the table owns
   is complete: a name no station answers is absent, not bridged. */

/* A row signature is a type tree: a fresh variable slot, a std or
   capability nominal, a list, an Option or Result application, a
   function, or the unit type. Slots are per-consultation: every use of
   a row instantiates its own variables. */
typedef struct ns_sig ns_sig;

struct ns_sig {
    int kind;
    const char *std;       /* NS_STD, NS_CAP: the type's name */
    int slot;              /* NS_VAR: the variable's slot */
    const ns_sig **params; /* NS_FN */
    size_t nparams;
    const ns_sig *result;
    int arrow;             /* NS_FN: the arrow form */
    const ns_sig *arg;     /* NS_LIST, NS_OPTION, NS_RESULT, NS_APP*,
                              NS_CAP1, NS_DICT */
    const ns_sig *arg2;    /* NS_APP2, NS_DICT: the second argument */
};

enum { NS_VAR, NS_STD, NS_CAP, NS_CAP1, NS_FN, NS_UNIT, NS_LIST,
       NS_OPTION, NS_RESULT, NS_APP1, NS_APP2, NS_DICT };

typedef struct {
    const char *ns;
    const char *name; /* NULL marks the namespace's row set complete */
    const ns_sig *sig;
} ns_row;

#define SIG_STD(n)  &(ns_sig){.kind = NS_STD, .std = (n)}
#define SIG_CAP(n)  &(ns_sig){.kind = NS_CAP, .std = (n)}
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

/* A capability nominal applied to one type argument: `Task b` of the
   concurrency surface. */
#define SIG_CAP1(n, a)                                                \
    &(ns_sig){.kind = NS_CAP1, .std = (n), .arg = (a)}

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

/* The capability module's type symbol of one name, or NULL: the
   private bundle type of the injected pair lives only there (Clause
   10.8). */
static const ek_symbol *cap_sym(ck_ctx *c, const char *name)
{
    size_t k, i;

    for (k = 0; k < c->in->module_count; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, "cap") != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *sym = &s->syms[i];

            if (sym->kind == EK_SYM_TYPE && strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

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
                   : (slots[s->slot] = ck_opaque(c));
    case NS_STD:
        if (strcmp(s->std, "Never") == 0) {
            return ck_never(c); /* the bottom type is no lookup */
        }
        return ck_std_type(c, s->std);
    case NS_CAP:
        sym = cap_sym(c, s->std);
        return sym ? ck_nominal(c, sym, NULL, 0) : ck_opaque(c);
    case NS_CAP1:
        sym = cap_sym(c, s->std);
        {
            const ek_type **args = ek_arena_alloc(c->arena, sizeof *args);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = build(c, s->arg, slots, nslots);
            return sym ? ck_nominal(c, sym, args, 1) : ck_opaque(c);
        }
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
            return sym ? ck_nominal(c, sym, args, 1) : ck_opaque(c);
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
            args[1] = perr ? ck_nominal(c, perr, NULL, 0) : ck_opaque(c);
            return sym ? ck_nominal(c, sym, args, 2) : ck_opaque(c);
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
            return sym ? ck_nominal(c, sym, args, 1) : ck_opaque(c);
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
            return sym ? ck_nominal(c, sym, args, 2) : ck_opaque(c);
        }
    case NS_DICT:
        return ck_dict(c, build(c, s->arg, slots, nslots),
                       build(c, s->arg2, slots, nslots));
    default:
        return ck_opaque(c);
    }
}

/* The namespace a receiver or head type carries: the structural list
   is List; a nominal is named by its std or capability symbol -- the
   capability placeholders stay std rows, and the handles are
   declarations of the capability module (Clause 9.1.1). NULL names a
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
        && (strcmp(t->u.nominal.sym->module, "std") == 0
            || strcmp(t->u.nominal.sym->module, "cap") == 0
            || strcmp(t->u.nominal.sym->module, "time") == 0)) {
        return t->u.nominal.sym->name;
    }
    return NULL;
}

/* The members the compiler supplies when no declaration owns the
   name. This table is the intrinsic boundary of the shipped surface:
   the scalar arithmetic, conversion, formatting, and parsing rows; the
   String scalar access and the Bytes operations; the Dict, Set, Range,
   and Path operations; the time parsers; Json.parse; List.append; the
   panic-coded unwrap/expect pair; and the compiler-provided capability
   rows. Every other inventory member is std, cap, or companion
   source. */
static const ns_row rows[] = {
    /* Bool.not is std source; the logical operators are sugar. */

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

    /* The machine-word sizes: the to_ family checks the range, and
       from_ covers only the machine domains the word includes. */
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

    /* The List namespace: only append is compiler-supplied, because a
       list has no source-level cons form; the std source declares the
       rest against the inductive patterns. */
    {"List", "append", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                              SIG_LIST(SIG_VAR(0)), SIG_LIST(SIG_VAR(0)))},

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

    /* The String namespace: the scalar sequence primitives. The
       composition (split, trim, replace, slice, padding) is std
       source over these. */
    {"String", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                SIG_STD("String"))},
    {"String", "chars", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_STD("Char")),
                               SIG_STD("String"))},
    {"String", "from_char", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                   SIG_STD("Char"))},
    {"String", "from_chars", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                    SIG_LIST(SIG_STD("Char")))},
    {"String", "append", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                SIG_STD("String"), SIG_STD("String"))},
    {"String", "to_bytes", SIG_FN(EK_ARROW_PURE, SIG_STD("Bytes"),
                                  SIG_STD("String"))},

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

    /* The Option namespace: the combinators are std source; the
       panicking observations carry the fixed P-codes and stay here. */
    {"Option", "unwrap", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_OPTION(SIG_VAR(0)))},
    {"Option", "expect", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_OPTION(SIG_VAR(0)), SIG_STD("String"))},

    /* The Result namespace: the combinators are std source; the
       panicking observations carry the fixed P-codes and stay here. */
    {"Result", "unwrap", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_APP2("Result", SIG_VAR(0),
                                         SIG_VAR(1)))},
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

    /* The wall-clock namespace: the printed form's parser alone; the
       arithmetic and the payload are companion source (std/time). */
    {"Time", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Time")),
                              SIG_STD("String"))},

    /* The monotonic namespace is companion source. */

    /* The duration namespace: the printed form's parser alone. */
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

    /* The capability surface the source does not declare (Clause 10.8):
       the two advancing derivations, the process exit, the four
       concurrency members, and the File family. `Never` is the bottom
       type, and the open arrows are the dispatchers. The
       capability-module source declares every other member (Clause
       9.1.1), so the namespaces are complete (Clause 7.9.1). The
       advancing derivations land on the std placeholders -- the same
       nominals the injected pairs wrap -- because the capability module
       declares no In or Rand type for the capability build to find. */
    {"In", "from_lines", SIG_FN(EK_ARROW_PURE, SIG_STD("In"),
                                SIG_LIST(SIG_STD("String")))},
    {"Rand", "with_seed", SIG_FN(EK_ARROW_PURE, SIG_STD("Rand"),
                                 SIG_STD("Int"))},
    {"Proc", "exit", SIG_FN(EK_ARROW_WORLD, SIG_STD("Never"),
                            SIG_STD("Proc"), SIG_STD("Int"))},
    {"Conc", "scope", SIG_FN(EK_ARROW_OPEN, SIG_VAR(0), SIG_STD("Conc"),
                             SIG_FN(EK_ARROW_OPEN, SIG_VAR(0),
                                    SIG_CAP("Scope")))},
    {"Scope", "spawn", SIG_FN(EK_ARROW_OPEN, SIG_CAP1("Task", SIG_VAR(0)),
                              SIG_CAP("Scope"),
                              SIG_FN0(EK_ARROW_OPEN, SIG_VAR(0)))},
    {"Task", "join", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_VAR(0)),
                            SIG_CAP1("Task", SIG_VAR(0)))},
    {"Task", "cancel", SIG_FN(EK_ARROW_PURE, SIG_UNIT,
                              SIG_CAP1("Task", SIG_VAR(0)))},
    {"File", "read", SIG_FN(EK_ARROW_WORLD,
                            SIG_APP2("Result", SIG_STD("String"),
                                     SIG_CAP("IoError")),
                            SIG_CAP("File"), SIG_STD("Int"))},
    {"File", "read_bytes", SIG_FN(EK_ARROW_WORLD,
                                  SIG_APP2("Result", SIG_STD("Bytes"),
                                           SIG_CAP("IoError")),
                                  SIG_CAP("File"), SIG_STD("Int"))},
    {"File", "read_line", SIG_FN(EK_ARROW_WORLD,
                                 SIG_APP2("Result",
                                          SIG_OPTION(SIG_STD("String")),
                                          SIG_CAP("IoError")),
                                 SIG_CAP("File"))},
    {"File", "seek", SIG_FN(EK_ARROW_WORLD,
                            SIG_APP2("Result", SIG_UNIT,
                                     SIG_CAP("IoError")),
                            SIG_CAP("File"), SIG_STD("UInt"))},
    {"File", "write", SIG_FN(EK_ARROW_WORLD,
                             SIG_APP2("Result", SIG_UNIT,
                                      SIG_CAP("IoError")),
                             SIG_CAP("File"), SIG_STD("String"))},
    {"File", "write_bytes", SIG_FN(EK_ARROW_WORLD,
                                   SIG_APP2("Result", SIG_UNIT,
                                            SIG_CAP("IoError")),
                                   SIG_CAP("File"), SIG_STD("Bytes"))},
    {"File", "flush", SIG_FN(EK_ARROW_WORLD,
                             SIG_APP2("Result", SIG_UNIT,
                                      SIG_CAP("IoError")),
                             SIG_CAP("File"))},
    {"File", "close", SIG_FN(EK_ARROW_WORLD,
                             SIG_APP2("Result", SIG_UNIT,
                                      SIG_CAP("IoError")),
                             SIG_CAP("File"))},

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
       the bridge for the names its rows do not carry yet. The
       capability namespaces are complete: the source members and the
       rows above carry every member of Annex C.9 (Clauses 7.9.1,
       10.8). */
    {"Args", NULL, NULL},
    {"Bool", NULL, NULL},
    {"Bytes", NULL, NULL},
    {"Char", NULL, NULL},
    {"Clock", NULL, NULL},
    {"Conc", NULL, NULL},
    {"Dict", NULL, NULL},
    {"Duration", NULL, NULL},
    {"Env", NULL, NULL},
    {"F32", NULL, NULL},
    {"F64", NULL, NULL},
    {"File", NULL, NULL},
    {"Float", NULL, NULL},
    {"Fs", NULL, NULL},
    {"I16", NULL, NULL},
    {"I32", NULL, NULL},
    {"I64", NULL, NULL},
    {"I8", NULL, NULL},
    {"In", NULL, NULL},
    {"Int", NULL, NULL},
    {"Json", NULL, NULL},
    {"List", NULL, NULL},
    {"Mono", NULL, NULL},
    {"Option", NULL, NULL},
    {"Out", NULL, NULL},
    {"Path", NULL, NULL},
    {"Proc", NULL, NULL},
    {"Rand", NULL, NULL},
    {"Range", NULL, NULL},
    {"Result", NULL, NULL},
    {"Scope", NULL, NULL},
    {"Set", NULL, NULL},
    {"String", NULL, NULL},
    {"Task", NULL, NULL},
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

/* The injected pairs of the capability module (Clause 10.8): the
   compiler rows behind the `_`-prefixed names the cap source
   references without declaring. Each pair is the unwrap and wrap of
   one session bundle. Their rows carry no declaration node, so the
   schemes hang here and the pair stays unnameable outside the
   capability module. The file bundle is generic over the callback
   result of `with_file`; its variable generalizes, so every use
   instantiates its own. */
static const struct {
    const char *name;
    const ns_sig *sig;
} pair_rows[] = {
    {"_out_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_OutB"),
                           SIG_STD("Out"))},
    {"_out_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Out"),
                         SIG_CAP("_OutB"))},
    {"_in_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_InB"),
                          SIG_STD("In"))},
    {"_in_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("In"),
                        SIG_CAP("_InB"))},
    {"_fs_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP1("_FsB", SIG_VAR(0)),
                          SIG_STD("Fs"))},
    {"_fs_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Fs"),
                        SIG_CAP1("_FsB", SIG_VAR(0)))},
    {"_env_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_EnvB"),
                           SIG_STD("Env"))},
    {"_env_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Env"),
                         SIG_CAP("_EnvB"))},
    {"_args_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_ArgsB"),
                            SIG_STD("Args"))},
    {"_args_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Args"),
                          SIG_CAP("_ArgsB"))},
    {"_clock_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_ClockB"),
                             SIG_STD("Clock"))},
    {"_clock_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Clock"),
                           SIG_CAP("_ClockB"))},
    {"_rand_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_RandB"),
                            SIG_STD("Rand"))},
    {"_rand_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Rand"),
                          SIG_CAP("_RandB"))},
};

/* The schemes of the injected pairs, hung on the cap module's
   undeclared function rows in the per-module phase: a body of the cap
   source reaches them by name, and every other module reaches neither
   the names nor the symbols. The signature's variables generalize, so
   a use instantiates them fresh. */
void ck_cap_schemes(ck_ctx *c)
{
    size_t k, i, j;

    for (k = 0; k < c->in->module_count && !c->oom; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, "cap") != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *sym = &s->syms[i];

            if (sym->kind != EK_SYM_FUNC || sym->decl || sym->type) {
                continue;
            }
            for (j = 0; j < sizeof pair_rows / sizeof *pair_rows; j++) {
                if (strcmp(sym->name, pair_rows[j].name) == 0) {
                    const ek_type *slots[4] = {0, 0, 0, 0};
                    const ek_type *t;
                    ck_scheme *held;
                    size_t v;

                    for (v = 0; v < 4; v++) {
                        ek_type *tv = ck_fresh(c, EK_T_TVAR);

                        if (!tv) {
                            return;
                        }
                        tv->u.tvar.name = pair_rows[j].name;
                        slots[v] = tv;
                    }
                    t = build(c, pair_rows[j].sig, slots, 4);
                    held = ck_mono(c, t);
                    if (held) {
                        *held = ck_generalize(c, t);
                        held->bridge = 1;
                    }
                    ck_map_put(c, &c->schemes, sym, held);
                }
            }
        }
    }
}
