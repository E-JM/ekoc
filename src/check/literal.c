#include <math.h>
#include <string.h>

#include "check/internal.h"

/* Literal placement by expected type. A std type's identity is the
   surface symbol the std module declares, placeholder row or file row;
   the symbol is canonicalized per run so every site reaches the same
   one, with a synthetic stand-in only for a name no row carries. */

static const ek_symbol *std_sym(ck_ctx *c, const char *name);

static int is_std_named(const ek_type *t, const char *name)
{
    return t && t->kind == EK_T_NOMINAL && t->u.nominal.sym
        && t->u.nominal.sym->module
        && strcmp(t->u.nominal.sym->module, "std") == 0
        && strcmp(t->u.nominal.sym->name, name) == 0;
}

int ck_is_machine_word(const ek_type *t)
{
    static const char *words[] = {"U8", "U16", "U32", "U64",
                                  "I8", "I16", "I32", "I64"};
    size_t i;

    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || t->u.nominal.sym->decl != NULL) {
        return 0;
    }
    for (i = 0; i < sizeof words / sizeof *words; i++) {
        if (strcmp(t->u.nominal.sym->name, words[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int ck_is_numeric(const ek_type *t)
{
    return is_std_named(t, "Int") || is_std_named(t, "UInt")
        || is_std_named(t, "Float");
}

/* Canonical std type symbol for name: memoized by spelling; the std
   module's surface row, or the one synthesized symbol for names no
   row declares. The synthesized table is content-keyed: a name
   reaches several call sites as distinct literals, and two symbols
   for one spelling would split nominal identity. */
static const ek_symbol *std_sym(ck_ctx *c, const char *name)
{
    ek_symbol *fresh;
    size_t k, i;

    for (k = 0; k < c->nsynth; k++) {
        if (strcmp(c->synth[k].name, name) == 0) {
            return c->synth[k].sym;
        }
    }
    if (c->oom) {
        return NULL;
    }
    for (k = 0; k < c->in->module_count; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, "std") != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *sym = &s->syms[i];

            if (sym->kind == EK_SYM_TYPE && strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    /* The shipped companions carry their types under their own module
       names (Clause 9.1.3); the stem list mirrors r_companion_stem
       (resolve/module.c). The scan precedes synthesis so a companion
       nominal keeps the resolve-stage symbol, and one identity, when
       the rows table builds its signatures. */
    for (k = 0; k < c->in->module_count; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, "time") != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *sym = &s->syms[i];

            if (sym->kind == EK_SYM_TYPE && strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    fresh = ek_arena_alloc(c->arena, sizeof *fresh);
    if (!fresh) {
        c->oom = 1;
        return NULL;
    }
    memset(fresh, 0, sizeof *fresh);
    fresh->name = name;
    fresh->kind = EK_SYM_TYPE;
    fresh->module = "std";
    if (c->nsynth == c->csynth) {
        size_t ncap = c->csynth ? c->csynth * 2 : 16;
        struct ck_synth_row *grown = ek_arena_alloc(c->arena,
                                                    ncap * sizeof *grown);

        if (!grown) {
            c->oom = 1;
            return fresh;
        }
        if (c->nsynth) {
            memcpy(grown, c->synth, c->nsynth * sizeof *grown);
        }
        c->synth = grown;
        c->csynth = ncap;
    }
    c->synth[c->nsynth].name = fresh->name;
    c->synth[c->nsynth].sym = fresh;
    c->nsynth++;
    return fresh;
}

const ek_type *ck_std_type(ck_ctx *c, const char *name)
{
    const ek_symbol *sym = std_sym(c, name);

    return sym ? ck_nominal(c, sym, NULL, 0) : NULL;
}

const ek_symbol *ck_std_symbol(ck_ctx *c, const char *name)
{
    return std_sym(c, name);
}

int ck_is_std(const ek_type *t, const char *name)
{
    return is_std_named(t, name);
}

/* Range of one integer target; neg is the unary-minus form. */
static int in_range(const ek_type *t, unsigned long long mag, int neg)
{
    unsigned long long hi;

    if (is_std_named(t, "Int")) {
        hi = 9223372036854775807ull; /* 2^63 - 1 */
        if (neg) {
            return mag <= hi + 1; /* -2^63 is representable */
        }
        return mag <= hi;
    }
    if (is_std_named(t, "UInt")) {
        return !neg && mag <= 18446744073709551615ull;
    }
    if (is_std_named(t, "U8")) {
        hi = 255;
    } else if (is_std_named(t, "U16")) {
        hi = 65535;
    } else if (is_std_named(t, "U32")) {
        hi = 4294967295ull;
    } else if (is_std_named(t, "U64")) {
        hi = 18446744073709551615ull;
    } else if (is_std_named(t, "I8")) {
        return neg ? mag <= 128 : mag <= 127;
    } else if (is_std_named(t, "I16")) {
        return neg ? mag <= 32768 : mag <= 32767;
    } else if (is_std_named(t, "I32")) {
        return neg ? mag <= 2147483648ull : mag <= 2147483647ull;
    } else if (is_std_named(t, "I64")) {
        return neg ? mag <= 9223372036854775808ull
                   : mag <= 9223372036854775807ull;
    } else {
        return 1;
    }
    return !neg && mag <= hi;
}

/* An integer target from the expected type; NULL when the placement
   defaults to Int. */
static const ek_type *int_target(const ek_type *expected)
{
    expected = ck_prune(expected);
    if (expected && expected->kind == EK_T_NOMINAL) {
        const char *n = expected->u.nominal.sym
                            ? expected->u.nominal.sym->name : "";

        if (strcmp(n, "Int") == 0 || strcmp(n, "UInt") == 0
            || ck_is_machine_word(expected)) {
            return expected;
        }
    }
    return NULL;
}

const ek_type *ck_place(ck_ctx *c, const ek_node *lit, const ek_type *expected,
                        int neg)
{
    if (c->oom || !lit) {
        return NULL;
    }
    if (lit->kind == EK_N_INT_LIT) {
        const ek_type *exp = ck_prune(expected);
        const ek_type *target;
        const ek_type *t;

        if (exp && exp->kind == EK_T_NOMINAL && is_std_named(exp, "Float")) {
            ck_error(c, lit, "E-type-mismatch",
                     "an integer literal is never a `Float`; write the "
                     "value as a floating literal (Clauses 5.7.4, 7.8.4)");
            return exp;
        }
        target = int_target(exp);
        t = target ? target : ck_std_type(c, "Int");
        if (lit->u.int_lit.overflow
            || !in_range(t, lit->u.int_lit.magnitude, neg)) {
            ck_error(c, lit, "E-literal-overflow",
                     "the integer literal `%s` lies outside the range of "
                     "its target type (Clauses 5.7.1, 5.7.4)",
                     lit->u.int_lit.text);
        }
        return t;
    }
    if (lit->kind == EK_N_FLOAT_LIT) {
        const ek_type *t = int_target(expected);

        /* The converted value must be finite: overflow arrives as
           infinity, and underflow to a subnormal or zero is finite. */
        if (!isfinite(lit->u.float_lit.value)) {
            ck_error(c, lit, "E-literal-overflow",
                     "the floating literal `%s` is not finite after "
                     "conversion (Clause 5.7.2)", lit->u.float_lit.text);
        }
        if (t) {
            ck_error(c, lit, "E-type-mismatch",
                     "a floating literal is never an `%s` (Clause 5.7.4)",
                     ck_type_name(c, t));
            return ck_std_type(c, "Float");
        }
        return ck_std_type(c, "Float");
    }
    if (lit->kind == EK_N_CHAR_LIT) {
        return ck_std_type(c, "Char");
    }
    return ck_std_type(c, "String");
}
