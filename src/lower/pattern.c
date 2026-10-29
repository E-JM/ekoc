#include <string.h>

#include "lower/internal.h"

/* Patterns: the tests and bindings one pattern contributes over a
   value slot. A test is a pure C condition over the slot, no retains
   and no releases; the conjuncts flow into the arm's match flag, and
   a binding is an owned keep of the destructured slot, assigned only
   when the whole cascade matched. List patterns walk cons cells; a
   prefix rest walks to the end, so its arm keeps the rebuilt prefix
   and the fixed tail alive until the arm's own temps release. */

static void pat_lit(lower_pat *pat, const ek_node *p, const char *slot);
static void pat_cons(lower_pat *pat, const ek_node *p, const char *slot);

void lower_pat_step(lower_pat *pat, const ek_node *p, const char *slot)
{
    switch (p->kind) {
    case EK_N_P_WILDCARD:
        return;
    case EK_N_P_VAR:
        if (p->u.path.count == 1 && strcmp(p->u.path.data[0], "_") != 0) {
            lower_pat_group_item(pat, p->u.path.data[0], slot);
        }
        return;
    case EK_N_P_LIT:
        pat_lit(pat, p, slot);
        return;
    case EK_N_P_CONS:
        pat_cons(pat, p, slot);
        return;
    case EK_N_P_BRACE:
        lower_pat_brace(pat, p, slot);
        return;
    case EK_N_P_PAREN:
        if (p->u.kids.count == 1) {
            lower_pat_step(pat, p->u.kids.data[0], slot);
        }
        return;
    case EK_N_P_FIELD:
        lower_pat_step(pat, p->u.p_field.pattern, slot);
        return;
    case EK_N_P_OR:
        lower_pat_or(pat, p, slot);
        return;
    default:
        return;
    }
}


/* One unpadded line of the test cascade. */
void lower_pat_line(lower_pat *pat, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (!ek_buf_vprintf(&pat->stmts, fmt, ap)) {
        pat->c->oom = 1;
    }
    va_end(ap);
    if (!ek_buf_append_char(&pat->stmts, '\n')) {
        pat->c->oom = 1;
    }
}

/* One conjunct of the match flag; the pending conjuncts flush into
   the flag when a statement must interleave. */
void lower_pat_cond(lower_pat *pat, const char *expr)
{
    const char **slots = lower_grow(pat->c, pat->parts, &pat->cpart,
                                    pat->npart + 1, sizeof *slots);

    if (slots) {
        slots[pat->npart++] = expr;
        pat->parts = slots;
    }
}

/* The pending conjuncts become flag updates; the flush count marks
   where statement tests happened, which an or-pattern forbids. */
void lower_pat_flush(lower_pat *pat)
{
    size_t i;

    for (i = 0; i < pat->npart; i++) {
        lower_pat_line(pat, "%s = %s && (%s);", pat->m, pat->m, pat->parts[i]);
    }
    pat->npart = 0;
    pat->nflush++;
}

/* A fresh binding group. */
void lower_pat_new_group(lower_pat *pat)
{
    lower_pat_group *slots = lower_grow(pat->c, pat->groups, &pat->cgroup,
                                        pat->ngroup + 1, sizeof *slots);

    if (slots) {
        pat->groups = slots;
        memset(&slots[pat->ngroup], 0, sizeof slots[pat->ngroup]);
        pat->cur = pat->ngroup++;
    }
}

/* One binding: the written name takes an owned keep of the slot. */
void lower_pat_group_item(lower_pat *pat, const char *name,
                          const char *slot)
{
    lower_pat_group *g = &pat->groups[pat->cur];
    lower_pat_item *slots = lower_grow(pat->c, g->items, &g->citem,
                                       g->nitem + 1, sizeof *slots);

    if (slots) {
        g->items = slots;
        g->items[g->nitem].cname = name;
        g->items[g->nitem].slot = slot;
        g->nitem++;
    }
}

/* The cons-cell predicate over a slot. */
const char *lower_pat_is_cons(lower_ctx *c, const char *slot)
{
    return lower_str(c, "(%s.tag == EK_VAL_REF && %s.as.ref != 0"
                        " && %s.as.ref->info->kind == EK_KIND_CONS)",
                     slot, slot, slot);
}

const char *lower_pat_slot_head(lower_ctx *c, const char *slot)
{
    return lower_str(c, "ek_cons_head(%s.as.ref)", slot);
}

const char *lower_pat_slot_tail(lower_ctx *c, const char *slot)
{
    return lower_str(c, "ek_cons_tail(%s.as.ref)", slot);
}

/* A string literal pattern: the object to compare hoists into the
   arm's own temps, so the condition itself stays pure. */
static const char *str_lit_cond(lower_pat *pat, const char *slot,
                                const ek_node *lit)
{
    ek_buf b;
    const char *t = lower_temp(pat->c, pat->scope);

    ek_buf_init(&b);
    if (!ek_buf_append_str(&b, "ek_val_ref(ek_str_from_cstr(")) {
        pat->c->oom = 1;
    }
    if (lit->u.str.count == 1) {
        lower_string_literal(pat->c, &b, lit->u.str.segs[0].text,
                             lit->u.str.segs[0].text_len);
    } else {
        lower_string_literal(pat->c, &b, "", 0);
    }
    if (!ek_buf_append_str(&b, "))")) {
        pat->c->oom = 1;
    }
    lower_pad(pat->c);
    lower_emitf(pat->c, "ek_val %s = %s;\n", t, lower_take(pat->c, &b));
    lower_clean_push(pat->c, t);
    return lower_str(pat->c, "(%s.tag == EK_VAL_REF && %s.as.ref != 0"
                             " && %s.as.ref->info->kind == EK_KIND_STR"
                             " && ek_str_eq(%s.as.ref, %s.as.ref))",
                     slot, slot, slot, slot, t);
}

/* The payload comparison a literal pattern contributes, decided by
   the pattern row's nominal. */
static void pat_lit(lower_pat *pat, const ek_node *p, const char *slot)
{
    lower_ctx *c = pat->c;
    const ek_ck_row *row = lower_row(c, p);
    const ek_type *t = row ? row->type : NULL;
    const ek_node *inner = p->u.wrap.inner;
    int neg = inner && inner->kind == EK_N_UNARY;
    const char *name;
    unsigned long long mag;

    if (neg) {
        inner = inner->u.wrap.inner;
    }
    if (!inner || !t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || (inner->kind != EK_N_INT_LIT && inner->kind != EK_N_FLOAT_LIT
            && inner->kind != EK_N_CHAR_LIT
            && inner->kind != EK_N_STRING_LIT)) {
        lower_error(c, p, "the literal pattern has no lowering");
        pat->failed = 1;
        return;
    }
    name = t->u.nominal.sym->name;
    if (strcmp(name, "String") == 0) {
        lower_pat_cond(pat, str_lit_cond(pat, slot, inner));
        return;
    }
    if (strcmp(name, "Bool") == 0) {
        lower_pat_cond(pat, lower_str(c, "%s.as.i == %d", slot,
                                inner->u.int_lit.magnitude ? 1 : 0));
        return;
    }
    if (strcmp(name, "UInt") == 0) {
        lower_pat_cond(pat, lower_str(c, "%s.as.u == %lluULL", slot,
                                inner->u.int_lit.magnitude));
        return;
    }
    if (strcmp(name, "Char") == 0) {
        lower_pat_cond(pat, lower_str(c, "%s.as.c == %uu", slot,
                                (unsigned)inner->u.char_lit.scalar));
        return;
    }
    if (strcmp(name, "Float") == 0) {
        lower_pat_cond(pat, lower_str(c, "%s.as.f == %s%.17g", slot,
                                neg ? "-" : "", inner->u.float_lit.value));
        return;
    }
    mag = inner->kind == EK_N_INT_LIT ? inner->u.int_lit.magnitude : 0;
    lower_pat_cond(pat, lower_str(c, "%s.as.i == %s(int64_t)%lluULL", slot,
                            neg ? "-" : "", mag));
}

/* A constructor pattern: the typeinfo pointer is the identity, and
   Bool's scalar constructors compare their Int slot. */
static void pat_cons(lower_pat *pat, const ek_node *p, const char *slot)
{
    lower_ctx *c = pat->c;
    const ek_ck_row *row = lower_row(c, p);
    const ek_type *t = row ? row->type : NULL;
    const ek_ref *ref = lower_ref(c, p);
    const ek_symbol *tsym = t && t->kind == EK_T_NOMINAL
                                ? t->u.nominal.sym : NULL;
    lower_gtype *g;

    if (tsym && strcmp(tsym->name, "Bool") == 0 && ref && ref->symbol
        && ref->symbol->owner && strcmp(ref->symbol->owner, "Bool") == 0) {
        lower_pat_cond(pat, lower_str(c, "%s.as.i == %d", slot,
                                strcmp(ref->symbol->name, "True") == 0));
        return;
    }
    g = tsym ? lower_ctor_family(c, p, ref, tsym) : NULL;
    if (!g) {
        lower_error(c, p, "the constructor pattern `%s` has no lowering",
                    ref && ref->symbol ? ref->symbol->name : "?");
        pat->failed = 1;
        return;
    }
    lower_pat_cond(pat, lower_str(c, "%s.as.ref->info == &ekT_%s", slot, g->stem));
    if (g->nf && p->u.cons.payload) {
        lower_pat_step(pat, p->u.cons.payload,
                 lower_str(c, "((ekS_%s *)%s.as.ref)->p0", g->stem, slot));
    }
}



/* One arm's pattern plan: the match flag mints into the arm scope and
   the plain group stands ready. */
void lower_pat_init(lower_ctx *c, lower_pat *pat, lower_scope *scope)
{
    memset(pat, 0, sizeof *pat);
    pat->c = c;
    pat->scope = scope;
    pat->m = lower_temp(c, scope);
    lower_pat_new_group(pat);
}

/* The tests and bindings of one arm's pattern over its column. */
int lower_pat_walk(lower_ctx *c, lower_pat *pat, const ek_node *p,
                   const char *slot)
{
    (void)c;
    lower_pat_step(pat, p, slot);
    if (pat->failed) {
        lower_pat_line(pat, "%s = 0;", pat->m);
        pat->failed = 0;
        return 0;
    }
    return 1;
}

/* One binding's spelling: minted once, deduplicated by name across
   the arm's groups. */
static const char *pat_decl(lower_ctx *c, lower_pat *pat, const char *name)
{
    lower_pat_decl *slots;
    size_t d;

    for (d = 0; d < pat->ndecl; d++) {
        if (strcmp(pat->decls[d].eko, name) == 0) {
            return pat->decls[d].cname;
        }
    }
    slots = lower_grow(c, pat->decls, &pat->cdecl, pat->ndecl + 1,
                       sizeof *slots);
    if (!slots) {
        c->oom = 1;
        return NULL;
    }
    pat->decls = slots;
    slots[pat->ndecl].eko = name;
    slots[pat->ndecl].cname = lower_c_name(c, pat->scope, name);
    pat->ndecl++;
    lower_scope_add(c, pat->scope, name, slots[pat->ndecl - 1].cname, 0,
                    NULL);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_val_unit();\n",
                slots[pat->ndecl - 1].cname);
    return slots[pat->ndecl - 1].cname;
}

/* The arm's pre-declared bindings and the buffered test statements. */
void lower_pat_emit(lower_ctx *c, lower_pat *pat)
{
    const char *p;
    size_t i, k;

    for (i = 0; i < pat->ngroup; i++) {
        for (k = 0; k < pat->groups[i].nitem; k++) {
            const char *cname = pat_decl(c, pat,
                                         pat->groups[i].items[k].cname);

            if (cname) {
                pat->groups[i].items[k].cname = cname;
            }
        }
    }
    lower_pat_flush(pat);
    p = pat->stmts.data;
    {
        const char *end = p ? p + pat->stmts.len : NULL;

        while (p && p < end) {
            const char *nl = memchr(p, '\n', (size_t)(end - p));
            size_t n = nl ? (size_t)(nl - p) : (size_t)(end - p);

            if (n) {
                lower_pad(c);
                lower_emitf(c, "%.*s\n", (int)n, p);
            }
            p += n + (nl ? 1 : 0);
        }
    }
}

/* The binding assignments, inside the arm's match: the plain group
   assigns unconditionally, an or-group runs its alternatives in
   order, each branch assigning the same names. */
void lower_pat_assign(lower_ctx *c, lower_pat *pat)
{
    size_t i, k;

    for (i = 0; i < pat->ngroup; i++) {
        const lower_pat_group *g = &pat->groups[i];

        if (g->ncond == 0) {
            for (k = 0; k < g->nitem; k++) {
                lower_pad(c);
                lower_emitf(c, "%s = ek_keep(%s);\n", g->items[k].cname,
                            g->items[k].slot);
            }
            continue;
        }
        for (k = 0; k < g->ncond; k++) {
            size_t w;

            lower_pad(c);
            lower_emitf(c, "%s (%s) {\n", k ? "} else if" : "if",
                        g->conds[k]);
            c->depth++;
            for (w = 0; w < g->nitem; w++) {
                lower_pad(c);
                lower_emitf(c, "%s = ek_keep(%s);\n", g->items[w].cname,
                            g->items[w].slot);
            }
            c->depth--;
        }
        lower_pad(c);
        lower_emitf(c, "}\n");
    }
}

void lower_pat_done(lower_ctx *c, lower_pat *pat)
{
    (void)c;
    ek_buf_destroy(&pat->stmts);
}
