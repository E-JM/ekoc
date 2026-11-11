#include <stdio.h>
#include <string.h>

#include "cli/internal.h"

/* The position query: the signature and the world bit of what a source
   position denotes. The query walks the root module's slice of the
   resolver's reference table for the innermost occurrence at or before
   the position and the root surface for the declaration it names; a
   member link resolves through the receiver's namespace row of the
   checked AST. The answer reads the tables the front already built, so
   a query and a check see the same program. */

/* The candidates one query line yields. */
typedef struct {
    const ek_ref *ref;      /* the innermost occurrence at the position */
    const ek_ref *runner;   /* the reading the innermost displaced */
    const ek_symbol *decl;  /* the declaration the position sits on */
    const ek_surface *surf; /* the declaring surface of decl */
    int ref_col;
    int decl_col;
} at_candidates;

/* The innermost root-file reference at or before the position: the
   largest start column wins; among equals a member link outranks a
   plain name, and a name that carries its definition outranks one
   that does not. */
static void at_scan_refs(const ek_resolve_result *res, int qline, int qcol,
                         at_candidates *ct)
{
    size_t i;

    for (i = res->root_refs[0]; i < res->root_refs[1]; i++) {
        const ek_ref *r = &res->refs[i];
        const ek_node *n = r->node;

        if (!n || n->line != qline || n->col <= 0 || n->col > qcol) {
            continue;
        }
        if (!ct->ref || n->col > ct->ref_col
            || (n->col == ct->ref_col
                && (r->kind == EK_SYM_MEMBER
                    || (ct->ref->kind != EK_SYM_MEMBER
                        && r->symbol != NULL)))) {
            if (ct->ref && ct->ref != r) {
                ct->runner = ct->ref;
            }
            ct->ref = r;
            ct->ref_col = n->col;
        }
    }
}

/* The declaration the position sits on, on the queried file's own
   surface; the innermost wins. */
static void at_scan_syms(const ek_resolve_result *res, int qline, int qcol,
                         at_candidates *ct)
{
    const ek_surface *s = &res->modules[0];
    size_t i;

    for (i = 0; i < s->count; i++) {
        const ek_symbol *sym = &s->syms[i];

        if (sym->line != qline || sym->col <= 0 || sym->col > qcol) {
            continue;
        }
        if (!ct->decl || sym->col > ct->decl_col) {
            ct->decl = sym;
            ct->surf = s;
            ct->decl_col = sym->col;
        }
    }
}

/* The answer for one member link. Returns 1 when the answer printed: a
   compiler-provided member has no surface row to name, so the receiver
   type and the reading answer the query. */
static int at_member_answer(const ek_resolve_result *res,
                            const ek_check_result *chk, const ek_ref *ref,
                            const ek_ck_row *row, const ek_symbol **sym,
                            const ek_surface **surf)
{
    const ek_symbol *ts = NULL;
    const ek_node *fld = ref->node->kind == EK_N_CALL
                             ? ref->node->u.call.callee : ref->node;

    *sym = at_member_def(res, chk, ref, surf, &ts);
    if (*sym || !ts) {
        return 0;
    }
    printf("%s:%d:%d: member `%s` of type `%s` "
           "(compiler-provided; Clause 9.1.1)\n", res->modules[0].file,
           ref->node->line, ref->node->col,
           fld && fld->kind == EK_N_FIELD ? fld->u.field.name : "?",
           ts->name);
    printf("world bit: %s\n", at_bit_name(row ? row->bit : EK_BIT_UNSET));
    return 1;
}

/* The answer for the innermost occurrence: its definition when the
   index names one, the displaced reading under a member link next, the
   bare occurrence otherwise. Returns 0 when the position was answered. */
static int at_ref_answer(const ek_resolve_result *res,
                         const ek_check_result *chk, ek_arena *arena,
                         const char *root, const at_candidates *ct)
{
    const ek_ref *ref = ct->ref;
    const ek_symbol *sym = ref->symbol;
    const ek_surface *surf = NULL;
    const ek_ck_row *row = at_row_of(chk, ref->node);
    const ek_ck_row *drow;
    ek_bit occ = row ? row->bit : EK_BIT_UNSET;

    if (!sym && ref->kind == EK_SYM_MEMBER
        && ref->station == EK_ST_RECEIVER
        && at_member_answer(res, chk, ref, row, &sym, &surf)) {
        return 0;
    }
    drow = row && row->reading == EK_RD_DERIVED
        ? row : (ct->runner ? at_row_of(chk, ct->runner->node) : NULL);
    if (!sym && drow && drow->reading == EK_RD_DERIVED) {
        /* A derived service's name carries no symbol: the link's
           reading and the bit answer the query. */
        printf("%s:%d:%d: a derived service (compiler-provided; "
               "Clause 9.8)\n", res->modules[0].file, ref->node->line,
               ref->node->col);
        printf("world bit: %s\n",
               at_bit_name(drow->bit != EK_BIT_UNSET ? drow->bit : occ));
        return 0;
    }
    if (!sym && ct->runner) {
        /* The member link answered nothing: the reading it displaced
           is the next reading of the position. */
        sym = ct->runner->symbol;
        row = at_row_of(chk, ct->runner->node);
        occ = row ? row->bit : EK_BIT_UNSET;
    }
    if (!sym) {
        printf("%s:%d:%d: an occurrence the index holds no definition "
               "for\n", res->modules[0].file, ref->node->line,
               ref->node->col);
        printf("world bit: %s\n", at_bit_name(occ));
        return 1;
    }
    if (!surf) {
        surf = at_surf_of(res, sym);
    }
    at_print_sym(arena, root, surf ? surf : &res->modules[0], sym, chk,
                 occ);
    return 0;
}

/* One query line's answer: the innermost candidate wins; a declaration
   at the same column is the more specific answer than the occurrence
   over it. Returns 0 when the position named something. */
static int at_answer(const ek_resolve_result *res,
                     const ek_check_result *chk, ek_arena *arena,
                     const char *root, int qline, int qcol)
{
    at_candidates ct;

    memset(&ct, 0, sizeof ct);
    at_scan_refs(res, qline, qcol, &ct);
    at_scan_syms(res, qline, qcol, &ct);
    if (ct.decl && (!ct.ref || ct.decl_col >= ct.ref_col)) {
        at_print_sym(arena, root, ct.surf, ct.decl, chk, EK_BIT_UNSET);
        return 0;
    }
    if (ct.ref) {
        return at_ref_answer(res, chk, arena, root, &ct);
    }
    printf("%s:%d:%d: no reference or declaration\n",
           res->modules[0].file, qline, qcol);
    return 1;
}

int cli_at(const char *root, const char *file, int line, int col)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();
    cli_diag_log log;
    ek_buf source;
    ek_resolve_result res;
    ek_check_result chk;
    int status;

    memset(&log, 0, sizeof log);
    memset(&res, 0, sizeof res);
    memset(&chk, 0, sizeof chk);
    ek_buf_init(&source);
    if (arena == NULL || diag == NULL) {
        fprintf(stderr, "ekoc: out of memory\n");
        return 2;
    }
    log.arena = arena;
    if (!cli_read_source(&source, file)) {
        fprintf(stderr, "ekoc: cannot read %s\n", file);
        ek_buf_destroy(&source);
        ek_diag_destroy(diag);
        ek_arena_destroy(arena);
        return 2;
    }
    cli_resolve(&log, arena, diag, file, cli_dir_of(arena, file),
                source.data != NULL ? source.data : "", source.len,
                &res, &chk);
    cli_log_drain(&log, diag);
    cli_log_print(&log);
    status = cli_log_errors(&log) != 0 ? 2 : 0;
    if (status == 0
        && at_answer(&res, &chk, arena, root, line, col) != 0) {
        status = 2;
    }
    ek_buf_destroy(&source);
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
    return status;
}
