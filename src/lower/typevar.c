#include "lower/internal.h"

/* The variable-occurrence queries over a type: whether a rigid
   binder's variable, or an inference variable the walk could not
   determine, occurs anywhere in it. */

int lower_type_has_var(const ek_type *t)
{
    size_t i;

    if (!t) {
        return 0;
    }
    if (t->kind == EK_T_TVAR || t->kind == EK_T_IVAR) {
        return 1;
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        for (i = 0; i < t->u.nominal.argc; i++) {
            if (lower_type_has_var(t->u.nominal.args[i])) {
                return 1;
            }
        }
        return 0;
    case EK_T_LIST:
        return lower_type_has_var(t->u.list.elem);
    case EK_T_DICT:
        return lower_type_has_var(t->u.dict.key)
            || lower_type_has_var(t->u.dict.val);
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            if (lower_type_has_var(t->u.tuple.items[i])) {
                return 1;
            }
        }
        return 0;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            if (lower_type_has_var(t->u.record.fields[i].type)) {
                return 1;
            }
        }
        return 0;
    case EK_T_FN:
        for (i = 0; i < t->u.fn.count; i++) {
            if (lower_type_has_var(t->u.fn.params[i])) {
                return 1;
            }
        }
        return lower_type_has_var(t->u.fn.result);
    default:
        return 0;
    }
}

/* Whether an inference variable occurs anywhere in the type: an
   opaque row the substitution cannot make concrete. */
int lower_type_has_ivar(const ek_type *t)
{
    size_t i;

    if (!t) {
        return 0;
    }
    if (t->kind == EK_T_IVAR) {
        return 1;
    }
    if (t->kind == EK_T_TVAR) {
        return 0;
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        for (i = 0; i < t->u.nominal.argc; i++) {
            if (lower_type_has_ivar(t->u.nominal.args[i])) {
                return 1;
            }
        }
        return 0;
    case EK_T_LIST:
        return lower_type_has_ivar(t->u.list.elem);
    case EK_T_DICT:
        return lower_type_has_ivar(t->u.dict.key)
            || lower_type_has_ivar(t->u.dict.val);
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            if (lower_type_has_ivar(t->u.tuple.items[i])) {
                return 1;
            }
        }
        return 0;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            if (lower_type_has_ivar(t->u.record.fields[i].type)) {
                return 1;
            }
        }
        return 0;
    case EK_T_FN:
        for (i = 0; i < t->u.fn.count; i++) {
            if (lower_type_has_ivar(t->u.fn.params[i])) {
                return 1;
            }
        }
        return lower_type_has_ivar(t->u.fn.result);
    default:
        return 0;
    }
}
