#include <string.h>

#include "resolve/internal.h"

/* Dotted references and receiver chains (parse.h representation
   notes). One walk classifies every link:

   - a non-last link spelled like an imported or the current module is
     a module segment, and the chain continues at station 4 (7.4.3);
   - a snake head carrying a binding value, and every other
     non-module link, is a receiver-form member of stations 1-3: the
     receiver position never consults lexical bindings, and the field,
     namespace, or current-module reading needs the receiver's static
     type, so check (M6) selects it (EK_SYM_MEMBER);
   - a snake head carrying a module continues as a qualified access
     `Mod.f` (7.4.4);
   - a Pascal head is the type namespace of Clause 7.4.1; its members
     live in the declaring module's surface, and unknown members defer
     to check (derived services, Clause 9.8.7).
*/

#define R_MAX_LINKS 32

typedef struct {
    const ek_node *head;
    const ek_node *links[R_MAX_LINKS]; /* head-first */
    const char *names[R_MAX_LINKS];
    int n;
} r_links;

static void flatten(const ek_node *top, r_links *l)
{
    const ek_node *nodes[R_MAX_LINKS];
    const char *names[R_MAX_LINKS];
    const ek_node *p = top;
    int n = 0, i;

    while (p && p->kind == EK_N_FIELD && n < R_MAX_LINKS) {
        nodes[n] = p;
        names[n] = p->u.field.name;
        n++;
        p = p->u.field.recv;
    }
    l->head = p;
    l->n = n;
    for (i = 0; i < n; i++) {
        l->links[i] = nodes[n - 1 - i];
        l->names[i] = names[n - 1 - i];
    }
}

const ek_symbol *r_module_member(const r_mod *m, const char *name)
{
    size_t i;

    if (!m || !m->surf) {
        return NULL;
    }
    for (i = 0; i < m->surf->count; i++) {
        const ek_symbol *s = &m->surf->syms[i];

        /* tests and namespace members are not module surface names
           (Clauses 7.3.2, 7.4.4) */
        if (s->kind != EK_SYM_TEST && s->kind != EK_SYM_NAMESPACE
            && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static const ek_symbol *namespace_member(const r_mod *m, const char *type,
                                         const char *name)
{
    size_t i;

    if (!m || !m->surf) {
        return NULL;
    }
    for (i = 0; i < m->surf->count; i++) {
        const ek_symbol *s = &m->surf->syms[i];

        if (s->kind == EK_SYM_NAMESPACE && s->owner
            && strcmp(s->owner, type) == 0 && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static void external_link(r_ctx *c, r_head *cur, const ek_node *link)
{
    cur->sym = NULL;
    cur->kind = EK_SYM_EXTERNAL;
    cur->st = EK_ST_EXTERNAL;
    r_add_ref(c, link, cur->kind, cur->st, NULL);
}

static void member_link(r_ctx *c, r_head *cur, const ek_node *link, int last,
                        int is_call)
{
    cur->sym = NULL;
    cur->kind = EK_SYM_MEMBER;
    cur->st = last ? (is_call ? EK_ST_RECEIVER : EK_ST_FIELD)
                   : EK_ST_RECEIVER;
    r_add_ref(c, link, cur->kind, cur->st, NULL);
}

/* Pascal chain head: the type-namespace reading (Clause 7.4.1). */
static void type_head(r_ctx *c, r_file *f, const ek_node *head, r_head *cur,
                      int *state, const ek_symbol **walk_type)
{
    const char *name = head->u.path.data[0];
    const ek_symbol *first = NULL;
    size_t k = r_lookup_type(f, name, &first);

    if (k > 1) {
        r_error(c, f->mod->file, head->line, head->col, "E-name-conflict",
                "`%s` names several visible types; qualify one "
                "(Clauses 7.4.5, 7.4.6)", name);
        cur->kind = EK_SYM_EXTERNAL;
    } else if (k == 1) {
        cur->sym = first;
        cur->kind = EK_SYM_TYPE;
        cur->st = EK_ST_QUALIFIED;
        *state = 2;
        *walk_type = first;
    } else {
        cur->sym = r_std_type(c, name);
        cur->kind = EK_SYM_EXTERNAL;
        cur->st = EK_ST_EXTERNAL;
    }
    r_add_ref(c, head, cur->kind, cur->st, cur->sym);
}

/* Station 4 from a module segment, or the qualified walk of 7.4.4. */
static int module_walk(r_ctx *c, r_file *f, r_head *cur, const ek_node *link,
                       const char *name, int last, int from_receiver,
                       r_mod **walk_mod, const ek_symbol **walk_type,
                       int *state)
{
    const ek_symbol *m;

    if (!*walk_mod || !(*walk_mod)->surf) {
        external_link(c, cur, link); /* the std placeholder bridge */
        *state = 0;
        *walk_mod = NULL;
        return 1;
    }
    m = r_module_member(*walk_mod, name);
    if (!m) {
        r_error(c, f->mod->file, link->line, link->col, "E-unbound-member",
                "module `%s` declares no member `%s` (Clause 7.4.4)",
                (*walk_mod)->stem, name);
        external_link(c, cur, link);
        *state = 0;
        *walk_mod = NULL;
        return 1;
    }
    if (!m->public) {
        r_error(c, f->mod->file, link->line, link->col, "E-private-name",
                "member `%s` is private in module `%s` (Clause 7.3.1)",
                name, (*walk_mod)->stem);
    }
    cur->sym = m;
    cur->kind = m->kind;
    cur->st = from_receiver ? EK_ST_MODULE_PATH : EK_ST_QUALIFIED;
    r_add_ref(c, link, cur->kind, cur->st, cur->sym);
    if (m->kind == EK_SYM_TYPE && !last) {
        *state = 2; /* Mod.Type.member: the namespace of the type */
        *walk_type = m;
        return 1;
    }
    *state = 0;
    *walk_mod = NULL;
    return 1;
}

static int namespace_walk(r_ctx *c, r_file *f, r_head *cur,
                          const ek_node *link, const char *name,
                          const ek_symbol *type, r_mod *owner, int *state)
{
    const ek_symbol *nm = namespace_member(owner, type->name, name);

    if (nm) {
        if (!nm->public) {
            r_error(c, f->mod->file, link->line, link->col,
                    "E-private-name",
                    "namespace member `%s.%s` is private (Clause 7.3.1)",
                    type->name, name);
        }
        cur->sym = nm;
        cur->kind = EK_SYM_NAMESPACE;
        cur->st = EK_ST_QUALIFIED;
    } else {
        /* Derived services attach as defaults (Clause 9.8.7); check
           owns them. */
        external_link(c, cur, link);
    }
    r_add_ref(c, link, cur->kind, cur->st, cur->sym);
    *state = 0;
    return 1;
}

/* The chain head: a snake name resolves at the expression head (a
   module continues the qualified walk), a Pascal name takes the
   type-namespace reading, anything else is an ordinary expression. */
static void chain_head(r_ctx *c, r_file *f, const ek_node *head, r_head *cur,
                       int *state, r_mod **walk_mod,
                       const ek_symbol **walk_type)
{
    if (!head) {
        return;
    }
    if (head->kind != EK_N_NAME || head->u.path.count != 1
        || strcmp(head->u.path.data[0], "_") == 0) {
        r_expr(c, f, head);
        return;
    }
    if (r_is_snake(head->u.path.data[0])) {
        r_name_head(c, f, head, 1, cur);
        if (cur->kind == EK_SYM_MODULE) {
            *state = 1;
            *walk_mod = r_module_find(c, cur->sym->name);
        }
        return;
    }
    type_head(c, f, head, cur, state, walk_type);
    if (cur->kind == EK_SYM_TYPE) {
        *walk_mod = r_module_find(c, cur->sym->module);
    }
}

/* The walk over a chain's links: module segments, namespace members,
   and receiver-form members (Clause 7.4.3). */
static void walk_links(r_ctx *c, r_file *f, const r_links *l, int is_call,
                       r_head *cur, int *state, r_mod **walk_mod,
                       const ek_symbol **walk_type, int *from_receiver)
{
    size_t i;

    for (i = 0; i < (size_t)l->n && !c->oom; i++) {
        const ek_node *link = l->links[i];
        const char *name = l->names[i];
        int last = (int)(i + 1) == l->n;
        const ek_symbol *msym;

        if (*state == 1) {
            module_walk(c, f, cur, link, name, last, *from_receiver,
                        walk_mod, walk_type, state);
        } else if (*state == 2) {
            namespace_walk(c, f, cur, link, name, *walk_type, *walk_mod,
                           state);
        } else if (!last && (msym = r_lookup_mod(f, name)) != NULL) {
            /* The module segment of station 4 (Clause 7.4.3); a field
               of the same name would win at station 1, which check
               decides from the receiver type. */
            cur->sym = msym;
            cur->kind = EK_SYM_MODULE;
            cur->st = EK_ST_MODULE_PATH;
            r_add_ref(c, link, cur->kind, cur->st, cur->sym);
            *state = 1;
            *walk_mod = r_module_find(c, msym->name);
            *from_receiver = 1;
        } else {
            member_link(c, cur, link, last, is_call);
        }
    }
}

void r_chain(r_ctx *c, r_file *f, const ek_node *node, int is_call,
             r_head *out)
{
    r_links l;
    r_head cur;
    r_mod *walk_mod = NULL;
    const ek_symbol *walk_type = NULL;
    int state = 0, from_receiver = 0;

    memset(out, 0, sizeof *out);
    if (node->kind == EK_N_CALL) {
        const ek_node *callee = node->u.call.callee;
        size_t k;

        for (k = 0; k < node->u.call.args.count; k++) {
            r_expr(c, f, node->u.call.args.data[k]);
        }
        if (callee && callee->kind == EK_N_NAME) {
            r_name_head(c, f, callee, 0, &cur);
            r_add_ref(c, node, cur.kind, cur.st, cur.sym);
            *out = cur;
            return;
        }
        if (!callee || callee->kind != EK_N_FIELD) {
            r_expr(c, f, callee);
            out->kind = EK_SYM_MEMBER;
            out->st = EK_ST_RECEIVER;
            r_add_ref(c, node, EK_SYM_MEMBER, EK_ST_RECEIVER, NULL);
            return;
        }
        flatten(callee, &l);
    } else {
        flatten(node, &l);
    }
    memset(&cur, 0, sizeof cur);
    chain_head(c, f, l.head, &cur, &state, &walk_mod, &walk_type);
    walk_links(c, f, &l, is_call, &cur, &state, &walk_mod, &walk_type,
               &from_receiver);
    *out = cur;
    if (node->kind == EK_N_CALL) {
        r_add_ref(c, node, cur.kind, cur.st, cur.sym);
    }
}
