#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cli/internal.h"

#include "check/check.h"
#include "emit/emit.h"
#include "layout/layout.h"
#include "lex/lex.h"
#include "lower/lower.h"
#include "parse/parse.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"

/* The build command: one Eko file through the whole pipeline into an
   executable. Diagnostics drain through the shared buffer after every
   stage and accumulate here, so the report at the end covers each of
   them in emission order. */

/* Every diagnostic drained so far, in emission order. Messages live in
   the diag buffer's own storage and stay valid until it is destroyed,
   so the log holds shallow copies. */
typedef struct {
    ek_diagnostic *items;
    size_t count;
    size_t cap;
    ek_arena *arena;
} diag_log;

/* Appends one drained buffer. A dropped run means an exhausted arena,
   which the stages report on their own. */
static void log_drain(diag_log *log, ek_diag *diag)
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

/* Prints every diagnostic as one report line. */
static void log_print(const diag_log *log)
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

/* The number of error-severity diagnostics drained so far. */
static size_t log_errors(const diag_log *log)
{
    size_t i, n = 0;

    for (i = 0; i < log->count; i++) {
        if (log->items[i].severity == EK_SEV_ERROR) {
            n++;
        }
    }
    return n;
}

/* Reads the whole file; 1 on success. */
static int read_source(ek_buf *buf, const char *path)
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

/* Everything before the last slash; "." when the path has none. */
static const char *dir_of(ek_arena *arena, const char *path)
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

/* The input's stem: the last component without its extension. */
static const char *stem_of(ek_arena *arena, const char *path)
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

/* The front of the pipeline, source bytes to lowered C text, with each
   stage's diagnostics drained into log. Returns 0 when an early stage
   lost its output to an exhausted arena. */
static int compile_front(diag_log *log, ek_arena *arena, ek_diag *diag,
                         const char *file, const char *dir,
                         const char *bytes, size_t len,
                         ek_lower_result *out)
{
    const char *path[1];
    ek_lex_result lexed;
    ek_layout_result laid;
    ek_parse_result ast;
    ek_resolve_result res;
    ek_check_result chk;

    path[0] = dir != NULL ? dir : ".";
    lexed = ek_lex(bytes, len, file, arena, diag);
    log_drain(log, diag);
    laid = ek_layout(lexed.tokens, lexed.count, file, arena, diag);
    log_drain(log, diag);
    ast = ek_parse(laid.tokens, laid.count, file, arena, diag);
    log_drain(log, diag);
    if (lexed.tokens == NULL || laid.tokens == NULL || ast.module == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-io", file, 0, 0,
                     "out of memory");
        log_drain(log, diag);
        return 0;
    }
    res = ek_resolve(ast.module, file, path, 1, arena, diag);
    log_drain(log, diag);
    chk = ek_check(res, arena, diag);
    log_drain(log, diag);
    *out = ek_lower(res, chk, arena, diag);
    log_drain(log, diag);
    return 1;
}

/* Hands the lowered text to emit when the front end is clean -- a
   failed front end leaves no unit worth compiling -- then reports and
   returns the process exit status. */
static int finish_build(diag_log *log, ek_arena *arena, ek_diag *diag,
                        const ek_lower_result *low, const char *out,
                        const char *tree_root)
{
    int failed = log_errors(log) != 0;

    if (!failed) {
        ek_emit_aot(low->text, out, tree_root, arena, diag);
        log_drain(log, diag);
        failed = log_errors(log) != 0;
    }
    if (failed) {
        log_print(log);
        return 2;
    }
    printf("wrote %s\n", out);
    return 0;
}

/* Frees the compilation's wholesale memory. */
static void finish(ek_buf *source, ek_diag *diag, ek_arena *arena)
{
    ek_buf_destroy(source);
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
}

int cli_build(const char *file, const char *out, const char *tree_root)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();
    diag_log log;
    ek_buf source;
    ek_lower_result low;
    const char *bytes;
    int status;

    memset(&log, 0, sizeof log);
    ek_buf_init(&source);
    if (arena == NULL || diag == NULL) {
        fprintf(stderr, "ekoc: out of memory\n");
        return 2;
    }
    log.arena = arena;
    if (!read_source(&source, file)) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-io", file, 0, 0,
                     "cannot read file: %s", strerror(errno));
        log_drain(&log, diag);
        log_print(&log);
        finish(&source, diag, arena);
        return 2;
    }
    if (out == NULL) {
        out = stem_of(arena, file);
    }
    if (out == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-io", file, 0, 0,
                     "out of memory");
        log_drain(&log, diag);
        log_print(&log);
        finish(&source, diag, arena);
        return 2;
    }
    bytes = source.data != NULL ? source.data : "";
    compile_front(&log, arena, diag, file, dir_of(arena, file), bytes,
                  source.len, &low);
    /* The log's report reads the diag buffer's own storage, so nothing
       is freed before it is printed. */
    status = finish_build(&log, arena, diag, &low, out, tree_root);
    finish(&source, diag, arena);
    return status;
}
