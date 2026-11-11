#include <stdio.h>
#include <string.h>

#include "cli/internal.h"

/* The answer side of the position query: the tables' lookups, the
   definition site, the signature line, the world bit, and the document
   the index holds. The query side walks the candidates; this side
   names what the position denotes. */

/* The check row of one node; NULL when the node carries no row. */
const ek_ck_row *at_row_of(const ek_check_result *chk,
                                  const ek_node *n)
{
    size_t i;

    for (i = 0; i < chk->count; i++) {
        if (chk->rows[i].node == n) {
            return &chk->rows[i];
        }
    }
    return NULL;
}

const char *at_bit_name(ek_bit b)
{
    switch (b) {
    case EK_BIT_WORLD:
        return "world-touching";
    case EK_BIT_OPEN:
        return "open";
    case EK_BIT_PURE:
        return "pure";
    default:
        return "unset";
    }
}

static const char *at_kind_name(ek_sym_kind k)
{
    /* In declaration order of ek_sym_kind. */
    static const char *const names[] = {
        "parameter", "local binding", "function", "type", "constructor",
        "module", "namespace member", "type variable", "test", "extern",
        "member link", "external"
    };

    return k < sizeof names / sizeof *names ? names[k] : "name";
}

/* One source line read back for the signature, on the arena; NULL when
   the file or the line is not there, which is the compiler-provided
   row's shape. The whole file is read so no line length bounds the
   answer. */
static const char *at_source_line(ek_arena *arena, const char *path,
                                  int line)
{
    ek_buf b;
    const char *p;
    const char *e;
    int cur = 0;
    char *out;

    if (!path || line <= 0) {
        return NULL;
    }
    ek_buf_init(&b);
    if (!cli_read_source(&b, path)) {
        ek_buf_destroy(&b);
        return NULL;
    }
    p = b.data;
    while (p && *p) {
        cur++;
        e = strchr(p, '\n');
        if (cur == line) {
            size_t n = e ? (size_t)(e - p) : strlen(p);

            while (n > 0 && (p[n - 1] == '\n' || p[n - 1] == '\r')) {
                n--;
            }
            out = ek_arena_alloc(arena, n + 1);
            if (out) {
                memcpy(out, p, n);
                out[n] = '\0';
            }
            ek_buf_destroy(&b);
            return out;
        }
        p = e ? e + 1 : NULL;
    }
    ek_buf_destroy(&b);
    return NULL;
}

/* The type a check row observes, with inference variables read
   through their bindings. */
static const ek_type *at_observed(const ek_type *t)
{
    while (t && t->kind == EK_T_IVAR && t->u.ivar.bound) {
        t = t->u.ivar.bound;
    }
    return t;
}

/* The namespace member one type's surface declares under owner and
   leaf name; NULL when no member answers. */
static const ek_symbol *at_member_in_surf(const ek_resolve_result *res,
                                          const ek_symbol *tsym,
                                          const char *leaf,
                                          const ek_surface **out_surf)
{
    size_t m, i;

    for (m = 0; m < res->module_count; m++) {
        const ek_surface *s = &res->modules[m];

        if (!s->module || strcmp(s->module, tsym->module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *sym = &s->syms[i];

            if (sym->kind == EK_SYM_NAMESPACE && sym->owner
                && strcmp(sym->owner, tsym->name) == 0 && sym->name
                && strcmp(sym->name, leaf) == 0) {
                *out_surf = s;
                return sym;
            }
        }
    }
    return NULL;
}
/* The namespace member a receiver link names: the receiver's row type
   names the declaring type, whose module surface holds the member.
   ts receives the receiver type's symbol even on a miss, so the caller
   can name the type a compiler-provided member belongs to. */
const ek_symbol *at_member_def(const ek_resolve_result *res,
                                      const ek_check_result *chk,
                                      const ek_ref *ref,
                                      const ek_surface **out_surf,
                                      const ek_symbol **ts)
{
    const ek_node *recv;
    const ek_node *fld;
    const ek_ck_row *rr;
    const ek_type *t;
    const ek_symbol *tsym;

    *ts = NULL;
    /* The link's ref sits on the call; the field beneath it carries
       the receiver and the member name. */
    fld = ref->node->kind == EK_N_CALL ? ref->node->u.call.callee
                                       : ref->node;
    if (!fld || fld->kind != EK_N_FIELD) {
        return NULL;
    }
    recv = fld->u.field.recv;
    rr = at_row_of(chk, recv);
    if (!rr || !rr->type) {
        return NULL;
    }
    t = at_observed(rr->type);
    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym) {
        return NULL;
    }
    tsym = t->u.nominal.sym;
    *ts = tsym;
    return at_member_in_surf(res, tsym, fld->u.field.name, out_surf);
}

/* The surface of the module a symbol declares in. */
const ek_surface *at_surf_of(const ek_resolve_result *res,
                                    const ek_symbol *sym)
{
    size_t m;

    for (m = 0; m < res->module_count; m++) {
        if (res->modules[m].module
            && strcmp(res->modules[m].module, sym->module) == 0) {
            return &res->modules[m];
        }
    }
    return NULL;
}

/* One definition's answer: the site, the signature line, one world
   bit -- the definition's declared bit when its row carries one, the
   occurrence's bit otherwise -- and the document the index holds. An
   extern's bit is its own declaration; a compiler-provided member has
   no signature line to read back. */
void at_print_sym(ek_arena *arena, const char *root,
                         const ek_surface *surf, const ek_symbol *sym,
                         const ek_check_result *chk, ek_bit occ)
{
    const char *line = sym->line > 0
        ? at_source_line(arena, surf->file, sym->line) : NULL;
    const ek_ck_row *row;
    ek_bit b;

    if (sym->line <= 0) {
        /* A placeholder symbol names no source site. */
        printf("%s: %s %s (compiler-provided; Clause 9.1.1)\n",
               surf->file, at_kind_name(sym->kind), sym->name);
        printf("world bit: %s\n", at_bit_name(occ));
        return;
    }
    printf("%s:%d:%d: %s %s\n", surf->file, sym->line, sym->col,
           at_kind_name(sym->kind), sym->name);
    if (!line && sym->module) {
        char path[4096];

        if (cli_shipped_path(root, sym->module, path, sizeof path)) {
            line = at_source_line(arena, path, sym->line);
        }
    }
    if (line) {
        printf("signature: %s\n", line);
    } else {
        printf("signature: compiler-provided (Clause 9.1.1)\n");
    }
    row = sym->decl ? at_row_of(chk, sym->decl) : NULL;
    b = row ? row->bit : EK_BIT_UNSET;
    if (b == EK_BIT_UNSET && sym->kind == EK_SYM_EXTERN) {
        b = sym->pure ? EK_BIT_PURE : EK_BIT_WORLD;
    }
    printf("world bit: %s\n", at_bit_name(b != EK_BIT_UNSET ? b : occ));
    if (sym->doc) {
        const char *p = sym->doc;

        while (*p) {
            const char *e = strchr(p, '\n');
            size_t n = e ? (size_t)(e - p) : strlen(p);

            printf("doc: %.*s\n", (int)n, p);
            p = e ? e + 1 : p + n;
        }
    }
}
