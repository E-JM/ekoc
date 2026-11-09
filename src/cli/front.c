#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cli/internal.h"

#include "check/check.h"
#include "layout/layout.h"
#include "lex/lex.h"
#include "parse/parse.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"

/* The commands' shared pipeline front: source reading, path helpers,
   the drained diagnostics log, and the stages up to the checked AST.
   The per-command drivers lower, report, and decide the exit status. */

void cli_log_drain(cli_diag_log *log, ek_diag *diag)
{
    const ek_diagnostic *got;
    ek_diagnostic *items;
    size_t n, cap;

    got = ek_diag_drain(diag, &n);
    if (n == 0) {
        return;
    }
    if (log->count + n > log->cap) {
        cap = log->cap != 0 ? log->cap : 16;
        while (cap < log->count + n) {
            cap *= 2;
        }
        items = ek_arena_alloc(log->arena, cap * sizeof *items);
        if (items == NULL) {
            return;
        }
        if (log->items != NULL) {
            memcpy(items, log->items, log->count * sizeof *items);
        }
        log->items = items;
        log->cap = cap;
    }
    memcpy(log->items + log->count, got, n * sizeof *got);
    log->count += n;
}

void cli_log_print(const cli_diag_log *log)
{
    size_t i;

    for (i = 0; i < log->count; i++) {
        const ek_diagnostic *d = &log->items[i];

        fprintf(stderr, "%s:%d:%d: %s: %s\n",
                d->file != NULL ? d->file : "-",
                d->line, d->col,
                d->code != NULL ? d->code : "-",
                d->message != NULL ? d->message : "");
    }
}

size_t cli_log_errors(const cli_diag_log *log)
{
    size_t i, n = 0;

    for (i = 0; i < log->count; i++) {
        if (log->items[i].severity == EK_SEV_ERROR) {
            n++;
        }
    }
    return n;
}

int cli_read_source(ek_buf *buf, const char *path)
{
    FILE *fp = fopen(path, "rb");
    char chunk[8192];
    size_t got;

    if (fp == NULL) {
        return 0;
    }
    while ((got = fread(chunk, 1, sizeof chunk, fp)) > 0) {
        if (!ek_buf_append_bytes(buf, chunk, got)) {
            fclose(fp);
            return 0;
        }
    }
    if (ferror(fp) != 0) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

const char *cli_dir_of(ek_arena *arena, const char *path)
{
    const char *slash = strrchr(path, '/');
    char *dir;
    size_t len;

    if (slash == NULL) {
        return ".";
    }
    if (slash == path) {
        return "/";
    }
    len = (size_t)(slash - path);
    dir = ek_arena_alloc(arena, len + 1);
    if (dir == NULL) {
        return NULL;
    }
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

const char *cli_stem_of(ek_arena *arena, const char *path)
{
    const char *base = strrchr(path, '/');
    const char *dot;
    char *stem;
    size_t len;

    base = base != NULL ? base + 1 : path;
    dot = strrchr(base, '.');
    len = dot != NULL && dot != base ? (size_t)(dot - base) : strlen(base);
    stem = ek_arena_alloc(arena, len + 1);
    if (stem == NULL) {
        return NULL;
    }
    memcpy(stem, base, len);
    stem[len] = '\0';
    return stem;
}

/* True when the root module declares a `main` function binding; check's
   entry rules validate its shape, and the host commands own its
   presence -- a test root is exempt (Clause 7.1.3). */
int cli_has_main(const ek_resolve_result *res)
{
    const ek_surface *s;
    size_t i;

    if (!res || res->module_count == 0) {
        return 0;
    }
    s = &res->modules[0];
    for (i = 0; i < s->count; i++) {
        if (s->syms[i].kind == EK_SYM_FUNC
            && strcmp(s->syms[i].name, "main") == 0) {
            return 1;
        }
    }
    return 0;
}

/* One module surface's test bindings in declaration order; names may
   be NULL for a counting pass. */
size_t cli_test_names(const ek_surface *s, const char **names)
{
    size_t i, n = 0;

    if (s == NULL) {
        return 0;
    }
    for (i = 0; i < s->count; i++) {
        if (s->syms[i].kind == EK_SYM_TEST) {
            if (names != NULL) {
                names[n] = s->syms[i].name;
            }
            n++;
        }
    }
    return n;
}

int cli_resolve(cli_diag_log *log, ek_arena *arena, ek_diag *diag,
                const char *file, const char *dir,
                const char *bytes, size_t len,
                ek_resolve_result *res, ek_check_result *chk)
{
    const char *path[1];
    ek_lex_result lexed;
    ek_layout_result laid;
    ek_parse_result ast;

    path[0] = dir != NULL ? dir : ".";
    lexed = ek_lex(bytes, len, file, arena, diag);
    cli_log_drain(log, diag);
    /* One defect, one diagnosis: a stage that reported errors stops
       the front there. The later stages read a corrupted
       intermediate, and their diagnostics would only cascade. */
    if (cli_log_errors(log) != 0) {
        return 1;
    }
    laid = ek_layout(lexed.tokens, lexed.count, file, arena, diag);
    cli_log_drain(log, diag);
    if (cli_log_errors(log) != 0) {
        return 1;
    }
    ast = ek_parse(laid.tokens, laid.count, file, arena, diag);
    cli_log_drain(log, diag);
    if (cli_log_errors(log) != 0) {
        return 1;
    }
    if (lexed.tokens == NULL || laid.tokens == NULL || ast.module == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-io", file, 0, 0,
                     "out of memory");
        cli_log_drain(log, diag);
        return 0;
    }
    *res = ek_resolve(ast.module, file, path, 1, arena, diag);
    cli_log_drain(log, diag);
    if (cli_log_errors(log) != 0) {
        return 1;
    }
    *chk = ek_check(*res, arena, diag);
    cli_log_drain(log, diag);
    return 1;
}
