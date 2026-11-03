#include <string.h>

#include "lower/internal.h"

/* The translation-unit assembly: the preamble and the front matter,
   the forward declarations, the bodies with the mode's entry shim,
   and the copy into the arena -- around the text the bodies and the
   derived services collected. */

static int unit_puts(ek_buf *b, const char *s)
{
    return ek_buf_append_str(b, s);
}

/* The runtime include stands alone at the top; the libc includes and
   the retention helpers join at assembly, when the bodies' needs are
   known. */
int unit_preamble(ek_buf *unit)
{
    return unit_puts(unit, "#include \"ekort.h\"\n");
}

/* True when some body constructs a generated family: the allocation
   helper's calloc joins the unit then. */
static int any_constructed(const lower_ctx *c)
{
    size_t i;

    for (i = 0; i < c->ngt; i++) {
        if (c->gtypes[i].constructed) {
            return 1;
        }
    }
    return 0;
}

/* The front matter after the runtime include: the libc the emitted
   text uses -- stdlib when a generated family constructs (the
   allocation helper's calloc), string when some body calls into the
   mem/str family or the test shim compares names -- the retention
   helpers, and the interned-literal cache declarations. */
static void emit_front_matter(ek_buf *unit, lower_ctx *c)
{
    if (any_constructed(c) && !unit_puts(unit, "#include <stdlib.h>\n")) {
        c->oom = 1;
    }
    if ((c->uses_string_h || c->ntests > 0)
        && !unit_puts(unit, "#include <string.h>\n")) {
        c->oom = 1;
    }
    if (!unit_puts(unit, "\n")
        || !unit_puts(unit, "static inline ek_val ek_keep(ek_val v) "
                            "{ ek_retain(v); return v; }\n\n")
        || !unit_puts(unit, "static inline void ek_release_obj(ek_obj *o) "
                            "{ ek_release(ek_val_ref(o)); }\n\n")) {
        c->oom = 1;
    }
    if (c->strdecls.data
        && !ek_buf_append_bytes(unit, c->strdecls.data, c->strdecls.len)) {
        c->oom = 1;
    }
    if (!unit_puts(unit, "\n")) {
        c->oom = 1;
    }
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

/* The interned-literal caches fill here, once, before the entry or any
   test can run: the shim is the process's single-threaded startup, so
   the writes cannot race. A first use filling its own cache instead
   would put one filler per task on the same statics once tasks run
   concurrently, which the runtime's atomic reference counts could not
   arbitrate. The runtime never releases the caches; they live for the
   process. */
static void emit_priming(ek_buf *unit, lower_ctx *c)
{
    size_t i;

    for (i = 0; i < c->nslit; i++) {
        putf(unit, c, "    %s = ek_val_ref(ek_str_from_cstr(%s));\n",
             c->strlits[i].name, c->strlits[i].lit);
    }
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
        putf(unit, c, "int main(void)\n{\n");
        emit_priming(unit, c);
        putf(unit, c, "    ek_val t = %s();\n", c->entry_mangled);
        putf(unit, c, "    ek_release(t);\n    return 0;\n}\n");
        return;
    }
    for (i = 0; i < c->nshim; i++) {
        has_args = has_args || strcmp(c->shim[i], "ek_cap_args") == 0;
    }
    putf(unit, c, "int main(int argc, char **argv)\n{\n");
    emit_priming(unit, c);
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

    putf(unit, c, "int main(int argc, char **argv)\n{\n");
    emit_priming(unit, c);
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

/* The unit around the collected text, in the order the C compiler
   reads: the front matter, the ctype C structs and extern prototypes
   (the prototypes name the structs, and nothing earlier reads either),
   the generated struct families, the forward declarations, and the
   bodies with the mode's shim. */
void unit_assemble(lower_ctx *c, ek_buf *unit, ek_lower_result *out,
                   ek_arena *arena)
{
    emit_front_matter(unit, c);
    if (c->cext.data && !ek_buf_append_bytes(unit, c->cext.data,
                                             c->cext.len)) {
        c->oom = 1;
    }
    lower_emit_types(c, unit);
    emit_decls(unit, c);
    emit_bodies(unit, c);
    unit_finish(unit, c, out, arena);
    out->fn_count = c->fn_count;
}
