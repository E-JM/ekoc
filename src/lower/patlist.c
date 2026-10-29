#include <string.h>

#include "lower/internal.h"

/* The structural pattern walks: records and tuples read their struct,
   lists walk cons cells, and or-patterns join their alternatives. The
   plan helpers and the walk dispatch they recurse into carry the
   lower_pat prefix. */

/* A record pattern: the fields bind and test through the struct; the
   shorthand item is the field's own binding. */
static void pat_record(lower_pat *pat, const ek_node *p, const char *slot,
                       lower_gtype *g)
{
    size_t i, k;

    for (i = 0; i < p->u.p_brace.items.count; i++) {
        const ek_node *item = p->u.p_brace.items.data[i];
        const ek_node *sub = NULL;
        const char *field;
        const char *sub_slot;

        if (item->kind == EK_N_P_FIELD) {
            field = item->u.p_field.name;
            sub = item->u.p_field.pattern;
        } else if (item->kind == EK_N_P_VAR && item->u.path.count == 1) {
            field = item->u.path.data[0];
            sub = item;
        } else {
            continue;
        }
        for (k = 0; k < g->nf; k++) {
            if (strcmp(g->fnames[k], field) == 0) {
                break;
            }
        }
        if (k >= g->nf) {
            continue; /* check diagnosed the missing field */
        }
        sub_slot = lower_str(pat->c, "((ekS_%s *)%s.as.ref)->f_%s",
                             g->stem, slot, field);
        if (sub == item) {
            lower_pat_group_item(pat, field, sub_slot);
        } else {
            lower_pat_step(pat, sub, sub_slot);
        }
    }
}

/* A tuple pattern: the components map by position. */
static void pat_tuple(lower_pat *pat, const ek_node *p, const char *slot,
                      lower_gtype *g)
{
    size_t i;

    for (i = 0; i < p->u.p_brace.items.count && i < g->nf; i++) {
        lower_pat_step(pat, p->u.p_brace.items.data[i],
                 lower_str(pat->c, "((ekS_%s *)%s.as.ref)->f_%zu",
                           g->stem, slot, i));
    }
}

/* The fixed tail of a prefix rest keeps each element into its own
   temp as the walk passes it. */
static void pat_tail_keeps(lower_pat *pat, size_t k, const char *wn,
                           const char **ht)
{
    size_t j;

    if (k == 0) {
        lower_pat_line(pat, "        ek_release(h);");
        return;
    }
    for (j = 0; j < k; j++) {
        lower_pat_line(pat, "%sif (wi + %zu == %s && %s >= %zu) {",
                       j ? "        } else " : "        ", k - j, wn, wn, k);
        lower_pat_line(pat, "            %s = h;", ht[j]);
    }
    lower_pat_line(pat, "        } else {");
    lower_pat_line(pat, "            ek_release(h);");
    lower_pat_line(pat, "        }");
}

/* The second pass of a prefix rest: the prefix rebuilds while the
   fixed tail keeps into per-item temps. */
static void pat_prefix_walk(lower_pat *pat, const ek_node *p,
                            const char *slot, const char *wn,
                            const char **ht, size_t k)
{
    const char *wp = lower_temp(pat->c, pat->scope);
    const char *wc = lower_temp(pat->c, pat->scope);
    size_t j;

    lower_pat_line(pat, "ek_val %s = ek_nil_val();", wp);
    lower_pat_line(pat, "ek_val %s = ek_keep(%s);", wc, slot);
    lower_pat_line(pat, "size_t wi = 0;");
    lower_pat_line(pat, "while (wi < %s) {", wn);
    lower_pat_line(pat, "    ek_val h = ek_keep(ek_cons_head(%s.as.ref));",
                   wc);
    lower_pat_line(pat, "    if (wi + %zu < %s) {", k, wn);
    if (p->u.p_brace.rest_name) {
        lower_pat_line(pat, "        ek_val cell = ek_cons(h, %s);", wp);
        lower_pat_line(pat, "        ek_release(h);");
        lower_pat_line(pat, "        ek_release(%s);", wp);
        lower_pat_line(pat, "        %s = cell;", wp);
    } else {
        lower_pat_line(pat, "        ek_release(h);");
    }
    lower_pat_line(pat, "    } else {");
    pat_tail_keeps(pat, k, wn, ht);
    lower_pat_line(pat, "    }");
    lower_pat_line(pat, "    ek_val nt = ek_keep(ek_cons_tail(%s.as.ref));",
                   wc);
    lower_pat_line(pat, "    ek_release(%s);", wc);
    lower_pat_line(pat, "    %s = nt;", wc);
    lower_pat_line(pat, "    wi = wi + 1;");
    lower_pat_line(pat, "}");
    lower_pat_line(pat, "ek_release(%s);", wc);
    for (j = 0; j < k; j++) {
        lower_pat_step(pat, p->u.p_brace.items.data[j], ht[j]);
    }
    if (p->u.p_brace.rest_name) {
        lower_clean_push(pat->c, wp);
        lower_pat_group_item(pat, p->u.p_brace.rest_name, wp);
    }
}

/* The first pass of a prefix rest: the cell count. */
static void pat_prefix_count(lower_pat *pat, const char *slot,
                             const char *w0, const char *wn)
{
    lower_pat_line(pat, "ek_val %s = ek_keep(%s);", w0, slot);
    lower_pat_line(pat, "size_t %s = 0;", wn);
    lower_pat_line(pat, "while (%s) {", lower_pat_is_cons(pat->c, w0));
    lower_pat_line(pat, "    ek_val nt = ek_keep(ek_cons_tail(%s.as.ref));",
                   w0);
    lower_pat_line(pat, "    ek_release(%s);", w0);
    lower_pat_line(pat, "    %s = nt;", w0);
    lower_pat_line(pat, "    %s = %s + 1;", wn, wn);
    lower_pat_line(pat, "}");
    lower_pat_line(pat, "ek_release(%s);", w0);
}

/* A list pattern with a prefix rest: the walk counts the cells, the
   match needs at least the fixed ones, and the second pass rebuilds
   the prefix while the fixed tail keeps into per-item temps. */
static void pat_list_prefix(lower_pat *pat, const ek_node *p,
                            const char *slot)
{
    lower_ctx *c = pat->c;
    size_t k = p->u.p_brace.items.count;
    const char *w0 = lower_temp(c, pat->scope);
    const char *wn = lower_temp(c, pat->scope);
    const char **ht;
    size_t cap = 0;
    size_t j;

    ht = lower_grow(c, NULL, &cap, k ? k : 1, sizeof *ht);
    if (!ht) {
        pat->failed = 1;
        return;
    }
    pat_prefix_count(pat, slot, w0, wn);
    lower_pat_cond(pat, lower_str(c, "%s >= %zu", wn, k));
    for (j = 0; j < k; j++) {
        ht[j] = lower_temp(c, pat->scope);
        lower_pat_line(pat, "ek_val %s = ek_val_unit();", ht[j]);
        lower_clean_push(c, ht[j]);
    }
    pat_prefix_walk(pat, p, slot, wn, ht, k);
}

/* A list pattern: one cons test per fixed element, an exact-length
   test without a rest, and the rest binding takes the walked
   remainder. */
static void pat_list(lower_pat *pat, const ek_node *p, const char *slot)
{
    lower_ctx *c = pat->c;
    size_t k = p->u.p_brace.items.count;
    const char *cur = slot;
    size_t i;

    if (p->u.p_brace.rest == EK_REST_PREFIX) {
        pat_list_prefix(pat, p, slot);
        return;
    }
    for (i = 0; i < k; i++) {
        lower_pat_cond(pat, lower_pat_is_cons(c, cur));
        lower_pat_step(pat, p->u.p_brace.items.data[i], lower_pat_slot_head(c, cur));
        cur = lower_pat_slot_tail(c, cur);
    }
    if (p->u.p_brace.rest == EK_REST_NONE) {
        lower_pat_cond(pat, lower_str(c, "ek_val_is_nil(%s)", cur));
    }
    if (p->u.p_brace.rest_name) {
        lower_pat_group_item(pat, p->u.p_brace.rest_name, cur);
    }
}

/* The brace family from the pattern row: a list walks, a record or a
   tuple reads its struct, and a declared type goes through its
   representation. */
void lower_pat_brace(lower_pat *pat, const ek_node *p, const char *slot)
{
    lower_ctx *c = pat->c;
    const ek_ck_row *row = lower_row(c, p);
    const ek_type *t = row ? row->type : NULL;
    lower_gtype *g = NULL;

    if (t && t->kind == EK_T_LIST) {
        pat_list(pat, p, slot);
        return;
    }
    if (t && t->kind == EK_T_TUPLE) {
        g = lower_anon_tuple(c, t->u.tuple.count);
    } else if (t && t->kind == EK_T_RECORD) {
        g = lower_anon_record(c, t);
    } else if (t && t->kind == EK_T_NOMINAL && t->u.nominal.sym) {
        g = lower_decl_repr(c, t->u.nominal.sym);
    }
    if (!g) {
        lower_error(c, p, "the brace pattern has no lowering");
        pat->failed = 1;
        return;
    }
    if (g->shape == GT_REC) {
        pat_record(pat, p, slot, g);
    } else {
        pat_tuple(pat, p, slot, g);
    }
}

/* The disjunction one or-pattern contributes: each alternative's own
   conjunction, joined with ||. */
static void pat_or_cond(lower_pat *pat, size_t g0)
{
    lower_ctx *c = pat->c;
    ek_buf any;
    size_t i, j;

    ek_buf_init(&any);
    for (i = g0; i < pat->ngroup; i++) {
        if (i > g0 && !ek_buf_append_str(&any, " || ")) {
            c->oom = 1;
        }
        if (pat->groups[i].ncond == 0) {
            if (!ek_buf_append_str(&any, "1")) {
                c->oom = 1;
            }
            continue;
        }
        if (pat->groups[i].ncond > 1 && !ek_buf_append_char(&any, '(')) {
            c->oom = 1;
        }
        for (j = 0; j < pat->groups[i].ncond; j++) {
            if (j && !ek_buf_append_str(&any, " && ")) {
                c->oom = 1;
            }
            if (!ek_buf_append_str(&any, pat->groups[i].conds[j])) {
                c->oom = 1;
            }
        }
        if (pat->groups[i].ncond > 1 && !ek_buf_append_char(&any, ')')) {
            c->oom = 1;
        }
    }
    lower_pat_cond(pat, lower_take(c, &any));
}

/* One alternative's conjuncts become that alternative's own
   condition; a statement test inside an alternative has no lowering. */
static int pat_or_alt(lower_pat *pat, const ek_node *alt, const char *slot,
                      size_t mark, unsigned saved_flush, size_t *alts)
{
    lower_pat_group *g;
    size_t n;

    lower_pat_new_group(pat);
    *alts = pat->cur;
    lower_pat_step(pat, alt, slot);
    if (pat->nflush != saved_flush) {
        lower_error(pat->c, alt, "the or-pattern has no lowering");
        pat->failed = 1;
        pat->npart = mark;
        return 0;
    }
    g = &pat->groups[*alts];
    n = pat->npart - mark;
    g->conds = NULL;
    g->ccond = 0;
    g->ncond = n;
    if (n) {
        size_t j;

        g->conds = lower_grow(pat->c, NULL, &g->ccond, n, sizeof *g->conds);
        if (g->conds) {
            for (j = 0; j < n; j++) {
                g->conds[j] = pat->parts[mark + j];
            }
        }
    }
    pat->npart = mark;
    return 1;
}

/* An or-pattern: the alternatives bind the same names, so each keeps
   its own group, and the disjunction joins the match. */
void lower_pat_or(lower_pat *pat, const ek_node *p, const char *slot)
{
    size_t saved_cur = pat->cur;
    size_t g0 = pat->ngroup;
    unsigned saved_flush = pat->nflush;
    size_t mark = pat->npart;
    size_t i;

    for (i = 0; i < p->u.kids.count; i++) {
        size_t alts = 0;

        if (!pat_or_alt(pat, p->u.kids.data[i], slot, mark, saved_flush,
                        &alts)) {
            pat->cur = saved_cur;
            return;
        }
    }
    pat_or_cond(pat, g0);
    pat->cur = saved_cur;
}
