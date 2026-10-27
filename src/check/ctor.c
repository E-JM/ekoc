#include <string.h>

#include "check/internal.h"
#include "support/buf.h"

/* Constructor candidates in value position. Constructors do not
   conflict by visibility: a reference carries every visible same-named
   one and the expected nominal type anchors the selection. Without an
   anchor only a sole non-generic constructor self-anchors; a std
   placeholder stays the opaque bridge. */

/* Ownership is the declaring module plus the owner's spelling, which
   a std placeholder constructor and its std type share. */
static int cand_owned(const ek_symbol *cand, const ek_symbol *type)
{
    return cand && type && cand->owner && cand->module && type->module
        && strcmp(cand->owner, type->name) == 0
        && strcmp(cand->module, type->module) == 0;
}

const ek_symbol *ck_ctor_pick(const ek_ref *ref, const ek_type *expected)
{
    const ek_type *exp = ck_prune(expected);
    size_t i;

    if (!ref || !ref->cands || !exp || exp->kind != EK_T_NOMINAL
        || !exp->u.nominal.sym) {
        return NULL;
    }
    for (i = 0; i < ref->ncands; i++) {
        if (cand_owned(ref->cands[i], exp->u.nominal.sym)) {
            return ref->cands[i];
        }
    }
    return NULL;
}

/* The std placeholders carry no declaration, so they are never
   generic; a declared owner with binders is. */
static int ck_ctor_generic(ck_ctx *c, const ek_symbol *cand)
{
    const ck_ctor *ct = ck_ctor_of(c, cand);
    const ck_decl *d = ct && ct->owner ? ck_decl_of(c, ct->owner) : NULL;

    return d && d->nbinders > 0;
}

static const char *count_word(size_t n)
{
    switch (n) {
    case 2:
        return "two";
    case 3:
        return "three";
    default:
        return NULL;
    }
}

/* The owners of the candidates, joined for a diagnostic. A std
   candidate carries its module, so its spelling is distinguishable
   from a user type of the same name. */
static char *owners_text(ck_ctx *c, const ek_ref *ref)
{
    ek_buf b;
    char *out = NULL;
    int ok = 1;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; ok && i < ref->ncands; i++) {
        const ek_symbol *cand = ref->cands[i];

        if (i) {
            ok = ek_buf_append_str(&b, " or ");
        }
        if (ok && cand->module
            && strcmp(cand->module, "std") == 0) {
            ok = ek_buf_append_str(&b, cand->module)
              && ek_buf_append_char(&b, '.');
        }
        if (ok) {
            ok = ek_buf_append_str(&b, cand->owner ? cand->owner : "?");
        }
    }
    if (ok && b.len) {
        out = ek_arena_alloc(c->arena, b.len + 1);
        if (out) {
            memcpy(out, b.data ? b.data : "", b.len + 1);
        }
    }
    if (!out) {
        c->oom = 1;
    }
    ek_buf_destroy(&b);
    return out;
}

/* Several candidates and no anchor: name them, since an annotation
   over any one of them resolves the reference. */
static void undetermined_many(ck_ctx *c, const ek_node *n, const ek_ref *ref)
{
    const char *owners = owners_text(c, ref);
    const char *name = ref->symbol ? ref->symbol->name : "?";
    const char *word = count_word(ref->ncands);

    if (!owners) {
        return;
    }
    if (word) {
        ck_error(c, n, "E-undetermined",
                 "%s constructors named `%s` are visible and no expected "
                 "type anchors the reference; write a `%s` annotation "
                 "(Clause 7.4.6)", word, name, owners);
        return;
    }
    ck_error(c, n, "E-undetermined",
             "%zu constructors named `%s` are visible and no expected "
             "type anchors the reference; write a `%s` annotation "
             "(Clause 7.4.6)", ref->ncands, name, owners);
}

ck_ctorsel ck_ctor_select(ck_ctx *c, const ek_node *n, const ek_ref *ref,
                          const ek_type *expected)
{
    ck_ctorsel out;
    const ek_type *exp = ck_prune(expected);
    const ek_symbol *pick;

    memset(&out, 0, sizeof out);
    if (exp && exp->kind == EK_T_NOMINAL && exp->u.nominal.sym) {
        pick = ck_ctor_pick(ref, exp);
        if (pick) {
            out.sym = pick;
            out.anchored = 1;
            return out;
        }
        ck_error(c, n, ck_conform_code(CK_NOMINAL),
                 "the constructor `%s` does not match the expected type "
                 "%s (Clause 7.5.2)",
                 ref->symbol ? ref->symbol->name : "?",
                 ck_type_name(c, exp));
        return out;
    }
    if (ref->ncands == 1 && exp && exp->kind == EK_T_IVAR) {
        /* The expectation is still undetermined: the sole candidate
           stands, and unification with the expectation decides its
           instantiation (a generic one takes fresh variables). */
        out.sym = ref->cands[0];
        return out;
    }
    if (ref->ncands == 1 && !ck_ctor_generic(c, ref->cands[0])) {
        out.sym = ref->cands[0];
        return out;
    }
    if (ref->ncands == 1) {
        ck_error(c, n, "E-undetermined",
                 "the constructor `%s` has no unique instantiation; an "
                 "expected type determines it (Clause 7.6.4)",
                 ref->symbol ? ref->symbol->name : "?");
    } else if (ref->ncands > 1) {
        undetermined_many(c, n, ref);
    }
    return out;
}

/* A constructor in value position: the expected nominal type selects
   among the candidates, a sole non-generic constructor self-anchors,
   and a std placeholder stays the opaque bridge. */
const ek_type *ck_ctor_type(ck_ctx *c, const ek_node *n,
                            const ek_ref *ref, const ek_type *expected)
{
    ck_ctorsel sel;
    const ck_ctor *ct;
    const ek_type *exp = ck_prune(expected);

    if (!ref->ncands) {
        /* A qualified constructor names one declaration. */
        ct = ref->symbol ? ck_ctor_of(c, ref->symbol) : NULL;
        if (ct && ct->owner) {
            const ck_decl *d = ck_decl_of(c, ct->owner);

            if (!d || d->nbinders == 0) {
                return ck_nominal(c, ct->owner, NULL, 0);
            }
            if (exp && exp->kind == EK_T_NOMINAL
                && exp->u.nominal.sym == ct->owner) {
                return exp; /* the expected type anchors it */
            }
            ck_error(c, n, "E-undetermined",
                     "the constructor `%s` has no unique "
                     "instantiation; an expected type determines it "
                     "(Clause 7.6.4)", ref->symbol->name);
        }
        return ck_ivar(c);
    }
    sel = ck_ctor_select(c, n, ref, expected);
    if (!sel.sym) {
        return ck_ivar(c);
    }
    if (sel.sym->decl == NULL) {
        /* A std placeholder: opaque, conforming into an anchor. */
        const ek_type *iv = ck_ivar(c);

        if (expected) {
            ck_conforms(c, iv, expected, 1);
        }
        return iv;
    }
    if (sel.anchored) {
        return exp;
    }
    ct = ck_ctor_of(c, sel.sym);
    if (!ct || !ct->owner) {
        return ck_ivar(c);
    }
    if (exp && exp->kind == EK_T_IVAR) {
        /* An undetermined expectation: the reference supplies fresh
           variables and the expectation takes the instance. */
        const ek_type *out = ck_nominal_fresh(c, ct->owner);

        ck_conforms(c, out, exp, 1);
        return out;
    }
    return ck_nominal(c, ct->owner, NULL, 0);
}
