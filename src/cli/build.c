#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cli/internal.h"

#include "emit/emit.h"
#include "lower/lower.h"
#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"

/* The build command: one Eko file through the whole pipeline into an
   executable. Diagnostics drain through the shared buffer after every
   stage and accumulate here, so the report at the end covers each of
   them in emission order. */

/* Hands the lowered text to emit when the front end is clean -- a
   failed front end leaves no unit worth compiling -- then reports and
   returns the process exit status. The report always prints: warnings
   stand on their own, also over a clean build. */
static int finish_build(cli_diag_log *log, ek_arena *arena, ek_diag *diag,
                        const ek_lower_result *low, const char *out,
                        const char *tree_root, const char *extra)
{
    int failed = cli_log_errors(log) != 0;

    if (!failed) {
        ek_emit_aot(low->text, out, tree_root, extra, arena, diag);
        cli_log_drain(log, diag);
        failed = cli_log_errors(log) != 0;
    }
    cli_log_print(log);
    if (failed) {
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

/* Reports one failure that stopped the command before the pipeline
   could speak for itself, and returns its exit status. */
static int early_failure(ek_arena *arena, ek_diag *diag, const char *file,
                         const char *message)
{
    cli_diag_log log;

    memset(&log, 0, sizeof log);
    log.arena = arena;
    ek_diag_emit(diag, EK_SEV_ERROR, "E-io", file, 0, 0, "%s", message);
    cli_log_drain(&log, diag);
    cli_log_print(&log);
    return 2;
}

int cli_build(const char *file, const char *out, const char *tree_root,
               const char *extra)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();
    cli_diag_log log;
    ek_buf source;
    ek_resolve_result res;
    ek_check_result chk;
    ek_lower_result low;
    const char *bytes;
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
        ek_diag_emit(diag, EK_SEV_ERROR, "E-io", file, 0, 0,
                     "cannot read file: %s", strerror(errno));
        cli_log_drain(&log, diag);
        cli_log_print(&log);
        finish(&source, diag, arena);
        return 2;
    }
    if (out == NULL) {
        out = cli_stem_of(arena, file);
    }
    if (out == NULL) {
        finish(&source, diag, arena);
        return early_failure(arena, diag, file, "out of memory");
    }
    bytes = source.data != NULL ? source.data : "";
    cli_resolve(&log, arena, diag, file, cli_dir_of(arena, file),
                bytes, source.len, &res, &chk);
    if (!cli_has_main(&res)) {
        /* the entry module owes a `main` before anything compiles; a
           missing one otherwise surfaces only as the linker's undefined
           reference (Clause 7.1.3) */
        ek_diag_emit(diag, EK_SEV_ERROR, "E-entry-point", file, 0, 0,
                     "the entry module declares no `main` (Clause 7.1.3)");
        cli_log_drain(&log, diag);
    }
    low = ek_lower(res, chk, arena, diag);
    cli_log_drain(&log, diag);
    /* The log's report reads the diag buffer's own storage, so nothing
       is freed before it is printed. */
    status = finish_build(&log, arena, diag, &low, out, tree_root,
                    extra);
    finish(&source, diag, arena);
    return status;
}
