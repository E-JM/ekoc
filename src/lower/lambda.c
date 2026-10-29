#include <string.h>

#include "lower/internal.h"

/* Closures and the inverted-binding chains. A lambda lowers to a
   static body function over a generated environment struct: the
   creation site keeps every captured free name into the struct, and
   the body reads its captures through the environment reference and
   owns its parameters exactly like an ordinary function. A top-level
   function referenced as a value lowers to a fresh closure over a
   static trampoline. An inverted binding is a call whose last
   argument is the continuation: the pattern's names are the
   continuation's parameters and the members after it are its body. */

typedef struct {
    lower_ctx *c;
    lower_scope *at;      /* the creation site: the names it sees */
    const char **names;
    size_t n, cap;
} reads;

/* The closure body's scopes: the borrowed environment reads and the
   owned parameter keeps over the argument slots. */
static void emit_closure_scope(lower_ctx *c, const lower_pending *p,
                               lower_scope *caps, lower_scope *ps)
{
    size_t i;

    for (i = 0; i < p->ncap; i++) {
        const char *cname = lower_str(c, "((ekS_%s *)env.as.ref)->f_%s",
                                      p->stem, p->caps[i]);

        lower_scope_add(c, caps, p->caps[i], cname, 1, NULL);
    }
    for (i = 0; i < p->nparams; i++) {
        const char *pname = p->params[i]
            ? p->params[i] : lower_str(c, "a%zu", i);
        const char *cname = lower_c_name(c, ps, pname);

        lower_emitf(c, "    ek_val %s = ek_keep(args[%zu]);\n", cname, i);
        lower_scope_add(c, ps, p->params[i] ? p->params[i] : cname, cname,
                        0, NULL);
    }
}

/* The body value as the closure's return temp: a jumped tail falls
   back to the unit value. */
static const char *closure_return(lower_ctx *c, lower_scope *top,
                                  lower_val v)
{
    const char *ret;

    if (!v.text) {
        v = lower_val_unit();
    }
    if (v.is_temp) {
        return v.text;
    }
    ret = lower_temp(c, top);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s;\n", ret, v.text);
    return ret;
}

/* One closure body: the environment reads are borrowed bindings, the
   parameters are owned keeps of the argument slots, and the return
   path releases the owned locals. */
static void emit_closure_body(lower_ctx *c, lower_pending *p)
{
    const char *file = c->file;
    unsigned d = c->depth;
    int saved_tail = c->in_tail;
    lower_scope caps, ps, top;
    lower_val v;
    const char *ret;

    c->file = p->file;
    c->temp = 0;
    c->tce_self = NULL;
    c->tce_params = NULL;
    c->tce_label = NULL;
    memset(&caps, 0, sizeof caps);
    memset(&ps, 0, sizeof ps);
    memset(&top, 0, sizeof top);
    ps.parent = &caps;
    top.parent = &ps;
    lower_emitf(c, "static ek_val %s(ek_val env, ek_val *args, "
                   "size_t argc)\n{\n", p->name);
    c->depth = 1;
    lower_emitf(c, "    (void)argc;\n");
    if (p->ncap == 0) {
        lower_emitf(c, "    (void)env;\n");
    }
    if (p->nparams == 0) {
        lower_emitf(c, "    (void)args;\n");
    }
    emit_closure_scope(c, p, &caps, &ps);
    c->in_tail = 1;
    v = lower_members(c, &top, p->members, p->nmembers);
    c->in_tail = saved_tail;
    ret = closure_return(c, &top, v);
    lower_release_scope(c, &top);
    lower_release_scope(c, &ps);
    lower_pad(c);
    lower_emitf(c, "return %s;\n", ret);
    c->depth = d;
    lower_emitf(c, "}\n\n");
    c->file = file;
}

/* A lambda at a value position: the environment builds and the
   closure is born here; the body lowers after the enclosing
   function. */
lower_val lower_lambda(lower_ctx *c, lower_scope *scope, const ek_node *e)
{
    lower_reads r;
    lower_gtype *g;
    const char *name;
    const char *clo;
    const char **params = NULL;
    size_t nparams = 0, pcap = 0;
    const ek_node **members = NULL;
    size_t nmembers = 0;
    lower_pending *p;

    memset(&r, 0, sizeof r);
    r.c = c;
    r.at = scope;
    lower_reads_walk(&r, e->u.pair.b);
    g = lower_env_family(c, r.names, r.n);
    if (!g) {
        return lower_val_unit();
    }
    name = lower_str(c, "ekl_%u", c->lambda++);
    lower_push_sig(c, lower_str(c, "static ek_val %s(ek_val env, ek_val *args, "
                             "size_t argc)", name));
    clo = lower_emit_closure_new(c, scope, e, g, r.names, r.n, name);
    lower_lambda_param_names(c, e->u.pair.a, &params, &nparams, &pcap);
    lower_body_members(c, e->u.pair.b, &members, &nmembers);
    p = ek_arena_alloc(c->arena, sizeof *p);
    if (!p) {
        c->oom = 1;
        return (lower_val){ clo, 1 };
    }
    memset(p, 0, sizeof *p);
    p->members = members;
    p->nmembers = nmembers;
    p->name = name;
    p->caps = r.names;
    p->ncap = r.n;
    p->params = params;
    p->nparams = nparams;
    p->stem = g->stem;
    p->file = c->file;
    lower_pending_add(c, p);
    return (lower_val){ clo, 1 };
}

/* The deferred definitions, in creation order; a closure body may
   queue more. */
void lower_flush_pending(lower_ctx *c)
{
    while (c->pend && !c->oom) {
        lower_pending *p = c->pend;

        c->pend = p->next;
        if (!c->pend) {
            c->pend_tail = NULL;
        }
        if (p->text) {
            if (!ek_buf_append_str(&c->body, p->text)) {
                c->oom = 1;
            }
        } else {
            emit_closure_body(c, p);
        }
    }
}
