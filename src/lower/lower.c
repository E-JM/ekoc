#include <string.h>

#include "lower/internal.h"

/* The entry point: reachability over the checked root module, function
   emission in discovery order, and the assembly of the translation
   unit around the collected text. */

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

/* Adds a symbol to the worklist unless it was visited already. */
void lower_enqueue(lower_ctx *c, const ek_symbol *sym)
{
    const ek_symbol **slots;
    size_t i;

    if (!sym) {
        return;
    }
    for (i = 0; i < c->nseen; i++) {
        if (c->seen[i] == sym) {
            return;
        }
    }
    slots = lower_grow(c, c->seen, &c->cseen, c->nseen + 1, sizeof *slots);
    if (slots) {
        slots[c->nseen++] = sym;
        c->seen = slots;
    }
    slots = lower_grow(c, c->work, &c->cwork, c->nwork + 1, sizeof *slots);
    if (slots) {
        slots[c->nwork++] = sym;
        c->work = slots;
    }
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

static int unit_puts(ek_buf *b, const char *s)
{
    return ek_buf_append_str(b, s);
}

static int unit_preamble(ek_buf *unit)
{
    return unit_puts(unit, "#include \"ekort.h\"\n\n")
        && unit_puts(unit, "static ek_val ek_keep(ek_val v) "
                           "{ ek_retain(v); return v; }\n\n");
}

/* A forward declaration per emitted function. */
static void emit_decls(ek_buf *unit, lower_ctx *c)
{
    size_t i;

    for (i = 0; i < c->nsig; i++) {
        if (!unit_puts(unit, c->sigs[i]) || !unit_puts(unit, ";\n")) {
            c->oom = 1;
        }
    }
    if (!unit_puts(unit, "\n")) {
        c->oom = 1;
    }
}

/* The collected bodies and, when the root module has a parameterless
   `main`, the entry shim. */
static void emit_bodies(ek_buf *unit, lower_ctx *c)
{
    if (c->body.data
        && !ek_buf_append_bytes(unit, c->body.data, c->body.len)) {
        c->oom = 1;
    }
    if (c->has_main
        && !ek_buf_printf(unit, "int main(void) { ek_val t = %s(); "
                                "ek_release(t); return 0; }\n",
                          c->entry_mangled)) {
        c->oom = 1;
    }
}

/* Copies the unit into the arena as the NUL-terminated result text. */
static void unit_finish(ek_buf *unit, lower_ctx *c, ek_lower_result *out,
                        ek_arena *arena)
{
    char *text = ek_arena_alloc(arena, unit->len + 1);

    if (text) {
        if (unit->data) {
            memcpy(text, unit->data, unit->len + 1);
        } else {
            text[0] = 0;
        }
        out->text = text;
        out->len = unit->len;
    } else {
        c->oom = 1;
    }
}

ek_lower_result ek_lower(ek_resolve_result resolved, ek_check_result checked,
                         ek_arena *arena, ek_diag *diag)
{
    ek_lower_result out;
    lower_ctx c;
    ek_buf unit;
    const ek_symbol *entry;

    memset(&out, 0, sizeof out);
    memset(&c, 0, sizeof c);
    c.resolved = resolved;
    c.checked = checked;
    c.arena = arena;
    c.diag = diag;
    ek_buf_init(&c.body);
    ek_buf_init(&unit);
    if (!unit_preamble(&unit)) {
        c.oom = 1;
    }
    if (resolved.module_count) {
        entry = find_main(&resolved);
        c.entry = entry;
        lower_enqueue(&c, entry);
    }
    while (c.head < c.nwork && !c.oom) {
        lower_function(&c, c.work[c.head]);
        c.head++;
    }
    emit_decls(&unit, &c);
    emit_bodies(&unit, &c);
    unit_finish(&unit, &c, &out, arena);
    out.fn_count = c.fn_count;
    ek_buf_destroy(&unit);
    ek_buf_destroy(&c.body);
    return out;
}
