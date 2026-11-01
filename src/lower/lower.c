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

static int unit_puts(ek_buf *b, const char *s)
{
    return ek_buf_append_str(b, s);
}

static int unit_preamble(ek_buf *unit)
{
    return unit_puts(unit, "#include \"ekort.h\"\n")
        && unit_puts(unit, "#include <stdlib.h>\n")
        && unit_puts(unit, "#include <string.h>\n\n")
        && unit_puts(unit, "static ek_val ek_keep(ek_val v) "
                           "{ ek_retain(v); return v; }\n\n")
        && unit_puts(unit, "static inline void ek_release_obj(ek_obj *o) "
                           "{ ek_release(ek_val_ref(o)); }\n\n");
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

/* Appends to the unit, recording exhaustion on the context. */
static void putf(ek_buf *unit, lower_ctx *c, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (!ek_buf_vprintf(unit, fmt, ap)) {
        c->oom = 1;
    }
    va_end(ap);
}

/* One constructed bundle of the shim, the Args slot carrying the
   argument vector without the program name. */
static void emit_shim_bundle(ek_buf *unit, lower_ctx *c, size_t i,
                             const char *ctor)
{
    if (strcmp(ctor, "ek_cap_args") != 0) {
        putf(unit, c, "    ek_val c%zu = %s();\n", i, ctor);
        return;
    }
    putf(unit, c, "    ek_val c%zu = ek_cap_args("
                  "argc > 0 ? argc - 1 : 0, "
                  "argc > 0 ? argv + 1 : (char **)0);\n", i);
}

/* The entry shim: a parameterless main is called bare; a main over the
   authorization manifest receives one constructed capability per
   position, released with the entry's result. The Args bundle carries
   the argument vector without the program name, so the shim is the
   one place that reads argc and argv. */
static void emit_main_shim(ek_buf *unit, lower_ctx *c)
{
    size_t i;
    int has_args = 0;

    if (c->nshim == 0) {
        putf(unit, c, "int main(void) { ek_val t = %s(); "
                      "ek_release(t); return 0; }\n", c->entry_mangled);
        return;
    }
    for (i = 0; i < c->nshim; i++) {
        has_args = has_args || strcmp(c->shim[i], "ek_cap_args") == 0;
    }
    putf(unit, c, "int main(int argc, char **argv) {\n");
    if (!has_args) {
        putf(unit, c, "    (void)argc; (void)argv;\n");
    }
    for (i = 0; i < c->nshim; i++) {
        emit_shim_bundle(unit, c, i, c->shim[i]);
    }
    putf(unit, c, "    ek_val t = %s(", c->entry_mangled);
    for (i = 0; i < c->nshim; i++) {
        putf(unit, c, "%sc%zu", i ? ", " : "", i);
    }
    putf(unit, c, ");\n");
    for (i = c->nshim; i-- > 0;) {
        putf(unit, c, "    ek_release(c%zu);\n", i);
    }
    putf(unit, c, "    ek_release(t);\n    return 0;\n}\n");
}

/* The quoted C literal of a test name, escaped the way string
   literals are. */
static const char *test_name_lit(lower_ctx *c, const char *name)
{
    ek_buf b;

    ek_buf_init(&b);
    lower_string_literal(c, &b, name, strlen(name));
    return lower_take(c, &b);
}

/* The dispatching shim of a TESTS-mode unit: one `argv[1]` match per
   test, the matched test called and released, and the refusal exit
   for a missing or unknown name -- the driver always passes a name it
   collected at compile time, so the exit guards a driver bug only. */
static void emit_test_shim(ek_buf *unit, lower_ctx *c)
{
    size_t i;

    putf(unit, c, "int main(int argc, char **argv) {\n");
    if (c->ntests == 0) {
        putf(unit, c, "    (void)argc; (void)argv;\n");
    } else {
        putf(unit, c, "    if (argc == 2) {\n");
        for (i = 0; i < c->ntests; i++) {
            putf(unit, c, "        if (strcmp(argv[1], %s) == 0) {\n",
                 test_name_lit(c, c->tests[i].name));
            putf(unit, c, "            ek_val t = %s();\n",
                 c->tests[i].cname);
            putf(unit, c, "            ek_release(t);\n"
                          "            return 0;\n"
                          "        }\n");
        }
        putf(unit, c, "    }\n");
    }
    putf(unit, c, "    return 2;\n}\n");
}

/* The collected bodies, the derived service bodies, and the entry
   shim of the mode: the program's main shim when the root module
   declares `main`, or the test dispatch. The service bodies stand
   after the functions; their forward declarations joined the unit
   already. */
static void emit_bodies(ek_buf *unit, lower_ctx *c)
{
    if (c->body.data
        && !ek_buf_append_bytes(unit, c->body.data, c->body.len)) {
        c->oom = 1;
    }
    if (c->dsvc.data
        && !ek_buf_append_bytes(unit, c->dsvc.data, c->dsvc.len)) {
        c->oom = 1;
    }
    if (c->mode == EK_LOWER_TESTS) {
        emit_test_shim(unit, c);
    } else if (c->has_main) {
        emit_main_shim(unit, c);
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

ek_lower_result ek_lower_with(ek_resolve_result resolved,
                              ek_check_result checked, ek_lower_mode mode,
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
    c.mode = mode;
    ek_buf_init(&c.body);
    ek_buf_init(&c.dsvc);
    ek_buf_init(&c.cext);
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
    /* the ctype C structs and the extern prototypes stand between the
       preamble and the struct families: the prototypes name the
       structs, and nothing earlier reads either */
    if (c.cext.data && !ek_buf_append_bytes(&unit, c.cext.data, c.cext.len)) {
        c.oom = 1;
    }
    lower_emit_types(&c, &unit);
    emit_decls(&unit, &c);
    emit_bodies(&unit, &c);
    unit_finish(&unit, &c, &out, arena);
    out.fn_count = c.fn_count;
    ek_buf_destroy(&unit);
    ek_buf_destroy(&c.body);
    ek_buf_destroy(&c.dsvc);
    ek_buf_destroy(&c.cext);
    return out;
}

ek_lower_result ek_lower(ek_resolve_result resolved, ek_check_result checked,
                         ek_arena *arena, ek_diag *diag)
{
    return ek_lower_with(resolved, checked, EK_LOWER_PROGRAM, arena, diag);
}
