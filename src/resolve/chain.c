#include <string.h>

#include "resolve/internal.h"

/* Dotted references and receiver chains (parse.h representation
   notes). One walk classifies every link:

   - a non-last link spelled like an imported or the current module is
     a module segment, and the chain continues as a module-path access;
   - a snake head carrying a binding value, and every other
     non-module link, is a receiver-form member: the receiver position
     never consults lexical bindings, and the field, namespace, or
     current-module reading needs the receiver's static type, so check
     selects it (EK_SYM_MEMBER);
   - a snake head carrying a module continues as a qualified access
     `Mod.f`;
   - a Pascal head is a type namespace; its members live in the
     declaring module's surface, and unknown members defer to check
     (derived services).
*/

/* One link of a chain: the field node and the name it spells. */
typedef struct {
    const ek_node *node;
    const char *name;
} r_link;

typedef struct {
    const ek_node *head;
    r_link *links; /* head-first */
    size_t n, cap;
} r_links;

static void reverse_links(r_link *links, size_t n)
{
    size_t i;

    for (i = 0; i < n / 2; i++) {
        r_link tmp = links[i];

        links[i] = links[n - 1 - i];
        links[n - 1 - i] = tmp;
    }
}

static void flatten(r_ctx *c, const ek_node *top, r_links *l)
{
    const ek_node *p = top;

    memset(l, 0, sizeof *l);
    while (p && p->kind == EK_N_FIELD) {
        l->links = r_grow(c, l->links, l->n, &l->cap,
                          sizeof *l->links);
        if (c->oom) {
            break;
        }
        l->links[l->n].node = p;
        l->links[l->n].name = p->u.field.name;
        l->n++;
        p = p->u.field.recv;
    }
    l->head = p;
    reverse_links(l->links, l->n);
}

static void member_link(r_ctx *c, r_file *f, r_head *cur, const ek_node *link,
                        int last, int is_call)
{
    cur->sym = NULL;
    cur->kind = EK_SYM_MEMBER;
    cur->st = last ? (is_call ? EK_ST_RECEIVER : EK_ST_FIELD)
                   : EK_ST_RECEIVER;
    r_add_ref(c, link, cur->kind, cur->st, NULL);
    if (last && is_call) {
        /* The current module may answer this call with a free function;
           the name is used whichever reading check selects. */
        const ek_symbol *fn = r_module_member(f->mod, link->u.field.name);

        if (fn && fn->kind == EK_SYM_FUNC) {
            const ek_symbol **grown = r_grow(c, c->forced, c->nforced,
                                             &c->cforced, sizeof *c->forced);

            if (grown) {
                grown[c->nforced++] = fn;
                c->forced = grown;
            }
        }
    }
}

/* Pascal chain head: the type-namespace reading. */
static void type_head(r_ctx *c, r_file *f, const ek_node *head, r_head *cur,
                      int *state, const ek_symbol **walk_type)
{
    const char *name = head->u.path.data[0];
    const ek_symbol *first = NULL;
    size_t k;

    if (strcmp(name, "Never") == 0) {
        /* The language's bottom type names no namespace and no
           value. */
        r_error(c, f->mod->file, head->line, head->col, "E-unbound-member",
                "`Never` is a type of the language; it names no namespace "
                "(Clause 7.5.4)");
        cur->kind = EK_SYM_EXTERNAL;
        cur->st = EK_ST_EXTERNAL;
        r_add_ref(c, head, cur->kind, cur->st, NULL);
        return;
    }
    k = r_lookup_type(f, name, &first);

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
        /* Unknown to the file and to std: check owns the members of
           an external head. */
        cur->kind = EK_SYM_EXTERNAL;
        cur->st = EK_ST_EXTERNAL;
    }
    r_add_ref(c, head, cur->kind, cur->st, cur->sym);
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
            /* The C namespace head carries no module symbol; its
               members bridge. */
            *state = 1;
            *walk_mod = cur->sym ? r_module_find(c, cur->sym->name) : NULL;
        }
        return;
    }
    type_head(c, f, head, cur, state, walk_type);
    if (cur->kind == EK_SYM_TYPE) {
        *walk_mod = r_module_find(c, cur->sym->module);
    }
}

/* The walk over a chain's links: module segments, namespace members,
   and receiver-form members. */
static void walk_links(r_ctx *c, r_file *f, const r_links *l, int is_call,
                       r_head *cur, int *state, r_mod **walk_mod,
                       const ek_symbol **walk_type, int *from_receiver)
{
    size_t i;

    for (i = 0; i < l->n && !c->oom; i++) {
        const ek_node *link = l->links[i].node;
        const char *name = l->links[i].name;
        int last = i + 1 == l->n;
        const ek_symbol *msym;

        if (*state == 1) {
            r_module_walk(c, f, cur, link, name, last, *from_receiver,
                          walk_mod, walk_type, state);
        } else if (*state == 2) {
            r_namespace_walk(c, f, cur, link, name, *walk_type, *walk_mod,
                             *from_receiver, state);
        } else if (!last && (msym = r_lookup_mod(f, name)) != NULL) {
            /* A module segment inside the chain; a field of the same
               name would win at the receiver reading, which check
               decides from the receiver type. */
            cur->sym = msym;
            cur->kind = EK_SYM_MODULE;
            cur->st = EK_ST_MODULE_PATH;
            r_add_ref(c, link, cur->kind, cur->st, cur->sym);
            *state = 1;
            *walk_mod = r_module_find(c, msym->name);
            *from_receiver = 1;
        } else {
            member_link(c, f, cur, link, last, is_call);
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
        flatten(c, callee, &l);
    } else {
        flatten(c, node, &l);
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
