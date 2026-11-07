#include <string.h>

#include "lower/internal.h"

/* The entry point: reachability over the checked root module and the
   functions emitted in discovery order, over the stage's shared
   plumbing -- text emission, diagnostics, the row lookups, the
   worklist. The translation-unit assembly stands in unit.c. */

void lower_emitf(lower_ctx *c, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (!ek_buf_vprintf(&c->body, fmt, ap)) {
        c->oom = 1;
    }
    va_end(ap);
}

void lower_pad(lower_ctx *c)
{
    unsigned i;

    for (i = 0; i < c->depth; i++) {
        if (!ek_buf_append_str(&c->body, "    ")) {
            c->oom = 1;
        }
    }
}

/* Finalizes a buffer as an arena string; never returns NULL. */
const char *lower_take(lower_ctx *c, ek_buf *b)
{
    char *out = ek_arena_alloc(c->arena, b->len + 1);

    if (out) {
        if (b->data) {
            memcpy(out, b->data, b->len + 1);
        } else {
            out[0] = 0;
        }
    } else {
        c->oom = 1;
    }
    ek_buf_destroy(b);
    return out ? out : "";
}

const char *lower_str(lower_ctx *c, const char *fmt, ...)
{
    ek_buf b;
    va_list ap;

    ek_buf_init(&b);
    va_start(ap, fmt);
    if (!ek_buf_vprintf(&b, fmt, ap)) {
        c->oom = 1;
    }
    va_end(ap);
    return lower_take(c, &b);
}

/* Grows an arena array in place; returns the storage for `need`
   elements, copying the old content. */
void *lower_grow(lower_ctx *c, void *data, size_t *cap, size_t need,
                 size_t width)
{
    size_t ncap;
    void *fresh;

    if (need <= *cap) {
        return data;
    }
    ncap = *cap ? *cap * 2 : 8;
    if (ncap < need) {
        ncap = need;
    }
    fresh = ek_arena_alloc(c->arena, ncap * width);
    if (!fresh) {
        c->oom = 1;
        return data;
    }
    if (*cap) {
        memcpy(fresh, data, *cap * width);
    }
    *cap = ncap;
    return fresh;
}

void lower_error(lower_ctx *c, const ek_node *at, const char *fmt, ...)
{
    va_list ap;

    if (!at) {
        return;
    }
    va_start(ap, fmt);
    ek_diag_emitv(c->diag, EK_SEV_ERROR, "E-lower", c->file, at->line,
                  at->col, fmt, ap);
    va_end(ap);
}

/* The check row of a node: a lower-bound search over rows sorted by
   node address (a node may carry several equal rows). */
const ek_ck_row *lower_row(lower_ctx *c, const ek_node *n)
{
    size_t lo = 0, hi = c->checked.count;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;

        if ((size_t)c->checked.rows[mid].node < (size_t)n) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < c->checked.count && c->checked.rows[lo].node == n) {
        return &c->checked.rows[lo];
    }
    return NULL;
}

/* The resolved reference of a node, found like the check rows. */
const ek_ref *lower_ref(lower_ctx *c, const ek_node *n)
{
    size_t lo = 0, hi = c->resolved.ref_count;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;

        if ((size_t)c->resolved.refs[mid].node < (size_t)n) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < c->resolved.ref_count && c->resolved.refs[lo].node == n) {
        return &c->resolved.refs[lo];
    }
    return NULL;
}

/* Adds one reachability item unless its name was queued already: the
   name is the identity, so two sites that spell one instantiation
   share the body, and the first site's applied arguments stand. args
   is the applied argument per binder, fn the applied function type
   the body's pairing pairs against; both NULL for the plain item. */
void lower_enqueue_item(lower_ctx *c, const ek_symbol *sym,
                        const ek_type **args, size_t nargs,
                        const ek_type *fn)
{
    const char *name;
    lower_item *slots;
    size_t i;

    if (!sym) {
        return;
    }
    name = args ? lower_inst_name(c, sym, args, nargs)
                : lower_mangled(c, sym);
    for (i = 0; i < c->nseen; i++) {
        if (strcmp(c->seen[i].name, name) == 0) {
            return;
        }
    }
    slots = lower_grow(c, c->seen, &c->cseen, c->nseen + 1,
                       sizeof *slots);
    if (slots) {
        memset(&slots[c->nseen], 0, sizeof slots[c->nseen]);
        slots[c->nseen].name = name;
        slots[c->nseen].sym = sym;
        slots[c->nseen].args = args;
        slots[c->nseen].nargs = nargs;
        slots[c->nseen].fn = fn;
        c->seen = slots;
        c->nseen++;
    }
    slots = lower_grow(c, c->work, &c->cwork, c->nwork + 1,
                       sizeof *slots);
    if (slots) {
        slots[c->nwork].name = name;
        slots[c->nwork].sym = sym;
        slots[c->nwork].args = args;
        slots[c->nwork].nargs = nargs;
        slots[c->nwork].fn = fn;
        c->work = slots;
        c->nwork++;
    }
}

/* Adds a symbol to the worklist under the empty instantiation. */
void lower_enqueue(lower_ctx *c, const ek_symbol *sym)
{
    lower_enqueue_item(c, sym, NULL, 0, NULL);
}

/* Records one live reference temp a retry jump would abandon. */
void lower_clean_push(lower_ctx *c, const char *temp)
{
    const char **slots = lower_grow(c, c->clean, &c->cclean, c->nclean + 1,
                                    sizeof *slots);

    if (slots) {
        slots[c->nclean++] = temp;
        c->clean = slots;
    }
}

/* Drops the newest entry once its normal release is emitted. */
void lower_clean_pop(lower_ctx *c)
{
    if (c->nclean) {
        c->nclean--;
    }
}

/* Emits the releases of the entries pushed after base, newest first,
   without dropping them: a retry jump abandons the whole live set. */
void lower_clean_release(lower_ctx *c, size_t base)
{
    size_t i = c->nclean;

    while (i > base) {
        i--;
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", c->clean[i]);
    }
}

/* Queues one deferred definition; the queue drains after the function
   that created it. */
void lower_pending_add(lower_ctx *c, lower_pending *p)
{
    p->next = NULL;
    if (c->pend_tail) {
        c->pend_tail->next = p;
    } else {
        c->pend = p;
    }
    c->pend_tail = p;
}

/* The file of the module a symbol was declared in. */
const char *surface_file(lower_ctx *c, const char *stem)
{
    size_t i;

    for (i = 0; i < c->resolved.module_count; i++) {
        if (strcmp(c->resolved.modules[i].module, stem) == 0) {
            return c->resolved.modules[i].file;
        }
    }
    return c->resolved.module_count ? c->resolved.modules[0].file : "";
}

/* The `main` binding of the root module. */
static const ek_symbol *find_main(const ek_resolve_result *r)
{
    const ek_surface *s = &r->modules[0];
    size_t i;

    for (i = 0; i < s->count; i++) {
        if (s->syms[i].kind == EK_SYM_FUNC
            && strcmp(s->syms[i].name, "main") == 0) {
            return &s->syms[i];
        }
    }
    return NULL;
}

/* The test bindings of the root module, in declaration order. */
static void enqueue_tests(lower_ctx *c, const ek_resolve_result *r)
{
    const ek_surface *s = &r->modules[0];
    size_t i;

    for (i = 0; i < s->count; i++) {
        if (s->syms[i].kind == EK_SYM_TEST) {
            lower_enqueue(c, &s->syms[i]);
        }
    }
}

/* The lowering state over one checked run: the identity pieces and the
   buffers the emitters fill, zeroed otherwise. */
static void ctx_init(lower_ctx *c, ek_resolve_result resolved,
                     ek_check_result checked, ek_lower_mode mode,
                     ek_arena *arena, ek_diag *diag)
{
    memset(c, 0, sizeof *c);
    c->resolved = resolved;
    c->checked = checked;
    c->arena = arena;
    c->diag = diag;
    c->mode = mode;
    ek_buf_init(&c->body);
    ek_buf_init(&c->dsvc);
    ek_buf_init(&c->cext);
    ek_buf_init(&c->strdecls);
}

ek_lower_result ek_lower_with(ek_resolve_result resolved,
                              ek_check_result checked, ek_lower_mode mode,
                              ek_arena *arena, ek_diag *diag)
{
    ek_lower_result out;
    lower_ctx c;
    ek_buf unit;
    const ek_symbol *entry;

    memset(&out, 0, sizeof out);
    ctx_init(&c, resolved, checked, mode, arena, diag);
    ek_buf_init(&unit);
    if (!unit_preamble(&unit)) {
        c.oom = 1;
    }
    if (resolved.module_count) {
        if (mode == EK_LOWER_TESTS) {
            enqueue_tests(&c, &resolved);
        } else {
            entry = find_main(&resolved);
            c.entry = entry;
            lower_enqueue(&c, entry);
        }
    }
    while (c.head < c.nwork && !c.oom) {
        lower_item item = c.work[c.head];

        c.head++;
        if (lower_is_value(item.sym)) {
            lower_value_binding(&c, &item);
        } else {
            lower_function(&c, &item);
        }
    }
    unit_assemble(&c, &unit, &out, arena);
    ek_buf_destroy(&unit);
    ek_buf_destroy(&c.body);
    ek_buf_destroy(&c.dsvc);
    ek_buf_destroy(&c.cext);
    ek_buf_destroy(&c.strdecls);
    return out;
}
