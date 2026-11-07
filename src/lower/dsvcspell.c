#include "lower/internal.h"

/* The spelling of one applied type: a closed alphabet of head words
   -- scalars bare, every compound tagged with a letter -- and `$`
   between components, which no written identifier supplies, so two
   distinct types never spell alike whatever their modules, fields, and
   arguments are named. The service names and the specialized function
   names share the alphabet. */

/* The bare word of a root scalar, or NULL for any other type: the
   scalar names are a closed set no other spelling can imitate, every
   other piece carrying its `$` tag. */
static const char *scalar_word(const ek_type *t)
{
    if (t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || t->u.nominal.argc != 0
        || lower_scalar_kind(t) == EKSC_OTHER) {
        return NULL;
    }
    return t->u.nominal.sym->name;
}

/* The spelling of a tuple or record shape: the arity head, then each
   component. */
static void spell_fields(lower_ctx *c, ek_buf *b, const ek_type *t)
{
    size_t n = t->kind == EK_T_TUPLE ? t->u.tuple.count
                                     : t->u.record.count;

    ek_buf_printf(b, "%s%zu$", t->kind == EK_T_TUPLE ? "T" : "R", n);
    for (size_t i = 0; i < n; i++) {
        if (i) {
            ek_buf_append_char(b, '$');
        }
        if (t->kind == EK_T_RECORD) {
            ek_buf_append_str(b, t->u.record.fields[i].name);
            ek_buf_append_char(b, '$');
            spell_into(c, b, t->u.record.fields[i].type);
        } else {
            spell_into(c, b, t->u.tuple.items[i]);
        }
    }
}

/* The spelling of a nominal and its applied arguments. */
static void spell_nominal(lower_ctx *c, ek_buf *b, const ek_type *t)
{
    if (!t->u.nominal.sym) {
        ek_buf_append_str(b, "N$x");
        return;
    }
    ek_buf_printf(b, "N$%s$%s", t->u.nominal.sym->module,
                  t->u.nominal.sym->name);
    for (size_t i = 0; i < t->u.nominal.argc; i++) {
        ek_buf_append_char(b, '$');
        spell_into(c, b, t->u.nominal.args[i]);
    }
}

/* The spelling pieces of the compound shapes. */
static void spell_shape(lower_ctx *c, ek_buf *b, const ek_type *t)
{
    switch (t->kind) {
    case EK_T_LIST:
        ek_buf_append_str(b, "L$");
        spell_into(c, b, t->u.list.elem);
        break;
    case EK_T_DICT:
        ek_buf_append_str(b, "D$");
        spell_into(c, b, t->u.dict.key);
        ek_buf_append_char(b, '$');
        spell_into(c, b, t->u.dict.val);
        break;
    case EK_T_TUPLE:
    case EK_T_RECORD:
        spell_fields(c, b, t);
        break;
    case EK_T_NOMINAL:
        spell_nominal(c, b, t);
        break;
    default:
        break;
    }
}

/* The identifier piece of an applied type: the applied arguments join
   the family spelling, following the struct registry's keying. */
void spell_into(lower_ctx *c, ek_buf *b, const ek_type *t)
{
    const char *word;

    if (!t) {
        ek_buf_append_str(b, "Opaque");
        return;
    }
    switch (t->kind) {
    case EK_T_UNIT:
        ek_buf_append_str(b, "Unit");
        break;
    case EK_T_NEVER:
        ek_buf_append_str(b, "Never");
        break;
    case EK_T_FN:
        ek_buf_append_str(b, "Fn");
        break;
    case EK_T_TVAR:
    case EK_T_IVAR:
        ek_buf_append_str(b, "Opaque");
        break;
    default:
        word = scalar_word(t);
        if (word) {
            ek_buf_append_str(b, word);
        } else {
            spell_shape(c, b, t);
        }
        break;
    }
}

/* The identifier pieces of one instantiation's applied arguments:
   the suffix a specialized function's name carries after the
   separator. */
const char *lower_spell_apply(lower_ctx *c, const ek_type *const *args,
                              size_t n)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; i < n; i++) {
        if (i && !ek_buf_append_char(&b, '$')) {
            c->oom = 1;
        }
        spell_into(c, &b, args[i]);
    }
    return lower_take(c, &b);
}
