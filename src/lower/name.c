#include <stdio.h>
#include <string.h>

#include "lower/internal.h"

/* Naming for the emitted unit: the mangled function symbols, the C
   spellings of locals and temps, and the C form of string bytes. */

/* The C99 keywords: a written name spelled as one is emitted with a
   trailing underscore. */
static int is_c_keyword(const char *name)
{
    static const char *words[] = {
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "inline", "int", "long", "register", "restrict", "return",
        "short", "signed", "sizeof", "static", "struct", "switch",
        "typedef", "union", "unsigned", "void", "volatile", "while",
        "_Bool", "_Complex", "_Imaginary"
    };
    size_t i;

    for (i = 0; i < sizeof words / sizeof *words; i++) {
        if (strcmp(name, words[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Runtime identifiers visible in every emitted body: the ek_ and EK_
   namespaces of the runtime header, the helper included. A written
   name in those shapes gains a u_ prefix, which appending cannot
   achieve. */
static int shadows_runtime(const char *name)
{
    return strncmp(name, "ek_", 3) == 0 || strncmp(name, "EK_", 3) == 0;
}

/* The file-scope variables of the interned literals share this prefix,
   so a written binding in its shape would shadow one. */
static const char cache_prefix[] = "ekc";

static int shadows_cache(const char *name)
{
    return strncmp(name, cache_prefix, sizeof cache_prefix - 1) == 0;
}

static int taken_here(const lower_scope *scope, const char *name)
{
    size_t i;

    for (i = 0; i < scope->nloc; i++) {
        if (strcmp(scope->locs[i].cname, name) == 0) {
            return 1;
        }
    }
    for (i = 0; i < scope->ntemp; i++) {
        if (strcmp(scope->temps[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int taken_visible(const lower_scope *scope, const char *name)
{
    for (; scope; scope = scope->parent) {
        if (taken_here(scope, name)) {
            return 1;
        }
    }
    return 0;
}

/* The C name of one emitted function: a plain binding is the module
   and its name; a namespace member carries its owner, so two members
   of different types never collide. */
const char *lower_mangled(lower_ctx *c, const ek_symbol *sym)
{
    if (sym->owner) {
        return lower_str(c, "ekf_%s_%s_%s", sym->module, sym->owner,
                         sym->name);
    }
    return lower_str(c, "ekf_%s_%s", sym->module, sym->name);
}

/* The C name of one instantiation: the plain spelling, then a `$`
   separator no Eko identifier character can supply, then the applied
   arguments in the service names' own spelling. */
const char *lower_inst_name(lower_ctx *c, const ek_symbol *sym,
                            const ek_type *const *args, size_t n)
{
    if (!n) {
        return lower_mangled(c, sym);
    }
    return lower_str(c, "%s$%s", lower_mangled(c, sym),
                     lower_spell_apply(c, args, n));
}

/* A fresh temp name, unique against every binding and temp visible
   here, so no temp ever shadows a name a later statement reads. */
const char *lower_temp(lower_ctx *c, lower_scope *scope)
{
    char buf[24];
    unsigned n = c->temp;
    const char *out;

    for (;;) {
        snprintf(buf, sizeof buf, "t%u", n);
        if (!taken_visible(scope, buf)) {
            break;
        }
        n++;
    }
    c->temp = n + 1;
    out = lower_str(c, "%s", buf);
    if (out) {
        const char **slot = lower_grow(c, scope->temps, &scope->ctemp,
                                       scope->ntemp + 1, sizeof *slot);

        if (slot) {
            slot[scope->ntemp++] = out;
            scope->temps = slot;
        }
    }
    return out;
}

/* The C spelling of a new binding: names that would collide with the
   runtime, shadow the literal caches, or repeat within the same block
   are adjusted until free. Shadowing an outer block's binding is the
   language's own scoping. */
const char *lower_c_name(lower_ctx *c, lower_scope *scope, const char *name)
{
    ek_buf b;
    const char *out;

    ek_buf_init(&b);
    if ((shadows_runtime(name) || shadows_cache(name))
        && !ek_buf_append_str(&b, "u_")) {
        c->oom = 1;
    }
    if (!ek_buf_append_str(&b, name)) {
        c->oom = 1;
    }
    while (!c->oom && (is_c_keyword(b.data) || taken_here(scope, b.data))) {
        if (!ek_buf_append_char(&b, '_')) {
            c->oom = 1;
        }
    }
    out = lower_str(c, "%s", b.data);
    ek_buf_destroy(&b);
    return out;
}

/* Appends the C string literal for the bytes: printable ASCII stands,
   the quote, backslash, and question mark gain a backslash, and every
   other byte becomes a three-digit octal escape. */
void lower_string_literal(lower_ctx *c, ek_buf *b, const char *bytes,
                          size_t len)
{
    size_t i;

    if (!ek_buf_append_char(b, '"')) {
        c->oom = 1;
    }
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)bytes[i];

        if (ch == '"') {
            if (!ek_buf_append_str(b, "\\\"")) {
                c->oom = 1;
            }
        } else if (ch == '\\') {
            if (!ek_buf_append_str(b, "\\\\")) {
                c->oom = 1;
            }
        } else if (ch == '?') {
            if (!ek_buf_append_str(b, "\\?")) {
                c->oom = 1;
            }
        } else if (ch >= 0x20 && ch <= 0x7E) {
            if (!ek_buf_append_char(b, (char)ch)) {
                c->oom = 1;
            }
        } else if (!ek_buf_printf(b, "\\%03o", (unsigned)ch)) {
            c->oom = 1;
        }
    }
    if (!ek_buf_append_char(b, '"')) {
        c->oom = 1;
    }
}

/* The cache variable of one interned string literal: identical byte
   sequences share one unit-level static ek_val, named after the cache
   prefix in first-use order, and the shim primes each variable once
   at startup. The names cannot collide by construction: a written
   identifier in the runtime's ek_ shape or in the cache prefix's own
   shape is emitted under the u_ prefix (lower_c_name), the temps are
   t<n> and s<n>, and no runtime or generated family (ekf_, ekT_,
   ekS_, ekC_, ekn_) spells the bare ekc<digits> shape.

   Interning changes only object identity, and identity is
   unobservable: strings compare structurally, pattern dispatch tests
   the family kind, and dictionary and set keying is structural. Each
   site reads the cache through ek_keep, so the site's own release
   pairs with that keep; the cache's own reference is never dropped
   and the runtime never releases the caches -- they live for the
   process. */
const char *lower_str_cache(lower_ctx *c, const char *bytes, size_t len)
{
    ek_buf b;
    const char *lit;
    lower_strlit *slots;
    const char *name;
    size_t i;

    ek_buf_init(&b);
    lower_string_literal(c, &b, bytes, len);
    lit = lower_take(c, &b);
    for (i = 0; i < c->nslit; i++) {
        if (strcmp(c->strlits[i].lit, lit) == 0) {
            return c->strlits[i].name;
        }
    }
    slots = lower_grow(c, c->strlits, &c->cslit, c->nslit + 1,
                       sizeof *slots);
    if (!slots) {
        return "";
    }
    c->strlits = slots;
    name = lower_str(c, "%s%zu", cache_prefix, c->nslit);
    if (!name) {
        return "";
    }
    slots[c->nslit].name = name;
    slots[c->nslit].lit = lit;
    c->nslit++;
    if (!ek_buf_printf(&c->strdecls, "static ek_val %s;\n", name)) {
        c->oom = 1;
    }
    return name;
}
