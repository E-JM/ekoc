#include <string.h>

#include "lower/internal.h"

/* The inverted binding and the trampolines. An inverted binding is a
   call whose last argument is the continuation: the pattern's names
   are the continuation's parameters and the members after it are its
   body. A top-level function referenced as a value travels as a fresh
   closure over a static trampoline. */

/* Drops the params of an inverted pattern out of the capture set:
   they are the continuation's own. */
static size_t without_params(const char **names, size_t n,
                             const char **params, size_t nparams,
                             const char **out)
{
    size_t i, k, kept = 0;

    for (i = 0; i < n; i++) {
        int found = 0;

        for (k = 0; k < nparams; k++) {
            if (params[k] && strcmp(params[k], names[i]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            out[kept++] = names[i];
        }
    }
    return kept;
}
/* The capture set of a continuation body: the free reads of its
   members; its own parameters shadow, and the over-captured names a
   shared outer binding contributes are harmless. */
static void collect_captures(lower_ctx *c, lower_scope *scope,
                             const ek_node **members, size_t nmembers,
                             lower_reads *r)
{
    size_t i;

    memset(r, 0, sizeof *r);
    r->c = c;
    r->at = scope;
    for (i = 0; i < nmembers; i++) {
        lower_reads_walk(r, members[i]);
    }
}

/* The continuation's deferred body. */
static void queue_continuation(lower_ctx *c,
                               const ek_node **members, size_t nmembers,
                               const char *name, const char **caps,
                               size_t ncaps, const char **params,
                               size_t nparams, lower_gtype *g)
{
    lower_pending *p = ek_arena_alloc(c->arena, sizeof *p);

    (void)params;
    (void)nparams;
    if (!p) {
        c->oom = 1;
        return;
    }
    memset(p, 0, sizeof *p);
    p->members = members;
    p->nmembers = nmembers;
    p->name = name;
    p->caps = caps;
    p->ncap = ncaps;
    p->params = params;
    p->nparams = nparams;
    p->stem = g->stem;
    p->file = c->file;
    lower_pending_add(c, p);
}

/* The inverted binding: the continuation closure over the remaining
   members is the call's last argument, and the call result is the
   binding's value. */
lower_val lower_invert(lower_ctx *c, lower_scope *scope, const ek_node *m,
                       const ek_node **members, size_t nmembers)
{
    lower_reads r;
    const char **params = NULL;
    size_t nparams = 0, pcap = 0;
    const char **caps = NULL;
    size_t ncaps = 0, ccap = 0;
    lower_gtype *g;
    const char *name;
    const char *clo;

    if (!m->u.bind.value || m->u.bind.value->kind != EK_N_CALL) {
        lower_error(c, m, "the `<-` binding has no lowering");
        if (m->u.bind.value) {
            lower_val v = lower_sub(c, scope, m->u.bind.value);
            const char *t = lower_as_temp(c, scope, &v);

            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", t);
        }
        return lower_val_unit();
    }
    lower_pat_param(c, m->u.bind.pattern, &params, &nparams, &pcap);
    collect_captures(c, scope, members, nmembers, &r);
    caps = lower_grow(c, NULL, &ccap, r.n ? r.n : 1, sizeof *caps);
    if (!caps) {
        return lower_val_unit();
    }
    ncaps = without_params(r.names, r.n, params, nparams, caps);
    g = lower_env_family(c, caps, ncaps);
    if (!g) {
        return lower_val_unit();
    }
    name = lower_str(c, "ekl_%u", c->lambda++);
    lower_push_sig(c, lower_str(c, "static ek_val %s(ek_val env, ek_val *args, "
                             "size_t argc)", name));
    clo = lower_emit_closure_new(c, scope, m, g, caps, ncaps, name);
    queue_continuation(c, members, nmembers, name, caps, ncaps, params,
                       nparams, g);
    /* the call machinery releases the appended callback with the
       argument temps */
    return lower_call_extra(c, scope, m->u.bind.value, clo);
}

/* The trampoline's body: the argument slots forward to the direct
   call. */
static const char *tramp_text(lower_ctx *c, const ek_symbol *sym,
                              const char *name)
{
    ek_buf b;
    size_t n = 0, i;

    if (sym->type && sym->type->kind == EK_N_T_FN) {
        n = sym->type->u.t_fn.params.count;
    }
    ek_buf_init(&b);
    if (!ek_buf_printf(&b, "static ek_val %s(ek_val env, ek_val *args, "
                           "size_t argc)\n{\n    (void)env;\n    (void)argc;"
                           "\n", name)) {
        c->oom = 1;
    }
    if (n == 0 && !ek_buf_append_str(&b, "    (void)args;\n")) {
        c->oom = 1;
    }
    if (!ek_buf_printf(&b, "    return %s(", lower_mangled(c, sym))) {
        c->oom = 1;
    }
    for (i = 0; i < n; i++) {
        if (!ek_buf_printf(&b, "%sargs[%zu]", i ? ", " : "", i)) {
            c->oom = 1;
        }
    }
    if (!ek_buf_append_str(&b, ");\n}\n\n")) {
        c->oom = 1;
    }
    return lower_take(c, &b);
}

/* The trampoline of a top-level function referenced as a value: it
   forwards the argument slots to the direct call. */
const char *lower_trampoline(lower_ctx *c, const ek_symbol *sym)
{
    lower_pending *p;
    const char *name;
    size_t i;

    for (i = 0; i < c->ntramp; i++) {
        if (c->tramps[i].sym == sym) {
            return c->tramps[i].name;
        }
    }
    name = lower_str(c, "ekt_%s$%s", sym->module, sym->name);
    lower_push_sig(c, lower_str(c, "static ek_val %s(ek_val env, ek_val *args, "
                             "size_t argc)", name));
    p = ek_arena_alloc(c->arena, sizeof *p);
    if (p) {
        memset(p, 0, sizeof *p);
        p->text = tramp_text(c, sym, name);
        lower_pending_add(c, p);
    } else {
        c->oom = 1;
    }
    lower_enqueue(c, sym);
    {
        lower_tramp *slots = lower_grow(c, c->tramps, &c->ctramp,
                                        c->ntramp + 1, sizeof *slots);

        if (slots) {
            slots[c->ntramp].sym = sym;
            slots[c->ntramp].name = name;
            c->ntramp++;
            c->tramps = slots;
        }
    }
    return name;
}

